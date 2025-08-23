#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/time_units.h>

#include "ws2812.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ws2812, LOG_LEVEL_INF);

/* GPIO21 per ai-pin.ato connection for addressable_led.data_in */
#define WS2812_GPIO_PIN 21

static const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

static inline void wait_ns_precise(uint32_t ns)
{
    uint32_t start = k_cycle_get_32();
    uint32_t wait_cyc = k_ns_to_cyc_ceil32(ns);
    while ((uint32_t)(k_cycle_get_32() - start) < wait_cyc)
    {
        /* busy wait */
    }
}

static void ws2812_write_byte(uint8_t b)
{
    /* WS2812 timing (approx):
       T0H ~0.35us, T0L ~0.8us, T1H ~0.7us, T1L ~0.6us
       Using coarse delays; ESP32-C3 is fast enough. */
    for (int i = 7; i >= 0; --i)
    {
        bool bit = (b >> i) & 1;
        gpio_pin_set_raw(gpio_dev, WS2812_GPIO_PIN, 1);
        if (bit)
        {
            wait_ns_precise(700);
            gpio_pin_set_raw(gpio_dev, WS2812_GPIO_PIN, 0);
            wait_ns_precise(600);
        }
        else
        {
            wait_ns_precise(350);
            gpio_pin_set_raw(gpio_dev, WS2812_GPIO_PIN, 0);
            wait_ns_precise(800);
        }
    }
}

void ws2812_init(void)
{
    if (!device_is_ready(gpio_dev))
    {
        LOG_ERR("GPIO controller not ready");
        return;
    }
    int rc = gpio_pin_configure(gpio_dev, WS2812_GPIO_PIN, GPIO_OUTPUT);
    if (rc)
    {
        LOG_ERR("gpio_pin_configure failed: %d", rc);
    }
    /* Ensure LED is off at boot and send a reset */
    gpio_pin_set_raw(gpio_dev, WS2812_GPIO_PIN, 0);
    k_busy_wait(300);
}

void ws2812_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    if (!device_is_ready(gpio_dev))
        return;
    /* WS2812 expects GRB order */
    unsigned int key = irq_lock();
    ws2812_write_byte(g);
    ws2812_write_byte(r);
    ws2812_write_byte(b);
    /* Reset: hold low for >50us */
    gpio_pin_set_raw(gpio_dev, WS2812_GPIO_PIN, 0);
    k_busy_wait(300);
    irq_unlock(key);
}

void ws2812_off(void)
{
    ws2812_set_rgb(0, 0, 0);
}
