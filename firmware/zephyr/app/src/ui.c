#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include "ui.h"
#include "ws2812.h"
#include "net.h"
#include "storage.h"

LOG_MODULE_REGISTER(ai_ui, LOG_LEVEL_INF);

/* Red indicator LED on GPIO20 per ai-pin.ato */
#define UI_LED_GPIO_PIN 20

static const struct device *ui_gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

void ui_init(void)
{
    if (!device_is_ready(ui_gpio_dev))
    {
        LOG_ERR("GPIO controller not ready");
    }
    else
    {
        int rc = gpio_pin_configure(ui_gpio_dev, UI_LED_GPIO_PIN, GPIO_OUTPUT_INACTIVE);
        if (rc)
        {
            LOG_ERR("gpio_pin_configure failed: %d", rc);
        }
    }
    ws2812_init();
}

void ui_tick(void)
{
    static bool led_on = false;
    static int64_t next_toggle = 0;
    int64_t now = k_uptime_get();
    if (now >= next_toggle)
    {
        led_on = !led_on;
        next_toggle = now + 100; // 10 Hz blink (toggle every 100 ms) for faster scope analysis

        if (ui_gpio_dev)
        {
            gpio_pin_set(ui_gpio_dev, UI_LED_GPIO_PIN, led_on ? 1 : 0);
        }

        /* Simple alternating pattern for scope analysis */
        static bool color_state = false;
        if (color_state) {
            ws2812_set_rgb(0, 0, 255);
        } else {
            ws2812_set_rgb(0, 255, 0);
        }
        color_state = !color_state;
    }
}
