# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (C) 2009 Samsung Electronics
# Minkyu Kang <mk7.kang@samsung.com>

obj-y	+= soc.o
obj-$(CONFIG_CPU_V7A) += clock.o pinmux.o power.o system.o
obj-$(CONFIG_ARM64)	+= mmu-arm64.o

ifneq ($(filter y,$(CONFIG_EXYNOS4412_SECURE_BOOT)$(CONFIG_EXYNOS5420)),)
obj-y	+= sec_boot.o
endif

ifdef CONFIG_SPL_BUILD
obj-$(CONFIG_EXYNOS5)	+= clock_init_exynos5.o
obj-$(CONFIG_EXYNOS5)	+= dmc_common.o dmc_init_ddr3.o
ifneq ($(filter y,$(CONFIG_EXYNOS4210)$(CONFIG_EXYNOS4412)),)
obj-y	+= dmc_init_exynos4.o clock_init_exynos4.o
endif
obj-y	+= spl_boot.o tzpc.o
obj-y	+= lowlevel_init.o
endif
