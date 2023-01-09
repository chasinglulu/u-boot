// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2022 Horizon Robotics Co., Ltd
 */

#include <common.h>
#include <asm/armv8/mmu.h>
#include <fdt_support.h>
#include <init.h>
#include <linux/sizes.h>

DECLARE_GLOBAL_DATA_PTR;

typedef enum mmap_region_type {
	MMAP_START,
	MMAP_OSPI = 0,
	MMAP_AON_SRAM_HEAD_4K,
	MMAP_AON_SRAM_TAIL_24K,
	MMAP_SRAM,
	MMAP_DEVICE_MMIO,
	MMAP_DDR_BANK_START,
	MMAP_DDR_BANK_END = MMAP_DDR_BANK_START + CONFIG_NR_DRAM_BANKS,
	MMAP_END,
} mmap_region_t;

#define MMAP_OSPI_BASE		0x10000000UL
#define MMAP_AON_SRAM_4K_BASE	0x20000000UL
#define MMAP_AON_SRAM_24K_BASE	0x2001a000UL
#define MMAP_SRAM_BASE		0x20400000UL
#define MMAP_DEVICE_BASE	0x40000000UL
#define MMAP_DDR_BANK_BASE	0x80000000UL

static struct mm_region j5_mem_map[MMAP_END] = {
	/* OSPI Memory Map */
	[MMAP_OSPI] = {
		.virt = MMAP_OSPI_BASE,
		.phys = MMAP_OSPI_BASE,
		.size = SZ_256M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	/* BOOT Flags */
	[MMAP_AON_SRAM_HEAD_4K] = {
		.virt = MMAP_AON_SRAM_4K_BASE,
		.phys = MMAP_AON_SRAM_4K_BASE,
		.size = SZ_4K,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) |
			PTE_BLOCK_INNER_SHARE
	},
	/* DSP or MCU used */
	[MMAP_AON_SRAM_TAIL_24K] = {
		.virt = MMAP_AON_SRAM_24K_BASE,
		.phys = MMAP_AON_SRAM_24K_BASE,
		.size = (SZ_1K + SZ_2K) << 3,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) |
			PTE_BLOCK_INNER_SHARE
	},
	[MMAP_SRAM] = {
		.virt = MMAP_SRAM_BASE,
		.phys = MMAP_SRAM_BASE,
		.size = SZ_2M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) |
			PTE_BLOCK_INNER_SHARE
	},
	[MMAP_DEVICE_MMIO] = {
		.virt = MMAP_DEVICE_BASE,
		.phys = MMAP_DEVICE_BASE,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	[MMAP_DDR_BANK_START] = {
		.virt = MMAP_DDR_BANK_BASE,
		.phys = MMAP_DDR_BANK_BASE,
		.size = SZ_4G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			PTE_BLOCK_INNER_SHARE
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = j5_mem_map;

void reset_cpu(ulong addr)
{
	return;
}

int dram_init(void)
{
	gd->ram_size = SZ_4G;
	return 0;
}

int dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = 0x8000000;
	gd->bd->bi_dram[0].size = SZ_4G;

	return 0;
}
