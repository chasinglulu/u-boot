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

#ifdef CONFIG_LMT_ENABLE_GICV2
#define GICD_BASE 0x00449000
#define GICC_BASE 0x0044a000
#endif

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
#define BOOTENV_DEV_SMH(devtypeu, devtypel, instance)               \
	"bootcmd_smh= "                                                 \
		"if load hostfs - ${boot_addr_r} ${boot_name}; then "       \
		"  setenv bootargs;"                                        \
		"  abootimg addr ${boot_addr_r};"                           \
		"  abootimg get dtb --index=0 fdt_addr_r;"                  \
		"  bootm ${boot_addr_r} ${boot_addr_r} ${fdt_addr_r};"      \
		"else "                                                     \
		"  if load hostfs - ${kernel_addr_r} ${kernel_name}; then"  \
		"    setenv fdt_high 0xffffffffffffffff;"                   \
		"    setenv initrd_high 0xffffffffffffffff;"                \
		"    load hostfs - ${fdt_addr_r} ${fdtfile};"               \
		"    load hostfs - ${ramdisk_addr_r} ${ramdisk_name};"      \
		"    fdt addr ${fdt_addr_r};"                               \
		"    fdt resize;"                                           \
		"    fdt chosen ${ramdisk_addr_r} ${filesize};"             \
		"    booti $kernel_addr_r - $fdt_addr_r;"                   \
		"  fi;"                                                     \
		"fi\0"
#define BOOTENV_DEV_NAME_SMH(devtypeu, devtypel, instance) "smh "

#if CONFIG_IS_ENABLED(SEMIHOSTING)
# define BOOT_TARGET_SMH(func) func(SMH, smh, na)
#else
# define BOOT_TARGET_SMH(func)
#endif

/* Boot by executing a U-Boot script pre-loaded into DRAM. */
#define BOOTENV_DEV_MEM(devtypeu, devtypel, instance)  \
	"bootcmd_mem= "                                    \
		"source ${scriptaddr}; "                       \
		"if test $? -eq 1; then "                      \
		"  env import -t ${scriptaddr}; "              \
		"  if test -n $uenvcmd; then "                 \
		"    echo Running uenvcmd ...; "               \
		"    run uenvcmd; "                            \
		"  fi; "                                       \
		"fi\0"
#define BOOTENV_DEV_NAME_MEM(devtypeu, devtypel, instance) "mem "
#define BOOT_TARGET_MEM(func) func(MEM, mem, na)

#define BOOTENV_DEV_FPGA(devtypeu, devtypel, instance)      \
	"bootcmd_fpga= "                                        \
		"if test -n ${ramdisk_size}; then "                 \
		"  fdt addr ${fdt_addr_r}; "                        \
		"  fdt resize; "                                    \
		"  fdt chosen ${ramdisk_addr_r} ${ramdisk_size}; "  \
		"  booti ${kernel_addr_r} - ${fdt_addr_r}; "        \
		"else"                                              \
		"  echo ramdisk_size not set; "                     \
		"fi\0"
#define BOOTENV_DEV_NAME_FPGA(devtypeu, devtypel, instance) "fpga "
#define BOOT_TARGET_FPGA(func) func(FPGA, fpga, na)

#ifdef CONFIG_TARGET_LMT_VIRT
#define BOOT_TARGET_DEVICES(func)   \
		BOOT_TARGET_SMH(func)       \
		BOOT_TARGET_MEM(func)       \
		BOOT_TARGET_VIRTIO(func)    \
		BOOT_TARGET_DHCP(func)
#elif defined(CONFIG_TARGET_LMT_FPGA)
#define BOOT_TARGET_DEVICES(func)   \
		BOOT_TARGET_FPGA(func)      \
		BOOT_TARGET_MEM(func)       \
		BOOT_TARGET_DHCP(func)
#else
#define BOOT_TARGET_DEVICES(func) \
		BOOT_TARGET_MMC(func) \
		BOOT_TARGET_MMC1(func) \
		BOOT_TARGET_DHCP(func)
#endif

#define EXTRA_ENV_NAMES                                                    \
		"kernel_name=" __stringify(CONFIG_LMT_KERNEL_LOAD_NAME)"\0"        \
		"ramdisk_name=" __stringify(CONFIG_LMT_RAMDISK_LOAD_NAME)"\0"      \
		"fdtfile=" __stringify(CONFIG_LMT_FDT_LOAD_NAME)"\0"               \
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
#define CONFIG_SYS_MONITOR_LEN    (1 << 20)
#define CONFIG_SPL_STACK          0x1ec00
#define CONFIG_SPL_BSS_START_ADDR 0x1f000

#endif
