#pragma once

#include <zephyr/kernel.h>

// Audio capture parameters (16 kHz, 16-bit, mono)
#define AI_AUDIO_SAMPLE_RATE_HZ 16000
#define AI_AUDIO_BITS_PER_SAMPLE 16
#define AI_AUDIO_CHANNELS 1

// Size of ring buffer in bytes (2 seconds of audio)
#define AI_AUDIO_RING_BYTES (AI_AUDIO_SAMPLE_RATE_HZ * 2 /*bytes*/ * 2 /*sec*/)

void audio_init(void);
void audio_start(void);
void audio_stop(void);

// Called by storage to pull PCM data; returns bytes copied
size_t audio_read(uint8_t *dst, size_t max_len, k_timeout_t timeout);
