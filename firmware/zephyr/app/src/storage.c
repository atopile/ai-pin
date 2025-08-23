#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>
#include <zephyr/drivers/disk.h>
#include <zephyr/logging/log.h>
#include <zephyr/posix/time.h>

#include "audio.h"
#include "storage.h"

LOG_MODULE_REGISTER(ai_storage, LOG_LEVEL_INF);

// Mount point for FATFS
static const char *mount_point = "/SD:";

static struct fs_mount_t mp = {
    .type = FS_FATFS,
    .mnt_point = "/SD:",
};

static struct k_thread storage_thread_data;
static K_THREAD_STACK_DEFINE(storage_stack, 4096);
static k_tid_t storage_tid;

static volatile bool recording = true;
static uint32_t segment_seconds = 300; // default 5 minutes

static int wav_open(struct fs_file_t *file, char *path, size_t path_len, uint32_t sample_rate, uint16_t bits, uint16_t channels)
{
    // Generate filename with monotonic time
    int64_t now_ms = k_uptime_get();
    snprintk(path, path_len, "%s/%lld.wav", mount_point, (long long)now_ms);

    int rc = fs_open(file, path, FS_O_CREATE | FS_O_WRITE);
    if (rc < 0)
    {
        LOG_ERR("fs_open %s failed: %d", path, rc);
        return rc;
    }
    // Write a 44-byte WAV header placeholder; we will fix sizes on close
    uint8_t header[44] = {0};
    // RIFF chunk
    memcpy(header + 0, "RIFF", 4);
    // chunk size placeholder at +4
    memcpy(header + 8, "WAVE", 4);
    // fmt chunk
    memcpy(header + 12, "fmt ", 4);
    header[16] = 16; // PCM fmt chunk size
    header[20] = 1;  // PCM format
    header[22] = channels & 0xFF;
    header[23] = (channels >> 8) & 0xFF;
    uint32_t sr = sample_rate;
    header[24] = sr & 0xFF;
    header[25] = (sr >> 8) & 0xFF;
    header[26] = (sr >> 16) & 0xFF;
    header[27] = (sr >> 24) & 0xFF;
    uint16_t bps = bits;
    uint16_t block_align = (channels * (bps / 8));
    uint32_t byte_rate = sr * block_align;
    header[28] = byte_rate & 0xFF;
    header[29] = (byte_rate >> 8) & 0xFF;
    header[30] = (byte_rate >> 16) & 0xFF;
    header[31] = (byte_rate >> 24) & 0xFF;
    header[32] = block_align & 0xFF;
    header[33] = (block_align >> 8) & 0xFF;
    header[34] = bps & 0xFF;
    header[35] = (bps >> 8) & 0xFF;
    // data chunk
    memcpy(header + 36, "data", 4);
    // data size at +40 placeholder

    rc = fs_write(file, header, sizeof(header));
    if (rc < 0)
    {
        LOG_ERR("fs_write header failed: %d", rc);
        fs_close(file);
        return rc;
    }
    return 0;
}

static int wav_finalize(struct fs_file_t *file)
{
    // Update RIFF and data sizes
    off_t file_size = fs_tell(file);
    if (file_size < 44)
        return -EINVAL;
    uint32_t data_size = file_size - 44;
    uint32_t riff_size = file_size - 8;
    // Seek and write
    fs_seek(file, 4, FS_SEEK_SET);
    uint8_t tmp[4];
    tmp[0] = riff_size & 0xFF;
    tmp[1] = (riff_size >> 8) & 0xFF;
    tmp[2] = (riff_size >> 16) & 0xFF;
    tmp[3] = (riff_size >> 24) & 0xFF;
    fs_write(file, tmp, 4);
    fs_seek(file, 40, FS_SEEK_SET);
    tmp[0] = data_size & 0xFF;
    tmp[1] = (data_size >> 8) & 0xFF;
    tmp[2] = (data_size >> 16) & 0xFF;
    tmp[3] = (data_size >> 24) & 0xFF;
    fs_write(file, tmp, 4);
    fs_sync(file);
    return 0;
}

static volatile bool g_mounted = false;

bool storage_is_mounted(void)
{
    return g_mounted;
}

static int ensure_mount(void)
{
    static bool mounted = false;
    if (mounted)
        return 0;

    // Allow card power-up settle
    k_sleep(K_MSEC(200));
    int rc = disk_access_init("SD");
    if (rc)
    {
        LOG_ERR("disk_access_init failed: %d", rc);
        return rc;
    }

    rc = fs_mount(&mp);
    if (rc == 0)
    {
        LOG_INF("Mounted FAT at %s", mount_point);
        mounted = true;
        g_mounted = true;
    }
    else if (rc == -ENODEV)
    {
        LOG_WRN("No SD card detected yet");
    }
    else
    {
        LOG_ERR("fs_mount failed: %d", rc);
    }
    return rc;
}

static void storage_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    int rc;
    while ((rc = ensure_mount()) != 0)
    {
        k_sleep(K_SECONDS(1));
    }

    // Create root dir if needed
    struct fs_dirent ent;
    rc = fs_stat(mount_point, &ent);
    if (rc < 0)
    {
        LOG_WRN("Root missing? rc=%d", rc);
    }

    struct fs_file_t file;
    fs_file_t_init(&file);
    char path[64];

    rc = wav_open(&file, path, sizeof(path), AI_AUDIO_SAMPLE_RATE_HZ, AI_AUDIO_BITS_PER_SAMPLE, AI_AUDIO_CHANNELS);
    if (rc)
    {
        LOG_ERR("Failed to open wav file: %d", rc);
        return;
    }

    const uint32_t bytes_per_sec = AI_AUDIO_SAMPLE_RATE_HZ * AI_AUDIO_CHANNELS * (AI_AUDIO_BITS_PER_SAMPLE / 8);
    uint32_t bytes_written_this_segment = 0;

    uint8_t buf[2048];
    int64_t segment_end = k_uptime_get() + (int64_t)segment_seconds * 1000;

    while (true)
    {
        size_t n = audio_read(buf, sizeof(buf), K_MSEC(200));
        if (n > 0)
        {
            int w = fs_write(&file, buf, n);
            if (w < 0)
            {
                LOG_ERR("fs_write failed: %d", w);
                k_sleep(K_MSEC(50));
            }
            else
            {
                bytes_written_this_segment += (uint32_t)w;
                if (bytes_written_this_segment >= bytes_per_sec)
                {
                    fs_sync(&file);
                    bytes_written_this_segment = 0;
                }
            }
        }

        if (k_uptime_get() >= segment_end)
        {
            // rotate
            fs_sync(&file);
            wav_finalize(&file);
            fs_close(&file);
            rc = wav_open(&file, path, sizeof(path), AI_AUDIO_SAMPLE_RATE_HZ, AI_AUDIO_BITS_PER_SAMPLE, AI_AUDIO_CHANNELS);
            if (rc)
            {
                LOG_ERR("rotate open failed: %d", rc);
                break;
            }
            segment_end = k_uptime_get() + (int64_t)segment_seconds * 1000;
        }
    }
}

int storage_init(void)
{
    return 0;
}

int storage_start(void)
{
    if (storage_tid)
        return 0;
    storage_tid = k_thread_create(&storage_thread_data, storage_stack, K_THREAD_STACK_SIZEOF(storage_stack),
                                  storage_thread, NULL, NULL, NULL, K_PRIO_PREEMPT(7), 0, K_NO_WAIT);
    return 0;
}

void storage_set_segment_duration(uint32_t seconds)
{
    segment_seconds = seconds;
}

void storage_begin_recording(void)
{
    recording = true;
}

void storage_end_recording(void)
{
    recording = false;
}
