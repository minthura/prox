#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration for the OpenWeatherMap client.
 */
typedef struct {
    const char *api_key;    /*!< OpenWeatherMap API key (appid) */
    float        lat;       /*!< Latitude  */
    float        lon;       /*!< Longitude */
} openweather_config_t;

/**
 * @brief Holds parsed weather information returned by the API.
 */
typedef struct {
    char     description[64];   /*!< Human-readable description, e.g. "light rain" */
    char     main[32];          /*!< Main category, e.g. "Rain"                     */
    float    temp_kelvin;       /*!< Temperature in Kelvin                          */
    float    feels_like_kelvin; /*!< Feels-like temperature in Kelvin               */
    float    humidity;          /*!< Humidity in %                                  */
    float    pressure_hpa;      /*!< Atmospheric pressure in hPa                    */
    float    wind_speed_ms;     /*!< Wind speed in m/s                              */
    char     city_name[64];     /*!< City name returned by the API                  */
    int64_t  sunrise_utc;       /*!< Sunrise as a Unix UTC timestamp                */
    int64_t  sunset_utc;        /*!< Sunset  as a Unix UTC timestamp                */
    int32_t  timezone_offset;   /*!< Shift in seconds from UTC (from API 'timezone')*/
} openweather_data_t;

/**
 * @brief Initialise the openweather component with a configuration.
 *
 * Copies the configuration so the caller does not need to keep it alive.
 *
 * @param  config  Pointer to a filled-in openweather_config_t.
 * @return ESP_OK on success.
 */
esp_err_t openweather_init(const openweather_config_t *config);

/**
 * @brief Fetch the current weather from the OpenWeatherMap API.
 *
 * Performs an HTTPS GET request and populates @p out with the parsed data.
 * Requires an active network connection.
 *
 * @param[out] out  Pointer to an openweather_data_t to be filled in.
 * @return ESP_OK on success, or an error code on network / parse failure.
 */
esp_err_t openweather_fetch(openweather_data_t *out);

/**
 * @brief Convert a Kelvin temperature to Celsius.
 */
static inline float openweather_kelvin_to_celsius(float k) { return k - 273.15f; }

/**
 * @brief Format a UTC Unix timestamp as a local 12-hour time string (e.g. "6:32 AM").
 *
 * @param utc_ts         Unix timestamp in UTC seconds.
 * @param tz_offset_sec  Timezone offset in seconds as returned by the API.
 * @param buf            Output buffer (at least 12 bytes recommended).
 * @param buf_len        Size of @p buf.
 */
static inline void openweather_format_time_12h(int64_t utc_ts, int32_t tz_offset_sec,
                                               char *buf, size_t buf_len)
{
    int64_t local_ts = utc_ts + tz_offset_sec;
    int total_secs   = (int)(local_ts % 86400);
    if (total_secs < 0) total_secs += 86400;
    int hour = total_secs / 3600;
    int min  = (total_secs % 3600) / 60;
    const char *ampm = (hour < 12) ? "AM" : "PM";
    int hour12 = hour % 12;
    if (hour12 == 0) hour12 = 12;
    snprintf(buf, buf_len, "%d:%02d %s", hour12, min, ampm);
}

#ifdef __cplusplus
}
#endif
