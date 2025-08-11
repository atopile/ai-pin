#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "audio.h"

LOG_MODULE_REGISTER(ai_audio, LOG_LEVEL_INF);

static uint8_t audio_ring_buf_mem[AI_AUDIO_RING_BYTES];
static struct ring_buf audio_ring;

static struct k_thread audio_thread_data;
static K_THREAD_STACK_DEFINE(audio_stack, 2048);
static k_tid_t audio_tid;

static volatile bool running = false;

static void audio_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    // Temporary stub source: generate silence into ring buffer at ~16 kHz
    running = true;
    const uint32_t bytes_per_ms = (AI_AUDIO_SAMPLE_RATE_HZ / 1000) * (AI_AUDIO_BITS_PER_SAMPLE / 8) * AI_AUDIO_CHANNELS;
    uint8_t zero_buf[512] = {0};
    while (running)
    {
        uint32_t to_write = MIN(bytes_per_ms * 10, (uint32_t)sizeof(zero_buf));
        uint32_t wrote = ring_buf_put(&audio_ring, zero_buf, to_write);
        if (wrote < to_write)
        {
            // drop silently
        }
        k_sleep(K_MSEC(10));
    }
}

void audio_init(void)
{
    ring_buf_init(&audio_ring, sizeof(audio_ring_buf_mem), audio_ring_buf_mem);
}

void audio_start(void)
{
    if (audio_tid)
        return;
    audio_tid = k_thread_create(&audio_thread_data, audio_stack, K_THREAD_STACK_SIZEOF(audio_stack),
                                audio_thread, NULL, NULL, NULL, K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
}

void audio_stop(void)
{
    running = false;
}

size_t audio_read(uint8_t *dst, size_t max_len, k_timeout_t timeout)
{
    size_t n = ring_buf_get(&audio_ring, dst, max_len);
    if (n == 0)
    {
        k_sleep(timeout);
        n = ring_buf_get(&audio_ring, dst, max_len);
    }
    return n;
}
