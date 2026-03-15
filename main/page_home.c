#include "page_home.h"

#include <stdio.h>
#include <time.h>
#include "u8g2.h"

/* =========================================================================
 * Page draw
 *
 * 128x128 layout (y = baseline):
 *
 *  Seconds bar — top strip (y 0..5):
 *    60 bars × 2 px wide, 1 px gap → 60*3-1 = 179… use 2 px + 0 gap = 120 px
 *    Offset from left: (128 - 120) / 2 = 4 px
 *    Filled bars = t.tm_sec (0 means empty, 59 means 59 bars filled)
 *
 *
 *  Vertically centred block:
 *    Time  — logisoso16  (12-hour HH:MM:SS AM/PM, ~18 px tall)
 *    gap   — 8 px
 *    Date  — t0_11b      (DD MMM YYYY, ~11 px tall)
 *
 *  Total block height ≈ 18 + 8 + 11 = 37 px
 *  Block top ≈ (128 - 37) / 2 = 45 px
 *  Time baseline ≈ 45 + 18 = 63
 *  Date baseline ≈ 63 + 8 + 11 = 82
 * ====================================================================== */

static const char *month_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static void draw(u8g2_t *u8g2)
{
    /* --- Get local time ------------------------------------------------- */
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);

    /* --- Time string: H:MM:SS AM/PM ------------------------------------- */
    char time_str[16];
    int hour12 = t.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    const char *ampm = (t.tm_hour < 12) ? "AM" : "PM";
    snprintf(time_str, sizeof(time_str), "%d:%02d:%02d %s",
             hour12, t.tm_min, t.tm_sec, ampm);

    /* --- Date string: DD MMM YYYY --------------------------------------- */
    char date_str[16];
    snprintf(date_str, sizeof(date_str), "%d %s %d",
             t.tm_mday,
             month_names[t.tm_mon],
             t.tm_year + 1900);

    /* --- Draw ----------------------------------------------------------- */
    u8g2_ClearBuffer(u8g2);

    /* Seconds progress bar — top strip
     * 60 bars, each 2 px wide × 5 px tall, no gap between bars.
     * Total width = 60 * 2 = 120 px; left offset = (128-120)/2 = 4 px.
     * Filled count = t.tm_sec  (0 = no bars, 59 = 59 bars filled).        */
    {
        const int bar_w   = 2;
        const int bar_h   = 5;
        const int n_bars  = 60;
        const int strip_x = (128 - n_bars * bar_w) / 2;  /* = 4 */
        for (int i = 0; i < t.tm_sec; i++) {
            u8g2_DrawBox(u8g2, strip_x + i * bar_w, 0, bar_w - 1, bar_h);
        }
    }

    /* Time — logisoso16 (~18 px ascent), shifted down 8 px → baseline y=71 */
    u8g2_SetFont(u8g2, u8g2_font_logisoso16_tf);
    uint16_t tw = u8g2_GetStrWidth(u8g2, time_str);
    u8g2_DrawStr(u8g2, (128 - tw) / 2, 71, time_str);

    /* Date — bytesize (~11 px ascent), baseline y=90 */
    u8g2_SetFont(u8g2, u8g2_font_bytesize_tf);
    uint16_t dw = u8g2_GetStrWidth(u8g2, date_str);
    u8g2_DrawStr(u8g2, (128 - dw) / 2, 90, date_str);

    u8g2_SendBuffer(u8g2);
}

/* =========================================================================
 * Public page instance
 * ====================================================================== */
const page_home_t page_home = {
    .draw = draw,
};
