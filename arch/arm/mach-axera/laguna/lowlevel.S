/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2024 (C) Charleye <wangkart@aliyun.com>
 */

#include <config.h>
#include <linux/linkage.h>

#define GTMR_CNT_ENABLE    (1 << 0)

ENTRY(lowlevel_init)
	mov	x29, lr			/* Save LR */

#if defined(CONFIG_LUA_GTMR_LOWLEVEL_INIT) && defined(CONFIG_SPL_BUILD)
	/* Enable generic timer counter */
	dmb	sy
	ldr	x0, =CONFIG_LUA_MAIN_SEC_GLB_BASE
	ldr	x1, =CONFIG_LUA_GTMR_CNT_OFFSET
	add	x0, x0, x1
	str	wzr, [x0]
	dmb	sy
	ldr	x0, =CONFIG_LUA_CPU_GTMR_BASE
	mov	x1, #0x08	//CNTCVL_OFFSET
	add	x0, x0, x1
	str	wzr, [x0]
	dmb	sy
	add	x0, x0, #0x04
	str	wzr, [x0]
	dmb	sy
	add	x0, x0, #0x14
	ldr	x1, =CONFIG_COUNTER_FREQUENCY
	str	x1, [x0]
	dmb	sy
	ldr	x0, =CONFIG_LUA_CPU_GTMR_BASE
	ldr	x1, =GTMR_CNT_ENABLE
	stur x1, [x0]
#endif

#if defined(CONFIG_LUA_GICV2_LOWLEVEL_INIT) && defined(CONFIG_SPL_BUILD)
	ldr	x0, =CONFIG_LUA_GICD_BASE
	bl	gic_init_secure
	ldr	x0, =CONFIG_LUA_GICD_BASE
	ldr	x1, =CONFIG_LUA_GICC_BASE
	bl	gic_init_secure_percpu
#endif

#if defined(CONFIG_LUA_SWITCH_TO_EL2)
	adr	x4, lowlevel_in_el2
	ldr	x5, =ES_TO_AARCH64
	bl	armv8_switch_to_el2

lowlevel_in_el2:
#endif

	mov	lr, x29			/* Restore LR */
	ret
ENDPROC(lowlevel_init)