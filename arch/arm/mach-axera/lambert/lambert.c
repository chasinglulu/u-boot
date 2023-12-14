/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (C) 2023 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <asm/armv8/mmu.h>
#include <linux/sizes.h>
#include <asm/sections.h>
#include <fdt_support.h>
#include <init.h>
#include <cpu_func.h>

DECLARE_GLOBAL_DATA_PTR;

typedef enum mmap_region_type {
	MMAP_DEVICE_MMIO,
	MMAP_PIPESYS_MMIO,
	MMAP_IRAM_SAFETY,
	MMAP_HIGH_DDR,
	MMAP_END,
} mmap_region_t;

#define MMAP_DEVICE_BASE       0x00020000UL
#define MMAP_PIPESYS_BASE      0x20000000UL
#define MMAP_IRAM_SAFETY_BASE  0x60c00000UL
#define MMAP_HIGH_DDR_BASE     0x400000000UL

static struct mm_region lmt_mem_map[] = {
	[MMAP_DEVICE_MMIO] = {
		.virt = MMAP_DEVICE_BASE,
		.phys = MMAP_DEVICE_BASE,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	[MMAP_PIPESYS_MMIO] = {
		.virt = MMAP_PIPESYS_BASE,
		.phys = MMAP_PIPESYS_BASE,
		.size = (SZ_1G + SZ_256M),
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	[MMAP_IRAM_SAFETY] = {
		.virt = MMAP_IRAM_SAFETY_BASE,
		.phys = MMAP_IRAM_SAFETY_BASE,
		.size = SZ_512K,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			PTE_BLOCK_INNER_SHARE
	},
	[MMAP_HIGH_DDR] = {
		.virt = MMAP_HIGH_DDR_BASE,
		.phys = MMAP_HIGH_DDR_BASE,
		.size = SZ_4G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			PTE_BLOCK_INNER_SHARE
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = lmt_mem_map;

void reset_cpu(void)
{
	return;
}

void enable_caches(void)
{
	icache_enable();
	dcache_enable();
}
