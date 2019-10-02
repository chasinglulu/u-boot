// SPDX-License-Identifier: GPL-2.0+
/*
 * Refer to uboot-2010.12 source code provided by samsung BSP
 * Copyright (C) 2019 Wang Xinlu <wangkart@aliyun.com>
 *
 * SMC funtion header file
 */

#ifndef __ASM_ARCH_SMC_H_
#define __ASM_ARCH_SMC_H_
/* Boot device */
#define SDMMC_CH2   0x00
#define EMMC44_CH4  0x14

/* secure service function */
#define SMC_CMD_LOAD_UBOOT  (-230)
#define SMC_CMD_COLD_BOOT   (-231)
#define SMC_CMD_WARM_BOOT   (-232)

/* image load address */
#define CONFIG_SYS_IRAM_BASE    (0x02020000)
#define CONFIG_SYS_TZSW_BASE    (CONFIG_SYS_IRAM_BASE + 0x8000)
#define SMC_SECURE_CONTEXT_BASE (CONFIG_SYS_IRAM_BASE + 0x4C00)
#define CONFIG_IMAGE_INFO_BASE  (CONFIG_SYS_SDRAM_BASE)
#define CONFIG_EXYNOS_RELOCATE_CODE_BASE   (CONFIG_SYS_TZSW_BASE + 0x27000)

#ifndef __ASSEMBLY__
/* secure service to copy uboot to ram */
void load_uboot_image(unsigned int boot_device);
void cold_boot(unsigned int boot_devicev);
#endif
#endif /* __ASM_ARCH_SMC_H_ */
