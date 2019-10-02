/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2013 Samsung Electronics
 * Rajeshwari Shinde <rajeshwari.s@samsung.com>
 */

#ifndef _EXYNOS_BOARD_H
#define _EXYNOS_BOARD_H

/*
 * Exynos baord specific changes for
 * board_init
 */
int exynos_init(void);

/*
 * Exynos board specific changes for
 * board_early_init_f
 */
int exynos_early_init_f(void);

/*
 * Exynos board specific changes for
 * board_power_init
 */
int exynos_power_init(void);

/*
 * It might could't use debug_uart in SPL,
 * so we can make led light to indicate debug info.
 *
 * led_num: corresponding led to light up
 */
void exynos_led_set_on_early(unsigned int led_num);

#endif	/* EXYNOS_BOARD_H */
