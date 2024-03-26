// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#ifndef CONFIGS_LAGUNA_R5F_H__
#define CONFIGS_LAGUNA_R5F_H__

#define CONFIG_EXTRA_ENV_SETTINGS

/* Serial drivers */
/* The following table includes the supported baudrates */
#define CONFIG_SYS_BAUDRATE_TABLE  \
	{300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400}

/* Boot configuration */

#define CONFIG_SYS_INIT_RAM_ADDR	0xFFFF0000
#define CONFIG_SYS_INIT_RAM_SIZE	0x1000

/* Extend size of kernel image for uncompression */

#endif
