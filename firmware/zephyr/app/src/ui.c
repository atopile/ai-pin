#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ui.h"

LOG_MODULE_REGISTER(ai_ui, LOG_LEVEL_INF);

void ui_init(void)
{
    // TODO: configure LED GPIO and button input per board overlay
}

void ui_tick(void)
{
    // TODO: heartbeat LED or state indications
}
