// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 Charleye <wangkart@aliyun.com>
 *
 */

#include <common.h>
#include <binman_sym.h>
#include <image.h>
#include <log.h>
#include <mapmem.h>
#include <spl.h>
#include <linux/libfdt.h>

#ifndef CONFIG_SPL_LOAD_FIT_ADDRESS
# define CONFIG_SPL_LOAD_FIT_ADDRESS	0
#endif

static ulong spl_ocm_load_read(struct spl_load_info *load, ulong sector,
			       ulong count, void *buf)
{
	ulong addr;

	debug("%s: sector %lx, count %lx, buf %lx\n",
	      __func__, sector, count, (ulong)buf);

	addr = (ulong)CONFIG_SPL_LOAD_FIT_ADDRESS + sector;
	if (CONFIG_IS_ENABLED(IMAGE_PRE_LOAD))
		addr += image_load_offset;

	memcpy(buf, (void *)addr, count);

	return count;
}

static int spl_ocm_load_image(struct spl_image_info *spl_image,
			      struct spl_boot_device *bootdev)
{
	struct image_header *header;
	ulong image_start = CONFIG_VAL(TEXT_BASE) + CONFIG_VAL(OCM_LATTER_OFFSET);
	ulong size = CONFIG_VAL(OCM_LATTER_SIZE);

	debug("latter image start address: 0x%lx size: 0x%lx\n", image_start, size);

	/* Copy u-boot.img image from OCM into DDR */
	memcpy((void *)CONFIG_SPL_LOAD_FIT_ADDRESS, (void *)image_start, size);

	header = (struct image_header *)CONFIG_SPL_LOAD_FIT_ADDRESS;

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
	    image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		debug("Found FIT\n");
		load.bl_len = 1;
		load.read = spl_ocm_load_read;
		spl_load_simple_fit(spl_image, &load, 0, header);
	} else {
		ulong u_boot_pos = spl_get_image_pos();

		debug("Legacy image\n");
		/*
		 * Get the header.  It will point to an address defined by
		 * handoff which will tell where the image located inside
		 * the flash.
		 */
		debug("u_boot_pos = %lx\n", u_boot_pos);
		if (u_boot_pos == BINMAN_SYM_MISSING) {
			/*
			 * No binman support or no information. For now, fix it
			 * to the address pointed to by U-Boot.
			 */
			u_boot_pos = (ulong)spl_get_load_buffer(-sizeof(*header),
								sizeof(*header));
		}
		header = (struct image_header *)map_sysmem(u_boot_pos, 0);

		spl_parse_image_header(spl_image, bootdev, header);
	}

	return 0;
}
SPL_LOAD_IMAGE_METHOD("OCM", 0, BOOT_DEVICE_BOARD, spl_ocm_load_image);
