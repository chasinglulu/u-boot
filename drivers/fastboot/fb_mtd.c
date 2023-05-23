// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 Horizon Robotics Co,. Ltd
 *
 * Based on fb_nand.c
 */

#include <common.h>
#include <env.h>
#include <mtd.h>
#include <linux/err.h>
#include <linux/ctype.h>

#include <fastboot.h>
#include <image-sparse.h>

struct fb_mtd_sparse {
	struct mtd_info		*mtd;
	struct part_info	*part;
};

static bool mtd_is_aligned_with_block_size(struct mtd_info *mtd, u64 size)
{
	return !do_div(size, mtd->erasesize);
}

static int fb_mtd_lookup(char *devstr,
              const char *partname,
			  struct mtd_info **mtd,
			  struct part_info **part,
			  char *response)
{
	struct mtd_device *dev;
    struct mtd_info *mi;
    struct part_info *pi;
	int ret;
	u8 pnum;

	mi = get_mtd_device_nm(devstr);
	if (IS_ERR_OR_NULL(mi))
		return -ENODEV;
	put_mtd_device(mi);

	ret = mtdparts_init();
	if (ret) {
		pr_err("Cannot initialize MTD partitions\n");
		fastboot_fail("cannot init mtdparts", response);
		return ret;
	}

	ret = find_dev_and_part(partname, &dev, &pnum, &pi);
	if (ret) {
		pr_err("cannot find partition: '%s'\n", partname);
		fastboot_fail("cannot find partition", response);
		return ret;
	}

	if (!mtd_is_aligned_with_block_size(mi, pi->offset)) {
		printf("Offset not aligned with a block (0x%x)\n",
		       mi->erasesize);
        fastboot_fail("Offset not aligned", response);
		return -EINVAL;
	}
	if (!mtd_is_aligned_with_block_size(mi, pi->size)) {
		printf("Size not aligned with a block (0x%x)\n",
		       mi->erasesize);
        fastboot_fail("Size not aligned", response);
		return -EINVAL;
	}

	*mtd = mi;
    *part = pi;

	return 0;
}

static lbaint_t fb_mtd_sparse_write(struct sparse_storage *info,
		lbaint_t blk, lbaint_t blkcnt, const void *buffer)
{
    return 0;
}

static lbaint_t fb_mtd_sparse_reserve(struct sparse_storage *info,
		lbaint_t blk, lbaint_t blkcnt)
{
    return 0;
}

static int _fb_mtd_erase(struct mtd_info *mtd, struct part_info *part)
{
	u64 off, lim, remaining, lock_ofs, lock_len;
	struct erase_info erase_op = {};
	int ret = 0;

	remaining = lock_len = round_up(part->size, mtd->erasesize);
	off = lock_ofs = part->offset;
	lim = part->offset + part->size;

	printf("Erasing blocks 0x%llx to 0x%llx\n", off, lim);

	erase_op.mtd = mtd;
	erase_op.addr = off;
	erase_op.len = mtd->erasesize;
	erase_op.scrub = 0;

	debug("Unlocking the mtd device\n");
	ret = mtd_unlock(mtd, lock_ofs, lock_len);
	if (ret && ret != -EOPNOTSUPP) {
		printf("MTD device unlock failed\n");
		return 0;
	}

	while (remaining) {
		if (erase_op.addr + remaining > lim) {
			printf("Limit reached 0x%llx while erasing at offset 0x%llx\n",
					lim, off);
			return -EIO;
		}

		ret = mtd_erase(mtd, &erase_op);

		if (ret) {
			/* Abort if its not a bad block error */
			if (ret != -EIO) {
				printf("Failure while erasing at offset 0x%llx\n",
						erase_op.fail_addr);
				return 0;
			}
			printf("Skipping bad block at 0x%08llx\n",
					erase_op.addr);
		} else {
			remaining -= mtd->erasesize;
		}

		/* Continue erase behind bad block */
		erase_op.addr += mtd->erasesize;
	}
	printf("........ erased 0x%llx bytes from '%s'\n",
	       part->size, part->name);

    /* Write done, lock again */
    debug("Locking the mtd device\n");
    ret = mtd_lock(mtd, lock_ofs, lock_len);
	if (ret == -EOPNOTSUPP)
		ret = 0;
	else if (ret)
    	printf("MTD device lock failed\n");

	return ret;
}

static int _fb_mtd_write(struct mtd_info *mtd, struct part_info *part,
			  void *buffer, u32 offset,
			  size_t length, size_t *written)
{
	u64 off, lim, remaining, lock_ofs, lock_len;
    u32 bad_skip = 0;
	struct mtd_oob_ops io_op = {};
	int ret = 0;
	bool has_pages = mtd->type == MTD_NANDFLASH ||
			 mtd->type == MTD_MLCNANDFLASH;

	if (!buffer) {
		return 0;
	}

	off = lock_ofs = part->offset + offset + bad_skip;
	lim = part->offset + part->size;

	if (off >= lim) {
		printf("Limit reached 0x%llx\n", lim);
		return -EIO;
	}
	/* limit request with the available size */
	if (off + length >= lim)
		length = lim - off;

	if (!mtd_is_aligned_with_block_size(mtd, off)) {
		printf("Offset not aligned with a block (0x%x)\n",
		       mtd->erasesize);
		return 0;
	}

    lock_len = round_up(length, mtd->erasesize);
    debug("Unlocking the mtd device\n");
    ret = mtd_unlock(mtd, lock_ofs, lock_len);
    if (ret && ret != -EOPNOTSUPP) {
        printf("MTD device unlock failed\n");
        return 0;
    }

	io_op.mode = MTD_OPS_AUTO_OOB;
	io_op.len = length;
	if (has_pages && io_op.len > mtd->writesize)
		io_op.len = mtd->writesize;
	io_op.ooblen = 0;
	io_op.datbuf = buffer;
	io_op.oobbuf = NULL;

	/* Loop over to do the actual read/write */
	remaining = length;
	while (remaining) {
		if (off + remaining > lim) {
			printf("Limit reached 0x%llx while writing at offset 0x%llx\n",
			       lim, off);
			return -EIO;
		}

		/* Skip the block if it is bad */
		if (mtd_is_aligned_with_block_size(mtd, off) &&
		    mtd_block_isbad(mtd, off)) {
			off += mtd->erasesize;
			bad_skip += mtd->erasesize;
			continue;
		}

		ret = mtd_write_oob(mtd, off, &io_op);
		if (ret) {
			printf("Failure while writing at offset 0x%llx ret=%d\n", off, ret);
			return -EIO;
		}

        if (written)
            *written += io_op.retlen;
		off += io_op.retlen;
		remaining -= io_op.retlen;
		io_op.datbuf += io_op.retlen;
		io_op.len = remaining;
		if (has_pages && io_op.len > mtd->writesize)
			io_op.len = mtd->writesize;
	}

    /* Write done, lock again */
    debug("Locking the mtd device\n");
    ret = mtd_lock(mtd, lock_ofs, lock_len);
    if (ret == -EOPNOTSUPP)
        ret = 0;
    else if (ret)
        printf("MTD device lock failed\n");

    return ret;
}

/**
 * fastboot_mtd_get_part_info() - Lookup MTD partion by name
 *
 * @part_name: Named device to lookup
 * @part_info: Pointer to returned part_info pointer
 * @response: Pointer to fastboot response buffer
 */
int fastboot_mtd_get_part_info(const char *part_name,
				struct part_info **part_info, char *response)
{
	struct mtd_info *mtd = NULL;
    char *ifname;
    char *devstr;

    ifname = env_get("fastboot.bootdevice");
    if (!ifname) {
		pr_err("device interface empty\n");
        fastboot_fail("device interface empty", response);
		return -ENODEV;
	} else if (strcmp(ifname, "mtd")) {
        pr_err("interface not mtd\n");
        fastboot_fail("interface not mtd", response);
        return -ENODEV;
    }

	devstr = env_get("fastboot.devnum");
    if (!devstr) {
		pr_err("device not found\n");
        fastboot_fail("no such device", response);
		return -ENODATA;
	}

	return fb_mtd_lookup(devstr, part_name, &mtd, part_info, response);
}

/**
 * fastboot_mtd_flash_write() - Write image to MTD for fastboot
 *
 * @cmd: Named device to write image to
 * @download_buffer: Pointer to image data
 * @download_bytes: Size of image data
 * @response: Pointer to fastboot response buffer
 */
void fastboot_mtd_flash_write(const char *cmd, void *download_buffer,
			       u32 download_bytes, char *response)
{
	struct part_info *part;
	struct mtd_info *mtd = NULL;
    char *ifname;
    char *devstr;
	int ret;

	ifname = env_get("fastboot.bootdevice");
    if (!ifname) {
		pr_err("device interface empty\n");
        fastboot_fail("device interface empty", response);
		return;
	} else if (strcmp(ifname, "mtd")) {
        pr_err("interface not mtd\n");
        fastboot_fail("interface not mtd", response);
        return;
    }

	devstr = env_get("fastboot.devnum");
    if (!devstr) {
		pr_err("device not found\n");
        fastboot_fail("no such device", response);
		return;
	}

	ret = fb_mtd_lookup(devstr, cmd, &mtd, &part, response);
	if (ret) {
		pr_err("invalid MTD device\n");
		fastboot_fail("invalid MTD device", response);
		return;
	}

	if (is_sparse_image(download_buffer)) {
		struct fb_mtd_sparse sparse_priv;
		struct sparse_storage sparse;

		sparse_priv.mtd = mtd;
		sparse_priv.part = part;

		sparse.blksz = mtd->writesize;
		sparse.start = part->offset / sparse.blksz;
		sparse.size = part->size / sparse.blksz;
		sparse.write = fb_mtd_sparse_write;
		sparse.reserve = fb_mtd_sparse_reserve;
		sparse.mssg = fastboot_fail;

		printf("Flashing sparse image at offset " LBAFU "\n",
		       sparse.start);

		sparse.priv = &sparse_priv;
		ret = write_sparse_image(&sparse, cmd, download_buffer,
					 response);
		if (!ret)
			fastboot_okay(NULL, response);
	} else {
		printf("Flashing raw image at offset 0x%llx\n",
		       part->offset);

		ret = _fb_mtd_write(mtd, part, download_buffer, part->offset,
				     download_bytes, NULL);

		printf("........ wrote %u bytes to '%s'\n",
		       download_bytes, part->name);
	}

	if (ret) {
		fastboot_fail("error writing the image", response);
		return;
	}

	fastboot_okay(NULL, response);
}

/**
 * fastboot_mtd_erase() - Erase MTD for fastboot
 *
 * @cmd: Named device to erase
 * @response: Pointer to fastboot response buffer
 */
void fastboot_mtd_erase(const char *cmd, char *response)
{
	struct part_info *part;
	struct mtd_info *mtd = NULL;
    char *ifname;
    char *devstr;
	int ret;

	ifname = env_get("fastboot.bootdevice");
    if (!ifname) {
		pr_err("device interface empty\n");
        fastboot_fail("device interface empty", response);
		return;
	} else if (strcmp(ifname, "mtd")) {
        pr_err("interface not mtd\n");
        fastboot_fail("interface not mtd", response);
        return;
    }

	devstr = env_get("fastboot.devnum");
    if (!devstr) {
		pr_err("device not found\n");
        fastboot_fail("no such device", response);
		return;
	}

	ret = fb_mtd_lookup(devstr, cmd, &mtd, &part, response);
	if (ret) {
		pr_err("invalid MTD device\n");
		fastboot_fail("invalid MTD device", response);
		return;
	}

	ret = _fb_mtd_erase(mtd, part);
	if (ret) {
		pr_err("failed erasing from device %s", mtd->name);
		fastboot_fail("failed erasing from device", response);
		return;
	}

	fastboot_okay(NULL, response);
}