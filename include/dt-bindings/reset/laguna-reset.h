/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#ifndef LAGUNA_RESET_H
#define LAGUNA_RESET_H

/* Common subsystem sw_rst_0 register */
#define TMR32_0_SW_PRST                     0
#define TMR32_1_SW_PRST                     1
#define TMR32_2_SW_PRST                     2
#define TMR32_3_SW_PRST                     3
#define TMR32_0_SW_RST                      4
#define TMR32_1_SW_RST                      5
#define TMR32_2_SW_RST                      6
#define TMR32_3_SW_RST                      7
#define WWDT0_SW_PRST                       8
#define WWDT1_SW_PRST                       9
#define WWDT2_SW_PRST                       10
#define WWDT3_SW_PRST                       11
#define WWDT0_SW_RST                        12
#define WWDT1_SW_RST                        13
#define WWDT2_SW_RST                        14
#define WWDT3_SW_RST                        15
#define AXERA_SW_INT1_SW_PRST               16
#define PERF_MNR_LAT0_SW_PRST               17
#define PERF_MNR_LAT1_SW_PRST               18
#define PERF_MNR_LAT0_24M_SW_RST            19
#define PERF_MNR_LAT1_24M_SW_RST            20
#define PERF_MNR_LAT0_SW_ARST               21
#define PERF_MNR_LAT1_SW_ARST               22
#define PLL_CTRL2_SW_RST                    23
#define A55_DBG_MNR_SW_PRST                 24
#define OCM_DBG_MNR_SW_PRST                 25
#define OCM_DBG_MNR_SW_ARST                 26
#define A55_DBG_MNR_SW_ARST                 27
#define DEB_GPIO_SW_RST                     28
#define COMM_TLB_SW_RST                     29
#define COMM_CLK_MNR_SW_RST                 30

/* Flash subsystem sw_rst_0 register */
#define FLASH_SPIFLASH_SW_HRST              0
#define FLASH_SPIFLASH_SW_RST               1
#define FLASH_DMA_PER_SW_PRST               2
#define FLASH_DMA_PER_SW_RST                3
#define SD_SDM_SW_RST                       4
#define SD_SW_RST                           5
#define EMMC_SDM_SW_RST                     6
#define EMMC_SW_RST                         7
#define FLASH_PSPI_SW_PRST                  8
#define FLASH_PSPI_SW_RST                   9
#define FLASH_I2C9_SW_PRST                  10
#define FLASH_I2C9_SW_RST                   11
#define FLASH_GPIO_SW_PRST                  12
#define FLASH_GPIO_SW_RST                   13
#define FLASH_PINMUX_SW_PRST                14
#define FLASH_DBGMNR_SW_PRST                15
#define FLASH_DBGMNR_SW_ARST                16
#define FLASH_LPC_SW_RST                    17
#define FLASH_BW_LMT_SW_RST                 18
#define FLASH_BW_LMT_SW_ARST                19
#define FLASH_TLB_SW_RST                    20
#define FLASH_ISO_24M_SW_RST                21
#define FLASH_ISO_SW_PRST                   22

/* Periph subsystem sw_rst_0 register */
#define PERIPH_SPI_S_SW_PRST                0
#define PERIPH_SPI_S_SW_RST                 1
#define PERIPH_GPIO4_SW_PRST                2
#define PERIPH_GPIO5_SW_PRST                3
#define PERIPH_GPIO4_SW_RST                 4
#define PERIPH_GPIO5_SW_RST                 5
#define I2S_M_SW_PRST                       6
#define I2S_S_SW_PRST                       7
#define I2S_M_SW_SRST                       8
#define I2S_S_SW_SRST                       9
#define PERIPH_PINREG_SW_PRST               10
#define PERIPH_UART1_SW_PRST                11
#define PERIPH_UART2_SW_PRST                12
#define PERIPH_UART3_SW_PRST                13
#define PERIPH_UART4_SW_PRST                14
#define PERIPH_UART5_SW_PRST                15
#define PERIPH_UART1_SW_RST                 16
#define PERIPH_UART2_SW_RST                 17
#define PERIPH_UART3_SW_RST                 18
#define PERIPH_UART4_SW_RST                 19
#define PERIPH_UART5_SW_RST                 20
#define PERIPH_I2C4_SW_PRST                 21
#define PERIPH_I2C5_SW_PRST                 22
#define PERIPH_I2C7_SW_PRST                 23
#define PERIPH_I2C8_SW_PRST                 24
#define PERIPH_I2C4_SW_RST                  25
#define PERIPH_I2C5_SW_RST                  26
#define PERIPH_I2C7_SW_RST                  27
#define PERIPH_I2C8_SW_RST                  28
#define PERIPH_SPI_M5_SW_PRST               29
#define PERIPH_SPI_M5_SW_RST                30
#define USB_PHY_POR_SW_RST                  31

/* Periph subsystem sw_rst_1 register */
#define USB_PHY_SW_PRST                     0
#define USB_CTRL_SW_ARST                    1
#define PERIPH_CRC_ENGINE_SW_PRST           2
#define PERIPH_CRC_ENGINE_SW_RST            3
#define PERIPH_AX_DMA_PER_SW_PRST           4
#define PERIPH_AX_DMA_PER_SW_RST            5
#define PERIPH_AX_DMA_SW_PRST               6
#define PERIPH_AX_DMA_SW_RST                7
#define PERIPH_LPC_SW_RST                   8
#define MAIN_EMAC_SW_ARST                   9
#define PERIPH_DMA_BW_LMT_24M_SW_RST        10
#define PERIPH_DMA_BW_LMT_SW_PRST           11
#define PERIPH_DMA_BW_LMT_SW_ARST           12
#define PERIPH_DMA_PER_DBGMNR_SW_PRST       13
#define PERIPH_DMA_PER_DBGMNR_SW_ARST       14
#define CAN0_SW_PRST                        15
#define CAN1_SW_PRST                        16
#define CAN0_CORE_SW_RST                    17
#define CAN1_CORE_SW_RST                    18
#define TIME_STAMP_CAN0_SW_RST              19
#define TIME_STAMP_CAN1_SW_RST              20
#define PERIPH_TLB_SW_RST                   21
#define PERIPH_ISO_24M_SW_RST               22
#define PERIPH_ISO_SW_PRST                  23

#endif /* LAGUNA_RESET_H */
