#pragma once

#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Home page — shows current time (12h with seconds) and date.
 *
 * Reads the system clock directly (no external data needed).
 * Time is rendered large and centred; date is rendered smaller below it.
 *
 * Usage:
 *   page_home.draw(&u8g2);
 */

typedef void (*page_home_draw_fn)(u8g2_t *u8g2);

typedef struct {
    page_home_draw_fn draw;
} page_home_t;

extern const page_home_t page_home;

#ifdef __cplusplus
}
#endif
