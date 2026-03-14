#pragma once

/**
 * @file u8g2_esp32_hal.h
 * @brief ESP-IDF HAL for the u8g2 display library.
 *
 * Provides the three callbacks that u8g2 requires on any platform:
 *   1. Byte-transport via hardware SPI  (u8g2_esp32_spi_byte_cb)
 *   2. Byte-transport via hardware I2C  (u8g2_esp32_i2c_byte_cb)
 *   3. GPIO and delay                   (u8g2_esp32_gpio_and_delay_cb)
 *
 * Usage – SPI example:
 * @code
 *   u8g2_esp32_hal_t cfg = U8G2_ESP32_HAL_DEFAULT;
 *   cfg.bus.spi.host   = SPI2_HOST;
 *   cfg.bus.spi.clk    = GPIO_NUM_18;
 *   cfg.bus.spi.mosi   = GPIO_NUM_23;
 *   cfg.bus.spi.miso   = GPIO_NUM_19;   // set to -1 if not needed
 *   cfg.bus.spi.cs     = GPIO_NUM_5;
 *   cfg.dc             = GPIO_NUM_16;
 *   cfg.reset          = GPIO_NUM_17;   // set to -1 if not used
 *
 *   u8g2_esp32_hal_init(&cfg);
 *
 *   u8g2_t u8g2;
 *   u8g2_Setup_ssd1306_128x64_noname_f(&u8g2,
 *       U8G2_R0,
 *       u8g2_esp32_spi_byte_cb,
 *       u8g2_esp32_gpio_and_delay_cb);
 *   u8g2_InitDisplay(&u8g2);
 *   u8g2_SetPowerSave(&u8g2, 0);
 * @endcode
 *
 * Usage – I2C example:
 * @code
 *   u8g2_esp32_hal_t cfg = U8G2_ESP32_HAL_DEFAULT;
 *   cfg.bus.i2c.port  = I2C_NUM_0;
 *   cfg.bus.i2c.sda   = GPIO_NUM_21;
 *   cfg.bus.i2c.scl   = GPIO_NUM_22;
 *   cfg.bus.i2c.speed = 400000;
 *
 *   u8g2_esp32_hal_init(&cfg);
 *
 *   u8g2_t u8g2;
 *   u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2,
 *       U8G2_R0,
 *       u8g2_esp32_i2c_byte_cb,
 *       u8g2_esp32_gpio_and_delay_cb);
 *   u8g2_InitDisplay(&u8g2);
 *   u8g2_SetPowerSave(&u8g2, 0);
 * @endcode
 */

#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * SPI bus configuration
 * ---------------------------------------------------------------------- */
typedef struct {
    spi_host_device_t host;  /*!< SPI peripheral: SPI2_HOST, SPI3_HOST … */
    gpio_num_t        clk;   /*!< SCLK pin  */
    gpio_num_t        mosi;  /*!< MOSI pin  */
    gpio_num_t        miso;  /*!< MISO pin, set to -1 when not needed  */
    gpio_num_t        cs;    /*!< Chip-select pin (software-controlled)  */
    int               clock_speed_hz; /*!< Bus clock, e.g. 4000000 (4 MHz)  */
} u8g2_esp32_spi_cfg_t;

/* -------------------------------------------------------------------------
 * I2C bus configuration
 * ---------------------------------------------------------------------- */
typedef struct {
    i2c_port_t port;        /*!< I2C_NUM_0 or I2C_NUM_1  */
    gpio_num_t sda;         /*!< SDA pin  */
    gpio_num_t scl;         /*!< SCL pin  */
    uint32_t   speed;       /*!< Bus speed in Hz, e.g. 400000  */
} u8g2_esp32_i2c_cfg_t;

/* -------------------------------------------------------------------------
 * Top-level HAL configuration
 * ---------------------------------------------------------------------- */
typedef struct {
    union {
        u8g2_esp32_spi_cfg_t spi;
        u8g2_esp32_i2c_cfg_t i2c;
    } bus;

    gpio_num_t dc;      /*!< Data/Command pin (SPI only); -1 if unused  */
    gpio_num_t reset;   /*!< Reset pin; set to -1 if not connected       */
} u8g2_esp32_hal_t;

/** Sensible defaults – every field that makes sense defaults to -1/0. */
#define U8G2_ESP32_HAL_DEFAULT  {                                   \
    .bus.spi = {                                                     \
        .host          = SPI2_HOST,                                  \
        .clk           = (gpio_num_t)-1,                             \
        .mosi          = (gpio_num_t)-1,                             \
        .miso          = (gpio_num_t)-1,                             \
        .cs            = (gpio_num_t)-1,                             \
        .clock_speed_hz = 4000000,                                   \
    },                                                               \
    .dc    = (gpio_num_t)-1,                                         \
    .reset = (gpio_num_t)-1,                                         \
}

/**
 * @brief Initialise the HAL with the given configuration.
 *
 * Must be called once before the u8g2 setup function.
 * Configures SPI master or I2C master (whichever pins are set).
 *
 * @param cfg  Pointer to a filled-in u8g2_esp32_hal_t.
 */
void u8g2_esp32_hal_init(const u8g2_esp32_hal_t *cfg);

/**
 * @brief u8g2 byte callback – hardware SPI transport.
 *
 * Pass this as the byte_cb argument to a u8g2_Setup_*_f() function
 * that uses 4-wire SPI.
 */
uint8_t u8g2_esp32_spi_byte_cb(u8x8_t *u8x8, uint8_t msg,
                                uint8_t arg_int, void *arg_ptr);

/**
 * @brief u8g2 byte callback – hardware I2C transport.
 *
 * Pass this as the byte_cb argument to a u8g2_Setup_*_i2c_*_f() function.
 */
uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg,
                                uint8_t arg_int, void *arg_ptr);

/**
 * @brief u8g2 GPIO and delay callback – shared by SPI and I2C.
 *
 * Pass this as the gpio_and_delay_cb argument to any u8g2_Setup_*() call.
 */
uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg,
                                     uint8_t arg_int, void *arg_ptr);

#ifdef __cplusplus
}
#endif
