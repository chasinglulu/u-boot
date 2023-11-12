/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (C) 2023 Charleye <wangkart@aliyun.com>
 */


#ifndef __CONFIG_H
#define __CONFIG_H

#include <linux/sizes.h>

#ifdef CONFIG_LMT_RISCV_NO_DRAM

#ifdef CONFIG_SPL

#define CONFIG_SPL_MAX_SIZE         0x00020000
#define CONFIG_SYS_SPL_MALLOC_START 0x60c10000
#define CONFIG_SYS_SPL_MALLOC_SIZE  0x00008000
#define CONFIG_SPL_BSS_START_ADDR   0x60c18000
#define CONFIG_SPL_BSS_MAX_SIZE     0x00004000

#define CONFIG_SPL_STACK            0x60c1c000

#endif

#define CONFIG_SYS_INIT_SP_ADDR     (0x60c20000 + 0x40000)
#define CONFIG_STANDALONE_LOAD_ADDR 0x60c20000

#else
#ifdef CONFIG_SPL

#define CONFIG_SPL_MAX_SIZE         0x00040000
#define CONFIG_SPL_BSS_START_ADDR   0x60c40000
#define CONFIG_SPL_BSS_MAX_SIZE     0x00020000
#define CONFIG_SYS_SPL_MALLOC_START 0x60c60000
#define CONFIG_SYS_SPL_MALLOC_SIZE  0x00020000
#define CONFIG_SPL_STACK            0x60c3d000

#endif

#define CONFIG_SYS_SDRAM_BASE		0x60e00000
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_SDRAM_BASE + SZ_2M)

#define CONFIG_STANDALONE_LOAD_ADDR	0x60e00000

#endif /* CONFIG_LMT_RISCV_NO_DRAM */

#define RISCV_MMODE_TIMERBASE		0x2000000
#define RISCV_MMODE_TIMER_FREQ		1000000

#define RISCV_SMODE_TIMER_FREQ		1000000

/* Environment options */

#ifndef CONFIG_SPL_BUILD
#define BOOT_TARGET_DEVICES(func) \
	func(QEMU, qemu, na)

#include <config_distro_bootcmd.h>

#define BOOTENV_DEV_QEMU(devtypeu, devtypel, instance) \
	"bootcmd_qemu=" \
		"if env exists kernel_start; then " \
			"bootm ${kernel_start} - ${fdtcontroladdr};" \
		"fi;\0"

#define BOOTENV_DEV_NAME_QEMU(devtypeu, devtypel, instance) \
	"qemu "

#define CONFIG_EXTRA_ENV_SETTINGS \
	"fdt_high=0xffffffffffffffff\0" \
	"initrd_high=0xffffffffffffffff\0" \
	"kernel_addr_r=0x84000000\0" \
	"fdt_addr_r=0x88000000\0" \
	"scriptaddr=0x88100000\0" \
	"pxefile_addr_r=0x88200000\0" \
	"ramdisk_addr_r=0x88300000\0" \
	BOOTENV
#endif

#endif /* __CONFIG_H */
