#pragma once

#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>

// Initialize filesystem and worker thread
int storage_init(void);
int storage_start(void);

// File rotation control (duration in seconds)
void storage_set_segment_duration(uint32_t seconds);

// Simple API to signal start/stop of recording
void storage_begin_recording(void);
void storage_end_recording(void);

// Status helpers
bool storage_is_mounted(void);
