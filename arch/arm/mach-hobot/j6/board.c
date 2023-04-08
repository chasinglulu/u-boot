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

extern uint32_t ram_base_msb;
DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_OF_BOARD
void *board_fdt_blob_setup(int *err)
{
	/* QEMU loads a generated DTB for us at the start of RAM. */
	void *fdt_addr = NULL;

	if (ram_base_msb >= 0x25)
		fdt_addr = (void *)CONFIG_SYS_NON_INTER_DDR_BASE;
	else
		fdt_addr = (void *)CONFIG_SYS_INTERLEAVE_DDR_BASE;

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

	if (ram_base_msb >= 0x20) {
		if (ram_base_msb >= 0x25)
			return CONFIG_SYS_NON_INTER_DDR_BASE + gd->ram_size;
		else
			return CONFIG_SYS_INTERLEAVE_DDR_BASE + gd->ram_size;
	} else {
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
    ulong value;
    const char *env_name[] = {
        "xen_addr_r",
        "kernel_addr_r",
        "fdt_addr_r",
        "ramdisk_addr_r",
        "domu1_addr_r",
        "domu1_ramdisk_addr_r",
        "domu2_addr_r",
        "domu2_ramdisk_addr_r",
    };

    if (ram_base_msb >= 0x25 || ram_base_msb < 0x20)
        return;

    for (i = 0; i < ARRAY_SIZE(env_name); i++) {
        s = env_get(env_name[i]);
        if(s) {
            value = simple_strtoul(s, NULL, 16);
            value -= 0x2000000000UL;
            snprintf(buf, sizeof(buf), "0x%lx", value);
            env_set(env_name[i], buf);
        }
    }
}

int last_stage_init(void)
{
    fixup_addr_env();

    return 0;
}
#endif

int dram_init(void)
{
	if (fdtdec_setup_mem_size_base() != 0)
		return -EINVAL;

	if (ram_base_msb > 0x20 && ram_base_msb < 0x25)
		gd->ram_base = CONFIG_SYS_INTERLEAVE_DDR_BASE;

	return 0;
}

int dram_init_banksize(void)
{
	fdtdec_setup_memory_banksize();

	if (ram_base_msb > 0x20 && ram_base_msb < 0x25)
		gd->bd->bi_dram[0].start = CONFIG_SYS_INTERLEAVE_DDR_BASE;

	return 0;
}
