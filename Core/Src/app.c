/*
 * app.c
 *
 * Двоичный двухразрядный счётчик
 *
 *  - зелёный светодиод — младший разряд, жёлтый — старший;
 *  - короткое нажатие: +1; переход 3 -> 0 — переполнение (счётчик
 *    переполнений +1) и анимация;
 *  - долгое нажатие: -1; переход 0 -> 3 — счётчик переполнений -1
 *    (если он больше 0) и анимация;
 *  - анимация: BOTH_BLINKS миганий обоими светодиодами, пауза, затем
 *    столько миганий зелёным, сколько переполнений зафиксировано.
 *
 * Работа построена по принципу кооперативной многозадачности: два процесса
 * (опрос кнопки и автомат анимации) вызываются из главного цикла,
 * проверяют по HAL_GetTick(), пора ли выполнять следующее действие,
 * и сразу возвращают управление.
 */

#include "app.h"
#include "led.h"

/* Временные параметры, мс */
#define BUTTON_DEBOUNCE_MS   30U
#define BUTTON_LONG_MS       1000U
#define BLINK_ON_MS          200U
#define BLINK_OFF_MS         200U
#define ANIM_PAUSE_MS        500U
#define BOTH_BLINKS          3U

#define COUNTER_MAX          3U

/* Аппаратура стенда SDK-1.1M: светодиоды PD13 (зелёный), PD14 (жёлтый),
   кнопка PC15 (при нажатии — низкий уровень, подтяжка на плате) */
static const led_t led_green  = { GPIOD, GPIO_DRV_PIN(13), true };
static const led_t led_yellow = { GPIOD, GPIO_DRV_PIN(14), true };

static button_t button = {
	.port          = GPIOC,
	.pin           = GPIO_DRV_PIN(15),
	.active_low    = true,
	.pull          = GPIO_DRV_PULL_NONE,
	.debounce_ms   = BUTTON_DEBOUNCE_MS,
	.long_press_ms = BUTTON_LONG_MS,
};

/*
 * Состояния автомата анимации переполнения/опустошения счётчика.
 * Последовательность: BOTH_OFF <-> BOTH_ON (BOTH_BLINKS раз) -> PAUSE ->
 * GREEN_OFF <-> GREEN_ON (overflows раз) -> показ счётчика -> IDLE.
 * Переход происходит, когда в текущем состоянии прошло заданное время.
 */
typedef enum {
	ANIM_IDLE,       /* анимации нет: светодиоды показывают счётчик, кнопка обрабатывается */
	ANIM_BOTH_OFF,   /* оба светодиода погашены на BLINK_OFF_MS, затем следующее мигание или пауза */
	ANIM_BOTH_ON,    /* оба светодиода горят BLINK_ON_MS */
	ANIM_PAUSE,      /* оба погашены на ANIM_PAUSE_MS между двумя частями анимации */
	ANIM_GREEN_OFF,  /* зелёный погашен на BLINK_OFF_MS, затем следующее мигание или конец */
	ANIM_GREEN_ON    /* зелёный горит BLINK_ON_MS */
} anim_state_t;

static uint8_t      counter;           /* значение 2-битного счётчика */
static uint32_t     overflows;         /* число переполнений */
static anim_state_t anim_state;        /* текущее состояние автомата анимации */
static uint32_t     anim_tick;         /* момент входа в текущее состояние */
static uint32_t     anim_blinks_left;  /* сколько миганий осталось в текущей части анимации */

/* Вывод состояния светодиодов (меняются только те, чьё состояние отличается) */
static void show(bool green, bool yellow)
{
	if (led_is_on(&led_green) != green) {
		led_set(&led_green, green);
	}
	if (led_is_on(&led_yellow) != yellow) {
		led_set(&led_yellow, yellow);
	}
}

static void show_counter(void)
{
	show((counter & 0x1U) != 0U, (counter & 0x2U) != 0U);
}

static void anim_goto(anim_state_t state, uint32_t now)
{
	anim_state = state;
	anim_tick = now;
}

/* Анимация начинается с гашения: иначе первое мигание обоими светодиодами
   сливалось бы с отображением значения 3 */
static void anim_start(uint32_t now)
{
	anim_blinks_left = BOTH_BLINKS;
	show(false, false);
	anim_goto(ANIM_BOTH_OFF, now);
}

/* Процесс анимации: неблокирующий конечный автомат */
static void anim_process(uint32_t now)
{
	uint32_t elapsed = now - anim_tick;

	switch (anim_state) {
	case ANIM_IDLE:
		break;

	case ANIM_BOTH_OFF:
		if (elapsed >= BLINK_OFF_MS) {
			if (anim_blinks_left > 0U) {
				anim_blinks_left--;
				show(true, true);
				anim_goto(ANIM_BOTH_ON, now);
			} else {
				anim_goto(ANIM_PAUSE, now);
			}
		}
		break;

	case ANIM_BOTH_ON:
		if (elapsed >= BLINK_ON_MS) {
			show(false, false);
			anim_goto(ANIM_BOTH_OFF, now);
		}
		break;

	case ANIM_PAUSE:
		if (elapsed >= ANIM_PAUSE_MS) {
			anim_blinks_left = overflows;
			anim_goto(ANIM_GREEN_OFF, now);
		}
		break;

	case ANIM_GREEN_OFF:
		if (elapsed >= BLINK_OFF_MS) {
			if (anim_blinks_left > 0U) {
				anim_blinks_left--;
				show(true, false);
				anim_goto(ANIM_GREEN_ON, now);
			} else {
				show_counter();
				anim_goto(ANIM_IDLE, now);
			}
		}
		break;

	case ANIM_GREEN_ON:
		if (elapsed >= BLINK_ON_MS) {
			show(false, false);
			anim_goto(ANIM_GREEN_OFF, now);
		}
		break;
	}
}

static void counter_increment(uint32_t now)
{
	if (counter == COUNTER_MAX) {
		counter = 0;
		overflows++;
		anim_start(now);
	} else {
		counter++;
		show_counter();
	}
}

static void counter_decrement(uint32_t now)
{
	if (counter == 0U) {
		counter = COUNTER_MAX;
		if (overflows > 0U) {
			overflows--;
		}
		anim_start(now);
	} else {
		counter--;
		show_counter();
	}
}

void app_init(uint32_t now)
{
	led_init(&led_green);
	led_init(&led_yellow);
	button_init(&button, now);

	counter = 0;
	overflows = 0;
	anim_state = ANIM_IDLE;
	show_counter();
}

void app_process(uint32_t now)
{
	/* Процесс 1: опрос кнопки (выполняется всегда, чтобы не терять состояние антидребезга) */
	button_event_t ev = button_poll(&button, now);

	/* Нажатия во время анимации игнорируются */
	if (anim_state == ANIM_IDLE) {
		if (ev == BUTTON_EVENT_SHORT_PRESS) {
			counter_increment(now);
		} else if (ev == BUTTON_EVENT_LONG_PRESS) {
			counter_decrement(now);
		}
	}

	/* Процесс 2: анимация */
	anim_process(now);
}
