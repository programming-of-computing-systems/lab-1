/*
 * gpio_drv.c
 *
 * Собственный драйвер GPIO для STM32F4.
 *
 * Регистры порта:
 *   MODER   — режим (2 бита на вывод)       OTYPER — тип выхода (1 бит)
 *   OSPEEDR — скорость (2 бита)             PUPDR  — подтяжка (2 бита)
 *   IDR     — входные данные                ODR    — выходные данные
 *   BSRR    — атомарная установка/сброс     AFR[2] — альтернативная функция (4 бита)
 *
 * Защита от «выбросов» при (пере)инициализации — порядок записи:
 *   уровень выхода (BSRR) -> OTYPER -> OSPEEDR -> PUPDR -> AFR -> MODER.
 * Режим (MODER) меняется последним: к моменту, когда вывод становится
 * выходом, уже заданы требуемый уровень и остальные параметры, поэтому
 * на выводе сразу появляется нужный уровень.
 */

#include "gpio_drv.h"

#define GPIO_DRV_PORT_STRIDE   (GPIOB_BASE - GPIOA_BASE)

static void set_field(volatile uint32_t *reg, uint32_t pos, uint32_t width_mask, uint32_t value)
{
	*reg = (*reg & ~(width_mask << pos)) | ((value & width_mask) << pos);
}

void gpio_drv_clock_enable(GPIO_TypeDef *port)
{
	uint32_t index = ((uint32_t)(uintptr_t)port - GPIOA_BASE) / GPIO_DRV_PORT_STRIDE;
	RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOAEN << index);
	(void)RCC->AHB1ENR;
}

void gpio_drv_init(GPIO_TypeDef *port, uint8_t pin, const gpio_drv_config_t *cfg)
{
	if (cfg->mode == GPIO_DRV_MODE_OUTPUT) {
		gpio_drv_write_pin(port, pin, cfg->level);
	}
	set_field(&port->OTYPER,        pin,             0x1U, cfg->otype);
	set_field(&port->OSPEEDR,       2U * pin,        0x3U, cfg->speed);
	set_field(&port->PUPDR,         2U * pin,        0x3U, cfg->pull);
	set_field(&port->AFR[pin / 8U], 4U * (pin % 8U), 0xFU, cfg->af);
	set_field(&port->MODER,         2U * pin,        0x3U, cfg->mode);
}

void gpio_drv_get_config(GPIO_TypeDef *port, uint8_t pin, gpio_drv_config_t *cfg)
{
	cfg->mode  = (gpio_drv_mode_t)((port->MODER >> (2U * pin)) & 0x3U);
	cfg->otype = (gpio_drv_otype_t)((port->OTYPER >> pin) & 0x1U);
	cfg->speed = (gpio_drv_speed_t)((port->OSPEEDR >> (2U * pin)) & 0x3U);
	cfg->pull  = (gpio_drv_pull_t)((port->PUPDR >> (2U * pin)) & 0x3U);
	cfg->af    = (uint8_t)((port->AFR[pin / 8U] >> (4U * (pin % 8U))) & 0xFU);
	cfg->level = (gpio_drv_level_t)((port->ODR >> pin) & 0x1U);
}

gpio_drv_level_t gpio_drv_read_pin(GPIO_TypeDef *port, uint8_t pin)
{
	return (gpio_drv_level_t)((port->IDR >> pin) & 0x1U);
}

gpio_drv_level_t gpio_drv_read_output_pin(GPIO_TypeDef *port, uint8_t pin)
{
	return (gpio_drv_level_t)((port->ODR >> pin) & 0x1U);
}

void gpio_drv_write_pin(GPIO_TypeDef *port, uint8_t pin, gpio_drv_level_t level)
{
	port->BSRR = (level == GPIO_DRV_HIGH) ? (1U << pin) : (1U << (pin + 16U));
}

uint16_t gpio_drv_read_pins(GPIO_TypeDef *port, uint16_t pins)
{
	return (uint16_t)(port->IDR & pins);
}

void gpio_drv_write_pins(GPIO_TypeDef *port, uint16_t pins, uint16_t values)
{
	uint32_t set   = (uint32_t)(values & pins);
	uint32_t reset = (uint32_t)(~values & pins);
	port->BSRR = set | (reset << 16U);
}
