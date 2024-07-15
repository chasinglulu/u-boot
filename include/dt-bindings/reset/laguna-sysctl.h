/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#ifndef RESET_LAGUNA_SYSCTL_H
#define RESET_LAGUNA_SYSCTL_H

/* Common subsystem */
#define LAGUNA_COMM_TIMER0_PRST         0
#define LAGUNA_COMM_TIMER1_PRST         1
#define LAGUNA_COMM_TIMER2_PRST         2
#define LAGUNA_COMM_TIMER3_PRST         3
#define LAGUNA_COMM_TIMER0_RST          4
#define LAGUNA_COMM_TIMER1_RST          5
#define LAGUNA_COMM_TIMER2_RST          6
#define LAGUNA_COMM_TIMER3_RST          7
#define LAGUNA_COMM_WWDT0_PRST          8
#define LAGUNA_COMM_WWDT1_PRST          9
#define LAGUNA_COMM_WWDT2_PRST          10
#define LAGUNA_COMM_WWDT3_PRST          11
#define LAGUNA_COMM_WWDT0_RST           12
#define LAGUNA_COMM_WWDT1_RST           13
#define LAGUNA_COMM_WWDT2_RST           14
#define LAGUNA_COMM_WWDT3_RST           15

/* CPU subsystem */
#define LAGUNA_CPU_DSU_RST              0
#define LAGUNA_CPU_ATGICCLK_RST         1
#define LAGUNA_CPU_DSU_PERI_RST         2
#define LAGUNA_CPU_PCLK_RST             3
#define LAGUNA_CPU_DBG_RST              4
#define LAGUNA_CPU_ROSC_RST             5

/* Flash subsystem */
#define LAGUNA_FLASH_SPIFLASH_HRST      0
#define LAGUNA_FLASH_SPIFLASH_RST       1
#define LAGUNA_FLASH_DMAPER_PRST        2
#define LAGUNA_FLASH_DMAPER_RST         3
#define LAGUNA_FLASH_SD_SDM_RST         4
#define LAGUNA_FLASH_SD_RST             5
#define LAGUNA_FLASH_EMMC_SDM_RST       6
#define LAGUNA_FLASH_EMMC_RST           7
#define LAGUNA_FLASH_PSPI_PRST          8
#define LAGUNA_FLASH_PSPI_RST           9
#define LAGUNA_FLASH_I2C9_PRST          10
#define LAGUNA_FLASH_I2C9_RST           11
#define LAGUNA_FLASH_GPIO_PRST          12
#define LAGUNA_FLASH_GPIO_RST           13
#define LAGUNA_FLASH_PINMUX_PRST        14
#define LAGUNA_FLASH_DBGMNR_PRST        15
#define LAGUNA_FLASH_DBGMNR_ARST        16
#define LAGUNA_FLASH_LPC_RST            17
#define LAGUNA_FLASH_BWLMT_RST          18
#define LAGUNA_FLASH_BWLMT_ARST         19
#define LAGUNA_FLASH_TLB_RST            20
#define LAGUNA_FLASH_ISO_24M_RST        21
#define LAGUNA_FLASH_ISO_PRST           22

/* Periph subsystem */
#define LAGUNA_PERI_SPI_PRST            0
#define LAGUNA_PERI_SPI_RST             1
#define LAGUNA_PERI_GPIO4_PRST          2
#define LAGUNA_PERI_GPIO5_PRST          3
#define LAGUNA_PERI_GPIO4_RST           4
#define LAGUNA_PERI_GPIO5_RST           5
#define LAGUNA_PERI_I2SM_PRST           6
#define LAGUNA_PERI_I2SS_PRST           7
#define LAGUNA_PERI_I2SM_SRST           8
#define LAGUNA_PERI_I2SS_SRST           9
#define LAGUNA_PERI_PINCTRL_PRST        10
#define LAGUNA_PERI_UART1_PRST          11
#define LAGUNA_PERI_UART2_PRST          12
#define LAGUNA_PERI_UART3_PRST          13
#define LAGUNA_PERI_UART4_PRST          14
#define LAGUNA_PERI_UART5_PRST          15
#define LAGUNA_PERI_UART1_RST           16
#define LAGUNA_PERI_UART2_RST           17
#define LAGUNA_PERI_UART3_RST           18
#define LAGUNA_PERI_UART4_RST           19
#define LAGUNA_PERI_UART5_RST           20
#define LAGUNA_PERI_I2C4_PRST           21
#define LAGUNA_PERI_I2C5_PRST           22
#define LAGUNA_PERI_I2C7_PRST           23
#define LAGUNA_PERI_I2C8_PRST           24
#define LAGUNA_PERI_I2C4_RST            25
#define LAGUNA_PERI_I2C5_RST            26
#define LAGUNA_PERI_I2C7_RST            27
#define LAGUNA_PERI_I2C8_RST            28
#define LAGUNA_PERI_SPIM5_PRST          29
#define LAGUNA_PERI_SPIM5_RST           30
#define LAGUNA_PERI_USBPHY_POR_RST      31

#define LAGUNA_PERI_USBPHY_PRST         0
#define LAGUNA_PERI_USBCTRL_HRST        1
#define LAGUNA_PERI_USBCTRL_ARST        2
#define LAGUNA_PERI_CRC_ENGI_PRST       3
#define LAGUNA_PERI_CRC_ENGI_RST        4
#define LAGUNA_PERI_AXDMAPER_PRST       5
#define LAGUNA_PERI_AXDMAPER_RST        6
#define LAGUNA_PERI_AXDMA_PRST          7
#define LAGUNA_PERI_AXDMA_RST           8
#define LAGUNA_PERI_LPC_RST             9
#define LAGUNA_PERI_EMAC_ARST           10
#define LAGUNA_PERI_DMABWLMT_24M_RST    11
#define LAGUNA_PERI_DMABWLMT_PRST       12
#define LAGUNA_PERI_DMABWLMT_ARST       13
#define LAGUNA_PERI_DMAPER_DBGMNR_PRST  14
#define LAGUNA_PERI_DMAPER_DBGMNR_ARST  15
#define LAGUNA_PERI_CAN0_PRST           16
#define LAGUNA_PERI_CAN1_PRST           17
#define LAGUNA_PERI_CAN0_CORE_RST       18
#define LAGUNA_PERI_CAN1_CORE_RST       19
#define LAGUNA_PERI_CAN0_TIMESTAMP_RST  20
#define LAGUNA_PERI_CAN1_TIMESTAMP_RST  21
#define LAGUNA_PERI_TLB_RST             22
#define LAGUNA_PERI_ISO_24M_RST         23
#define LAGUNA_PERI_ISO_PRST            24

#endif /* RESET_LAGUNA_SYSCTL_H */
