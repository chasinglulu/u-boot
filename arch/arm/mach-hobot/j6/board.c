// SPDX-License-Identifier: GPL-2.0+
/*
 * Common board-specific code
 *
 * Copyright (c) 2023 Horizon Robotics Co., Ltd
 */

#include <common.h>
#include <init.h>
#include <fdt_support.h>
#include <asm/sections.h>
#include <linux/sizes.h>
#include <exports.h>
#include <env.h>

extern uint64_t boot_start_addr;
DECLARE_GLOBAL_DATA_PTR;

bool inline is_dram_addr(ulong addr)
{
	int high = (addr >> 32) & 0xF0;
	if (high)
		return true;
	else
		return false;
}

bool inline is_interleave_dram_addr(ulong addr)
{
	int high = (addr >> 32) & 0xF0;

	assert(high);
	if (high < 0x30)
		return true;
	else
		return false;
}

#ifdef CONFIG_OF_BOARD
void *board_fdt_blob_setup(int *err)
{
	/* QEMU loads a generated DTB for us at the start of RAM. */
	void *fdt_addr = NULL;

	if (is_interleave_dram_addr(boot_start_addr))
		fdt_addr = (void *)CONFIG_SYS_INTERLEAVE_DDR_BASE;
	else
		fdt_addr = (void *)CONFIG_SYS_NON_INTER_DDR_BASE;

	if (!fdt_check_header(fdt_addr)) {
		*err = 0;
		return fdt_addr;
	}

	/* FDT is at end of image */
	fdt_addr = (ulong *)&_end;
	*err = 0;

	return fdt_addr;
}
#endif

/*
 * Get the top of usable RAM
 *
 * Assuming the size of the OCM is 32MiB
 */
ulong board_get_usable_ram_top(ulong total_size)
{

	if (is_dram_addr(boot_start_addr)) {
		if (is_interleave_dram_addr(boot_start_addr))
			return CONFIG_SYS_INTERLEAVE_DDR_BASE + gd->ram_size;
		else
			return CONFIG_SYS_NON_INTER_DDR_BASE + gd->ram_size;
	} else {
		/* TODO: detect OCM size dynamically */
		return CONFIG_SYS_OCM_BASE + SZ_32M;
	}

	/* Never reach here */
	return 0;
}

#ifdef CONFIG_LAST_STAGE_INIT
void fixup_addr_env(void)
{
	int i;
	char *s = NULL;
	char buf[32];
	ulong addr;
	const char *env_name[] = {
		"scriptaddr",
		"xen_addr_r",
		"kernel_addr_r",
		"fdt_addr_r",
		"ramdisk_addr_r",
		"domu1_addr_r",
		"domu1_ramdisk_addr_r",
		"domu2_addr_r",
		"domu2_ramdisk_addr_r",
	};

	/* For OCM, use default dram addr */
	if (!is_dram_addr(boot_start_addr))
		return;

	for (i = 0; i < ARRAY_SIZE(env_name); i++) {
		s = env_get(env_name[i]);
		if (!s)
			continue;

		addr = simple_strtoul(s, NULL, 16);
		if ((is_interleave_dram_addr(boot_start_addr) &&
				is_interleave_dram_addr(addr)) ||
				(!is_interleave_dram_addr(boot_start_addr) &&
				 !is_interleave_dram_addr(addr)))
			continue;
		else {
			if (!is_interleave_dram_addr(addr) &&
				 is_interleave_dram_addr(boot_start_addr))
				addr -= 0x2000000000UL;
			else
				addr += 0x2000000000UL;
		}

		debug("env name: %s addr: 0x%lx\n", env_name[i], addr);
		snprintf(buf, sizeof(buf), "0x%lx", addr);
		env_set(env_name[i], buf);
	}
}

int last_stage_init(void)
{
    fixup_addr_env();

    return 0;
}
#endif

#ifdef CONFIG_OF_LIBFDT
#ifdef CONFIG_OF_SYSTEM_SETUP
void fdt_fixup_xen(void *fdt_blob)
{
	uint64_t addr, size;
	int ret;
	int nodeoffset, subnode;
	struct fdt_resource res;
	char tmp[16];
	int address_cells = fdt_address_cells(fdt_blob, 0);
	int size_cells = fdt_size_cells(fdt_blob, 0);
	char *p = tmp;

	nodeoffset = fdt_subnode_offset(fdt_blob, 0, "chosen");
	if (nodeoffset >= 0) {
		subnode = fdt_first_subnode(fdt_blob, nodeoffset);
		while (subnode >= 0) {
			/* check if this subnode has a reg property */
			ret = fdt_get_resource(fdt_blob, subnode, "reg", 0,
					       &res);
			if (!ret) {
				addr = res.start;
				size = res.end - res.start + 1;

				if (!is_interleave_dram_addr(addr) &&
					 is_interleave_dram_addr(boot_start_addr))
					addr -= 0x2000000000UL;
				else if (is_interleave_dram_addr(addr) &&
						 !is_interleave_dram_addr(boot_start_addr))
						addr += 0x2000000000UL;

				debug("addr: 0x%llx size: 0x%llx\n", addr, size);

				if (address_cells == 2)
					*(fdt64_t *)p = cpu_to_fdt64(addr);
				else
					*(fdt32_t *)p = cpu_to_fdt32(addr);
				p += 4 * address_cells;

				if (size_cells == 2)
					*(fdt64_t *)p = cpu_to_fdt64(size);
				else
					*(fdt32_t *)p = cpu_to_fdt32(size);
				p += 4 * size_cells;
				fdt_setprop(fdt_blob, subnode, "reg", tmp, (char *)p - tmp);
			}

			subnode = fdt_next_subnode(fdt_blob, subnode);
		}
	}
}

int ft_system_setup(void *blob, struct bd_info *bd)
{
	int boot_xen = env_get_yesno("boot_xen");

	if (boot_xen) {
		fdt_fixup_xen(blob);
	}

	return 0;
}
#endif
#endif

int dram_init(void)
{
	if (fdtdec_setup_mem_size_base() != 0)
		return -EINVAL;

	if (is_interleave_dram_addr(boot_start_addr))
		gd->ram_base = CONFIG_SYS_INTERLEAVE_DDR_BASE;

	return 0;
}

int dram_init_banksize(void)
{
	fdtdec_setup_memory_banksize();

	if (is_interleave_dram_addr(boot_start_addr))
		gd->bd->bi_dram[0].start = CONFIG_SYS_INTERLEAVE_DDR_BASE;

	return 0;
}
