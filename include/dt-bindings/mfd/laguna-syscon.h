/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#ifndef LAGUNA_SYSCON_H
#define LAGUNA_SYSCON_H

/* safety glb register */
#define LAGUNA_SYSCON_SAFETY_UART                0x8C

/* safety glb aon register */
#define LAGUNA_SYSCON_SAFETY_RST_CTRL            0x0C
#define LAGUNA_SYSCON_SAFETY_RST_ALARM           0x18

/* safety fcu register */
#define LAGUNA_SYSCON_SAFETY_ABC                 0x00

/* periph glb register */
#define LAGUNA_SYSCON_PERIPH_EMAC                0x28
#define LAGUNA_SYSCON_PERIPH_UART                0x30
#define LAGUNA_SYSCON_PERIPH_EMAC1               0x6C

/* periph glb UART register field */
#define UART_XFER_RS232_2LINE                    0
#define UART_XFER_RS232_4LINE                    1
#define UART_XFER_IRDA_2LINE                     2
#define UART_XFER_RS485_4LINE                    3

/* Top glb register */
#define LAGUNA_SYSCON_TOP_ABORT_ALARM            0x88

#endif