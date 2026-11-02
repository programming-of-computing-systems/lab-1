/*
 * led.c
 *
 * Драйвер светодиодного индикатора.
 */

#include "led.h"

static gpio_drv_level_t led_level(const led_t *led, bool on)
{
	return (on == led->active_high) ? GPIO_DRV_HIGH : GPIO_DRV_LOW;
}

void led_init(const led_t *led)
{
	const gpio_drv_config_t cfg = {
		.mode  = GPIO_DRV_MODE_OUTPUT,
		.otype = GPIO_DRV_OTYPE_PUSH_PULL,
		.speed = GPIO_DRV_SPEED_LOW,
		.pull  = GPIO_DRV_PULL_NONE,
		.af    = 0,
		.level = led_level(led, false),   /* светодиод погашен сразу после настройки */
	};
	gpio_drv_clock_enable(led->port);
	gpio_drv_init(led->port, led->pin, &cfg);
}

void led_set(const led_t *led, bool on)
{
	gpio_drv_write_pin(led->port, led->pin, led_level(led, on));
}

void led_toggle(const led_t *led)
{
	gpio_drv_toggle_pins(led->port, led->pin);
}

bool led_is_on(const led_t *led)
{
	return (gpio_drv_read_output_pin(led->port, led->pin) == GPIO_DRV_HIGH) == led->active_high;
}
