/*
 * led.h
 *
 * Драйвер светодиодного индикатора. Не содержит прикладной логики:
 * только включение, выключение и чтение состояния.
 */

#ifndef INC_LED_H_
#define INC_LED_H_

#include "gpio_drv.h"
#include <stdbool.h>

typedef struct {
	GPIO_TypeDef *port;
	uint8_t       pin;         /* номер вывода 0..15 */
	bool          active_high; /* true — светодиод горит при высоком уровне */
} led_t;

void led_init(const led_t *led);
void led_set(const led_t *led, bool on);
bool led_is_on(const led_t *led);

#endif /* INC_LED_H_ */
