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
	/* Safety OCM */
	{
		.virt = MMAP_SAFETY_OCM_BASE,
		.phys = MMAP_SAFETY_OCM_BASE,
		.size = SZ_2M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			PTE_BLOCK_INNER_SHARE
	},
	/* Safety iRAM */
	{
		.virt = MMAP_SAFETY_IRAM_BASE,
		.phys = MMAP_SAFETY_IRAM_BASE,
		.size = SZ_64K,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			PTE_BLOCK_INNER_SHARE
	},
#if defined(CONFIG_LUA_SAFETY_INIT)
	/* Safety peripherals region */
	{
		.virt = MMAP_SAFETY_PERI_BASE,
		.phys = MMAP_SAFETY_PERI_BASE,
		.size = SZ_4M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
#endif
	/* common subsystem region */
	{
		.virt = MMAP_COMMSYS_BASE,
		.phys = MMAP_COMMSYS_BASE,
		.size = SZ_32M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	{
		.virt = MMAP_DDRSYS_BASE,
		.phys = MMAP_DDRSYS_BASE,
		.size = SZ_32M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	/* cpu subsystem region */
	{
		.virt = MMAP_CPUSYS_BASE,
		.phys = MMAP_CPUSYS_BASE,
		.size = SZ_32M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	/* flash subsystem region */
	{
		.virt = MMAP_FLASHSYS_BASE,
		.phys = MMAP_FLASHSYS_BASE,
		.size = SZ_32M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	/* peripherals subsystem region */
	{
		.virt = MMAP_PERISYS_BASE,
		.phys = MMAP_PERISYS_BASE,
		.size = SZ_32M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	{
		.virt = MMAP_MMSYS_BASE,
		.phys = MMAP_MMSYS_BASE,
		.size = SZ_32M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	/* NPU OCM */
	{
		.virt = MMAP_NPUOCM_BASE,
		.phys = MMAP_NPUOCM_BASE,
		.size = SZ_2M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			PTE_BLOCK_INNER_SHARE
	},
	{
		.virt = MMAP_NPUSYS_BASE,
		.phys = MMAP_NPUSYS_BASE,
		.size = SZ_16M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	{
		.virt = MMAP_VPUSYS_BASE,
		.phys = MMAP_VPUSYS_BASE,
		.size = SZ_32M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	/* top subsystem region */
	{
		.virt = MMAP_TOP_BASE,
		.phys = MMAP_TOP_BASE,
		.size = SZ_32M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN |
			PTE_BLOCK_UXN
	},
	/* DDR region */
	{
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
