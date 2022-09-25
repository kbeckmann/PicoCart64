/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#include "utils.h"

#include <stdio.h>

#include "hardware/watchdog.h"

void assert_handler(char *file, int line, char *statement)
{
	printf("Assert failed %s:%d:\n  %s\n", file, line, statement);

	// Reboot after 5s
	watchdog_reboot(0, 0, 5000);

	while (true) {
		tight_loop_contents();
	}
}
