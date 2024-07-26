/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */


#ifndef LAGUNA_SYSCTL_H
#define LAGUNA_SYSCTL_H

/* ck_rst regions */
#define LAGUNA_SYSCTL_COMM_SWRST         0x120
#define LAGUNA_SYSCTL_CPU_SWRST          0x120
#define LAGUNA_SYSCTL_FLASH_SWRST        0x120
#define LAGUNA_SYSCTL_PERIPH_SWRST0      0x120
#define LAGUNA_SYSCTL_PERIPH_SWRST1      0x124
#define LAGUNA_SYSCTL_PERIPH_CAN_SWRST   0x164

/* glb regions */
#define LAGUNA_SYSCON_PERIPH_EMAC       0x24
#define LAGUNA_SYSCON_PERIPH_UART       0x2C
#define LAGUNA_SYSCON_PERIPH_EMAC1      0x68

/* Periph UART register field */
#define UART_XFER_RS232_2LINE         0
#define UART_XFER_RS232_4LINE         1
#define UART_XFER_IRDA_2LINE          2
#define UART_XFER_RS485_4LINE         3

#endif