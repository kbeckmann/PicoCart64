/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include "FreeRTOS.h"
#include "task.h"

#include "pins_mcu2.h"
#include "led_task.h"
#include "ws2812.h"

void led_task_entry(__unused void *params)
{
#if 0
	ws2812_init(pio0, 21);		// PicoCart v1
#else
	ws2812_init(pio0, PIN_LED);
#endif

	int i = 0;

	while (true) {
		i++;

		int imax = 64;
		int imax2 = imax / 2;
		int ii = i % imax;
		int v = (ii > imax2) ? (imax - ii) : ii;

		ws2812_write_rgb(v, 0, 0);

		vTaskDelay(50);
	}
}
