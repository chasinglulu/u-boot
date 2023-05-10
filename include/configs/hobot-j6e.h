/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2023 Horizone Co., Ltd
 */

#ifndef __HOBOT_J6ESOC_H
#define __HOBOT_J6ESOC_H

#define HOBOT_SIP_FUNC_ID	0x82000009

/* ---------------------------------------------------------------------
 * Board boot configuration
 */
/* Environment options */

#define FASTBOOT_USB_CMD "fastboot_usb_cmd=fastboot usb 0\0"
#define FASTBOOT_NET_CMD "fastboot_net_cmd=fastboot net\0"

#if CONFIG_IS_ENABLED(CMD_MMC)
# define BOOT_TARGET_MMC(func) func(MMC, mmc, 0)
#else
# define BOOT_TARGET_MMC(func)
# define BOOT_TARGET_MMC1(func)
#endif

#if CONFIG_IS_ENABLED(CMD_USB)
# define BOOT_TARGET_USB(func) func(USB, usb, 0)
#else
# define BOOT_TARGET_USB(func)
#endif

#if CONFIG_IS_ENABLED(CMD_SCSI)
# define BOOT_TARGET_SCSI(func) func(SCSI, scsi, 0)
#else
# define BOOT_TARGET_SCSI(func)
#endif

#if CONFIG_IS_ENABLED(CMD_VIRTIO)
# define BOOT_TARGET_VIRTIO(func) func(VIRTIO, virtio, 0)
#else
# define BOOT_TARGET_VIRTIO(func)
#endif

#if CONFIG_IS_ENABLED(CMD_NVME)
# define BOOT_TARGET_NVME(func) func(NVME, nvme, 0)
#else
# define BOOT_TARGET_NVME(func)
#endif

#if CONFIG_IS_ENABLED(CMD_DHCP)
# define BOOT_TARGET_DHCP(func) func(DHCP, dhcp, na)
#else
# define BOOT_TARGET_DHCP(func)
#endif

#define BOOT_TARGET_DEVICES(func) \
       BOOT_TARGET_MMC(func) \
       BOOT_TARGET_SCSI(func) \
       BOOT_TARGET_NVME(func) \
       BOOT_TARGET_VIRTIO(func) \
       BOOT_TARGET_DHCP(func) \
       BOOT_TARGET_USB(func)

#include <config_distro_bootcmd.h>

#define CONFIG_EXTRA_ENV_SETTINGS		\
		"stdout=serial\0" 				\
		"stdin=serial\0" 				\
		"stderr=serial\0"				\
		"scriptaddr=0x88000000\0"     \
		"fdt_addr_r=0x88800000\0"		\
		"xen_addr_r=0x8a000000\0"		\
		"kernel_addr_r=0x8d000000\0"  \
		"domu1_addr_r=0x8f000000\0"	\
		"ramdisk_addr_r=0x91000000\0" \
		"domu1_ramdisk_addr_r=0x98000000\0"	\
		"domu2_addr_r=0x9a000000\0"	\
		"domu2_ramdisk_addr_r=0x9c000000\0"	\
		BOOTENV

#endif
