#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/time_units.h>
#include <zephyr/irq.h>

#include "ws2812.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ws2812, LOG_LEVEL_INF);

/* GPIO21 per ai-pin.ato connection for addressable_led.data_in */
#define WS2812_GPIO_PIN 21

static const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

// T1H 1 code ,high voltage time 0.8us ±150ns
// T1L 1 code ,low voltage time 0.45us ±150ns
// T0H 0 code ,high voltage time 0.4us ±150ns
// T0L 0 code , low voltage time 0.85us ±150ns

static inline void ws2812_nops(uint32_t n)
{
    for (volatile uint32_t i = 0; i < n; ++i)
    {
        __asm__ volatile ("nop");
    }
}

static void ws2812_write_byte(uint8_t b)
{
    for (int i = 7; i >= 0; --i)
    {
        bool bit = (b >> i) & 1;
        gpio_pin_set_raw(gpio_dev, WS2812_GPIO_PIN, 1);

        if (bit)
        {
            ws2812_nops(7);
            gpio_pin_set_raw(gpio_dev, WS2812_GPIO_PIN, 0);
            ws2812_nops(3);
        }
        else
        {
            ws2812_nops(2);
            gpio_pin_set_raw(gpio_dev, WS2812_GPIO_PIN, 0);
            ws2812_nops(8);
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
        return;
    }
    LOG_INF("WS2812 GPIO initialized on pin %d", WS2812_GPIO_PIN);
    /* Ensure LED is off at boot and send a reset */
    gpio_pin_set_raw(gpio_dev, WS2812_GPIO_PIN, 0);
    k_busy_wait(300);
    /* Force LED completely off first */
    LOG_INF("Turning WS2812 OFF");
    ws2812_off();
    k_sleep(K_MSEC(100));
    /* Test with a simple red color to verify it's working */
    LOG_INF("Testing WS2812 with red color - you should see timing on scope");
    ws2812_set_rgb(255, 0, 0); /* Full brightness red for easier scope measurement */
    
    /* Send a test pattern for easier scope analysis */
    LOG_INF("Sending test pattern: 0xAA (alternating 1010 pattern)");
    for (int i = 0; i < 3; i++) {
        ws2812_write_byte(0xAA); /* 10101010 pattern for easy timing measurement */
    }
    gpio_pin_set_raw(gpio_dev, WS2812_GPIO_PIN, 0);
    k_busy_wait(80);
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
    k_busy_wait(80);
    irq_unlock(key);
}

void ws2812_off(void)
{
    ws2812_set_rgb(0, 0, 0);
}
