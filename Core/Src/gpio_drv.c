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
 * Защита от «выбросов» при (пере)инициализации:
 *   1. Все выводы группы настраиваются одной записью в каждый регистр
 *      (значение собирается в локальной переменной, затем пишется целиком),
 *      поэтому промежуточные состояния полей на выводах не появляются.
 *   2. Порядок записи: ODR(через BSRR) -> OTYPER -> OSPEEDR -> AFR -> PUPDR -> MODER.
 *      Режим (MODER) меняется последним: к моменту, когда вывод становится
 *      выходом, уже заданы требуемый уровень, тип выхода, скорость, подтяжка
 *      и номер альтернативной функции. При переходе выход -> вход подтяжка
 *      включается до отключения выходного драйвера, и линия переходит сразу
 *      к уровню подтяжки.
 *   3. Регистр, значение которого не изменилось, не перезаписывается.
 *   4. Последовательность выполняется с запрещёнными прерываниями, чтобы
 *      обработчик прерывания не вклинился между чтением и записью регистра.
 */

#include "gpio_drv.h"

/* Расстояние между базовыми адресами портов GPIOA, GPIOB, ... в адресном пространстве */
#define GPIO_DRV_PORT_STRIDE   (GPIOB_BASE - GPIOA_BASE)

/* Начало/конец критической секции с сохранением прежнего состояния PRIMASK */
static inline uint32_t gpio_drv_lock(void)
{
	uint32_t primask = __get_PRIMASK();
	__disable_irq();
	return primask;
}

static inline void gpio_drv_unlock(uint32_t primask)
{
	__set_PRIMASK(primask);
}

/* Запись в регистр с маской: биты вне mask не меняются; запись только при изменении */
static inline void gpio_drv_update_reg(volatile uint32_t *reg, uint32_t mask, uint32_t value)
{
	uint32_t old = *reg;
	uint32_t upd = (old & ~mask) | (value & mask);
	if (upd != old) {
		*reg = upd;
	}
}

void gpio_drv_clock_enable(GPIO_TypeDef *port)
{
	uint32_t index = ((uint32_t)(uintptr_t)port - GPIOA_BASE) / GPIO_DRV_PORT_STRIDE;
	uint32_t primask = gpio_drv_lock();
	RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOAEN << index);
	/* Чтение после записи — задержка до появления тактирования (errata STM32F4) */
	(void)RCC->AHB1ENR;
	gpio_drv_unlock(primask);
}

void gpio_drv_init(GPIO_TypeDef *port, uint16_t pins, const gpio_drv_config_t *cfg)
{
	uint32_t mask1 = 0, mask2 = 0, mask4_lo = 0, mask4_hi = 0;
	uint32_t moder = 0, otyper = 0, ospeedr = 0, pupdr = 0, afr_lo = 0, afr_hi = 0;

	/* Сборка масок и значений полей для всех выбранных выводов */
	for (uint32_t i = 0; i < 16U; i++) {
		if ((pins & (1U << i)) == 0U) {
			continue;
		}
		mask1   |= 1U << i;
		mask2   |= 0x3U << (2U * i);
		moder   |= (uint32_t)cfg->mode  << (2U * i);
		otyper  |= (uint32_t)cfg->otype << i;
		ospeedr |= (uint32_t)cfg->speed << (2U * i);
		pupdr   |= (uint32_t)cfg->pull  << (2U * i);
		if (i < 8U) {
			mask4_lo |= 0xFU << (4U * i);
			afr_lo   |= (uint32_t)(cfg->af & 0xFU) << (4U * i);
		} else {
			mask4_hi |= 0xFU << (4U * (i - 8U));
			afr_hi   |= (uint32_t)(cfg->af & 0xFU) << (4U * (i - 8U));
		}
	}
	if (mask1 == 0U) {
		return;
	}

	uint32_t primask = gpio_drv_lock();

	/* 1. Уровень выхода задаётся заранее, пока вывод ещё в прежнем режиме */
	if (cfg->mode == GPIO_DRV_MODE_OUTPUT) {
		port->BSRR = (cfg->level == GPIO_DRV_HIGH) ? mask1 : (mask1 << 16U);
	}
	/* 2. Тип выхода и скорость */
	gpio_drv_update_reg(&port->OTYPER, mask1, otyper);
	gpio_drv_update_reg(&port->OSPEEDR, mask2, ospeedr);
	/* 3. Номер альтернативной функции — до подключения вывода к периферии */
	if (cfg->mode == GPIO_DRV_MODE_AF) {
		gpio_drv_update_reg(&port->AFR[0], mask4_lo, afr_lo);
		gpio_drv_update_reg(&port->AFR[1], mask4_hi, afr_hi);
	}
	/* 4. Подтяжка */
	gpio_drv_update_reg(&port->PUPDR, mask2, pupdr);
	/* 5. Режим */
	gpio_drv_update_reg(&port->MODER, mask2, moder);

	gpio_drv_unlock(primask);
}

void gpio_drv_get_config(GPIO_TypeDef *port, uint8_t pin_num, gpio_drv_config_t *cfg)
{
	uint32_t i = pin_num & 0xFU;
	cfg->mode  = (gpio_drv_mode_t)((port->MODER >> (2U * i)) & 0x3U);
	cfg->otype = (gpio_drv_otype_t)((port->OTYPER >> i) & 0x1U);
	cfg->speed = (gpio_drv_speed_t)((port->OSPEEDR >> (2U * i)) & 0x3U);
	cfg->pull  = (gpio_drv_pull_t)((port->PUPDR >> (2U * i)) & 0x3U);
	cfg->af    = (uint8_t)((port->AFR[i >> 3U] >> (4U * (i & 0x7U))) & 0xFU);
	cfg->level = (gpio_drv_level_t)((port->ODR >> i) & 0x1U);
}

gpio_drv_level_t gpio_drv_read_pin(GPIO_TypeDef *port, uint16_t pin)
{
	return ((port->IDR & pin) != 0U) ? GPIO_DRV_HIGH : GPIO_DRV_LOW;
}

uint16_t gpio_drv_read_pins(GPIO_TypeDef *port, uint16_t pins)
{
	return (uint16_t)(port->IDR & pins);
}

gpio_drv_level_t gpio_drv_read_output_pin(GPIO_TypeDef *port, uint16_t pin)
{
	return ((port->ODR & pin) != 0U) ? GPIO_DRV_HIGH : GPIO_DRV_LOW;
}

uint16_t gpio_drv_read_output_pins(GPIO_TypeDef *port, uint16_t pins)
{
	return (uint16_t)(port->ODR & pins);
}

void gpio_drv_write_pin(GPIO_TypeDef *port, uint16_t pins, gpio_drv_level_t level)
{
	/* BSRR: биты 0..15 — установка, 16..31 — сброс; запись атомарна */
	port->BSRR = (level == GPIO_DRV_HIGH) ? (uint32_t)pins : ((uint32_t)pins << 16U);
}

void gpio_drv_write_pins(GPIO_TypeDef *port, uint16_t pins, uint16_t values)
{
	/* Одна запись в BSRR: выводы из pins, где в values 1, — в 1, остальные из pins — в 0.
	   Все выводы меняются одновременно, выводы вне pins не затрагиваются. */
	uint32_t set   = (uint32_t)(values & pins);
	uint32_t reset = (uint32_t)(~values & pins);
	port->BSRR = set | (reset << 16U);
}

void gpio_drv_toggle_pins(GPIO_TypeDef *port, uint16_t pins)
{
	uint32_t odr = port->ODR;
	port->BSRR = (~odr & pins) | ((odr & pins) << 16U);
}
