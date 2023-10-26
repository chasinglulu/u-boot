/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (c) 2023 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <debug_uart.h>
#include <asm/system.h>
#include <semihosting.h>
#include <hang.h>
#include <image.h>
#include <init.h>
#include <log.h>
#include <spl.h>

u32 spl_boot_device(void)
{
	if (semihosting_enabled())
		return BOOT_DEVICE_SMH;

	return BOOT_DEVICE_RAM;
}

void board_boot_order(u32 *spl_boot_list)
{
	spl_boot_list[0] = spl_boot_device();
	if (spl_boot_list[0] == BOOT_DEVICE_SMH)
		spl_boot_list[1] = BOOT_DEVICE_RAM;
}

struct image_header *spl_get_load_buffer(ssize_t offset, size_t size)
{
	return (struct image_header *)(CONFIG_SYS_LOAD_ADDR);
}

int spl_parse_board_header(struct spl_image_info *spl_image,
				  const struct spl_boot_device *bootdev,
				  const void *image_header, size_t size)
{
	if (IS_ENABLED(CONFIG_SPL_ATF) &&
		    bootdev->boot_device == BOOT_DEVICE_RAM
		    && current_el() == 3) {
		spl_image->entry_point = CONFIG_LMT_SPL_ATF_LOAD_ADDR;
		spl_image->load_addr = CONFIG_LMT_SPL_ATF_LOAD_ADDR;

		return 0;
	}

	return -EINVAL;
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

#define MPIDR_HWID_BITMASK	UL(0xff00ffffff)
void spl_display_print(void)
{
	unsigned long mpidr = read_mpidr() & MPIDR_HWID_BITMASK;

	printf("EL level: EL%x\n", current_el());
	printf("Boot SPL on physical CPU 0x%010lx [0x%08lx]\n",
				mpidr, read_midr());
}
#endif

void spl_perform_fixups(struct spl_image_info *spl_image)
{
	if (IS_ENABLED(CONFIG_SPL_ATF) &&
		    spl_image->boot_device == BOOT_DEVICE_RAM
		    && !(spl_image->flags & SPL_FIT_FOUND)
		    && (current_el() == 3)) {
		spl_image->os = IH_OS_ARM_TRUSTED_FIRMWARE;
		spl_image->name = "ARM Trusted Firmware";
	}
}