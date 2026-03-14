#pragma once

#include "u8g2.h"
#include "openweather.h"

/**
 * @brief Common interface for all display pages.
 *
 * Each page module exposes a single `const page_t` instance that contains
 * a pointer to its draw function.  The caller (app_main) drives the page
 * lifecycle; pages are stateless from the perspective of this interface.
 *
 * Adding a new page:
 *   1. Create page_<name>.c / page_<name>.h
 *   2. Define a static draw() function matching page_draw_fn
 *   3. Expose: const page_t page_<name> = { .draw = draw };
 *   4. Add page_<name>.c to SRCS in main/CMakeLists.txt
 */

/** Function signature every page draw function must match. */
typedef void (*page_draw_fn)(u8g2_t *u8g2, const openweather_data_t *w);

/** A display page – currently just a draw callback. */
typedef struct {
    page_draw_fn draw;
} page_t;
