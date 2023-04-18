/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 Horizon Robotics Co., Ltd
 */

#include <config.h>
#include <asm/macro.h>

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

.global lowlevel_init
lowlevel_init:
	mov	x29, lr			/* Save LR */
#if defined(CONFIG_HOBOT_SWITCH_TO_EL2)
	/*
	 * All CPU will enter EL2.
	 */
	adr	x4, lowlevel_in_el2
	ldr	x5, =ES_TO_AARCH64
	bl	armv8_switch_to_el2
lowlevel_in_el2:
#endif

	mov	lr, x29			/* Restore LR */
	ret