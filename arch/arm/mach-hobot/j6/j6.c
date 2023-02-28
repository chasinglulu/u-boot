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
	MMAP_START,
	MMAP_SRAM = 0,
	MMAP_DEVICE_MMIO,
	MMAP_DDR_BANK_START,
	MMAP_DDR_BANK_END = MMAP_DDR_BANK_START + CONFIG_NR_DRAM_BANKS,
	MMAP_END,
} mmap_region_t;

#define MMAP_SRAM_BASE		0x04000000UL
#define MMAP_DEVICE_BASE	0x20000000UL
#define MMAP_DDR_BANK_BASE	0x3000000000UL

static struct mm_region j6_mem_map[MMAP_END] = {
	[MMAP_SRAM] = {
		.virt = MMAP_SRAM_BASE,
		.phys = MMAP_SRAM_BASE,
		.size = SZ_32M,
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

struct mm_region *mem_map = j6_mem_map;

void reset_cpu(void)
{
	return;
}

int dram_init(void)
{
	if (fdtdec_setup_mem_size_base() != 0)
			return -EINVAL;

	return 0;
}

int dram_init_banksize(void)
{
	fdtdec_setup_memory_banksize();

	return 0;
}

void *board_fdt_blob_setup(int *err)
{
	/* QEMU loads a generated DTB for us at the start of RAM. */
	void *fdt_addr = (void *)CONFIG_SYS_SDRAM_BASE;

	if (!fdt_check_header(fdt_addr)) {
			*err = 0;
			return fdt_addr;
	} else {
			printf("No valid device tree binary found at %p\n",
									fdt_addr);
	}

	/* FDT is at end of image */
	fdt_addr = (ulong *)&_end;
	*err = 0;

	return fdt_addr;
}

void enable_caches(void)
{
	icache_enable();
	dcache_enable();
}
