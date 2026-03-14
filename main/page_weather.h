#pragma once

#include "page.h"

/**
 * @brief Weather + time display page.
 *
 * Renders the current local time (large, centred) and a full weather
 * summary on the 128x128 SSD1327 display.
 *
 * Usage:
 *   page_weather.draw(&u8g2, &weather_data);
 */
extern const page_t page_weather;
