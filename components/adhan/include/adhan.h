#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * AlAdhan API – query parameters
 * Adjust these constants to fine-tune the prayer-time calculation.
 * ======================================================================== */

/** Calculation method.
 *  11 = Majlis Ugama Islam Singapura (MUIS), Singapore. */
#define ADHAN_METHOD            11

/** Shafaq type used when method=99 (custom). Ignored for other methods.
 *  Valid strings: "general", "ahmer", "abyad". */
#define ADHAN_SHAFAQ            "general"

/** Per-prayer minute adjustments (URL-encoded: comma-separated).
 *  Order: Imsak,Fajr,Sunrise,Dhuhr,Asr,Maghrib,Sunset,Isha,Midnight */
#define ADHAN_TUNE              "1,1,0,2,2,2,-1,1,-6"

/** IANA timezone string for the location. */
#define ADHAN_TIMEZONE          "Asia/Singapore"

/** Islamic calendar method.
 *  "UAQ" = Umm Al-Qura University, Makkah (used by MUIS). */
#define ADHAN_CALENDAR_METHOD   "UAQ"

/* =========================================================================
 * Data structures
 * ======================================================================== */

/**
 * @brief Configuration passed to adhan_init().
 *
 * Latitude and longitude should match the values used in prox.c
 * (OW_LAT / OW_LON).
 */
typedef struct {
    float lat;   /*!< Latitude of the prayer location  */
    float lon;   /*!< Longitude of the prayer location */
} adhan_config_t;

/**
 * @brief Prayer-time strings returned by adhan_fetch().
 *
 * Each field holds a null-terminated "HH:MM" string as returned by
 * the AlAdhan API (local time, already adjusted by the tune offsets).
 * An empty string means the field was not present in the response.
 */
typedef struct {
    char imsak  [6]; /*!< Pre-dawn meal end (Suhoor cut-off) */
    char fajr   [6]; /*!< Dawn prayer                        */
    char dhuhr  [6]; /*!< Midday prayer                      */
    char asr    [6]; /*!< Afternoon prayer                   */
    char maghrib[6]; /*!< Sunset prayer                      */
    char isha   [6]; /*!< Night prayer                       */
} adhan_timings_t;

/* =========================================================================
 * Public API
 * ======================================================================== */

/**
 * @brief Initialise the adhan component with location coordinates.
 *
 * Must be called once before adhan_fetch().
 *
 * @param  config  Pointer to a filled-in adhan_config_t.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if config is NULL.
 */
esp_err_t adhan_init(const adhan_config_t *config);

/**
 * @brief Fetch prayer timings for today's date from the AlAdhan API.
 *
 * Derives today's date from the system clock (previously synchronised via
 * SNTP) and performs an HTTPS GET request to api.aladhan.com.
 * Requires an active network connection.
 *
 * @param[out] out  Pointer to an adhan_timings_t to be filled in.
 * @return ESP_OK on success, or an error code on network / parse failure.
 */
esp_err_t adhan_fetch(adhan_timings_t *out);

#ifdef __cplusplus
}
#endif
