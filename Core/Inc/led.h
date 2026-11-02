/*
 * led.h
 *
 * Драйвер светодиодного индикатора. Не содержит прикладной логики:
 * только включение, выключение, переключение и чтение состояния.
 */

#ifndef INC_LED_H_
#define INC_LED_H_

#include "gpio_drv.h"
#include <stdbool.h>

typedef struct {
	GPIO_TypeDef *port;
	uint16_t      pin;         /* маска вывода, например GPIO_DRV_PIN(13) */
	bool          active_high; /* true — светодиод горит при высоком уровне */
} led_t;

void led_init(const led_t *led);
void led_set(const led_t *led, bool on);
void led_toggle(const led_t *led);
bool led_is_on(const led_t *led);

#endif /* INC_LED_H_ */
