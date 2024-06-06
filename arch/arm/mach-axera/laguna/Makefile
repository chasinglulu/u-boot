# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (C) 2024 Charleye <wangkart@aliyun.com>
#

obj-y += common.o lowlevel.o

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
else
obj-y += laguna.o cpu.o
endif
