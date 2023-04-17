/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 Horizon Robotics Co., Ltd
 */

#include <config.h>

.align 8
.global boot_start_addr
boot_start_addr:
	.space 64

/*
 * Routine: save_boot_params (called after reset from start.S)
 */

.global save_boot_params
save_boot_params:
	adr	x0, boot_start_addr
	adr x1, _start
	str	x1, [x0]

	/* Returns */
	b save_boot_params_ret

