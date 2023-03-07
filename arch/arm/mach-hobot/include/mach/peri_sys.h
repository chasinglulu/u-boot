/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2023 Horizon Robotics Co., Ltd
 *
 */

#ifndef _ASM_ARCH_PERI_SYS_SIGI_H
#define _ASM_ARCH_PERI_SYS_SIGI_H

#ifndef __ASSEMBLY__
#include <linux/bitops.h>
#endif

struct sigi_peri_sys {
	unsigned int clk_gate_en[3];
	unsigned int softrst_con[4];
	unsigned int dma_cfg;
	unsigned int spi_cfg;
	unsigned int noc_idle_monitor;
	unsigned int ufs_sram_ctrl_stat;
	unsigned int rtc_valid_read;
	unsigned int rtc_snaptime[2];
	unsigned int rtc_time[2];
	unsigned int i2s_mclk_div;
	unsigned int can_ctrl[2];
	unsigned int noc_shaping[3];
	unsigned int reserved1[42];
	unsigned int emmc_cfg[2];
	unsigned int emmc0_boot_cfg[3];
};
check_member(sigi_peri_sys, emmc_cfg[0], 0x100);

#endif
