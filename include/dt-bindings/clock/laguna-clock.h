/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#ifndef LAGUNA_CLOCK_H
#define LAGUNA_CLOCK_H

/* safety PLL */
#define SAFE_EPLL                       0
#define SAFE_RPLL                       1
#define SAFE_FAB_PLL                    2

/* safety subsystem clk_mux_0 register */
#define CLK_SAFE_R5F                    0
#define CLK_SAFE_QSPI                   1
#define CLK_SAFE_CE_BUS                 2
#define CLK_SAFE_GPIO                   3
#define CLK_SAFE_TMR0                   4
#define CLK_SAFE_TMR1                   5
#define CLK_SAFE_WDT                    6
#define CLK_SAFE_SER                    7
#define CLK_SAFE_RGMII_TX               8
#define CLK_SAFE_CAN_PE                 9
#define CLK_SAFE_EPHY_REF               10

#define CLK_SAFE_R5F_PARENT_NUM         4
#define CLK_SAFE_QSPI_PARENT_NUM        4
#define CLK_SAFE_CE_BUS_PARENT_NUM      3
#define CLK_SAFE_GPIO_PARENT_NUM        2
#define CLK_SAFE_TMR0_PARENT_NUM        2
#define CLK_SAFE_TMR1_PARENT_NUM        2
#define CLK_SAFE_WDT_PARENT_NUM         2
#define CLK_SAFE_SER_PARENT_NUM         3
#define CLK_SAFE_RGMII_TX_PARENT_NUM    3
#define CLK_SAFE_CAN_PE_PARENT_NUM      3
#define CLK_SAFE_EPHY_REF_PARENT_NUM    4

#define CLK_SAFE_R5F_OFFSET             0
#define CLK_SAFE_QSPI_OFFSET            2
#define CLK_SAFE_CE_BUS_OFFSET          4
#define CLK_SAFE_GPIO_OFFSET            6
#define CLK_SAFE_TMR0_OFFSET            7
#define CLK_SAFE_TMR1_OFFSET            8
#define CLK_SAFE_WDT_OFFSET             9
#define CLK_SAFE_SER_OFFSET             10
#define CLK_SAFE_RGMII_TX_OFFSET        12
#define CLK_SAFE_CAN_PE_OFFSET          14
#define CLK_SAFE_EPHY_REF_OFFSET        16
#define CLK_SAFE_R5F_WIDTH              2
#define CLK_SAFE_QSPI_WIDTH             2
#define CLK_SAFE_CE_BUS_WIDTH           2
#define CLK_SAFE_GPIO_WIDTH             1
#define CLK_SAFE_TMR0_WIDTH             1
#define CLK_SAFE_TMR1_WIDTH             1
#define CLK_SAFE_WDT_WIDTH              1
#define CLK_SAFE_SER_WIDTH              2
#define CLK_SAFE_RGMII_TX_WIDTH         2
#define CLK_SAFE_CAN_PE_WIDTH           2
#define CLK_SAFE_EPHY_REF_WIDTH         2

/* safety subsystem clk_m_eb_0 register */
#define CLK_SAFE_QSPI_EB                0
#define CLK_SAFE_GPIO_EB                1
#define CLK_SAFE_TMR0_EB                2
#define CLK_SAFE_TMR1_EB                3
#define CLK_SAFE_WDT_EB                 4
#define CLK_SAFE_SER_EB                 5
#define CLK_SAFE_RGMII_TX_EB            6
#define CLK_SAFE_RMII_PHY_EB            7
#define CLK_SAFE_CAN_PE_EB              8
#define CLK_SAFE_EPHY_REF_EB            9
#define CLK_SAFE_EMAC_PTP_EB            10
#define CLK_SAFE_8M_EB                  11

/* safety subsystem clk_g_eb_0 register */
#define PCLK_SAFE_SPI_S                 0
#define CLK_SAFE_SPI_S                  1
#define HCLK_SAFE_QSPI                  2
#define ACLK_SAFE_EMAC                  3
#define PCLK_SAFE_AXERA_SW_INT1         4
#define PCLK_SAFE_AXERA_PWM1            5
#define CLK_SAFE_AXERA_PWM11            6
#define CLK_SAFE_AXERA_PWM12            7
#define CLK_SAFE_AXERA_PWM13            8
#define CLK_SAFE_AXERA_PWM14            9
#define CLK_SAFE_AXERA_PWM15            10
#define CLK_SAFE_AXERA_PWM16            11
#define CLK_SAFE_AXERA_PWM17            12
#define CLK_SAFE_AXERA_PWM18            13
#define PCLK_SAFE_AXERA_PWM2            14
#define CLK_SAFE_AXERA_PWM21            15
#define CLK_SAFE_AXERA_PWM22            16
#define CLK_SAFE_AXERA_PWM23            17
#define CLK_SAFE_AXERA_PWM24            18
#define CLK_SAFE_AXERA_PWM25            19
#define CLK_SAFE_AXERA_PWM26            20
#define CLK_SAFE_AXERA_PWM27            21
#define CLK_SAFE_AXERA_PWM28            22
#define PCLK_SAFE_AXERA_ECAP            23
#define CLK_SAFE_AXERA_ECAP             24
#define PCLK_SAFE_GZIP                  25
#define ACLK_SAFE_GZIP                  26
#define PCLK_SAFE_FW0                   27
#define PCLK_SAFE_FW1                   28
#define PCLK_SAFE_FW2                   29
#define PCLK_SAFE_CRC_ENGINE            30
#define CLK_SAFE_CRC_ENGINE             31

/* safety subsystem clk_g_eb_1 register */
#define PCLK_SAFE_AX_DMA_PER            0
#define CLK_SAFE_AX_DMA_PER             1
#define PCLK_SAFE_AX_DMA                2
#define CLK_SAFE_AX_DMA                 3
#define PCLK_SAFE_UART0                 4
#define CLK_SAFE_UART0                  5
#define PCLK_SAFE_SPI_M1                6
#define CLK_SAFE_SPI_M1                 7
#define PCLK_SAFE_SPI_M2                8
#define CLK_SAFE_SPI_M2                 9
#define PCLK_SAFE_SPI_M3                10
#define CLK_SAFE_SPI_M3                 11
#define PCLK_SAFE_SPI_M4                12
#define CLK_SAFE_SPI_M4                 13
#define PCLK_SAFE_SPI_M6                14
#define CLK_SAFE_SPI_M6                 15
#define PCLK_SAFE_I2C2                  16
#define PCLK_SAFE_I2C3                  17
#define CLK_SAFE_I2C2                   18
#define CLK_SAFE_I2C3                   19
#define PCLK_SAFE_CAN2                  20
#define PCLK_SAFE_CAN3                  21
#define PCLK_SAFE_CAN4                  22
#define PCLK_SAFE_CAN5                  23
#define PCLK_SAFE_CAN6                  24
#define CLK_SAFE_CAN2_CORE              25
#define CLK_SAFE_CAN3_CORE              26
#define CLK_SAFE_CAN4_CORE              27
#define CLK_SAFE_CAN5_CORE              28
#define CLK_SAFE_CAN6_CORE              29
#define TIME_STAMP_CLK_SAFE_CAN2        30
#define TIME_STAMP_CLK_SAFE_CAN3        31

/* safety subsystem clk_g_eb_2 register */
#define TIME_STAMP_CLK_SAFE_CAN4        0
#define TIME_STAMP_CLK_SAFE_CAN5        1
#define TIME_STAMP_CLK_SAFE_CAN6        2
#define PCLK_SAFE_GPIO1                 3
#define PCLK_SAFE_GPIO2                 4
#define PCLK_SAFE_GPIO3                 5
#define CLK_SAFE_GPIO1                  6
#define CLK_SAFE_GPIO2                  7
#define CLK_SAFE_GPIO3                  8
#define PCLK_SAFE_MAILBOX               9
#define PCLK_SAFE_SPINLOCK              10
#define PCLK_SAFE_R5F0_DBGMNR           11
#define PCLK_SAFE_R5F1_DBGMNR           12
#define PCLK_SAFE_DMA_PER_DBGMNR        13
#define PCLK_SAFE_OCM_DBGMNR            14
#define ACLK_SAFE_R5F0_DBGMNR           15
#define ACLK_SAFE_R5F1_DBGMNR           16
#define ACLK_SAFE_DMA_PER_DBGMNR        17
#define ACLK_SAFE_OCM_DBGMNR            18
#define PCLK_SAFE_TMR32_4               19
#define PCLK_SAFE_TMR32_5               20
#define PCLK_SAFE_TMR32_6               21
#define PCLK_SAFE_TMR32_7               22
#define PCLK_SAFE_TMR32_8               23
#define PCLK_SAFE_TMR32_9               24
#define PCLK_SAFE_TMR32_10              25
#define PCLK_SAFE_TMR32_11              26
#define TCLK_SAFE_TMR32_4               27
#define TCLK_SAFE_TMR32_5               28
#define TCLK_SAFE_TMR32_6               29
#define TCLK_SAFE_TMR32_7               30
#define TCLK_SAFE_TMR32_8               31

/* safety subsystem clk_g_eb_3 register */
#define TCLK_SAFE_TMR32_9               0
#define TCLK_SAFE_TMR32_10              1
#define TCLK_SAFE_TMR32_11              2
#define PCLK_SAFE_WWDT4                 3
#define PCLK_SAFE_WWDT5                 4
#define TCLK_SAFE_WWDT4                 5
#define TCLK_SAFE_WWDT5                 6
#define CLK_SAFE_THM_ADC                7
#define PCLK_SAFE_THM_ADC               8
#define PCLK_SAFE_CE                    9
#define CLK_SAFE_CE_CNT                 10
#define CLK_SAFE_CE                     11
#define PCLK_SAFE_TIMESTAMP0            12
#define CLK_SAFE_TIMESTAMP0             13
#define PCLK_R5F0_ADDR_REMAP            14
#define PCLK_R5F1_ADDR_REMAP            15
#define PCLK_SAFETY_CLK_MNR             16
#define PCLK_ISO_M7                     17
#define PCLK_ISO_M8                     18
#define PCLK_ISO_M9                     19
#define PCLK_ISO_M10                    20
#define PCLK_ISO_S0                     21
#define PCLK_ISO_S1                     22
#define CLK_ISO_M7_24M                  23
#define CLK_ISO_M8_24M                  24
#define CLK_ISO_M9_24M                  25
#define CLK_ISO_M10_24M                 26
#define CLK_ISO_S0_24M                  27
#define CLK_ISO_S1_24M                  28
#define PCLK_SAFE_I2C10                 29
#define CLK_SAFE_I2C10                  30

/* safety subsystem top clk_mux_0 register */
#define CLK_SAFE_FOSU                   0

#define CLK_SAFE_FOSU_OFFSET            0
#define CLK_SAFE_FOSU_WIDTH             1

/* safety subsystem top clk_m_eb_0 register */
#define CLK_SAFE_FOSU_EB                0

/* safety subsystem top clk_g_eb_0 register */
#define PCLK_SAFE_PINREG                0
#define CLK_SAFE_LPC                    1
#define CLK_SAFE_EFS_CTRL0              2
#define CLK_SAFE_EFS_CTRL1              3
#define PCLK_SAFETY_PLL_CTRL            4

/* main PLL */
#define FAB_PLL                         0
#define CPU_PLL                         1
#define NPU_PLL                         2
#define APLL                            3
#define EPLL                            4

/* common subsystem clk_mux_0 register */
#define ACLK_TOP0                       0
#define ACLK_TOP1                       1
#define ACLK_TOP2                       2
#define PCLK_TOP                        3
#define CLK_COMM_TMR                    4
#define CLK_COMM_WWDT                   5

#define ACLK_TOP0_PARENT_NUM            5
#define ACLK_TOP1_PARENT_NUM            6
#define ACLK_TOP2_PARENT_NUM            5
#define PCLK_TOP_PARENT_NUM             3
#define CLK_COMM_TMR_PARENT_NUM         2
#define CLK_COMM_WWDT_PARENT_NUM        2

#define ACLK_TOP0_OFFSET                0
#define ACLK_TOP0_WIDTH                 3
#define ACLK_TOP1_OFFSET                3
#define ACLK_TOP1_WIDTH                 3
#define ACLK_TOP2_OFFSET                6
#define ACLK_TOP2_WIDTH                 3
#define PCLK_TOP_OFFSET                 9
#define PCLK_TOP_WIDTH                  2
#define CLK_COMM_TMR_OFFSET             11
#define CLK_COMM_TMR_WIDTH              1
#define CLK_COMM_WWDT_OFFSET            12
#define CLK_COMM_WWDT_WIDTH             1

/* common subsystem clk_m_eb_0 register */
#define CLK_COMM_TMR_EB                 0
#define CLK_COMM_WWDT_EB                1

/* common subsystem clk_g_eb_0 register */
#define PCLK_TMR32_0                    0
#define PCLK_TMR32_1                    1
#define PCLK_TMR32_2                    2
#define PCLK_TMR32_3                    3
#define TCLK_TMR32_0                    4
#define TCLK_TMR32_1                    5
#define TCLK_TMR32_2                    6
#define TCLK_TMR32_3                    7
#define PCLK_WWDT0                      8
#define PCLK_WWDT1                      9
#define PCLK_WWDT2                      10
#define PCLK_WWDT3                      11
#define TCLK_WWDT0                      12
#define TCLK_WWDT1                      13
#define TCLK_WWDT2                      14
#define TCLK_WWDT3                      15
#define PCLK_AXERA_SW_INT1              16
#define PCLK_PERF_MNR_LAT0              17
#define PCLK_PERF_MNR_LAT1              18
#define CLK_PERF_MNR_LAT0_24M           19
#define CLK_PERF_MNR_LAT1_24M           20
#define ACLK_PERF_MNR_LAT0              21
#define ACLK_PERF_MNR_LAT1              22
#define PCLK_PLL_CTRL2                  23
#define PCLK_A55_DBG_MNR                24
#define PCLK_OCM_DBG_MNR                25
#define ACLK_OCM_DBG_MNR                26
#define ACLK_A55_DBG_MNR                27
#define CLK_DEB_GPIO                    28
#define CLK_COMM_TLB                    29
#define PCLK_COMM_CLK_MNR               30
#define PCLK_MBIST_24M                  31

/* cpu subsystem clk_mux_0 register */
#define CLK_CPU0                        0
#define CLK_CPU1                        1
#define CLK_CPU2                        2
#define CLK_CPU3                        3
#define CLK_DSU                         4
#define CLK_DSU_PERIPH                  5

#define CLK_CPU0_PARENT_NUM             3
#define CLK_CPU1_PARENT_NUM             3
#define CLK_CPU2_PARENT_NUM             3
#define CLK_CPU3_PARENT_NUM             3
#define CLK_DSU_PARENT_NUM              4
#define CLK_DSU_PERIPH_PARENT_NUM       2

#define CLK_CPU0_OFFSET                 0
#define CLK_CPU1_OFFSET                 2
#define CLK_CPU2_OFFSET                 4
#define CLK_CPU3_OFFSET                 6
#define CLK_DSU_OFFSET                  8
#define CLK_DSU_PERIPH_OFFSET           10
#define CLK_CPU0_WIDTH                  2
#define CLK_CPU1_WIDTH                  2
#define CLK_CPU2_WIDTH                  2
#define CLK_CPU3_WIDTH                  2
#define CLK_DSU_WIDTH                   2
#define CLK_DSU_PERIPH_WIDTH            2

/* cpu subsystem clk_g_eb_0 register */
#define CLK_CPU_CORE0                   0
#define CLK_CPU_CORE1                   1
#define CLK_CPU_CORE2                   2
#define CLK_CPU_CORE3                   3
#define SCLK_DSU                        4
#define CLK_CPU_DBG                     5
#define ATGICCLK_CPU                    6
#define CLK_CPU_ROSC                    7

/* flash subsystem clk_mux_0 register */
#define CLK_FLASH_GLB                   0
#define CLK_FLASH_OSPI                  1
#define CLK_FLASH_SDM                   2
#define CLK_FLASH_SER                   3

#define CLK_FLASH_GLB_PARENT_NUM        4
#define CLK_FLASH_OSPI_PARENT_NUM       4
#define CLK_FLASH_SDM_PARENT_NUM        2
#define CLK_FLASH_SER_PARENT_NUM        2

#define CLK_FLASH_GLB_OFFSET            0
#define CLK_FLASH_OSPI_OFFSET           2
#define CLK_FLASH_SDM_OFFSET            4
#define CLK_FLASH_SER_OFFSET            5
#define CLK_FLASH_GLB_WIDTH             2
#define CLK_FLASH_OSPI_WIDTH            2
#define CLK_FLASH_SDM_WIDTH             1
#define CLK_FLASH_SER_WIDTH             1

/* flash subsystem clk_m_eb_0 register */
#define CLK_FLASH_OSPI_EB               0
#define CLK_FLASH_SDM_EB                1
#define CLK_FLASH_SER_EB                2

/* flash subsystem clk_g_eb_0 register */
#define HCLK_FLASH_OSPI                 0
#define PCLK_FLASH_DMA_PER0             1
#define ACLK_FLASH_DMA_PER0             2
#define SDMCLK_SD                       3
#define CLK_SD                          4
#define SDMCLK_EMMC                     5
#define CLK_EMMC                        6
#define PCLK_FLASH_SPI_M7               7
#define CLK_FLASH_SPI_M7                8
#define PCLK_FLASH_I2C9                 9
#define CLK_FLASH_I2C9                  10
#define PCLK_FLASH_GPIO6                11
#define CLK_FLASH_GPIO6                 12
#define PCLK_FLASH_PINMUX               13
#define PCLK_FLASH_DBGMNR               14
#define ACLK_FLASH_DBGMNR               15
#define CLK_FLASH_LPC                   16
#define CLK_FLASH_BW_LMT_24M            17
#define ACLK_FLASH_BW_LMT               18
#define CLK_FLASH_TLB                   19
#define PCLK_FLASH_ISO                  20
#define CLK_FLASH_ISO_24M               21

/* periph subsystem clk_mux_0 register */
#define CLK_PERIPH_GLB                  0
#define SCLK_I2S_M                      1
#define CLK_PERIPH_GPIO                 2
#define CLK_PERIPH_SER                  3
#define PCLK_USB_PHY_BUS                4
#define CLK_MAIN_RGMII_TX               5
#define CLK_MAIN_CAN_PE                 6
#define CLK_MAIN_EPHY_REF               7
#define CLK_I2S_REF                     8

#define CLK_PERIPH_GLB_PARENT_NUM       4
#define SCLK_I2S_M_PARENT_NUM           3
#define CLK_PERIPH_GPIO_PARENT_NUM      2
#define CLK_PERIPH_SER_PARENT_NUM       3
#define PCLK_USB_PHY_BUS_PARENT_NUM     2
#define CLK_MAIN_RGMII_TX_PARENT_NUM    3
#define CLK_MAIN_CAN_PE_PARENT_NUM      3
#define CLK_MAIN_EPHY_REF_PARENT_NUM    4
#define CLK_I2S_REF_PARENT_NUM          4

#define CLK_PERIPH_GLB_OFFSET           0
#define CLK_PERIPH_GLB_WIDTTH           2
#define SCLK_I2S_M_OFFSET               3
#define SCLK_I2S_M_WIDTH                2
#define CLK_PERIPH_GPIO_OFFSET          4
#define CLK_PERIPH_GPIO_WIDTH           1
#define CLK_PERIPH_SER_OFFSET           5
#define CLK_PERIPH_SER_WIDTH            2
#define PCLK_USB_PHY_BUS_OFFSET         7
#define PCLK_USB_PHY_BUS_WIDTH          1
#define CLK_MAIN_RGMII_TX_OFFSET        8
#define CLK_MAIN_RGMII_TX_WIDTH         2
#define CLK_MAIN_CAN_PE_OFFSET          10
#define CLK_MAIN_CAN_PE_WIDTH           2
#define CLK_MAIN_EPHY_REF_OFFSET        12
#define CLK_MAIN_EPHY_REF_WIDTH         2
#define CLK_I2S_REF_OFFSET              14
#define CLK_I2S_REF_WIDTH               2

/* periph subsystem clk_m_eb_0 register */
#define SCLK_I2S_M_EB                   0
#define CLK_PERIPH_GPIO_EB              1
#define CLK_PERIPH_SER_EB               2
#define UTMI_REFCLK_EB                  3
#define CLK_MAIN_RGMII_TX_EB            4
#define CLK_MAIN_RMII_PHY_EB            5
#define CLK_MAIN_CAN_PE_EB              6
#define CLK_MAIN_EPHY_REF_EB            7
#define CLK_PERIPH_EMAC_PTP_EB          8
#define CLK_I2S_REF_EB                  9

/* periph subsystem clk_g_eb_0 register */
#define PCLK_PERIPH_SPI_S               0
#define CLK_PERIPH_SPI_S                1
#define PCLK_PERIPH_GPIO4               2
#define PCLK_PERIPH_GPIO5               3
#define CLK_PERIPH_GPIO4                4
#define CLK_PERIPH_GPIO5                5
#define PCLK_I2S_M                      6
#define PCLK_I2S_S                      7
#define PCLK_PERIPH_PINREG              8
#define PCLK_PERIPH_UART1               9
#define PCLK_PERIPH_UART2               10
#define PCLK_PERIPH_UART3               11
#define PCLK_PERIPH_UART4               12
#define PCLK_PERIPH_UART5               13
#define CLK_PERIPH_UART1                14
#define CLK_PERIPH_UART2                15
#define CLK_PERIPH_UART3                16
#define CLK_PERIPH_UART4                17
#define CLK_PERIPH_UART5                18
#define PCLK_PERIPH_I2C4                19
#define PCLK_PERIPH_I2C5                20
#define PCLK_PERIPH_I2C7                21
#define PCLK_PERIPH_I2C8                22
#define CLK_PERIPH_I2C4                 23
#define CLK_PERIPH_I2C5                 24
#define CLK_PERIPH_I2C7                 25
#define CLK_PERIPH_I2C8                 26
#define PCLK_PERIPH_SPI_M5              27
#define CLK_PERIPH_SPI_M5               28
#define PCLK_USB_PHY                    29
#define ACLK_USB_CTRL                   30
#define PCLK_PERIPH_CRC_ENGINE          31

/* periph subsystem clk_g_eb_1 register */
#define CLK_PERIPH_CRC_ENGINE           0
#define PCLK_PERIPH_AX_DMA_PER          1
#define CLK_PERIPH_AX_DMA_PER           2
#define PCLK_PERIPH_AX_DMA              3
#define CLK_PERIPH_AX_DMA               4
#define PCLK_CAN0                       5
#define PCLK_CAN1                       6
#define CLK_CAN0_CORE                   7
#define CLK_CAN1_CORE                   8
#define TIME_STAMP_CLK_CAN0             9
#define TIME_STAMP_CLK_CAN1             10
#define CLK_PERIPH_LPC                  11
#define ACLK_MAIN_EMAC                  12
#define CLK_PERIPH_DMA_BW_LMT_24M       13
#define ACLK_PERIPH_DMA_BW_LMT          14
#define PCLK_PERIPH_DMA_BW_LMT          15
#define PCLK_PERIPH_DMA_PER_DBGMNR      16
#define ACLK_PERIPH_DMA_PER_DBGMNR      17
#define CLK_PERIPH_TLB                  18
#define PCLK_PERIPH_ISO                 19
#define CLK_PERIPH_ISO_24M              20

/* periph subsystem clk_m_div_0 register */
#define SCLK_I2S_M_DIVN                 0
#define CLK_I2S_REF_DIVN                1

#define SCLK_I2S_M_DIVN_OFFSET          0
#define SCLK_I2S_M_DIVN_WIDTH           6
#define SCLK_I2S_M_DIVN_UPDATE          6
#define CLK_I2S_REF_DIVN_OFFSET         7
#define CLK_I2S_REF_DIVN_WIDTH          4
#define CLK_I2S_REF_DIVN_UPDATE         11

#endif
