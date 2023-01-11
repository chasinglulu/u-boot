/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 Horizon Robotics Co., Ltd
 */

#include <config.h>

.align 8
.global ram_base_msb
ram_base_msb:
	.space 32

/*
 * Routine: save_boot_params (called after reset from start.S)
 */

.global save_boot_params
save_boot_params:
	adr	x0, ram_base_msb
    adr x1, _start
    clz x1, x1
    mov w2, #0x3f
    sub w2, w2, w1
	str	w2, [x0]

    /* Returns */
	b save_boot_params_ret
