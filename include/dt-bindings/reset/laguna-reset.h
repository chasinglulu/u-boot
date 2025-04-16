/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#ifndef LAGUNA_RESET_H
#define LAGUNA_RESET_H

/* safety subsystem sw_rst_0 register */
#define R5F0_SW_RST                         0
#define R5F1_SW_RST                         1
#define R5F0_DBG_SW_RST                     2
#define R5F1_DBG_SW_RST                     3
#define SAFE_CE_SW_PRST                     4
#define SAFE_CE_CNT_SW_RST                  5
#define SAFE_CE_SW_RST                      6
#define SAFE_CE_MAIN_SW_RST                 7
#define SAFE_CE_SOFT_SW_RST                 8
#define SAFE_SPI_S_SW_PRST                  9
#define SAFE_SPI_S_SW_RST                   10
#define SAFE_QSPI_SW_HRST                   11
#define SAFE_QSPI_SW_RST                    12
#define SAFE_EMAC_SW_ARST                   13
#define SAFE_AXERA_SW_INT1_SW_PRST          14
#define SAFE_AXERA_PWM1_SW_PRST             15
#define SAFE_AXERA_PWM11_SW_RST             16
#define SAFE_AXERA_PWM12_SW_RST             17
#define SAFE_AXERA_PWM13_SW_RST             18
#define SAFE_AXERA_PWM14_SW_RST             19
#define SAFE_AXERA_PWM15_SW_RST             20
#define SAFE_AXERA_PWM16_SW_RST             21
#define SAFE_AXERA_PWM17_SW_RST             22
#define SAFE_AXERA_PWM18_SW_RST             23
#define SAFE_AXERA_PWM2_SW_PRST             24
#define SAFE_AXERA_PWM21_SW_RST             25
#define SAFE_AXERA_PWM22_SW_RST             26
#define SAFE_AXERA_PWM23_SW_RST             27
#define SAFE_AXERA_PWM24_SW_RST             28
#define SAFE_AXERA_PWM25_SW_RST             29
#define SAFE_AXERA_PWM26_SW_RST             30
#define SAFE_AXERA_PWM27_SW_RST             31

/* safety subsystem sw_rst_1 register */
#define SAFE_AXERA_PWM28_SW_RST             0
#define SAFE_AXERA_ECAP_SW_PRST             1
#define SAFE_AXERA_ECAP_SW_RST              2
#define GZIP_SW_PRST                        3
#define GZIP_SW_ARST                        4
#define SAFE_FW0_SW_PRST                    5
#define SAFE_FW1_SW_PRST                    6
#define SAFE_FW2_SW_PRST                    7
#define SAFE_CRC_ENGINE_SW_PRST             8
#define SAFE_CRC_ENGINE_SW_RST              9
#define SAFE_AX_DMA_PER_SW_PRST             10
#define SAFE_AX_DMA_PER_SW_RST              11
#define SAFE_AX_DMA_SW_PRST                 12
#define SAFE_AX_DMA_SW_RST                  13
#define SAFE_UART0_SW_PRST                  14
#define SAFE_UART0_SW_RST                   15
#define SAFE_SPI_M1_SW_PRST                 16
#define SAFE_SPI_M1_SW_RST                  17
#define SAFE_SPI_M2_SW_PRST                 18
#define SAFE_SPI_M2_SW_RST                  19
#define SAFE_SPI_M3_SW_PRST                 20
#define SAFE_SPI_M3_SW_RST                  21
#define SAFE_SPI_M4_SW_PRST                 22
#define SAFE_SPI_M4_SW_RST                  23
#define SAFE_SPI_M6_SW_PRST                 24
#define SAFE_SPI_M6_SW_RST                  25
#define SAFE_I2C2_SW_PRST                   26
#define SAFE_I2C2_SW_RST                    27
#define SAFE_I2C3_SW_PRST                   28
#define SAFE_I2C3_SW_RST                    29
#define CAN2_SW_PRST                        30
#define CAN3_SW_PRST                        31

/* safety subsystem sw_rst_2 register */
#define CAN4_SW_PRST                        0
#define CAN5_SW_PRST                        1
#define CAN6_SW_PRST                        2
#define CAN2_CORE_SW_RST                    3
#define CAN3_CORE_SW_RST                    4
#define CAN4_CORE_SW_RST                    5
#define CAN5_CORE_SW_RST                    6
#define CAN6_CORE_SW_RST                    7
#define CAN2_TMR_SW_RST                     8
#define CAN3_TMR_SW_RST                     9
#define CAN4_TMR_SW_RST                     10
#define CAN5_TMR_SW_RST                     11
#define CAN6_TMR_SW_RST                     12
#define SAFE_GPIO1_SW_PRST                  13
#define SAFE_GPIO1_SW_RST                   14
#define SAFE_GPIO2_SW_PRST                  15
#define SAFE_GPIO2_SW_RST                   16
#define SAFE_GPIO3_SW_PRST                  17
#define SAFE_GPIO3_SW_RST                   18
#define SAFE_MAILBOX_SW_PRST                19
#define SAFE_SPINLOCK_SW_PRST               20
#define SAFE_R5F0_DBGMNR_SW_PRST            21
#define SAFE_R5F1_DBGMNR_SW_PRST            22
#define SAFE_DMA_PER_DBGMNR_SW_PRST         23
#define SAFE_OCM_DBGMNR_SW_PRST             24
#define SAFE_R5F0_DBGMNR_SW_ARST            25
#define SAFE_R5F1_DBGMNR_SW_ARST            26
#define SAFE_DMA_PER_DBGMNR_SW_ARST         27
#define SAFE_OCM_DBGMNR_SW_ARST             28
#define SAFE_LPC_SW_RST                     29
#define TMR32_4_SW_PRST                     30
#define TMR32_5_SW_PRST                     31

/* safety subsystem sw_rst_3 register */
#define TMR32_6_SW_PRST                     0
#define TMR32_7_SW_PRST                     1
#define TMR32_8_SW_PRST                     2
#define TMR32_9_SW_PRST                     3
#define TMR32_10_SW_PRST                    4
#define TMR32_11_SW_PRST                    5
#define TMR32_4_SW_TRST                     6
#define TMR32_5_SW_TRST                     7
#define TMR32_6_SW_TRST                     8
#define TMR32_7_SW_TRST                     9
#define TMR32_8_SW_TRST                     10
#define TMR32_9_SW_TRST                     11
#define TMR32_10_SW_TRST                    12
#define TMR32_11_SW_TRST                    13
#define WWDT4_SW_PRST                       14
#define WWDT5_SW_PRST                       15
#define WWDT4_SW_TRST                       16
#define WWDT5_SW_TRST                       17
#define THM_ADC_SW_RST                      18
#define THM_ADC_SW_PRST                     19
#define SAFE_TIMESTAMP0_SW_PRST             20
#define SAFE_TIMESTAMP0_SW_RST              21
#define R5F0_ADDR_REMAP_SW_PRST             22
#define R5F1_ADDR_REMAP_SW_PRST             23
#define SAFETY_CLK_MNR_SW_PRST              24
#define ISO_M7_SW_PRST                      25
#define ISO_M8_SW_PRST                      26
#define ISO_M9_SW_PRST                      27
#define ISO_M10_SW_PRST                     28
#define ISO_S0_SW_PRST                      29
#define ISO_S1_SW_PRST                      30
#define ISO_M7_24M_SW_RST                   31

/* safety subsystem sw_rst_4 register */
#define ISO_M8_24M_SW_RST                   0
#define ISO_M9_24M_SW_RST                   1
#define ISO_M10_24M_SW_RST                  2
#define ISO_S0_24M_SW_RST                   3
#define ISO_S1_24M_SW_RST                   4
#define SAFE_I2C10_SW_PRST                  5
#define SAFE_I2C10_SW_RST                   6

/* safety subsystem can_soft_reset register */
#define SAFE_CAN2_SOFT_RESET                0
#define SAFE_CAN3_SOFT_RESET                1
#define SAFE_CAN4_SOFT_RESET                2
#define SAFE_CAN5_SOFT_RESET                3
#define SAFE_CAN6_SOFT_RESET                4

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
#define MBIST_24M_SW_RST                    31

/* cpu subsystem sw_rst_0 register */
#define DSU_SW_RST                          0
#define ATGICCLK_SW_RST                     1
#define DSU_PERIPH_SW_RST                   2
#define PCLK_CPU_SW_RST                     3
#define CPU_DBG_SW_RST                      4
#define CPU_ROSC_SW_RST                     5

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
