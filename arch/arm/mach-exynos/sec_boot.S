/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2013 Samsung Electronics
 * Akshay Saraswat <akshay.s@samsung.com>
 */

#include <config.h>
#include <asm/arch/cpu.h>
#include <asm/arch/smc.h>

	.globl relocate_wait_code
relocate_wait_code:
	adr     r0, code_base		@ r0: source address (start)
	adr     r1, code_end		@ r1: source address (end)
	ldr     r2, =0x0204f000
1:
	ldmia   r0!, {r3-r6}
	stmia   r2!, {r3-r6}
	cmp     r0, r1
	blt     1b

        dsb
        isb
        mov pc, lr

        .align 4
code_base:
        adr     r0, ns_reg5
        b   1f

        .word   0x0     @ REG0: RESUME_ADDR
        .word   0x0     @ REG1: RESUME_FLAG
        .word   0x0     @ REG2
        .word   0x0     @ REG3
        .word   0x0     @ REG4
ns_reg5:
        .word   0x0     @ REG5: CPU1_BOOT_REG
        .word   0x0     @ REG6: REG_DIRECTGO_FLAG
        .word   0x0     @ REG7: REG_DIRECTGO_ADDR
        .word   0x0     @ REG8
        .word   0x0     @ REG9

        nop
        nop

1:
        mrc p15, 0, r1, c0, c0, 5       @ MPIDR
        and r1, r1, #0x3
        add r0, r0, r1, lsl #0x2
cpu_wait:
        wfe
        ldr r1, [r0]
        cmp r1, #0x0
        bxne    r1
        b   cpu_wait
        nop
code_end:
