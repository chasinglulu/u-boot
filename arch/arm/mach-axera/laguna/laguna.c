/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
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
	MMAP_SAFETY_IRAM,
	MMAP_PERI_MMIO,
	MMAP_OCM,
	MMAP_DRAM,
	MMAP_END,
} mmap_region_t;

#define MMAP_OCM_BASE          0x14000000U
#define MMAP_PERI_BASE         0x08000000U
#define MMAP_SAFETY_IRAM_BASE  0x00200000U
#define MMAP_DRAM_BASE         0x100000000UL

static struct mm_region lua_mem_map[] = {
	[MMAP_SAFETY_IRAM] = {
		.virt = MMAP_SAFETY_IRAM_BASE,
		.phys = MMAP_SAFETY_IRAM_BASE,
		.size = SZ_64K,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	[MMAP_PERI_MMIO] = {
		.virt = MMAP_PERI_BASE,
		.phys = MMAP_PERI_BASE,
		.size = SZ_128M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	[MMAP_OCM] = {
		.virt = MMAP_OCM_BASE,
		.phys = MMAP_OCM_BASE,
		.size = SZ_512K,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			PTE_BLOCK_INNER_SHARE
	},
	[MMAP_DRAM] = {
		.virt = MMAP_DRAM_BASE,
		.phys = MMAP_DRAM_BASE,
		.size = SZ_4G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			PTE_BLOCK_INNER_SHARE
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = lua_mem_map;

void enable_caches(void)
{
	icache_enable();
	dcache_enable();
}
