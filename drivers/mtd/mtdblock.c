// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MTD block - abstraction over MTD subsystem, allowing
 * to read and write in blocks using BLK UCLASS.
 *
 * - Read algorithm:
 *
 *   1. Convert start block number to start address.
 *   2. Read block_dev->blksz bytes using mtd_read() and
 *      add to start address pointer block_dev->blksz bytes,
 *      until the requested number of blocks have been read.
 *
 * - Write algorithm:
 *
 *   1. Convert start block number to start address.
 *   2. Round this address down by mtd->erasesize.
 *
 *   Erase addr      Start addr
 *      |                |
 *      v                v
 *      +----------------+----------------+----------------+
 *      |     blksz      |      blksz     |      blksz     |
 *      +----------------+----------------+----------------+
 *
 *   3. Calculate offset between this two addresses.
 *   4. Read mtd->erasesize bytes using mtd_read() into
 *      temporary buffer from erase address.
 *
 *   Erase addr      Start addr
 *      |                |
 *      v                v
 *      +----------------+----------------+----------------+
 *      |     blksz      |      blksz     |      blksz     |
 *      +----------------+----------------+----------------+
 *      ^
 *      |
 *      |
 *   mtd_read()
 *   from here
 *
 *   5. Copy data from user buffer to temporary buffer with offset,
 *      calculated at step 3.
 *   6. Erase and write mtd->erasesize bytes at erase address
 *      pointer using mtd_erase/mtd_write().
 *   7. Add to erase address pointer mtd->erasesize bytes.
 *   8. goto 1 until the requested number of blocks have
 *      been written.
 *
 * (C) Copyright 2024 SaluteDevices, Inc.
 *
 * Author: Alexey Romanov <avromanov@salutedevices.com>
 */

#include <blk.h>
#include <part.h>
#include <dm/device.h>
#include <dm/device-internal.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/mtdparts.h>
#include <linux/sizes.h>

int mtd_bind(struct udevice *dev, struct mtd_info **mtdp)
{
	struct blk_desc *bdesc;
	struct udevice *bdev;
	struct mtd_info *mtd;
	int blksz;
	int ret;

	if (unlikely(!mtdp))
		return -EINVAL;
	mtd = *mtdp;

	if (mtd && (mtd->type == MTD_NANDFLASH ||
	       mtd->type == MTD_MLCNANDFLASH))
		blksz = mtd->writesize;
	else
		blksz = SZ_512;

	ret = blk_create_devicef(dev, "mtd_blk", "blk", IF_TYPE_MTD,
				 dev_seq(dev), blksz, 0, &bdev);
	if (ret) {
		pr_err("Cannot create block device\n");
		return ret;
	}

	bdesc = dev_get_uclass_plat(bdev);
	dev_set_priv(bdev, mtdp);
	bdesc->bdev = bdev;
	bdesc->part_type = PART_TYPE_MTD;
	bdesc->log2blksz = LOG2(bdesc->blksz);

	if (mtd) {
		bdesc->lba = lldiv(mtd->size, bdesc->blksz);
		if (mtd_type_is_nand(mtd))
			snprintf(bdesc->product, BLK_PRD_SIZE, "Nand");
		else
			snprintf(bdesc->product, BLK_PRD_SIZE, "Nor");
	}

	return 0;
}

#ifdef CONFIG_MTD_BLOCK_BBS
/*
 * Skip bad blocks in the range [offset, offset + length)
 *
 * reference: mtd-utils/nand-utils/nanddump.c
 * nanddump --skip_bad_blocks_to_start --bb skipbad -s part_start -l part_size
 */
static int skip_bad_blocks(struct mtd_info *mtd, loff_t offset, size_t length,
                            loff_t from, loff_t *new_start)
{
	loff_t bbs_offset = offset;
	loff_t start, end;

	while (bbs_offset < from) {
		if (mtd_block_isbad(mtd, bbs_offset)) {
			pr_warn("skip_bad_blocks: bad block at 0x%llx\n",
				ALIGN_DOWN(bbs_offset, mtd->erasesize));
			from += mtd->erasesize;
		}
		bbs_offset += mtd->erasesize;
	}

	end = offset + length;
	for (start = from; start < end; start += mtd->writesize) {
		if (mtd_block_isbad(mtd, start)) {
			pr_warn("skip_bad_blocks: skipping bad block at 0x%llx\n",
				ALIGN_DOWN(start, mtd->erasesize));
			start += mtd->erasesize - mtd->writesize;
			continue;
		}
		break;
	}

	if (start >= end) {
		pr_err("skip_bad_blocks: no valid blocks found\n");
		return -EIO;
	}

	*new_start = start;
	return 0;
}

static int find_part_info(struct mtd_info *mtd, loff_t from, struct mtd_info **mtdp)
{
	struct mtd_info *part;

	if (!mtd_type_is_nand(mtd) ||
	    !(dev_get_flags(mtd->dev) & DM_FLAG_ACTIVATED))
		return -ENOTSUPP;

	if (list_empty(&mtd->partitions))
		return -ENOMEDIUM;

	list_for_each_entry(part, &mtd->partitions, node) {
		if (from >= part->offset &&
			from < part->offset + part->size) {
			*mtdp = part;
			break;
		}
	}

	return 0;
}

static int virt_to_phys(struct mtd_info *mtd, loff_t from, loff_t *new_start)
{
	struct mtd_info *part;
	int ret;

	debug("%s: mtd name: %s\n", __func__, mtd->name);
	ret = find_part_info(mtd, from, &part);
	if (ret < 0) {
		if (ret == -ENOTSUPP) {
			*new_start = from;
			debug("find_part_info: orignal from: 0x%llx new from: 0x%llx (ret = %d)\n",
			            from, *new_start, ret);
			return 0;
		}
		return ret;
	}

	debug("part name: %s offset: 0x%llx size: 0x%llx\n",
	                   part->name, part->offset, part->size);
	ret = skip_bad_blocks(mtd, part->offset, part->size,
		 from, new_start);
	if (ret < 0) {
		pr_err("virt_to_phys: exceed partition size\n");
		return ret;
	}
	debug("skip_bad_blocks: orignal from: 0x%llx new from: 0x%llx\n",
	                           from, *new_start);

	return 0;
}
#else
static inline int virt_to_phys(struct mtd_info *mtd, loff_t from, loff_t *new_start)
{
	*new_start = from;
	return 0;
}
#endif /* CONFIG_MTD_BLOCK_BBS */

static ulong mtd_blk_read(struct udevice *dev, lbaint_t start, lbaint_t blkcnt,
			  void *dst)
{
	struct blk_desc *block_dev = dev_get_uclass_plat(dev);
	struct mtd_info *mtd = blk_desc_to_mtd(block_dev);
	unsigned int sect_size = block_dev->blksz;
	lbaint_t cur = start;
	ulong read_cnt = 0;

	while (read_cnt < blkcnt) {
		loff_t sect_start;
		size_t retlen;
		int ret;

		ret = virt_to_phys(mtd, cur * sect_size, &sect_start);
		if (ret < 0) {
			if (ret != -ENOMEDIUM)
				pr_err("mtd_blk_read: failed to convert logical address 0x%llx\n",
				       (loff_t)cur * sect_size);
			return ret;
		}

		ret = mtd_read(mtd, sect_start, sect_size, &retlen, dst);
		if (ret)
			return ret;

		if (retlen != sect_size) {
			pr_err("mtdblock: failed to read block 0x" LBAF "\n", cur);
			return -EIO;
		}

		cur++;
		dst += sect_size;
		read_cnt++;
	}

	return read_cnt;
}

static int mtd_erase_write(struct mtd_info *mtd, uint64_t start, const void *src)
{
	int ret;
	struct erase_info erase = { 0 };

	erase.mtd = mtd;
	erase.addr = start;
	erase.len = mtd->erasesize;

	ret = mtd_erase(mtd, &erase);
	if (ret)
		return ret;

#if defined(CONFIG_MTD_BLOCK_WRITE_OOB)
	struct mtd_oob_ops io_op = { 0 };
	io_op.mode = MTD_OPS_AUTO_OOB;
	io_op.len = mtd->erasesize;
	io_op.ooblen = 0;
	io_op.datbuf = (void *)src;
	io_op.oobbuf = NULL;

	ret = mtd_write_oob(mtd, start, &io_op);
	if (ret) {
		pr_err("mtd_write_oob(0x%llx\n) error %d", start, ret);
		return ret;
	}

	if (io_op.retlen != mtd->erasesize) {
		pr_err("Wrong mtd write length at 0x%llx (expected %u, written %zu)\n",
		       start, mtd->erasesize, io_op.retlen);
		return -EIO;
	}
#else
	size_t retlen;

	ret = mtd_write(mtd, start, mtd->erasesize, &retlen, src);
	if (ret) {
		pr_err("mtd_write(0x%llx\n) error %d", start, ret);
		return ret;
	}

	if (retlen != mtd->erasesize) {
		pr_err("Wrong mtd write length at 0x%llx (expected %u, written %zu)\n",
		       start, mtd->erasesize, retlen);
		return -EIO;
	}
#endif

	return 0;
}

#if defined(CONFIG_MTD_BLOCK_NAND_QUIRKS)
static ulong mtd_nand_blk_write(struct udevice *dev, lbaint_t start, lbaint_t blkcnt,
                                  const void *src)
{
	struct blk_desc *block_dev = dev_get_uclass_plat(dev);
	struct mtd_info *mtd = blk_desc_to_mtd(block_dev);
	unsigned int sect_size = block_dev->blksz;
	lbaint_t cur = start;
	ulong write_cnt = 0;

	debug("%s: start: 0x%lx, blkcnt: %ld, src: %p\n",
	           __func__, start, blkcnt, src);

	while (write_cnt < blkcnt) {
		int ret;
		loff_t sect_start = cur * sect_size;
		struct mtd_oob_ops io_op = { 0 };

		while (mtd_block_isbad(mtd, sect_start)) {
			pr_warn("mtd_nand_blk_write: skipping bad block at 0x%llx\n",
			              ALIGN_DOWN(sect_start, mtd->erasesize));
			sect_start += mtd->erasesize;
			debug("%s: new sector start: 0x%llx\n", __func__, sect_start);
		}

		io_op.mode = MTD_OPS_AUTO_OOB;
		io_op.len = sect_size;
		io_op.ooblen = 0;
		io_op.datbuf = (void *)src;
		io_op.oobbuf = NULL;

		debug("%s: writing %u bytes to 0x%llx\n", __func__, sect_size, sect_start);
		ret = mtd_write_oob(mtd, sect_start, &io_op);
		if (ret) {
			pr_err("mtd_write_oob(0x%llx\n) error %d", sect_start, ret);
			return ret;
		}

		if (io_op.retlen != sect_size) {
			pr_err("Wrong mtd write length at 0x%lx (expected %u, written %zu)\n",
			              start, sect_size, io_op.retlen);
			return -EIO;
		}

		cur++;
		src += sect_size;
		write_cnt++;
	}

	return write_cnt;
}
#endif

/* Core write function using read-modify-write for erase blocks */
static ulong mtd_blk_write_rmw(struct udevice *dev, lbaint_t start, lbaint_t blkcnt,
                                 const void *src)
{
	struct blk_desc *block_dev = dev_get_uclass_plat(dev);
	struct mtd_info *mtd = blk_desc_to_mtd(block_dev);
	unsigned int sect_size = block_dev->blksz;
	lbaint_t cur = start, blocks_todo = blkcnt;
	ulong write_cnt = 0;
	u8 *buf;
	int ret = 0;

	buf = malloc(mtd->erasesize);
	if (!buf)
		return -ENOMEM;

	while (blocks_todo > 0) {
		loff_t sect_start, erase_start;
		u32 offset;
		size_t cur_size, retlen;
		lbaint_t written;

		ret = virt_to_phys(mtd, cur * sect_size, &sect_start);
		if (ret < 0) {
			if (ret != -ENOMEDIUM)
				pr_err("mtd_blk_write_rmw: failed to convert logical address 0x%llx\n",
				       (loff_t)cur * sect_size);
			goto out;
		}

		erase_start = ALIGN_DOWN(sect_start, mtd->erasesize);
		offset = sect_start - erase_start;
		cur_size = min_t(size_t, mtd->erasesize - offset,
		                                      blocks_todo * sect_size);

		ret = mtd_read(mtd, erase_start, mtd->erasesize, &retlen, buf);
		if (ret)
			goto out;

		if (retlen != mtd->erasesize) {
			pr_err("Wrong mtd read length at 0x" LBAF "(expected %llu, got %zu)\n",
			           cur, erase_start, retlen);
			ret = -EIO;
			goto out;
		}

		memcpy(buf + offset, src, cur_size);

		ret = mtd_erase_write(mtd, erase_start, buf);
		if (ret)
			goto out;

		written = cur_size / sect_size;

		blocks_todo -= written;
		cur += written;
		src += cur_size;
		write_cnt += written;
	}

out:
	free(buf);

	if (ret)
		return ret;

	return write_cnt;
}

static ulong mtd_blk_write(struct udevice *dev, lbaint_t start, lbaint_t blkcnt,
	const void *src)
{
#if defined(CONFIG_MTD_BLOCK_NAND_QUIRKS)
	struct blk_desc *block_dev = dev_get_uclass_plat(dev);
	struct mtd_info *mtd = blk_desc_to_mtd(block_dev);

	if (mtd_type_is_nand(mtd)) {
		/* Use NAND-specific write function if enabled and applicable */
		return mtd_nand_blk_write(dev, start, blkcnt, src);
	} else
#endif
	{
		return mtd_blk_write_rmw(dev, start, blkcnt, src);
	}
}

static unsigned long mtd_blk_erase(struct udevice *dev, lbaint_t start, lbaint_t blkcnt)
{
	struct blk_desc *block_dev = dev_get_uclass_plat(dev);
	struct mtd_info *mtd = blk_desc_to_mtd(block_dev);
	unsigned int sect_size = block_dev->blksz;
	ulong erased_total = 0;
	struct erase_info erase_op = { 0 };
	u64 offset, len;
	int ret;

	offset = start * sect_size;
	len = blkcnt * sect_size;

	erase_op.mtd = mtd;
	erase_op.addr = offset;
	erase_op.len = mtd->erasesize;
	erase_op.scrub = 0;

	while (len) {
		ret = mtd_erase(mtd, &erase_op);

		if (ret) {
			/* Abort if its not a bad block error */
			if (ret != -EIO)
				break;
			printf("mtd_blk_erase: Skipping bad block at 0x%08llx\n",
			       erase_op.addr);
		}

		len -= mtd->erasesize;
		erase_op.addr += mtd->erasesize;
		erased_total++;
	}

	return erased_total * (mtd->erasesize / sect_size);
}

static int mtd_blk_probe(struct udevice *dev)
{
	struct blk_desc *bdesc;
	struct mtd_info *mtd;
	int ret;

	ret = device_probe(dev);
	if (ret) {
		pr_err("Probing %s failed (err=%d)\n", dev->name, ret);
		return ret;
	}

	bdesc = dev_get_uclass_plat(dev);
	mtd = blk_desc_to_mtd(bdesc);

	if (mtd_type_is_nand(mtd))
		pr_warn("MTD device '%s' is NAND, please use UBI devices instead\n",
			mtd->name);

	return 0;
}

static const struct blk_ops mtd_blk_ops = {
	.read = mtd_blk_read,
	.write = mtd_blk_write,
	.erase = mtd_blk_erase,
};

U_BOOT_DRIVER(mtd_blk) = {
	.name = "mtd_blk",
	.id = UCLASS_BLK,
	.ops = &mtd_blk_ops,
	.probe = mtd_blk_probe,
};

#ifdef CONFIG_MTD_BLOCK_BBS
ulong mtd_blk_read_bbs(struct blk_desc *dev_desc, lbaint_t part_start, lbaint_t part_size,
                       lbaint_t from, lbaint_t blkcnt, void *dst)
{
	struct mtd_info *mtd = blk_desc_to_mtd(dev_desc);
	unsigned int sect_size = dev_desc->blksz;
	lbaint_t cur = from;
	ulong read_cnt = 0;

	if (unlikely(!mtd))
		return -ENODEV;

	if (!mtd_type_is_nand(mtd)) {
		return blk_dread(dev_desc, from, blkcnt, dst);
	}

	while (read_cnt < blkcnt) {
		loff_t sect_start;
		size_t retlen;
		int ret;

		ret = skip_bad_blocks(mtd, part_start * sect_size, part_size * sect_size,
		                        cur * sect_size, &sect_start);
		if (ret < 0) {
			pr_err("mtd_blk_read_bb: exception while skipping bad blocks\n");
			return ret;
		}

		ret = mtd_read(mtd, sect_start, sect_size, &retlen, dst);
		if (ret)
			return ret;

		if (retlen != sect_size) {
			pr_err("mtd_blk_read_bbs: failed to read block 0x" LBAF "\n", cur);
			return -EIO;
		}

		cur++;
		dst += sect_size;
		read_cnt++;
	}

	return read_cnt;
}

ulong mtd_blk_write_bbs(struct blk_desc *dev_desc, lbaint_t part_start, lbaint_t part_size,
                         lbaint_t from, lbaint_t blkcnt, const void *src)
{
	struct mtd_info *mtd = blk_desc_to_mtd(dev_desc);
	unsigned int sect_size = dev_desc->blksz;
	lbaint_t cur = from, blocks_todo = blkcnt;
	ulong write_cnt = 0;
	u8 *buf;
	int ret = 0;

	if (unlikely(!mtd))
		return -ENODEV;

	if (!mtd_type_is_nand(mtd))
		return blk_dwrite(dev_desc, from, blkcnt, src);

	buf = malloc(mtd->erasesize);
	if (!buf)
		return -ENOMEM;

	while (blocks_todo > 0) {
		loff_t sect_start, erase_start;
		u32 offset;
		size_t cur_size, retlen;
		lbaint_t written;

		ret = skip_bad_blocks(mtd, part_start * sect_size, part_size * sect_size,
		                cur * sect_size, &sect_start);
		if (ret < 0) {
			pr_err("mtd_blk_write_bbs: exception while skipping bad blocks\n");
			goto out;
		}

		erase_start = ALIGN_DOWN(sect_start, mtd->erasesize);
		offset = sect_start - erase_start;
		cur_size = min_t(size_t, mtd->erasesize - offset,
		                                      blocks_todo * sect_size);

		ret = mtd_read(mtd, erase_start, mtd->erasesize, &retlen, buf);
		if (ret)
			goto out;

		if (retlen != mtd->erasesize) {
			pr_err("mtd_blk_write_bbs: wrong mtd read length at 0x" LBAF "(expected %llu, got %zu)\n",
			           cur, erase_start, retlen);
			ret = -EIO;
			goto out;
		}

		memcpy(buf + offset, src, cur_size);

		ret = mtd_erase_write(mtd, erase_start, buf);
		if (ret)
			goto out;

		written = cur_size / sect_size;

		blocks_todo -= written;
		cur += written;
		src += cur_size;
		write_cnt += written;
	}

out:
	free(buf);

	if (ret)
		return ret;

	return write_cnt;
}
#endif /* CONFIG_MTD_BLOCK_BBS */