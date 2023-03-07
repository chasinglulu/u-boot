/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2023 Horizon Robotics Co., Ltd.
 */

#ifndef _DT_BINDINGS_RESET_HOBOT_SIGI_H
#define _DT_BINDINGS_RESET_HOBOT_SIGI_H

/* soft-reset indices */
#define SRST_LPWM3          0
#define SRST_LPWM3_APB      1
#define SRST_LPWM2          2
#define SRST_LPWM2_APB      3
#define SRST_LPWM1          4
#define SRST_LPWM1_APB      5
#define SRST_LPWM0          6
#define SRST_LPWM0_APB      7
#define SRST_I2C9           8
#define SRST_I2C9_APB		9
#define SRST_I2C8           10
#define SRST_I2C8_APB       11
#define SRST_I2C7           12
#define SRST_I2C7_APB       13
#define SRST_I2C6           14
#define SRST_I2C6_APB       15
#define SRST_I2C5           16
#define SRST_I2C5_APB		17
#define SRST_I2C4           18
#define SRST_I2C4_APB       19
#define SRST_I2C3           20
#define SRST_I2C3_APB       21
#define SRST_I2C2           22
#define SRST_I2C2_APB       23
#define SRST_I2C1           24
#define SRST_I2C1_APB       25
#define SRST_I2C0           26
#define SRST_I2C0_APB       27
#define SRST_USB_ULTRA      29

#define SRST_INNER_SPIS1        32
#define SRST_INNER_SPIS1_APB    33
#define SRST_INNER_SPIS0        34
#define SRST_INNER_SPIS0_APB    35
#define SRST_INNER_SPIM1        36
#define SRST_INNER_SPIM1_APB    37
#define SRST_INNER_SPIM0        38
#define SRST_INNER_SPIM0_APB    39
#define SRST_SPIM3              40
#define SRST_SPIM3_APB          41
#define SRST_SPIM2              42
#define SRST_SPIM2_APB          43
#define SRST_SPIM1              44
#define SRST_SPIM1_APB          45
#define SRST_SPIM0              46
#define SRST_SPIM0_APB          47
#define SRST_EMMC1_SDM          48
#define SRST_EMMC1_MAIN         49
#define SRST_EMMC0_SDM          50
#define SRST_EMMC0_MAIN         51
#define SRST_PCM3               52
#define SRST_PCM3_APB           53
#define SRST_PCM2               54
#define SRST_PCM2_APB           55
#define SRST_PCM1               56
#define SRST_PCM1_APB           57
#define SRST_PCM0               58
#define SRST_PCM0_APB           59
#define SRST_EMMC1_NOC          60
#define SRST_EMMC0_NOC          61
#define SRST_UFS_NOC            62
#define SRST_USB_NOC            63

#define SRST_UFS_AXI            64
#define SRST_UFS                65
#define SRST_EMMC1_APB          66
#define SRST_EMMC0_APB          67
#define SRST_USB_APB            68
#define SRST_CMM1_APB           70
#define SRST_CMM0_APB           71
#define SRST_FCHM_APB           72
#define SRST_CRC                73
#define SRST_CRC_APB            74
#define SRST_INNER_SPIS3        75
#define SRST_INNER_SPIS3_APB    76
#define SRST_INNER_SPIS2        77
#define SRST_INNER_SPIS2_APB    78
#define SRST_INNER_SPIM3        79
#define SRST_INNER_SPIM3_APB    80
#define SRST_INNER_SPIM2        81
#define SRST_INNER_SPIM2_APB    82
#define SRST_PERI_PADC          83
#define SRST_CANFD0             84
#define SRST_CANFD0_APB         85
#define SRST_CANFD1             86
#define SRST_CANFD1_APB         87
#define SRST_CANFD2             88
#define SRST_CANFD2_APB         89
#define SRST_CANFD3             90
#define SRST_CANFD3_APB         91
#define SRST_FW0_APB            92
#define SRST_FW1_APB            93
#define SRST_PERI_UFS           94

#define SRST_USBPHY_POR         96
#define SRST_USB_VCC            97
#define SRST_UART7              98
#define SRST_UART7_APB          99
#define SRST_UART6              100
#define SRST_UART6_APB          101
#define SRST_UART5              102
#define SRST_UART5_APB          103
#define SRST_UART4              104
#define SRST_UART4_APB          105
#define SRST_UART3              106
#define SRST_UART3_APB          107
#define SRST_UART2              108
#define SRST_UART2_APB          109
#define SRST_UART1              110
#define SRST_UART1_APB          111
#define SRST_UART0              112
#define SRST_UART0_APB          113
#define SRST_GPIO3              114
#define SRST_GPIO3_APB          115
#define SRST_GPIO2              116
#define SRST_GPIO2_APB          117
#define SRST_GPIO1              118
#define SRST_GPIO1_APB          119
#define SRST_GPIO0              120
#define SRST_GPIO0_APB          121
#define SRST_DMA0               122
#define SRST_DMA1               123
#define SRST_DMA2               124
#define SRST_DMA3               125
#define SRST_DMA4               126
#define SRST_DMA5               127

#endif