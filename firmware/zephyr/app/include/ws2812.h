#pragma once

#include <zephyr/kernel.h>

/*
 Minimal WS2812/SK6805 single-LED driver using GPIO bit-banging.
 Timing is approximate; sufficient for a single LED status indicator.
*/

void ws2812_init(void);
void ws2812_set_rgb(uint8_t r, uint8_t g, uint8_t b);
void ws2812_off(void);
/* Allow tuning of timing (counts of inline NOPs) */
void ws2812_set_timing(uint32_t t0h_nops, uint32_t t0l_nops, uint32_t t1h_nops, uint32_t t1l_nops);
