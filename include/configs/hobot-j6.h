/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2023 Horizone Co., Ltd
 */

#ifndef __HOBOT_J6SOC_H
#define __HOBOT_J6SOC_H

#define HOBOT_SIP_FUNC_ID	0x82000009

/* ---------------------------------------------------------------------
 * Board boot configuration
 */

#define BOOT_TARGET_DEVICES(func)	\
	func(MMC, mmc, 0)				\
	func(MMC, mmc, 1)				\
	func(DHCP, dhcp, na)			\
	func(NVME, nvme, 0)				\
	func(USB, usb, 0)				\

#include <config_distro_bootcmd.h>

#define CONFIG_EXTRA_ENV_SETTINGS		\
		"stdout=serial\0" 				\
		"stdin=serial\0" 				\
		"stderr=serial\0"				\
		BOOTENV

#endif
