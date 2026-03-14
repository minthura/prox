/**
 * @file u8g2_esp32_hal.c
 * @brief ESP-IDF HAL implementation for the u8g2 display library.
 *
 * Implements SPI (4-wire) and I2C byte transports plus the GPIO/delay
 * callback using ESP-IDF v5.x driver APIs.
 */

#include "u8g2_esp32_hal.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "rom/ets_sys.h"

static const char *TAG = "u8g2_hal";

/* -------------------------------------------------------------------------
 * Static state – one HAL instance at a time
 * ---------------------------------------------------------------------- */
static u8g2_esp32_hal_t s_hal_cfg;
static spi_device_handle_t s_spi = NULL;
static bool s_i2c_installed = false;

/* =========================================================================
 * Initialisation
 * ====================================================================== */
void u8g2_esp32_hal_init(const u8g2_esp32_hal_t *cfg)
{
    assert(cfg != NULL);
    memcpy(&s_hal_cfg, cfg, sizeof(s_hal_cfg));

    /* Determine if caller wants SPI or I2C from which pins are set */
    bool use_spi = (cfg->bus.spi.clk != (gpio_num_t)-1);
    bool use_i2c = !use_spi && (cfg->bus.i2c.sda != (gpio_num_t)-1);

    /* ---- Reset pin (optional, shared by both buses) ------------------- */
    if (cfg->reset != (gpio_num_t)-1) {
        gpio_config_t rst_cfg = {
            .pin_bit_mask = 1ULL << cfg->reset,
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        gpio_config(&rst_cfg);
        gpio_set_level(cfg->reset, 1);
    }

    if (use_spi) {
        /* ---- Data/Command pin ---------------------------------------- */
        if (cfg->dc != (gpio_num_t)-1) {
            gpio_config_t dc_cfg = {
                .pin_bit_mask = 1ULL << cfg->dc,
                .mode         = GPIO_MODE_OUTPUT,
                .pull_up_en   = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type    = GPIO_INTR_DISABLE,
            };
            gpio_config(&dc_cfg);
        }

        /* ---- SPI bus ------------------------------------------------- */
        spi_bus_config_t bus_cfg = {
            .mosi_io_num     = cfg->bus.spi.mosi,
            .miso_io_num     = cfg->bus.spi.miso,
            .sclk_io_num     = cfg->bus.spi.clk,
            .quadwp_io_num   = -1,
            .quadhd_io_num   = -1,
            .max_transfer_sz = 0,   /* use default */
        };
        ESP_ERROR_CHECK(spi_bus_initialize(cfg->bus.spi.host, &bus_cfg,
                                           SPI_DMA_CH_AUTO));

        /* ---- SPI device ---------------------------------------------- */
        spi_device_interface_config_t dev_cfg = {
            .clock_speed_hz = cfg->bus.spi.clock_speed_hz > 0
                              ? cfg->bus.spi.clock_speed_hz : 4000000,
            .mode           = 0,               /* CPOL=0, CPHA=0 */
            .spics_io_num   = cfg->bus.spi.cs, /* software CS */
            .queue_size     = 7,
            .flags          = 0,
        };
        ESP_ERROR_CHECK(spi_bus_add_device(cfg->bus.spi.host, &dev_cfg, &s_spi));
        ESP_LOGI(TAG, "SPI HAL ready (host=%d)", cfg->bus.spi.host);

    } else if (use_i2c) {
        /* ---- I2C master ---------------------------------------------- */
        i2c_config_t i2c_cfg = {
            .mode             = I2C_MODE_MASTER,
            .sda_io_num       = cfg->bus.i2c.sda,
            .scl_io_num       = cfg->bus.i2c.scl,
            .sda_pullup_en    = GPIO_PULLUP_ENABLE,
            .scl_pullup_en    = GPIO_PULLUP_ENABLE,
            .master.clk_speed = cfg->bus.i2c.speed > 0
                                ? cfg->bus.i2c.speed : 400000,
        };
        ESP_ERROR_CHECK(i2c_param_config(cfg->bus.i2c.port, &i2c_cfg));
        ESP_ERROR_CHECK(i2c_driver_install(cfg->bus.i2c.port,
                                           I2C_MODE_MASTER, 0, 0, 0));
        s_i2c_installed = true;
        ESP_LOGI(TAG, "I2C HAL ready (port=%d)", cfg->bus.i2c.port);
    }
}

/* =========================================================================
 * SPI byte callback
 * ====================================================================== */
uint8_t u8g2_esp32_spi_byte_cb(u8x8_t *u8x8, uint8_t msg,
                                uint8_t arg_int, void *arg_ptr)
{
    static spi_transaction_t t;

    switch (msg) {
    case U8X8_MSG_BYTE_SEND:
        memset(&t, 0, sizeof(t));
        t.length    = arg_int * 8;   /* bits */
        t.tx_buffer = arg_ptr;
        ESP_ERROR_CHECK(spi_device_transmit(s_spi, &t));
        break;

    case U8X8_MSG_BYTE_INIT:
        /* Nothing extra – init done by u8g2_esp32_hal_init() */
        break;

    case U8X8_MSG_BYTE_SET_DC:
        if (s_hal_cfg.dc != (gpio_num_t)-1) {
            gpio_set_level(s_hal_cfg.dc, arg_int);
        }
        break;

    case U8X8_MSG_BYTE_START_TRANSFER:
        /* CS is managed automatically by the SPI driver */
        break;

    case U8X8_MSG_BYTE_END_TRANSFER:
        break;

    default:
        return 0;
    }
    return 1;
}

/* =========================================================================
 * I2C byte callback
 * ====================================================================== */
uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg,
                                uint8_t arg_int, void *arg_ptr)
{
    static i2c_cmd_handle_t cmd = NULL;

    switch (msg) {
    case U8X8_MSG_BYTE_INIT:
        /* Already done by u8g2_esp32_hal_init() */
        break;

    case U8X8_MSG_BYTE_START_TRANSFER:
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        /* u8x8 stores the 7-bit device address */
        i2c_master_write_byte(cmd,
            (u8x8_GetI2CAddress(u8x8) << 1) | I2C_MASTER_WRITE,
            true);
        break;

    case U8X8_MSG_BYTE_SEND:
        i2c_master_write(cmd, (uint8_t *)arg_ptr, arg_int, true);
        break;

    case U8X8_MSG_BYTE_END_TRANSFER:
        i2c_master_stop(cmd);
        ESP_ERROR_CHECK(i2c_master_cmd_begin(
            s_hal_cfg.bus.i2c.port, cmd,
            pdMS_TO_TICKS(100)));
        i2c_cmd_link_delete(cmd);
        cmd = NULL;
        break;

    default:
        return 0;
    }
    return 1;
}

/* =========================================================================
 * GPIO and delay callback (shared by SPI and I2C)
 * ====================================================================== */
uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg,
                                     uint8_t arg_int, void *arg_ptr)
{
    switch (msg) {
    /* -- Delays --------------------------------------------------------- */
    case U8X8_MSG_DELAY_100NANO:
        /* esp_rom_delay_us handles sub-microsecond delays */
        ets_delay_us(1);
        break;

    case U8X8_MSG_DELAY_10MICRO:
        ets_delay_us(10);
        break;

    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(pdMS_TO_TICKS(arg_int));
        break;

    case U8X8_MSG_DELAY_I2C:
        /* arg_int: 0 = 100 kHz (5 µs), 1 = 400 kHz (1.25 µs) */
        ets_delay_us(arg_int == 0 ? 5 : 2);
        break;

    /* -- Reset pin ------------------------------------------------------ */
    case U8X8_MSG_GPIO_RESET:
        if (s_hal_cfg.reset != (gpio_num_t)-1) {
            gpio_set_level(s_hal_cfg.reset, arg_int);
        }
        break;

    /* -- CS pin (software, SPI) ---------------------------------------- */
    case U8X8_MSG_GPIO_CS:
        if (s_hal_cfg.bus.spi.cs != (gpio_num_t)-1) {
            gpio_set_level(s_hal_cfg.bus.spi.cs, arg_int);
        }
        break;

    /* -- D/C pin (SPI) -------------------------------------------------- */
    case U8X8_MSG_GPIO_DC:
        if (s_hal_cfg.dc != (gpio_num_t)-1) {
            gpio_set_level(s_hal_cfg.dc, arg_int);
        }
        break;

    /* -- I2C clock / data (bit-bang, not normally used with HW I2C) ----- */
    case U8X8_MSG_GPIO_I2C_CLOCK:
    case U8X8_MSG_GPIO_I2C_DATA:
        /* Hardware I2C – nothing to do here */
        break;

    case U8X8_MSG_GPIO_MENU_SELECT:
    case U8X8_MSG_GPIO_MENU_NEXT:
    case U8X8_MSG_GPIO_MENU_PREV:
    case U8X8_MSG_GPIO_MENU_HOME:
        u8x8_SetGPIOResult(u8x8, /* get menu-button state */ 0);
        break;

    default:
        return 0;
    }
    return 1;
}
