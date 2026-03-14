#pragma once

#include "u8g2.h"
#include "adhan.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Islamic prayer-times display page.
 *
 * Renders Imsak, Fajr, Dhuhr, Asr, Maghrib and Isha (converted to 12-hour
 * AM/PM format) on the 128x128 SSD1327 OLED display.
 *
 * Usage:
 *   page_adhan.draw(&u8g2, &adhan_timings);
 */

typedef void (*page_adhan_draw_fn)(u8g2_t *u8g2, const adhan_timings_t *t);

typedef struct {
    page_adhan_draw_fn draw;
} page_adhan_t;

extern const page_adhan_t page_adhan;

#ifdef __cplusplus
}
#endif
