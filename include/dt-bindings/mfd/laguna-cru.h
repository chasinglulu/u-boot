/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#ifndef LAGUNA_CRU_H
#define LAGUNA_CRU_H

#define CLK_MUX_OFFSET         0x00
#define CLK_MUX_EB_OFFSET      0x20
#define CLK_GATE0_EB_OFFSET    0x40
#define CLK_GATE1_EB_OFFSET    0x44
#define CLK_GATE2_EB_OFFSET    0x48
#define CLK_GATE3_EB_OFFSET    0x4C
#define SW_RST0_OFFSET         0x120
#define SW_RST1_OFFSET         0x124
#define SW_RST2_OFFSET         0x128
#define SW_RST3_OFFSET         0x12C
#define SW_RST4_OFFSET         0x130

/* safety wrap clock & rst */
#define LAGUNA_SAFETY_CLK_MUX_0          CLK_MUX_OFFSET
#define LAGUNA_SAFETY_CLK_M_EB_0         CLK_MUX_EB_OFFSET
#define LAGUNA_SAFETY_CLK_G_EB_0         CLK_GATE0_EB_OFFSET
#define LAGUNA_SAFETY_CLK_G_EB_1         CLK_GATE1_EB_OFFSET
#define LAGUNA_SAFETY_CLK_G_EB_2         CLK_GATE2_EB_OFFSET
#define LAGUNA_SAFETY_CLK_G_EB_3         CLK_GATE3_EB_OFFSET
#define LAGUNA_SAFETY_SW_RST_0           SW_RST0_OFFSET
#define LAGUNA_SAFETY_SW_RST_1           SW_RST1_OFFSET
#define LAGUNA_SAFETY_SW_RST_2           SW_RST2_OFFSET
#define LAGUNA_SAFETY_SW_RST_3           SW_RST3_OFFSET
#define LAGUNA_SAFETY_SW_RST_4           SW_RST4_OFFSET

/* safety top clock & rst */
#define LAGUNA_SAFETY_TOP_CLK_MUX_0      CLK_MUX_OFFSET
#define LAGUNA_SAFETY_TOP_CLK_M_EB_0     CLK_MUX_EB_OFFSET
#define LAGUNA_SAFETY_TOP_CLK_G_EB_0     CLK_GATE0_EB_OFFSET
#define LAGUNA_SAFETY_TOP_SW_RST_0       SW_RST0_OFFSET

/* cpu clock & rst */
#define LAGUNA_CPU_CLK_MUX_0             CLK_MUX_OFFSET
#define LAGUNA_CPU_CLK_G_EB_0            CLK_GATE0_EB_OFFSET
#define LAGUNA_CPU_SW_RST_0              SW_RST0_OFFSET

/* DDR clock & rst */
#define LAGUNA_DDR_CLK_MUX_0             CLK_MUX_OFFSET
#define LAGUNA_DDR_CLK_G_EB_0            CLK_GATE0_EB_OFFSET
#define LAGUNA_DDR_SW_RST_0              SW_RST0_OFFSET
#define LAGUNA_DDR_SW_RST_1              SW_RST1_OFFSET

/* common clock & rst */
#define LAGUNA_COMM_CLK_MUX_0            CLK_MUX_OFFSET
#define LAGUNA_COMM_CLK_M_EB_0           CLK_MUX_EB_OFFSET
#define LAGUNA_COMM_CLK_G_EB_0           CLK_GATE0_EB_OFFSET
#define LAGUNA_COMM_SW_RST_0             SW_RST0_OFFSET

/* flash clock & rst */
#define LAGUNA_FLASH_CLK_MUX_0            CLK_MUX_OFFSET
#define LAGUNA_FLASH_CLK_M_EB_0           CLK_MUX_EB_OFFSET
#define LAGUNA_FLASH_CLK_G_EB_0           CLK_GATE0_EB_OFFSET
#define LAGUNA_FLASH_SW_RST_0             SW_RST0_OFFSET

/* periph clock & rst */
#define LAGUNA_PERIPH_CLK_MUX_0          CLK_MUX_OFFSET
#define LAGUNA_PERIPH_CLK_M_EB_0         CLK_MUX_EB_OFFSET
#define LAGUNA_PERIPH_CLK_G_EB_0         CLK_GATE0_EB_OFFSET
#define LAGUNA_PERIPH_CLK_G_EB_1         CLK_GATE1_EB_OFFSET
#define LAGUNA_PERIPH_CLK_M_DIV_0        0x80
#define LAGUNA_PERIPH_SW_RST_0           SW_RST0_OFFSET
#define LAGUNA_PERIPH_SW_RST_1           SW_RST1_OFFSET
#define LAGUNA_PERIPH_CAN_SOFT_RESET     0x164

#endif