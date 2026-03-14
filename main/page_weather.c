#include "page_weather.h"

#include <stdio.h>
#include <string.h>
#include "openweather.h"
#include "u8g2.h"

/* =========================================================================
 * Page draw
 *
 * 128x128 layout (y = baseline, font heights in brackets):
 *
 *  y=20  City name   — logisoso16 [h=18] (large, centred)
 *  ─── separator ─────────────────────────────────────────
 *  y=36  Condition   — 6x10
 *  y=48  Temp / Hum  — 6x10   e.g. "28.5C  Hum:72%"
 *  y=60  Feels like  — 6x10   e.g. "Feels: 30.1C"
 *  ─── separator ─────────────────────────────────────────
 *  y=76  Pressure    — 6x10   e.g. "Pressure: 1012 hPa"
 *  y=88  Wind        — 6x10   e.g. "Wind: 3.5 m/s"
 *  ─── separator ─────────────────────────────────────────
 *  y=104 Sunrise     — 6x10   label + value right of it
 *  y=116 Sunset      — 6x10
 * ====================================================================== */
static void draw(u8g2_t *u8g2, const openweather_data_t *w)
{
    /* --- Derived weather strings ---------------------------------------- */
    float temp_c = openweather_kelvin_to_celsius(w->temp_kelvin);
    char temp_hum[24];
    snprintf(temp_hum, sizeof(temp_hum), "%.1fC  Hum:%.0f%%",
             temp_c, w->humidity);

    float feels_c = openweather_kelvin_to_celsius(w->feels_like_kelvin);
    char feels_str[20];
    snprintf(feels_str, sizeof(feels_str), "Feels: %.1fC", feels_c);

    char pressure_str[22];
    snprintf(pressure_str, sizeof(pressure_str), "Pressure: %.0f hPa", w->pressure_hpa);

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

    /* City name (large, centred) */
    u8g2_SetFont(u8g2, u8g2_font_t0_11b_tf);
    uint8_t cw = u8g2_GetStrWidth(u8g2, w->city_name);
    u8g2_DrawStr(u8g2, (128 - cw) / 2, 20, w->city_name);

    u8g2_DrawHLine(u8g2, 0, 24, 128);

    /* Condition description */
    u8g2_SetFont(u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(u8g2, 0, 36, desc);

    /* Temperature + humidity */
    u8g2_DrawStr(u8g2, 0, 48, temp_hum);

    /* Feels like */
    u8g2_DrawStr(u8g2, 0, 60, feels_str);

    u8g2_DrawHLine(u8g2, 0, 64, 128);

    /* Pressure */
    u8g2_DrawStr(u8g2, 0, 76, pressure_str);

    /* Wind speed */
    u8g2_DrawStr(u8g2, 0, 88, wind_str);

    u8g2_DrawHLine(u8g2, 0, 92, 128);

    /* Sunrise */
    u8g2_DrawStr(u8g2, 0,  104, "Sunrise:");
    u8g2_DrawStr(u8g2, 56, 104, sunrise_str);

    /* Sunset */
    u8g2_DrawStr(u8g2, 0,  116, "Sunset:");
    u8g2_DrawStr(u8g2, 56, 116, sunset_str);

    u8g2_SendBuffer(u8g2);
}

/* =========================================================================
 * Public page instance
 * ====================================================================== */
const page_t page_weather = {
    .draw = draw,
};
