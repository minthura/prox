#include "page_weather.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "openweather.h"
#include "u8g2.h"

/* =========================================================================
 * Local helpers
 * ====================================================================== */

/* Format the current UTC timestamp into a local 12-hour time string
 * including seconds, e.g. "3:34:07 PM".
 * tz_offset_sec is the API's 'timezone' field (seconds east of UTC). */
static void format_local_time_12h(int32_t tz_offset_sec,
                                  char *buf, size_t buf_len)
{
    time_t utc_now  = time(NULL);
    int64_t local   = (int64_t)utc_now + tz_offset_sec;
    int total_secs  = (int)(local % 86400);
    if (total_secs < 0) total_secs += 86400;
    int hour = total_secs / 3600;
    int min  = (total_secs % 3600) / 60;
    int sec  = total_secs % 60;
    const char *ampm = (hour < 12) ? "AM" : "PM";
    int hour12 = hour % 12;
    if (hour12 == 0) hour12 = 12;
    snprintf(buf, buf_len, "%d:%02d:%02d %s", hour12, min, sec, ampm);
}

/* =========================================================================
 * Page draw
 *
 * 128x128 layout (y = baseline):
 *
 *  y=18  Time        — logisoso16 (big, centred)
 *  ─── separator ─────────────────────────────
 *  y=32  City name   — 6x10 (centred)
 *  y=44  Condition   — 6x10
 *  y=56  Temp / Hum  — 6x10
 *  ─── separator ─────────────────────────────
 *  y=72  Sunrise     — 6x10
 *  y=84  Sunset      — 6x10
 *  y=96  Feels like  — 6x10
 *  y=108 Pressure    — 6x10
 *  y=120 Wind        — 6x10
 * ====================================================================== */
static void draw(u8g2_t *u8g2, const openweather_data_t *w)
{
    /* --- Current time --------------------------------------------------- */
    char time_str[20];
    format_local_time_12h(w->timezone_offset, time_str, sizeof(time_str));

    /* --- Derived weather strings ---------------------------------------- */
    float temp_c = openweather_kelvin_to_celsius(w->temp_kelvin);
    char temp_hum[24];
    snprintf(temp_hum, sizeof(temp_hum), "%.1fC  Hum:%.0f%%",
             temp_c, w->humidity);

    float feels_c = openweather_kelvin_to_celsius(w->feels_like_kelvin);
    char feels_str[20];
    snprintf(feels_str, sizeof(feels_str), "Feels like: %.1fC", feels_c);

    char pressure_str[20];
    snprintf(pressure_str, sizeof(pressure_str), "Pressure: %.0fhPa", w->pressure_hpa);

    char wind_str[20];
    snprintf(wind_str, sizeof(wind_str), "Wind: %.1f m/s", w->wind_speed_ms);

    char sunrise_str[12], sunset_str[12];
    openweather_format_time_12h(w->sunrise_utc, w->timezone_offset,
                                sunrise_str, sizeof(sunrise_str));
    openweather_format_time_12h(w->sunset_utc,  w->timezone_offset,
                                sunset_str,  sizeof(sunset_str));

    /* Capitalised description, max 21 chars */
    char desc[22];
    strncpy(desc, w->description, 21);
    desc[21] = '\0';
    if (desc[0] >= 'a' && desc[0] <= 'z') desc[0] -= 32;

    /* --- Draw ----------------------------------------------------------- */
    u8g2_ClearBuffer(u8g2);

    /* Time (large, centred) */
    u8g2_SetFont(u8g2, u8g2_font_logisoso16_tf);
    uint8_t tw = u8g2_GetStrWidth(u8g2, time_str);
    u8g2_DrawStr(u8g2, (128 - tw) / 2, 18, time_str);

    u8g2_DrawHLine(u8g2, 0, 21, 128);

    /* City name (centred) */
    u8g2_SetFont(u8g2, u8g2_font_6x10_tf);
    uint8_t cw = u8g2_GetStrWidth(u8g2, w->city_name);
    u8g2_DrawStr(u8g2, (128 - cw) / 2, 32, w->city_name);

    /* Condition description */
    u8g2_DrawStr(u8g2, 0, 44, desc);

    /* Temperature + humidity */
    u8g2_DrawStr(u8g2, 0, 56, temp_hum);

    u8g2_DrawHLine(u8g2, 0, 60, 128);

    /* Sunrise */
    u8g2_DrawStr(u8g2, 0,  72, "Sunrise:");
    u8g2_DrawStr(u8g2, 56, 72, sunrise_str);

    /* Sunset */
    u8g2_DrawStr(u8g2, 0,  84, "Sunset:");
    u8g2_DrawStr(u8g2, 56, 84, sunset_str);

    /* Feels like */
    u8g2_DrawStr(u8g2, 0,  96, feels_str);

    /* Pressure */
    u8g2_DrawStr(u8g2, 0, 108, pressure_str);

    /* Wind speed */
    u8g2_DrawStr(u8g2, 0, 120, wind_str);

    u8g2_SendBuffer(u8g2);
}

/* =========================================================================
 * Public page instance
 * ====================================================================== */
const page_t page_weather = {
    .draw = draw,
};
