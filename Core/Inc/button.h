/*
 * button.h
 *
 * Драйвер кнопки с программной защитой от дребезга и распознаванием
 * короткого и долгого нажатия. Функции неблокирующие: button_poll()
 * вызывается периодически из главного цикла и сразу возвращает управление.
 */

#ifndef INC_BUTTON_H_
#define INC_BUTTON_H_

#include "gpio_drv.h"
#include <stdbool.h>

typedef enum {
	BUTTON_EVENT_NONE = 0,
	BUTTON_EVENT_SHORT_PRESS,   /* отпущена раньше, чем истёк long_press_ms */
	BUTTON_EVENT_LONG_PRESS     /* удерживается long_press_ms (генерируется без ожидания отпускания) */
} button_event_t;

typedef struct {
	/* Параметры */
	GPIO_TypeDef   *port;
	uint8_t         pin;            /* номер вывода 0..15 */
	bool            active_low;     /* true — при нажатии на выводе низкий уровень */
	gpio_drv_pull_t pull;           /* внутренняя подтяжка */
	uint32_t        debounce_ms;    /* время установления сигнала, мс */
	uint32_t        long_press_ms;  /* порог долгого нажатия, мс */

	/* Внутреннее состояние */
	bool     raw;              /* последнее считанное «сырое» состояние */
	uint32_t raw_change_tick;  /* момент последнего изменения сырого состояния */
	bool     stable;           /* состояние после подавления дребезга */
	uint32_t press_tick;       /* момент устойчивого нажатия */
	bool     long_reported;    /* долгое нажатие уже выдано (или нажатие нужно игнорировать) */
} button_t;

void button_init(button_t *btn, uint32_t now);
button_event_t button_poll(button_t *btn, uint32_t now);
bool button_is_pressed(const button_t *btn);

#endif /* INC_BUTTON_H_ */
