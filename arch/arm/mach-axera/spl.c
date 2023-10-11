/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (c) 2023 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <debug_uart.h>
#include <asm/system.h>
#include <hang.h>
#include <image.h>
#include <init.h>
#include <log.h>
#include <spl.h>

u32 spl_boot_device(void)
{
	return BOOT_DEVICE_RAM;
}

struct image_header *spl_get_load_buffer(ssize_t offset, size_t size)
{
	return (struct image_header *)(CONFIG_SYS_LOAD_ADDR);
}

#ifdef CONFIG_SPL_BOARD_INIT
void spl_board_init(void)
{
}
#endif

#ifdef CONFIG_SPL_DISPLAY_PRINT
static inline unsigned long read_midr(void)
{
	unsigned long val;

	asm volatile("mrs %0, midr_el1" : "=r" (val));

	return val;
}

#define MPIDR_HWID_BITMASK	0xff00ffffffUL
void spl_display_print(void)
{
	unsigned long mpidr = read_mpidr() & MPIDR_HWID_BITMASK;

	printf("EL level:  EL%x\n", current_el());
	printf("Boot SPL on physical CPU 0x%04lx [0x%08lx]\n",
				mpidr, read_midr());
}
#endif