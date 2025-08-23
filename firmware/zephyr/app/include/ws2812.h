#pragma once

#include <zephyr/kernel.h>

/*
 Minimal WS2812/SK6805 single-LED driver using GPIO bit-banging.
 Timing is approximate; sufficient for a single LED status indicator.
*/

void ws2812_init(void);
void ws2812_set_rgb(uint8_t r, uint8_t g, uint8_t b);
void ws2812_off(void);
