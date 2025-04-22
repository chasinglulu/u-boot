/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (C) 2023 Charleye <wangkart@aliyun.com>
 */

#ifndef __AXERA_LMTSOC_H
#define __AXERA_LMTSOC_H

/* ---------------------------------------------------------------------
 * Board boot configuration
 */
#ifdef CONFIG_LUA_GICV2_LOWLEVEL_INIT
#define GICC_BASE    CONFIG_LUA_GICC_BASE
#define GICD_BASE    CONFIG_LUA_GICD_BASE
#endif

#ifdef CONFIG_LUA_CUSTOM_SERVERIP
#define SERVERIP_ENV "serverip=" CONFIG_LUA_SERVERIP "\0"
#else
#define SERVERIP_ENV "serverip=""\0"
#endif

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
		"else "                                             \
		"  echo ramdisk_size not set; "                     \
		"  booti ${kernel_addr_r} - ${fdt_addr_r}; "        \
		"  echo ramdisk not builtin into kernel image; "    \
		"fi\0"
#define BOOTENV_DEV_NAME_FPGA(devtypeu, devtypel, instance) "fpga "
#define BOOT_TARGET_FPGA(func) func(FPGA, fpga, na)

#define BOOTENV_DEV_TFTP(devtypeu, devtypel, instance)      \
	"bootcmd_tftp= "                                        \
		"if test ! -e ${ipaddr}; then "                     \
		"  dhcp; "                                          \
		"  if test $? -ne 0; then "                         \
		"    echo dhcp failed; "                            \
		"    exit -2; "                                     \
		"  fi; "                                            \
		"fi; "                                              \
		"if test -n ${serverip}; then "                     \
		"  ping ${serverip}; "                              \
		"  if test $? -ne 0; then "                         \
		"    echo ping faild; "                             \
		"    exit -2; "                                     \
		"  fi; "                                            \
		"  tftpboot ${kernel_addr_r} ${kernel_name}; "      \
		"  tftpboot ${fdt_addr_r} ${fdtfile}; "             \
		"  tftpboot ${ramdisk_addr_r} ${ramdisk_name}; "    \
		"  if test $? -ne 0; then "                         \
		"    booti ${kernel_addr_r} - ${fdt_addr_r}; "      \
		"  else "                                           \
		"    fdt addr ${fdt_addr_r}; "                      \
		"    fdt resize; "                                  \
		"    fdt chosen ${ramdisk_addr_r} ${filesize}; "    \
		"    booti ${kernel_addr_r} - ${fdt_addr_r}; "      \
		"  fi; "                                            \
		"else "                                             \
		"  echo serverip not set; "                         \
		"fi\0"
#define BOOTENV_DEV_NAME_TFTP(devtypeu, devtypel, instance) "tftp "
#if CONFIG_IS_ENABLED(CMD_TFTPBOOT)
#define BOOT_TARGET_TFTP(func) func(TFTP, tftp, na)
#else
#define BOOT_TARGET_TFTP(func)
#endif

/* booting from any bootable partition */
#define BOOTENV_DEV_BLK_BOOTPART(devtypeu, devtypel, instance)    \
		"bootcmd_block= run scan_dev_for_boot_part\0"
#define BOOTENV_DEV_NAME_BLK_BOOTPART(devtypeu, devtypel, instance) "blk_bootpart "

#if CONFIG_IS_ENABLED(CMD_BLOCK)
#define BOOT_TARGET_BLK_BOOTPART(func) func(BLK_BOOTPART, block, na)
#else
#define BOOT_TARGET_BLK_BOOTPART(func)
#endif

/* Based on shell script booting from two bootable partition (Slot A or B) */
#define BOOTENV_DEV_BLOCK_AB(devtypeu, devtypel, instance)                        \
		"bootcmd_block_ab= "                                                      \
		"  ab_select slot ${devtype} ${devnum}#misc; "                            \
		"  if test $? -ne 0; then "                                               \
		"    ab_select slot ${devtype} ${devnum}#misc_bak; "                      \
		"    if test $? -ne 0; then "                                             \
		"      echo Inavild AB control; "                                         \
		"      exit -2; "                                                         \
		"    else "                                                               \
		"      echo Using backup AB control; "                                    \
		"    fi; "                                                                \
		"  fi; "                                                                  \
		"  part list ${devtype} ${devnum} -bootable devplist; "                   \
		"  env exists devplist || exit -2; "                                      \
		"  setenv count 0; "                                                      \
		"  for part in ${devplist}; do "                                          \
		"    setexpr count ${count} + 1; "                                        \
		"    if test ${count} -eq 1 && test ${slot} = \"a\"; then "               \
		"      echo Using slot A; "                                               \
		"      setenv distro_bootpart ${part}; "                                  \
		"    elif test ${count} -eq 2 && test ${slot} = \"b\"; then "             \
		"      echo Using slot B; "                                               \
		"      setenv distro_bootpart ${part}; "                                  \
		"    fi; "                                                                \
		"  done; "                                                                \
		"  setenv count; "                                                        \
		"  echo Current part 0x${distro_bootpart}; "                              \
		"  if fstype ${devtype} ${devnum}:${distro_bootpart} bootfstype; then "   \
		"    run scan_dev_for_boot; "                                             \
		"  else "                                                                 \
		"    echo Boot failed; "                                                  \
		"    reset; "                                                             \
		"  fi\0"
#define BOOTENV_DEV_NAME_BLOCK_AB(devtypeu, devtypel, instance) "block_ab "
#if CONFIG_IS_ENABLED(CMD_AB_SELECT)
#define BOOT_TARGET_BLOCK_AB(func) func(BLOCK_AB, block_ab, na)
#else
#define BOOT_TARGET_BLOCK_AB(func)
#endif

/* booting from two bootable partition (Slot A or B)
 * top-half: setup devtype, devnum, distro_bootpart and prefix environment variables
 * bottom-half: using shell script to load image and boot
 */
#define BOOTENV_DEV_BLK_EXTLINUX(devtypeu, devtypel, instance)    \
		"bootcmd_blk_extlinux= "                                  \
		"  if test -e ${devtype} ${devnum}:${distro_bootpart}"    \
		"       ${prefix}${boot_syslinux_conf}; then "            \
		"    echo Found ${prefix}${boot_syslinux_conf}; "         \
		"    run boot_extlinux; "                                 \
		"    echo BOOT EXTLINUX FAILED; "                         \
		"  else "                                                 \
		"    echo Not Found ${prefix}${boot_syslinux_conf} on "   \
		"           ${devtype} ${devnum}:${distro_bootpart}; "    \
		"    exit -2; "                                           \
		"fi\0"
#define BOOTENV_DEV_NAME_BLK_EXTLINUX(devtypeu, devtypel, instance) "blk_extlinux "
#if CONFIG_IS_ENABLED(CMD_BLOCK)
#define BOOT_TARGET_BLK_EXTLINUX(func) func(BLK_EXTLINUX, block, na)
#else
#define BOOT_TARGET_BLK_EXTLINUX(func)
#endif

#ifdef CONFIG_TARGET_LUA_VIRT
#define BOOT_TARGET_DEVICES(func)   \
		BOOT_TARGET_BLK_EXTLINUX(func)
#elif defined(CONFIG_TARGET_LUA_FPGA)
#define BOOT_TARGET_DEVICES(func)   \
		BOOT_TARGET_FPGA(func)      \
		BOOT_TARGET_TFTP(func)      \
		BOOT_TARGET_MEM(func)
#else
#define BOOT_TARGET_DEVICES(func)   \
		BOOT_TARGET_MMC(func)       \
		BOOT_TARGET_MMC1(func)      \
		BOOT_TARGET_DHCP(func)      \
		BOOT_TARGET_MEM(func)
#endif

#define EXTRA_ENV_NAMES                                                       \
		"kernel_name=" __stringify(CONFIG_LUA_KERNEL_LOAD_NAME)"\0"           \
		"ramdisk_name=" __stringify(CONFIG_LUA_RAMDISK_LOAD_NAME)"\0"         \
		"fdtfile=" __stringify(CONFIG_LUA_FDT_LOAD_NAME)"\0"                  \
		"boot_name=boot.img\0"                                                \
		"boot_addr_r=" __stringify(CONFIG_LUA_KERNEL_LOAD_ADDR)"\0"           \
		"main_mtd=0\0"                                                        \
		"safe_mtd=0\0"                                                        \
		"mmc_dev=0\0"                                                         \
		"secureboot=0\0"                                                      \
		"devtype=none\0"                                                      \
		"devnum=0\0"                                                          \
		"bootslot=0\0"

#include <config_distro_bootcmd.h>

#define CONFIG_EXTRA_ENV_SETTINGS                                             \
		"autoload=0\0"                                                        \
		"scriptaddr=" __stringify(CONFIG_LUA_SCRIPT_LOAD_ADDR) "\0"           \
		"fdt_addr_r=" __stringify(CONFIG_LUA_FDT_LOAD_ADDR) "\0"              \
		"kernel_addr_r=" __stringify(CONFIG_LUA_KERNEL_LOAD_ADDR) "\0"        \
		"ramdisk_addr_r=" __stringify(CONFIG_LUA_RAMDISK_LOAD_ADDR) "\0"      \
		"kernel_comp_addr_r=" __stringify(CONFIG_LUA_KERNEL_COMP_ADDR) "\0"   \
		"kernel_comp_size=" __stringify(CONFIG_LUA_KERNEL_COMP_SIZE) "\0"     \
		EXTRA_ENV_NAMES                                                       \
		SERVERIP_ENV                                                          \
		BOOTENV

/* SPL support */
#define CONFIG_SYS_MONITOR_LEN    (2 << 20)

/* nand cmd */
#if defined(CONFIG_CMD_NAND) || defined(CONFIG_MTD_RAW_NAND)
#	define CONFIG_SYS_MAX_NAND_DEVICE	1
#	define CONFIG_SYS_NAND_SELF_INIT 1
#endif

#endif
