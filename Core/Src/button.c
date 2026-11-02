/*
 * button.c
 *
 * Драйвер кнопки.
 *
 * Подавление дребезга: состояние считается устойчивым, если сырой сигнал
 * не менялся в течение debounce_ms. Время берётся из параметра now
 * (миллисекундный счётчик HAL_GetTick()), поэтому драйвер не ждёт
 * и не зависит от частоты вызова button_poll().
 */

#include "button.h"

static bool button_read_raw(const button_t *btn)
{
	bool high = gpio_drv_read_pin(btn->port, btn->pin) == GPIO_DRV_HIGH;
	return high != btn->active_low;
}

void button_init(button_t *btn, uint32_t now)
{
	const gpio_drv_config_t cfg = {
		.mode  = GPIO_DRV_MODE_INPUT,
		.otype = GPIO_DRV_OTYPE_PUSH_PULL,
		.speed = GPIO_DRV_SPEED_LOW,
		.pull  = btn->pull,
		.af    = 0,
		.level = GPIO_DRV_LOW,
	};
	gpio_drv_clock_enable(btn->port);
	gpio_drv_init(btn->port, btn->pin, &cfg);

	btn->raw = button_read_raw(btn);
	btn->raw_change_tick = now;
	btn->stable = btn->raw;
	btn->press_tick = now;
	/* Кнопка, нажатая уже в момент инициализации, не порождает событий */
	btn->long_reported = btn->stable;
}

button_event_t button_poll(button_t *btn, uint32_t now)
{
	bool raw = button_read_raw(btn);

	if (raw != btn->raw) {
		btn->raw = raw;
		btn->raw_change_tick = now;
	}

	if (btn->raw != btn->stable && (now - btn->raw_change_tick) >= btn->debounce_ms) {
		btn->stable = btn->raw;
		if (btn->stable) {
			btn->press_tick = now;
			btn->long_reported = false;
		} else if (!btn->long_reported) {
			return BUTTON_EVENT_SHORT_PRESS;
		}
	}

	if (btn->stable && !btn->long_reported && (now - btn->press_tick) >= btn->long_press_ms) {
		btn->long_reported = true;
		return BUTTON_EVENT_LONG_PRESS;
	}

	return BUTTON_EVENT_NONE;
}

bool button_is_pressed(const button_t *btn)
{
	return btn->stable;
}
