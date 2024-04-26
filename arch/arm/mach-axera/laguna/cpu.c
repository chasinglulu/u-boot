/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>

static u32 reset_cause = -1;

u32 lua_get_reset_cause(void)
{
	return reset_cause;
}

#if defined(CONFIG_DISPLAY_CPUINFO) && !defined(CONFIG_SPL_BUILD)
static char *get_reset_cause(void)
{
	switch (lua_get_reset_cause()) {
	case 0x0001:
	case 0x0011:
		return "POR";
	case 0x0004:
		return "CSU";
	case 0x0010:
		return "WDOG";
	case 0x0040:
		return "JTAG SW";
	default:
		return "unknown reset";
	}
}

int print_cpuinfo(void)
{
	printf("Reset cause: %s\n", get_reset_cause());

	return 0;
}
#endif