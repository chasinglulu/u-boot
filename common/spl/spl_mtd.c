// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <spl.h>
#include <mtd.h>
#include <linux/compiler.h>
#include <errno.h>
#include <asm/u-boot.h>
#include <errno.h>
#include <image.h>
#include <android_ab.h>

static int mtd_load_legacy(struct spl_image_info *spl_image,
                struct spl_boot_device *bootdev,
                struct mtd_info *mtd,
                ulong off, struct image_header *header)
{
	u32 image_offset_sectors;
	u32 image_size_sectors;
	unsigned long count;
	u32 image_offset;
	int ret;

	ret = spl_parse_image_header(spl_image, bootdev, header);
	if (ret)
		return ret;

	/* convert offset to sectors - round down */
	// image_offset_sectors = spl_image->offset / mmc->read_bl_len;
	// /* calculate remaining offset */
	// image_offset = spl_image->offset % mmc->read_bl_len;

	// /* convert size to sectors - round up */
	// image_size_sectors = (spl_image->size + mmc->read_bl_len - 1) /
	// 		     mmc->read_bl_len;

	/* Read the header too to avoid extra memcpy */
	// count = blk_dread(mmc_get_blk_desc(mmc),
	// 		  sector + image_offset_sectors,
	// 		  image_size_sectors,
	// 		  (void *)(ulong)spl_image->load_addr);
	debug("read %x sectors to %lx\n", image_size_sectors,
	      spl_image->load_addr);
	if (count != image_size_sectors)
		return -EIO;

	if (image_offset)
		memmove((void *)(ulong)spl_image->load_addr,
			(void *)(ulong)spl_image->load_addr + image_offset,
			spl_image->size);

	return 0;
}

static ulong spl_mtd_load_read(struct spl_load_info *load, ulong sector,
                ulong count, void *buf)
{
	struct mtd_info *mtd = load->dev;

	//return blk_dread(mmc_get_blk_desc(mmc), sector, count, buf);
	return 0;
}

static int mtd_load_image_raw_sector(struct spl_image_info *spl_image,
                    struct spl_boot_device *bootdev,
                    struct mtd_info *mtd, unsigned long offset)
{
	unsigned long count;
	struct image_header *header;
	// struct blk_desc *bd = mmc_get_blk_desc(mmc);
	int ret = -EINVAL;

	// header = spl_get_load_buffer(-sizeof(*header), bd->blksz);

	/* read image header to find the image size & load address */
	// count = blk_dread(bd, sector, 1, header);
	debug("mtd read offset %lx, count=%lu\n", offset, count);
	if (count == 0) {
		ret = -EIO;
		goto end;
	}

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
		image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		debug("Found FIT\n");
		load.dev = mtd;
		load.priv = NULL;
		load.filename = NULL;
		if (bootdev->boot_device == BOOT_DEVICE_NAND)
			load.bl_len = mtd->writesize;
		else
			load.bl_len = 1;
		load.read = spl_mtd_load_read;
		ret = spl_load_simple_fit(spl_image, &load, offset, header);
	} else if (image_get_magic(header) == IH_MAGIC) {
		debug("Found Legacy Image\n");

		ret = mtd_load_legacy(spl_image, bootdev, mtd, offset, header);
	}

end:
	if (ret) {
		printf("mtd partition read error at [0x%lx]\n", offset);
		return -1;
	}

	return 0;
}

static int spl_mtd_load_image(struct spl_image_info *spl_image,
		       struct spl_boot_device *bootdev)
{
	printf("%s\n", __func__);
	return 0;
}

SPL_LOAD_IMAGE_METHOD("NOR", 0, BOOT_DEVICE_NOR, spl_mtd_load_image);
SPL_LOAD_IMAGE_METHOD("NAND", 0, BOOT_DEVICE_NAND, spl_mtd_load_image);
SPL_LOAD_IMAGE_METHOD("SPI", 1, BOOT_DEVICE_SPI, spl_mtd_load_image);
