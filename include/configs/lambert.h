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

/* Boot by executing a U-Boot script pre-loaded into DRAM. */
#define BOOTENV_DEV_MEM(devtypeu, devtypel, instance) \
	"bootcmd_mem= " \
		"source ${scriptaddr}; " \
		"if test $? -eq 1; then " \
		"  env import -t ${scriptaddr}; " \
		"  if test -n $uenvcmd; then " \
		"    echo Running uenvcmd ...; " \
		"    run uenvcmd; " \
		"  fi; " \
		"fi\0"
#define BOOTENV_DEV_NAME_MEM(devtypeu, devtypel, instance) "mem "
#define BOOT_TARGET_MEM(func) func(MEM, mem, na)

#ifdef CONFIG_TARGET_LMT_VIRT
#define BOOT_TARGET_DEVICES(func)   \
		BOOT_TARGET_SMH(func)       \
		BOOT_TARGET_MEM(func)       \
		BOOT_TARGET_VIRTIO(func)    \
		BOOT_TARGET_DHCP(func)
#elif defined(CONFIG_TARGET_LMT_FPGA)
#define BOOT_TARGET_DEVICES(func)   \
		BOOT_TARGET_MEM(func)       \
		BOOT_TARGET_DHCP(func)
#else
#define BOOT_TARGET_DEVICES(func) \
		BOOT_TARGET_MMC(func) \
		BOOT_TARGET_MMC1(func) \
		BOOT_TARGET_DHCP(func)
#endif

#define EXTRA_ENV_NAMES                                                    \
		"kernel_name=Image\0"                                              \
		"ramdisk_name=rootfs.cpio.lz4\0"                                   \
		"fdtfile=lambert-virt.dtb\0"                                       \
		"boot_name=boot.img\0"                                             \
		"boot_addr_r=" __stringify(CONFIG_LMT_KERNEL_LOAD_ADDR) "\0"

#include <config_distro_bootcmd.h>

#define CONFIG_EXTRA_ENV_SETTINGS                                          \
		"stdout=serial\0"                                                  \
		"stdin=serial\0"                                                   \
		"stderr=serial\0"                                                  \
		"scriptaddr=" __stringify(CONFIG_LMT_SCRIPT_LOAD_ADDR) "\0"        \
		"fdt_addr_r=" __stringify(CONFIG_LMT_FDT_LOAD_ADDR) "\0"           \
		"kernel_addr_r=" __stringify(CONFIG_LMT_KERNEL_LOAD_ADDR) "\0"     \
		"ramdisk_addr_r=" __stringify(CONFIG_LMT_RAMDISK_LOAD_ADDR) "\0"   \
		EXTRA_ENV_NAMES                                                    \
		BOOTENV

/* SPL support */
#define CONFIG_SYS_UBOOT_START	CONFIG_SYS_TEXT_BASE
#define CONFIG_SPL_MAX_SIZE		0x20000
#define CONFIG_SPL_STACK		0x1ec00
#define CONFIG_SPL_BSS_START_ADDR	0x1f000
#define CONFIG_SPL_BSS_MAX_SIZE		0x1000

#endif
