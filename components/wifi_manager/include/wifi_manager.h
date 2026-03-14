#pragma once

#include "esp_err.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi mode selection
 */
typedef enum {
    WIFI_MANAGER_MODE_STA = 0,  /*!< Station mode – connect to an AP */
    WIFI_MANAGER_MODE_AP,       /*!< Access-Point mode */
    WIFI_MANAGER_MODE_APSTA,    /*!< Combined AP + Station mode */
} wifi_manager_mode_t;

/**
 * @brief Configuration passed to wifi_manager_init()
 */
typedef struct {
    wifi_manager_mode_t mode;
    const char *ssid;       /*!< Target AP SSID (STA / APSTA modes) */
    const char *password;   /*!< Target AP password  */
    uint8_t     max_retry;  /*!< How many times to retry before giving up */
} wifi_manager_config_t;

/**
 * @brief Initialise NVS, netif, event loop and WiFi driver, then attempt
 *        to connect using the provided configuration.
 *
 * Blocks until an IP address is obtained or max_retry is exhausted.
 *
 * @param  config  Pointer to a filled-in wifi_manager_config_t struct.
 * @return ESP_OK on successful IP acquisition, ESP_FAIL otherwise.
 */
esp_err_t wifi_manager_init(const wifi_manager_config_t *config);

/**
 * @brief Disconnect and deinitialise the WiFi driver.
 */
void wifi_manager_deinit(void);

/**
 * @brief Returns true if the station is connected and has an IP address.
 */
bool wifi_manager_is_connected(void);

#ifdef __cplusplus
}
#endif
