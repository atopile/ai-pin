// Core
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

// Subsystems
#include "audio.h"
#include "storage.h"
#include "net.h"
#include "ui.h"

LOG_MODULE_REGISTER(ai_pin, LOG_LEVEL_INF);

static void start_workers(void)
{
    // storage_init();
    // audio_init();
    // net_init();
    ui_init();

    // storage_start();
    // audio_start();
}

int main(void)
{
    LOG_INF("AI Pin firmware (Zephyr) booting");

    start_workers();

    while (true)
    {
        ui_tick();
        k_sleep(K_MSEC(100));
    }
    return 0;
}
