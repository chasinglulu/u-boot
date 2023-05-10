// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2022 Horizon Robotics Co., Ltd
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
	MMAP_FLASH,
	MMAP_DEVICE_MMIO,
	MMAP_LOW_DDR,
	MMAP_HIGH_DDR,
	MMAP_END,
} mmap_region_t;

#define MMAP_FLASH_BASE		0x18000000UL
#define MMAP_DEVICE_BASE	0x20000000UL
#define MMAP_LOW_DDR_BASE	0x80000000UL
#define MMAP_HIGH_DDR_BASE	0x400000000UL
#define DDR_SIZE		(SZ_4G)

static struct mm_region j6e_mem_map[] = {
	[MMAP_FLASH] = {
		.virt = MMAP_FLASH_BASE,
		.phys = MMAP_FLASH_BASE,
		.size = SZ_128M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) |
			PTE_BLOCK_INNER_SHARE
	},
	[MMAP_DEVICE_MMIO] = {
		.virt = MMAP_DEVICE_BASE,
		.phys = MMAP_DEVICE_BASE,
		.size = SZ_2G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	[MMAP_LOW_DDR] = {
		.virt = MMAP_LOW_DDR_BASE,
		.phys = MMAP_LOW_DDR_BASE,
		.size = SZ_2G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			PTE_BLOCK_INNER_SHARE
	},
	[MMAP_HIGH_DDR] = {
		.virt = MMAP_HIGH_DDR_BASE,
		.phys = MMAP_HIGH_DDR_BASE,
		.size = DDR_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			PTE_BLOCK_INNER_SHARE
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = j6e_mem_map;

void reset_cpu(void)
{
	return;
}

void enable_caches(void)
{
	icache_enable();
	dcache_enable();
}
