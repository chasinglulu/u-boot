/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#ifndef LAGUNA_CLOCK_H
#define LAGUNA_CLOCK_H

/* common subsystem clk_mux_0 register */
#define ACLK_TOP0                       0
#define ACLK_TOP1                       1
#define ACLK_TOP2                       2
#define PCLK_TOP                        3
#define CLK_COMM_TMR                    4
#define CLK_COMM_WWDT                   5

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

/* flash subsystem clk_mux_0 register */
#define CLK_FLASH_GLB                   0
#define CLK_FLASH_OSPI                  1
#define CLK_FLASH_SDM                   2
#define CLK_FLASH_SER                   3

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
