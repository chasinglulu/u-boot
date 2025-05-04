# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (C) 2024 Charleye <wangkart@aliyun.com>
#

obj-y += common.o lowlevel.o memmap.o
oby-y += soc_init.o

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
else
obj-y += cpu.o
obj-$(CONFIG_CMD_CLK) += clk.o
endif

obj-$(CONFIG_LUA_PKEY_CODE) += keys.o
