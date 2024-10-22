/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <asm/armv8/mmu.h>
#include <asm/arch/cpu.h>
#include <linux/sizes.h>
#include <asm/sections.h>
#include <fdt_support.h>
#include <init.h>
#include <cpu_func.h>

static struct mm_region lua_mem_map[] = {
	[MMIO_SAFETY_OCM] = {
		.virt = MMAP_SAFETY_OCM_BASE,
		.phys = MMAP_SAFETY_OCM_BASE,
		.size = SZ_2M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			PTE_BLOCK_INNER_SHARE
	},
	[MMIO_SAFETY_IRAM] = {
		.virt = MMAP_SAFETY_IRAM_BASE,
		.phys = MMAP_SAFETY_IRAM_BASE,
		.size = SZ_64K,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			PTE_BLOCK_INNER_SHARE
	},
#if defined(CONFIG_LUA_SAFETY_INIT)
	[MMIO_SAFETY_PERI] = {
		.virt = MMAP_SAFETY_PERI_BASE,
		.phys = MMAP_SAFETY_PERI_BASE,
		.size = SZ_4M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
#endif
	[MMIO_COMMSYS] = {
		.virt = MMAP_COMMSYS_BASE,
		.phys = MMAP_COMMSYS_BASE,
		.size = SZ_32M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	[MMIO_CPUSYS] = {
		.virt = MMAP_CPUSYS_BASE,
		.phys = MMAP_CPUSYS_BASE,
		.size = SZ_32M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	[MMIO_FLASHSYS] = {
		.virt = MMAP_FLASHSYS_BASE,
		.phys = MMAP_FLASHSYS_BASE,
		.size = SZ_32M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	[MMIO_PERISYS] = {
		.virt = MMAP_PERISYS_BASE,
		.phys = MMAP_PERISYS_BASE,
		.size = SZ_32M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	[MMIO_NPU_OCM] = {
		.virt = MMAP_NPUOCM_BASE,
		.phys = MMAP_NPUOCM_BASE,
		.size = SZ_2M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			PTE_BLOCK_INNER_SHARE
	},
	[MMIO_TOP] = {
		.virt = MMAP_TOP_BASE,
		.phys = MMAP_TOP_BASE,
		.size = SZ_32M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	[MMIO_DRAM] = {
		.virt = MMAP_DRAM_BASE,
		.phys = MMAP_DRAM_BASE,
		.size = SZ_4G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			PTE_BLOCK_INNER_SHARE
	},
	{
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = lua_mem_map;
