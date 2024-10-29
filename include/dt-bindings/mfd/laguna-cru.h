/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#ifndef LAGUNA_CRU_H
#define LAGUNA_CRU_H

/* safety clock & rst */
#define LAGUNA_SAFETY_CLK_MUX_0          0x00
#define LAGUNA_SAFETY_CLK_M_EB_0         0x20
#define LAGUNA_SAFETY_CLK_G_EB_0         0x40
#define LAGUNA_SAFETY_CLK_G_EB_1         0x44
#define LAGUNA_SAFETY_CLK_G_EB_2         0x48
#define LAGUNA_SAFETY_CLK_G_EB_3         0x4C
#define LAGUNA_SAFETY_SW_RST_0           0x120
#define LAGUNA_SAFETY_SW_RST_1           0x124
#define LAGUNA_SAFETY_SW_RST_2           0x128
#define LAGUNA_SAFETY_SW_RST_3           0x12C
#define LAGUNA_SAFETY_SW_RST_4           0x130

/* cpu clock & rst */
#define LAGUNA_CPU_CLK_MUX_0             0x00
#define LAGUNA_CPU_CLK_G_EB_0            0x40
#define LAGUNA_CPU_SW_RST_0              0x120

/* common clock & rst */
#define LAGUNA_COMM_CLK_MUX_0            0x00
#define LAGUNA_COMM_CLK_M_EB_0           0x20
#define LAGUNA_COMM_CLK_G_EB_0           0x40
#define LAGUNA_COMM_SW_RST_0             0x120

/* flash clock & rst */
#define LAGUNA_FLASH_CLK_MUX_0            0x00
#define LAGUNA_FLASH_CLK_M_EB_0           0x20
#define LAGUNA_FLASH_CLK_G_EB_0           0x40
#define LAGUNA_FLASH_SW_RST_0             0x120

/* periph clock & rst */
#define LAGUNA_PERIPH_CLK_MUX_0          0x00
#define LAGUNA_PERIPH_CLK_M_EB_0         0x20
#define LAGUNA_PERIPH_CLK_G_EB_0         0x40
#define LAGUNA_PERIPH_CLK_G_EB_1         0x44
#define LAGUNA_PERIPH_CLK_M_DIV_0        0x80
#define LAGUNA_PERIPH_SW_RST_0           0x120
#define LAGUNA_PERIPH_SW_RST_1           0x124
#define LAGUNA_PERIPH_CAN_SOFT_RESET     0x164

#endif