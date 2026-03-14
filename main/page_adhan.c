#include "page_adhan.h"

#include <stdio.h>
#include <string.h>
#include "u8g2.h"
#include "adhan.h"

/* =========================================================================
 * Local helpers
 * ====================================================================== */

/**
 * @brief Convert an "HH:MM" 24-hour string to a 12-hour "H:MM AM/PM" string.
 *
 * @param src    Null-terminated "HH:MM" string from the AlAdhan API.
 * @param dst    Output buffer (at least 10 bytes recommended).
 * @param dst_sz Size of the output buffer.
 */
static void convert_to_12h(const char *src, char *dst, size_t dst_sz)
{
    int hour = 0, minute = 0;

    if (src == NULL || src[0] == '\0') {
        snprintf(dst, dst_sz, "--:--");
        return;
    }

    /* Parse "HH:MM" */
    if (sscanf(src, "%d:%d", &hour, &minute) != 2) {
        snprintf(dst, dst_sz, "--:--");
        return;
    }

    const char *ampm = (hour < 12) ? "AM" : "PM";
    int hour12 = hour % 12;
    if (hour12 == 0) hour12 = 12;

    snprintf(dst, dst_sz, "%d:%02d %s", hour12, minute, ampm);
}

/* =========================================================================
 * Layout constants — single-column, one prayer per row, no dividers
 *
 * 128x128 pixel screen, SSD1327:
 *
 *   y=14   Title "Prayer Times"     — u8g2_font_8x13_tf, centred
 *   y=30   Imsak    |  5:20AM       — u8g2_font_7x13_tf
 *   y=47   Fajr     |  5:35AM
 *   y=64   Dhuhr    |  1:10PM
 *   y=81   Asr      |  4:30PM
 *   y=98   Maghrib  |  7:20PM
 *   y=115  Isha     |  8:45PM
 *
 *   x=0   – prayer name (left-aligned)
 *   x=72  – time value  (left-aligned, right section)
 *
 *   Both name and value use u8g2_font_7x13_tf (7×13, bold-friendly).
 * ====================================================================== */

#define COL_NAME   0
#define COL_TIME   72

/* Row baselines (y), spaced ~17px apart starting after the title */
#define ROW_IMSAK_Y    30
#define ROW_FAJR_Y     47
#define ROW_DHUHR_Y    64
#define ROW_ASR_Y      81
#define ROW_MAGHRIB_Y  98
#define ROW_ISHA_Y    115

/* =========================================================================
 * draw()
 * ====================================================================== */
static void draw(u8g2_t *u8g2, const adhan_timings_t *t)
{
    /* Pre-convert all six timings to 12-hour strings */
    char imsak_str  [10], fajr_str   [10];
    char dhuhr_str  [10], asr_str    [10];
    char maghrib_str[10], isha_str   [10];

    convert_to_12h(t->imsak,   imsak_str,   sizeof(imsak_str));
    convert_to_12h(t->fajr,    fajr_str,    sizeof(fajr_str));
    convert_to_12h(t->dhuhr,   dhuhr_str,   sizeof(dhuhr_str));
    convert_to_12h(t->asr,     asr_str,     sizeof(asr_str));
    convert_to_12h(t->maghrib, maghrib_str, sizeof(maghrib_str));
    convert_to_12h(t->isha,    isha_str,    sizeof(isha_str));

    /* ------------------------------------------------------------------
     * Render — single-column, one prayer per row, no dividers
     * ---------------------------------------------------------------- */
    u8g2_ClearBuffer(u8g2);

    /* ---- Title -------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_8x13_tf);
    const char *title = "Prayer Times";
    uint8_t tw = u8g2_GetStrWidth(u8g2, title);
    u8g2_DrawStr(u8g2, (128 - tw) / 2, 14, title);

    /* ---- Six prayer rows (name left, time right, same font) ---------- */
    u8g2_SetFont(u8g2, u8g2_font_7x13_tf);

    u8g2_DrawStr(u8g2, COL_NAME, ROW_IMSAK_Y,   "Imsak");
    u8g2_DrawStr(u8g2, COL_TIME, ROW_IMSAK_Y,   imsak_str);

    u8g2_DrawStr(u8g2, COL_NAME, ROW_FAJR_Y,    "Fajr");
    u8g2_DrawStr(u8g2, COL_TIME, ROW_FAJR_Y,    fajr_str);

    u8g2_DrawStr(u8g2, COL_NAME, ROW_DHUHR_Y,   "Dhuhr");
    u8g2_DrawStr(u8g2, COL_TIME, ROW_DHUHR_Y,   dhuhr_str);

    u8g2_DrawStr(u8g2, COL_NAME, ROW_ASR_Y,     "Asr");
    u8g2_DrawStr(u8g2, COL_TIME, ROW_ASR_Y,     asr_str);

    u8g2_DrawStr(u8g2, COL_NAME, ROW_MAGHRIB_Y, "Maghrib");
    u8g2_DrawStr(u8g2, COL_TIME, ROW_MAGHRIB_Y, maghrib_str);

    u8g2_DrawStr(u8g2, COL_NAME, ROW_ISHA_Y,    "Isha");
    u8g2_DrawStr(u8g2, COL_TIME, ROW_ISHA_Y,    isha_str);

    u8g2_SendBuffer(u8g2);
}

/* =========================================================================
 * Public page instance
 * ====================================================================== */
const page_adhan_t page_adhan = {
    .draw = draw,
};
