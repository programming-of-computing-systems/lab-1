/*
 * app.h
 *
 * Прикладная логика: двоичный двухразрядный счётчик на зелёном и жёлтом
 * светодиодах с анимацией переполнения.
 */

#ifndef INC_APP_H_
#define INC_APP_H_

#include "button.h"

void app_init(uint32_t now);
void app_process(uint32_t now);

#endif /* INC_APP_H_ */
