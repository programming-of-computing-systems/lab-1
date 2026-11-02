/*
 * gpio_drv.h
 *
 * Собственный драйвер портов ввода-вывода общего назначения (GPIO) STM32F4.
 * Работает напрямую с регистрами порта (GPIO_TypeDef из CMSIS), не использует
 * структуры и функции HAL/LL для GPIO.
 *
 * Возможности:
 *  - включение тактирования порта;
 *  - инициализация/переинициализация группы выводов одного порта
 *    (режим, тип выхода, скорость, подтяжка, альтернативная функция,
 *    начальный уровень выхода) без «выбросов» на выходах;
 *  - чтение текущих настроек вывода из регистров;
 *  - чтение/запись одиночных выводов и групп выводов в пределах порта.
 */

#ifndef INC_GPIO_DRV_H_
#define INC_GPIO_DRV_H_

#include "main.h"
#include <stdint.h>

/* Маски выводов (можно объединять через |) */
#define GPIO_DRV_PIN(n)      ((uint16_t)(1U << (n)))
#define GPIO_DRV_PIN_ALL     ((uint16_t)0xFFFFU)

/* Режим вывода — значение поля MODERy[1:0] */
typedef enum {
	GPIO_DRV_MODE_INPUT  = 0x0U,
	GPIO_DRV_MODE_OUTPUT = 0x1U,
	GPIO_DRV_MODE_AF     = 0x2U,
	GPIO_DRV_MODE_ANALOG = 0x3U
} gpio_drv_mode_t;

/* Тип выхода — значение бита OTy */
typedef enum {
	GPIO_DRV_OTYPE_PUSH_PULL  = 0x0U,
	GPIO_DRV_OTYPE_OPEN_DRAIN = 0x1U
} gpio_drv_otype_t;

/* Скорость (крутизна фронтов) — значение поля OSPEEDRy[1:0] */
typedef enum {
	GPIO_DRV_SPEED_LOW       = 0x0U,
	GPIO_DRV_SPEED_MEDIUM    = 0x1U,
	GPIO_DRV_SPEED_HIGH      = 0x2U,
	GPIO_DRV_SPEED_VERY_HIGH = 0x3U
} gpio_drv_speed_t;

/* Подтяжка — значение поля PUPDRy[1:0] */
typedef enum {
	GPIO_DRV_PULL_NONE = 0x0U,
	GPIO_DRV_PULL_UP   = 0x1U,
	GPIO_DRV_PULL_DOWN = 0x2U
} gpio_drv_pull_t;

/* Логический уровень вывода */
typedef enum {
	GPIO_DRV_LOW  = 0U,
	GPIO_DRV_HIGH = 1U
} gpio_drv_level_t;

/* Полная конфигурация вывода */
typedef struct {
	gpio_drv_mode_t  mode;
	gpio_drv_otype_t otype;
	gpio_drv_speed_t speed;
	gpio_drv_pull_t  pull;
	uint8_t          af;      /* номер альтернативной функции 0..15 (для GPIO_DRV_MODE_AF) */
	gpio_drv_level_t level;   /* начальный уровень ODR (для OUTPUT, для AF/INPUT игнорируется аппаратно) */
} gpio_drv_config_t;

/* Тактирование */
void gpio_drv_clock_enable(GPIO_TypeDef *port);

/* Настройка */
void gpio_drv_init(GPIO_TypeDef *port, uint16_t pins, const gpio_drv_config_t *cfg);
void gpio_drv_get_config(GPIO_TypeDef *port, uint8_t pin_num, gpio_drv_config_t *cfg);

/* Чтение */
gpio_drv_level_t gpio_drv_read_pin(GPIO_TypeDef *port, uint16_t pin);
uint16_t gpio_drv_read_pins(GPIO_TypeDef *port, uint16_t pins);
gpio_drv_level_t gpio_drv_read_output_pin(GPIO_TypeDef *port, uint16_t pin);
uint16_t gpio_drv_read_output_pins(GPIO_TypeDef *port, uint16_t pins);

/* Запись */
void gpio_drv_write_pin(GPIO_TypeDef *port, uint16_t pins, gpio_drv_level_t level);
void gpio_drv_write_pins(GPIO_TypeDef *port, uint16_t pins, uint16_t values);
void gpio_drv_toggle_pins(GPIO_TypeDef *port, uint16_t pins);

#endif /* INC_GPIO_DRV_H_ */
