#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_sntp.h"
#include "wifi_manager.h"
#include "openweather.h"
#include "adhan.h"
#include "u8g2_esp32_hal.h"
#include "u8g2.h"
#include "page_weather.h"
#include "page_adhan.h"
#include "page_home.h"

static const char *TAG = "app_main";

/* ---- WiFi (see menuconfig → Prox) --------------------------------------- */
#define WIFI_SSID  CONFIG_PROX_WIFI_SSID
#define WIFI_PASS  CONFIG_PROX_WIFI_PASS

/* ---- OpenWeatherMap (see menuconfig → Prox) ------------------------------ */
#define OW_API_KEY  CONFIG_PROX_OW_API_KEY
/* Lat/lon are strings in Kconfig; converted to float in app_main via atof() */

/* ---- SSD1327 SPI pin mapping (see menuconfig → Prox) -------------------- */
#define DISP_MOSI   ((gpio_num_t)CONFIG_PROX_DISP_MOSI)
#define DISP_CLK    ((gpio_num_t)CONFIG_PROX_DISP_CLK)
#define DISP_CS     ((gpio_num_t)CONFIG_PROX_DISP_CS)
#define DISP_DC     ((gpio_num_t)CONFIG_PROX_DISP_DC)
#define DISP_RESET  ((gpio_num_t)CONFIG_PROX_DISP_RESET)

/* ---- BOOT button (see menuconfig → Prox) --------------------------------- */
#define BOOT_BTN    ((gpio_num_t)CONFIG_PROX_BOOT_BTN)

/* ---- Task stack sizes ---------------------------------------------------- */
#define STACK_WEATHER  8192   /* needs HTTPS/TLS headroom */
#define STACK_ADHAN    8192   /* needs HTTPS/TLS headroom */
#define STACK_DISPLAY  4096   /* GPIO + u8g2 drawing only */

/* ---- Task periods -------------------------------------------------------- */
#define WEATHER_FETCH_MS   (30 * 60 * 1000)       /* 30 minutes */
#define ADHAN_FETCH_MS     (6 * 60 * 60 * 1000)   /*  6 hours   */
#define DISPLAY_TICK_MS    100                     /* 100 ms — responsive button */
#define DISPLAY_REFRESH_MS 1000                    /*  1 s  — screen update     */

/* ---- Button debounce ----------------------------------------------------- */
#define BTN_DEBOUNCE_TICKS  3   /* × DISPLAY_TICK_MS = 300 ms hold */

/* ---- Pages --------------------------------------------------------------- */
typedef enum {
    PAGE_HOME = 0,
    PAGE_WEATHER,
    PAGE_ADHAN,
    PAGE_COUNT,
} page_id_t;

/* =========================================================================
 * Shared state
 * All reads/writes to s_weather, s_timings, s_page go through s_data_mutex.
 * ====================================================================== */
static SemaphoreHandle_t   s_data_mutex;
static openweather_data_t  s_weather  = {0};
static adhan_timings_t     s_timings  = {0};
static page_id_t           s_page     = PAGE_HOME;

/* u8g2 instance – only ever touched by display_task */
static u8g2_t u8g2;

/* =========================================================================
 * SNTP helpers
 * ====================================================================== */
static void sntp_init_and_sync(void)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    int retries = 0;
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retries++ < 20) {
        ESP_LOGI(TAG, "Waiting for NTP sync... (%d/20)", retries);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        ESP_LOGI(TAG, "NTP time synchronised.");
    } else {
        ESP_LOGW(TAG, "NTP sync timed out - clock may be wrong.");
    }
}

/* =========================================================================
 * weather_task  –  periodic OpenWeatherMap fetch
 * ====================================================================== */
static void weather_task(void *arg)
{
    ESP_LOGI(TAG, "weather_task started.");

    while (1) {
        if (wifi_manager_is_connected()) {
            openweather_data_t fresh = {0};
            esp_err_t ret = openweather_fetch(&fresh);
            if (ret == ESP_OK) {
                float tc = openweather_kelvin_to_celsius(fresh.temp_kelvin);
                ESP_LOGI(TAG, "Weather: %s, %.1f C, Hum %.0f%%",
                         fresh.description, tc, fresh.humidity);

                xSemaphoreTake(s_data_mutex, portMAX_DELAY);
                memcpy(&s_weather, &fresh, sizeof(s_weather));
                xSemaphoreGive(s_data_mutex);
            } else {
                ESP_LOGE(TAG, "Weather fetch failed: %s", esp_err_to_name(ret));
            }
        } else {
            ESP_LOGW(TAG, "weather_task: WiFi not connected, skipping fetch.");
        }

        vTaskDelay(pdMS_TO_TICKS(WEATHER_FETCH_MS));
    }
}

/* =========================================================================
 * adhan_task  –  periodic AlAdhan prayer-times fetch
 * ====================================================================== */
static void adhan_task(void *arg)
{
    ESP_LOGI(TAG, "adhan_task started.");

    while (1) {
        if (wifi_manager_is_connected()) {
            adhan_timings_t fresh = {0};
            esp_err_t ret = adhan_fetch(&fresh);
            if (ret == ESP_OK) {
                xSemaphoreTake(s_data_mutex, portMAX_DELAY);
                memcpy(&s_timings, &fresh, sizeof(s_timings));
                xSemaphoreGive(s_data_mutex);
            } else {
                ESP_LOGE(TAG, "Adhan fetch failed: %s", esp_err_to_name(ret));
            }
        } else {
            ESP_LOGW(TAG, "adhan_task: WiFi not connected, skipping fetch.");
        }

        vTaskDelay(pdMS_TO_TICKS(ADHAN_FETCH_MS));
    }
}

/* =========================================================================
 * display_task  –  button polling + display rendering
 * ====================================================================== */
static void display_task(void *arg)
{
    ESP_LOGI(TAG, "display_task started.");

    /* --- Button init ---------------------------------------------------- */
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << BOOT_BTN),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_cfg);

    /* --- Local debounce state ------------------------------------------- */
    int  debounce_cnt  = 0;
    bool last_pressed  = false;
    int  refresh_ticks = 0;

    /* Local snapshot of shared data (copied under mutex before drawing) */
    openweather_data_t local_weather = {0};
    adhan_timings_t    local_timings = {0};
    page_id_t          local_page    = PAGE_WEATHER;

    while (1) {
        /* ---- Button: debounced falling-edge detection ------------------- */
        bool pressed = (gpio_get_level(BOOT_BTN) == 0);

        if (pressed) {
            if (++debounce_cnt >= BTN_DEBOUNCE_TICKS && !last_pressed) {
                last_pressed = true;

                /* Toggle page (under mutex) */
                xSemaphoreTake(s_data_mutex, portMAX_DELAY);
                s_page = (page_id_t)((s_page + 1) % PAGE_COUNT);
                xSemaphoreGive(s_data_mutex);

                ESP_LOGI(TAG, "Page -> %s",
                         s_page == PAGE_HOME    ? "home"    :
                         s_page == PAGE_WEATHER ? "weather" : "adhan");

                /* Immediate redraw after page switch */
                refresh_ticks = DISPLAY_REFRESH_MS / DISPLAY_TICK_MS;
            }
        } else {
            debounce_cnt = 0;
            last_pressed = false;
        }

        /* ---- Display refresh every DISPLAY_REFRESH_MS ------------------- */
        if (++refresh_ticks >= (DISPLAY_REFRESH_MS / DISPLAY_TICK_MS)) {
            refresh_ticks = 0;

            /* Snapshot shared data */
            xSemaphoreTake(s_data_mutex, portMAX_DELAY);
            memcpy(&local_weather, &s_weather, sizeof(local_weather));
            memcpy(&local_timings, &s_timings, sizeof(local_timings));
            local_page = s_page;
            xSemaphoreGive(s_data_mutex);

            /* Draw the active page */
            switch (local_page) {
            case PAGE_HOME:
                page_home.draw(&u8g2);
                break;

            case PAGE_WEATHER:
                if (local_weather.city_name[0] != '\0') {
                    page_weather.draw(&u8g2, &local_weather);
                }
                break;

            case PAGE_ADHAN:
                if (local_timings.fajr[0] != '\0') {
                    page_adhan.draw(&u8g2, &local_timings);
                }
                break;

            default:
                break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(DISPLAY_TICK_MS));
    }
}

/* =========================================================================
 * app_main  –  hardware init, service init, then spawn tasks
 * ====================================================================== */
void app_main(void)
{
    /* ------------------------------------------------------------------
     * 1. Initialise SSD1327 display via SPI
     * ---------------------------------------------------------------- */
    u8g2_esp32_hal_t hal = U8G2_ESP32_HAL_DEFAULT;
    hal.bus.spi.host           = SPI2_HOST;
    hal.bus.spi.mosi           = DISP_MOSI;
    hal.bus.spi.clk            = DISP_CLK;
    hal.bus.spi.miso           = (gpio_num_t)-1;
    hal.bus.spi.cs             = DISP_CS;
    hal.bus.spi.clock_speed_hz = 4000000;
    hal.dc                     = DISP_DC;
    hal.reset                  = DISP_RESET;

    u8g2_esp32_hal_init(&hal);
    u8g2_Setup_ssd1327_ws_128x128_f(
        &u8g2, U8G2_R0,
        u8g2_esp32_spi_byte_cb,
        u8g2_esp32_gpio_and_delay_cb);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    /* Splash */
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&u8g2, 10, 55, "Connecting...");
    u8g2_SendBuffer(&u8g2);

    /* ------------------------------------------------------------------
     * 2. Shared data mutex
     * ---------------------------------------------------------------- */
    s_data_mutex = xSemaphoreCreateMutex();
    configASSERT(s_data_mutex);

    /* ------------------------------------------------------------------
     * 3. Connect to WiFi
     * ---------------------------------------------------------------- */
    const wifi_manager_config_t wifi_cfg = {
        .mode      = WIFI_MANAGER_MODE_STA,
        .ssid      = WIFI_SSID,
        .password  = WIFI_PASS,
        .max_retry = 5,
    };
    if (wifi_manager_init(&wifi_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connection failed.");
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
        u8g2_DrawStr(&u8g2, 0, 55, "WiFi failed!");
        u8g2_SendBuffer(&u8g2);
        return;
    }
    ESP_LOGI(TAG, "WiFi connected!");

    /* ------------------------------------------------------------------
     * 4. Sync time via NTP
     * ---------------------------------------------------------------- */
    sntp_init_and_sync();

    /* Apply the timezone selected in menuconfig so that localtime_r()
     * returns the correct local wall-clock time on the home page. */
    setenv("TZ", CONFIG_PROX_TZ_POSIX, 1);
    tzset();
    ESP_LOGI(TAG, "Timezone set to: %s", CONFIG_PROX_TZ_POSIX);

    /* ------------------------------------------------------------------
     * 5. Initialise OpenWeather
     * ---------------------------------------------------------------- */
    float cfg_lat = (float)atof(CONFIG_PROX_OW_LAT);
    float cfg_lon = (float)atof(CONFIG_PROX_OW_LON);

    const openweather_config_t ow_cfg = {
        .api_key = OW_API_KEY,
        .lat     = cfg_lat,
        .lon     = cfg_lon,
    };
    if (openweather_init(&ow_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "openweather_init failed.");
        return;
    }

    /* ------------------------------------------------------------------
     * 6. Initialise Adhan
     * ---------------------------------------------------------------- */
    const adhan_config_t adhan_cfg = {
        .lat = cfg_lat,
        .lon = cfg_lon,
    };
    if (adhan_init(&adhan_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "adhan_init failed.");
        return;
    }

    /* ------------------------------------------------------------------
     * 7. Spawn FreeRTOS tasks
     *
     *  Task          | Stack  | Priority | Notes
     *  --------------|--------|----------|---------------------------------
     *  weather_task  | 8 KB   |    5     | HTTPS/TLS — needs large stack
     *  adhan_task    | 8 KB   |    5     | HTTPS/TLS — needs large stack
     *  display_task  | 4 KB   |    6     | Higher prio for smooth UI
     * ---------------------------------------------------------------- */
    xTaskCreate(weather_task, "weather",  STACK_WEATHER, NULL, 5, NULL);
    xTaskCreate(adhan_task,   "adhan",    STACK_ADHAN,   NULL, 5, NULL);
    xTaskCreate(display_task, "display",  STACK_DISPLAY, NULL, 6, NULL);

    /* app_main can now return — the scheduler keeps the tasks alive */
    ESP_LOGI(TAG, "All tasks started.");
}
