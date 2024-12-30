// SPDX-License-Identifier: BSD-2-Clause
/*
 * The FDL image update funtions depends on board design.
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <fdl.h>

__weak int fdl_blk_write_data(const char *part_name, size_t image_size)
{
	/* Nothing to do */
	return 0;
}

__weak int fdl_blk_write_partition(struct fdl_part_table *tab)
{
	/* Nothing to do */
	return 0;
}

__weak int fdl_blk_erase(const char *part_name, size_t size)
{
	/* Nothing to do */
	return 0;
}