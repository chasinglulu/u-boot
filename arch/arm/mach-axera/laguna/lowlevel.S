/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2024 (C) Charleye <wangkart@aliyun.com>
 */

#include <config.h>
#include <linux/linkage.h>

#define GTMR_CNT_ENABLE    (1 << 0)

#define ACTLR_EL2_CLUSTERPMUEN  (1 << 12)
#define ACTLR_EL3_CLUSTERPMUEN  (1 << 12)

ENTRY(lowlevel_init)
	mov	x29, lr			/* Save LR */

#if defined(CONFIG_SPL_BUILD)
#if defined(CONFIG_LUA_DSU_PMU_INIT)
	mrs x0, ACTLR_EL2
	orr x0, x0, #ACTLR_EL2_CLUSTERPMUEN
	msr ACTLR_EL2, x0
	mrs x0, ACTLR_EL3
	orr x0, x0, #ACTLR_EL3_CLUSTERPMUEN
	msr ACTLR_EL3, x0
#endif

	/* Enable system counter of Generic Timer */
#if defined(CONFIG_LUA_GTMR_LOWLEVEL_INIT)
	dmb	sy
	ldr	x0, =CONFIG_LUA_MAIN_SEC_GLB_BASE
	ldr	x1, =CONFIG_LUA_GTMR_CNT_OFFSET
	add	x0, x0, x1
	str	wzr, [x0]
	dmb	sy
	ldr	x0, =CONFIG_LUA_CPU_GTMR_CNT_BASE
	mov	x1, #0x08	//CNTCVL Offset
	add	x0, x0, x1
	str	wzr, [x0]
	dmb	sy
	add	x0, x0, #0x04	//CNTCVU Offset
	str	wzr, [x0]
	dmb	sy
	add	x0, x0, #0x14	// CNTFID0 Offset
	ldr	x1, =CONFIG_COUNTER_FREQUENCY
	str	x1, [x0]
	dmb	sy
	ldr	x0, =CONFIG_LUA_CPU_GTMR_CNT_BASE
	ldr	x1, =GTMR_CNT_ENABLE
	stur x1, [x0]
#endif

#if defined(CONFIG_LUA_GICV2_LOWLEVEL_INIT)
	ldr	x0, =CONFIG_LUA_GICD_BASE
	bl	gic_init_secure
	ldr	x0, =CONFIG_LUA_GICD_BASE
	ldr	x1, =CONFIG_LUA_GICC_BASE
	bl	gic_init_secure_percpu
#endif
#endif /* CONFIG_SPL_BUILD */

#if defined(CONFIG_LUA_SWITCH_TO_EL2)
	adr	x4, lowlevel_in_el2
	ldr	x5, =ES_TO_AARCH64
	bl	armv8_switch_to_el2

lowlevel_in_el2:
#endif

	mov	lr, x29			/* Restore LR */
	ret
ENDPROC(lowlevel_init)