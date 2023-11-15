/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2023 Horizone Co., Ltd
 */

#ifndef __HOBOT_J6SOC_H
#define __HOBOT_J6SOC_H

#if CONFIG_IS_ENABLED(CMD_MMC)
# define BOOT_TARGET_MMC(func) func(MMC, mmc, 0)
# define BOOT_TARGET_MMC1(func) func(MMC, mmc, 1)
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

/*
 * Boot by loading an Android image, or kernel, initrd and FDT through
 * semihosting into DRAM.
 */
#define BOOTENV_DEV_SMH(devtypeu, devtypel, instance) \
	"bootcmd_smh= " 						\
		"if smhload ${boot_name} ${boot_addr_r}; then"		\
		"  setenv bootargs;"					\
		"  abootimg addr ${boot_addr_r};"			\
		"  abootimg get dtb --index=0 fdt_addr_r;"		\
		"  bootm ${boot_addr_r} ${boot_addr_r} ${fdt_addr_r};"	\
		"else"							\
		"  if smhload ${kernel_name} ${kernel_addr_r}; then"	\
		"    setenv fdt_high 0xffffffffffffffff;"		\
		"    setenv initrd_high 0xffffffffffffffff;"		\
		"    smhload ${fdtfile} ${fdt_addr_r};"			\
		"    smhload ${ramdisk_name} ${ramdisk_addr_r} ramdisk_end;" \
		"    fdt addr ${fdt_addr_r};"				\
		"    fdt resize;"					\
		"    fdt chosen ${ramdisk_addr_r} ${ramdisk_end};"	\
		"    booti $kernel_addr_r - $fdt_addr_r;"		\
		"  fi;"							\
		"fi\0"
#define BOOTENV_DEV_NAME_SMH(devtypeu, devtypel, instance) "smh "

#if CONFIG_IS_ENABLED(SEMIHOSTING)
# define BOOT_TARGET_SMH(func) func(SMH, smh, na)
#else
# define BOOT_TARGET_SMH(func)
#endif

#define EXTRA_ENV_NAMES                                                    \
		"kernel_name=Image\0"                                              \
		"ramdisk_name=rootfs.cpio.lz4\0"                                   \
		"fdtfile=hobot-sigi-virt.dtb\0"                                    \
		"boot_name=boot.img\0"

#define BOOT_TARGET_DEVICES(func)        \
		BOOT_TARGET_SMH(func)            \
		BOOT_TARGET_MMC(func)            \
		BOOT_TARGET_MMC1(func)           \
		BOOT_TARGET_SCSI(func)           \
		BOOT_TARGET_NVME(func)           \
		BOOT_TARGET_VIRTIO(func)         \
		BOOT_TARGET_DHCP(func)           \
		BOOT_TARGET_USB(func)

#include <config_distro_bootcmd.h>

#define CONFIG_EXTRA_ENV_SETTINGS                                  \
		"stdout=serial\0"                                          \
		"stdin=serial\0"                                           \
		"stderr=serial\0"                                          \
		"scriptaddr=0x3081000000\0"                                \
		"fdt_addr_r=0x3081800000\0"                                \
		"xen_addr_r=0x3082000000\0"                                \
		"kernel_addr_r=0x3085000000\0"                             \
		"domu1_addr_r=0x3087000000\0"                              \
		"ramdisk_addr_r=0x3089000000\0"                            \
		"domu1_ramdisk_addr_r=0x3090000000\0"                      \
		"domu2_addr_r=0x3092000000\0"                              \
		"domu2_ramdisk_addr_r=0x3094000000\0"                      \
		"boot_addr_r=0x3081000000\0"                               \
		EXTRA_ENV_NAMES                                            \
		BOOTENV

#endif
