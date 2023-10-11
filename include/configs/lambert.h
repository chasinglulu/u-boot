/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (C) 2023 Charleye <wangkart@aliyun.com>
 */

#ifndef __AXERA_LMTSOC_H
#define __AXERA_LMTSOC_H

/* ---------------------------------------------------------------------
 * Board boot configuration
 */
/* Environment options */

#define FASTBOOT_USB_CMD "fastboot_usb_cmd=fastboot usb 0\0"
#define FASTBOOT_NET_CMD "fastboot_net_cmd=fastboot net\0"

#if CONFIG_IS_ENABLED(CMD_MMC)
# define BOOT_TARGET_MMC(func) func(MMC, mmc, 0)
# define BOOT_TARGET_MMC1(func) func(MMC, mmc, 1)
#else
# define BOOT_TARGET_MMC(func)
# define BOOT_TARGET_MMC1(func)
#endif

#if CONFIG_IS_ENABLED(CMD_VIRTIO)
# define BOOT_TARGET_VIRTIO(func) func(VIRTIO, virtio, 0)
#else
# define BOOT_TARGET_VIRTIO(func)
#endif

#if CONFIG_IS_ENABLED(CMD_DHCP)
# define BOOT_TARGET_DHCP(func) func(DHCP, dhcp, na)
#else
# define BOOT_TARGET_DHCP(func)
#endif

#define BOOT_TARGET_DEVICES(func) \
		BOOT_TARGET_MMC(func) \
		BOOT_TARGET_MMC1(func) \
		BOOT_TARGET_VIRTIO(func) \
		BOOT_TARGET_DHCP(func)

#include <config_distro_bootcmd.h>

#define CONFIG_EXTRA_ENV_SETTINGS		\
		"stdout=serial\0" 				\
		"stdin=serial\0" 				\
		"stderr=serial\0"				\
		"scriptaddr=0x3081000000\0"     \
		"fdt_addr_r=0x3081800000\0"		\
		"kernel_addr_r=0x3085000000\0"  \
		"ramdisk_addr_r=0x3089000000\0" \
		BOOTENV


#define CONFIG_SYS_UBOOT_START	CONFIG_SYS_TEXT_BASE

/* SPL support */
#define CONFIG_SPL_MAX_SIZE		0x20000
#define CONFIG_SPL_STACK		0x1ec00

#define CONFIG_SPL_BSS_START_ADDR	0x1f000
#define CONFIG_SPL_BSS_MAX_SIZE		0x1000

#endif
