// SPDX-License-Identifier: GPL-2.0+
/*
 * Axera Laguna SoC clock header
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#ifndef AXERA_LUA_CLK_H__
#define AXERA_LUA_CLK_H__

#include <asm/io.h>

struct clk *clk_register_pll(const char *name, const char *parent_name,
				void __iomem *reg);

#endif /* AXERA_LUA_CLK_H__ */
