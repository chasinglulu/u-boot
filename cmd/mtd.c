// SPDX-License-Identifier:  GPL-2.0+
/*
 * mtd.c
 *
 * Generic command to handle basic operations on any memory device.
 *
 * Copyright: Bootlin, 2018
 * Author: Miquèl Raynal <miquel.raynal@bootlin.com>
 */

#include <command.h>
#include <common.h>
#include <console.h>
#include <malloc.h>
#include <mapmem.h>
#include <mtd.h>
#include <dm/devres.h>
#include <linux/err.h>
#include <memalign.h>

#include <linux/ctype.h>

static
struct mtd_info *get_mtd_by_part(struct mtd_info *master,
                                    const char *name)
{
	struct mtd_info *part;

	list_for_each_entry(part, &master->partitions, node)
		if (!strcmp(name, part->name))
			return part;

	debug("Partition '%s' not found on MTD device %s\n", name, master->name);
	return ERR_PTR(-ENODEV);
}

static struct mtd_info *get_mtd_by_name(const char *name)
{
	struct mtd_info *mtd, *part;
	const char *mtd_part = NULL, *mtd_nm = NULL;
	const char *end = NULL;

	mtd_probe_devices();

	end = strchr(name, ':');
	if (end) {
		mtd_part = end + 1;
		if (!strlen(mtd_part)) {
			printf("MTD part name is empty.\n");
			return ERR_PTR(-EINVAL);
		}
		mtd_nm = strndup(name, end - name);
	}

	mtd = get_mtd_unique_nm(end ? mtd_nm: name);
	if (IS_ERR_OR_NULL(mtd)) {
		switch (PTR_ERR(mtd)) {
		case -ENOTUNIQ:
			break;
		default:
			printf("MTD device \"%s\" not found, ret %ld\n",
			        end ? mtd_nm: name, PTR_ERR(mtd));
		}
	}
	kfree(mtd_nm);

	if (mtd_part && !IS_ERR_OR_NULL(mtd)) {
		part = get_mtd_by_part(mtd, mtd_part);
		if (IS_ERR_OR_NULL(part)) {
			printf("MTD part \"%s\" not found, ret %ld\n", mtd_part,
			       PTR_ERR(part));
			goto fail_part;
		}
		__get_mtd_device(part);

fail_part:
		put_mtd_device(mtd);
		return part;
	}

	return mtd;
}

static uint mtd_len_to_pages(struct mtd_info *mtd, u64 len)
{
	do_div(len, mtd->writesize);

	return len;
}

static bool mtd_is_aligned_with_min_io_size(struct mtd_info *mtd, u64 size)
{
	return !do_div(size, mtd->writesize);
}

static bool mtd_is_aligned_with_block_size(struct mtd_info *mtd, u64 size)
{
	return !do_div(size, mtd->erasesize);
}

static void mtd_dump_buf(const u8 *buf, uint len, uint offset)
{
	int i, j;

	for (i = 0; i < len; ) {
		printf("0x%08x:\t", offset + i);
		for (j = 0; j < 8; j++)
			printf("%02x ", buf[i + j]);
		printf(" ");
		i += 8;
		for (j = 0; j < 8; j++)
			printf("%02x ", buf[i + j]);
		printf("\n");
		i += 8;
	}
}

static void mtd_dump_device_buf(struct mtd_info *mtd, u64 start_off,
				const u8 *buf, u64 len, bool woob)
{
	bool has_pages = mtd->type == MTD_NANDFLASH ||
		mtd->type == MTD_MLCNANDFLASH;
	int npages = mtd_len_to_pages(mtd, len);
	uint page;

	if (has_pages) {
		for (page = 0; page < npages; page++) {
			u64 data_off = page * mtd->writesize;

			printf("\nDump %d data bytes from 0x%08llx:\n",
			       mtd->writesize, start_off + data_off);
			mtd_dump_buf(&buf[data_off],
				     mtd->writesize, start_off + data_off);

			if (woob) {
				u64 oob_off = page * mtd->oobsize;

				printf("Dump %d OOB bytes from page at 0x%08llx:\n",
				       mtd->oobsize, start_off + data_off);
				mtd_dump_buf(&buf[len + oob_off],
					     mtd->oobsize, 0);
			}
		}
	} else {
		printf("\nDump %lld data bytes from 0x%llx:\n",
		       len, start_off);
		mtd_dump_buf(buf, len, start_off);
	}
}

static void mtd_show_parts(struct mtd_info *mtd, int level)
{
	struct mtd_info *part;
	int i;

	list_for_each_entry(part, &mtd->partitions, node) {
		for (i = 0; i < level; i++)
			printf("\t");
		printf("  - 0x%012llx-0x%012llx : \"%s\"\n",
		       part->offset, part->offset + part->size, part->name);

		mtd_show_parts(part, level + 1);
	}
}

static void mtd_show_device(struct mtd_info *mtd)
{
	/* Device */
	printf("* %s\n", mtd->name);
#if defined(CONFIG_DM)
	if (mtd->dev) {
		printf("  - device: %s\n", mtd->dev->name);
		printf("  - parent: %s\n", mtd->dev->parent->name);
		printf("  - driver: %s\n", mtd->dev->driver->name);
	}
#endif
	if (IS_ENABLED(CONFIG_OF_CONTROL) && mtd->dev) {
		char buf[256];
		int res;

		res = ofnode_get_path(mtd_get_ofnode(mtd), buf, 256);
		printf("  - path: %s\n", res == 0 ? buf : "unavailable");
	}

	/* MTD device information */
	printf("  - type: ");
	switch (mtd->type) {
	case MTD_RAM:
		printf("RAM\n");
		break;
	case MTD_ROM:
		printf("ROM\n");
		break;
	case MTD_NORFLASH:
		printf("NOR flash\n");
		break;
	case MTD_NANDFLASH:
		printf("NAND flash\n");
		break;
	case MTD_DATAFLASH:
		printf("Data flash\n");
		break;
	case MTD_UBIVOLUME:
		printf("UBI volume\n");
		break;
	case MTD_MLCNANDFLASH:
		printf("MLC NAND flash\n");
		break;
	case MTD_ABSENT:
	default:
		printf("Unknown\n");
		break;
	}

	printf("  - block size: 0x%x bytes\n", mtd->erasesize);
	printf("  - min I/O: 0x%x bytes\n", mtd->writesize);

	if (mtd->oobsize) {
		printf("  - OOB size: %u bytes\n", mtd->oobsize);
		printf("  - OOB available: %u bytes\n", mtd->oobavail);
	}

	if (mtd->ecc_strength) {
		printf("  - ECC strength: %u bits\n", mtd->ecc_strength);
		printf("  - ECC step size: %u bytes\n", mtd->ecc_step_size);
		printf("  - bitflip threshold: %u bits\n",
		       mtd->bitflip_threshold);
	}

	printf("  - 0x%012llx-0x%012llx : \"%s\"\n",
	       mtd->offset, mtd->offset + mtd->size, mtd->name);

	/* MTD partitions, if any */
	mtd_show_parts(mtd, 1);
}

/* Logic taken from fs/ubifs/recovery.c:is_empty() */
static bool mtd_oob_write_is_empty(struct mtd_oob_ops *op)
{
	int i;

	for (i = 0; i < op->len; i++)
		if (op->datbuf[i] != 0xff)
			return false;

	for (i = 0; i < op->ooblen; i++)
		if (op->oobbuf[i] != 0xff)
			return false;

	return true;
}

static int do_mtd_list(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	struct mtd_info *mtd;
	int dev_nb = 0;

	/* Ensure all devices (and their partitions) are probed */
	mtd_probe_devices();

	printf("List of MTD devices:\n");
	mtd_for_each_device(mtd) {
		if (!mtd_is_partition(mtd))
			mtd_show_device(mtd);

		dev_nb++;
	}

	if (!dev_nb) {
		printf("No MTD device found\n");
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int mtd_special_write_oob(struct mtd_info *mtd, u64 off,
				 struct mtd_oob_ops *io_op,
				 bool write_empty_pages, bool woob)
{
	int ret = 0;

	/*
	 * By default, do not write an empty page.
	 * Skip it by simulating a successful write.
	 */
	if (!write_empty_pages && mtd_oob_write_is_empty(io_op)) {
		io_op->retlen = mtd->writesize;
		io_op->oobretlen = woob ? mtd->oobsize : 0;
	} else {
		ret = mtd_write_oob(mtd, off, io_op);
	}

	return ret;
}

static int do_mtd_io(struct cmd_tbl *cmdtp, int flag, int argc,
		     char *const argv[])
{
	bool dump, read, raw, woob, write_empty_pages, has_pages = false;
	u64 start_off, off, len, remaining, default_len;
	struct mtd_oob_ops io_op = {};
	ulong user_addr = 0;
	uint npages;
	const char *cmd = argv[0];
	struct mtd_info *mtd;
	u32 oob_len;
	u8 *buf;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	mtd = get_mtd_by_name(argv[1]);
	if (IS_ERR_OR_NULL(mtd))
		return CMD_RET_FAILURE;

	if (mtd->type == MTD_NANDFLASH || mtd->type == MTD_MLCNANDFLASH)
		has_pages = true;

	dump = !strncmp(cmd, "dump", 4);
	read = dump || !strncmp(cmd, "read", 4);
	raw = strstr(cmd, ".raw");
	woob = strstr(cmd, ".oob");
	write_empty_pages = !has_pages || strstr(cmd, ".dontskipff");

	argc -= 2;
	argv += 2;

	if (!dump) {
		if (!argc) {
			ret = CMD_RET_USAGE;
			goto out_put_mtd;
		}

		user_addr = hextoul(argv[0], NULL);
		argc--;
		argv++;
	}

	start_off = argc > 0 ? hextoul(argv[0], NULL) : 0;
	if (!mtd_is_aligned_with_min_io_size(mtd, start_off)) {
		printf("Offset not aligned with a page (0x%x)\n",
		       mtd->writesize);
		ret = CMD_RET_FAILURE;
		goto out_put_mtd;
	}

	default_len = dump ? mtd->writesize : mtd->size;
	len = argc > 1 ? hextoul(argv[1], NULL) : default_len;
	if (!mtd_is_aligned_with_min_io_size(mtd, len)) {
		len = round_up(len, mtd->writesize);
		printf("Size not on a page boundary (0x%x), rounding to 0x%llx\n",
		       mtd->writesize, len);
	}

	remaining = len;
	npages = mtd_len_to_pages(mtd, len);
	oob_len = woob ? npages * mtd->oobsize : 0;

	if (dump)
		buf = kmalloc(len + oob_len, GFP_KERNEL);
	else
		buf = map_sysmem(user_addr, 0);

	if (!buf) {
		printf("Could not map/allocate the user buffer\n");
		ret = CMD_RET_FAILURE;
		goto out_put_mtd;
	}

	if (has_pages)
		printf("%s %lld byte(s) (%d page(s)) at offset 0x%08llx%s%s%s\n",
		       read ? "Reading" : "Writing", len, npages, start_off,
		       raw ? " [raw]" : "", woob ? " [oob]" : "",
		       !read && write_empty_pages ? " [dontskipff]" : "");
	else
		printf("%s %lld byte(s) at offset 0x%08llx\n",
		       read ? "Reading" : "Writing", len, start_off);

	io_op.mode = raw ? MTD_OPS_RAW : MTD_OPS_AUTO_OOB;
	io_op.len = has_pages ? mtd->writesize : len;
	io_op.ooblen = woob ? mtd->oobsize : 0;
	io_op.datbuf = buf;
	io_op.oobbuf = woob ? &buf[len] : NULL;

	/* Search for the first good block after the given offset */
	off = start_off;
	while (mtd_block_isbad(mtd, off))
		off += mtd->erasesize;

	/* Loop over the pages to do the actual read/write */
	while (remaining) {
		/* Skip the block if it is bad */
		if (mtd_is_aligned_with_block_size(mtd, off) &&
		    mtd_block_isbad(mtd, off)) {
			off += mtd->erasesize;
			continue;
		}

		if (read)
			ret = mtd_read_oob(mtd, off, &io_op);
		else
			ret = mtd_special_write_oob(mtd, off, &io_op,
						    write_empty_pages, woob);

		if (ret) {
			printf("Failure while %s at offset 0x%llx\n",
			       read ? "reading" : "writing", off);
			break;
		}

		off += io_op.retlen;
		remaining -= io_op.retlen;
		io_op.datbuf += io_op.retlen;
		io_op.oobbuf += io_op.oobretlen;
	}

	if (!ret && dump)
		mtd_dump_device_buf(mtd, start_off, buf, len, woob);

	if (dump)
		kfree(buf);
	else
		unmap_sysmem(buf);

	if (ret) {
		printf("%s on %s failed with error %d\n",
		       read ? "Read" : "Write", mtd->name, ret);
		ret = CMD_RET_FAILURE;
	} else {
		ret = CMD_RET_SUCCESS;
	}

out_put_mtd:
	put_mtd_device(mtd);

	return ret;
}

static int do_mtd_erase(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	struct erase_info erase_op = {};
	struct mtd_info *mtd;
	u64 off, len;
	bool scrub;
	int ret = 0;

	if (argc < 2)
		return CMD_RET_USAGE;

	mtd = get_mtd_by_name(argv[1]);
	if (IS_ERR_OR_NULL(mtd))
		return CMD_RET_FAILURE;

	scrub = strstr(argv[0], ".dontskipbad");

	argc -= 2;
	argv += 2;

	off = argc > 0 ? hextoul(argv[0], NULL) : 0;
	len = argc > 1 ? hextoul(argv[1], NULL) : mtd->size;

	if (!mtd_is_aligned_with_block_size(mtd, off)) {
		printf("Offset not aligned with a block (0x%x)\n",
		       mtd->erasesize);
		ret = CMD_RET_FAILURE;
		goto out_put_mtd;
	}

	if (!mtd_is_aligned_with_block_size(mtd, len)) {
		printf("Size not a multiple of a block (0x%x)\n",
		       mtd->erasesize);
		ret = CMD_RET_FAILURE;
		goto out_put_mtd;
	}

	printf("Erasing 0x%08llx ... 0x%08llx (%d eraseblock(s))\n",
	       off, off + len - 1, mtd_div_by_eb(len, mtd));

	erase_op.mtd = mtd;
	erase_op.addr = off;
	erase_op.len = mtd->erasesize;
	erase_op.scrub = scrub;

	while (len) {
		ret = mtd_erase(mtd, &erase_op);

		if (ret) {
			/* Abort if its not a bad block error */
			if (ret != -EIO)
				break;
			printf("Skipping bad block at 0x%08llx\n",
			       erase_op.addr);
		}

		len -= mtd->erasesize;
		erase_op.addr += mtd->erasesize;
	}

	if (ret && ret != -EIO)
		ret = CMD_RET_FAILURE;
	else
		ret = CMD_RET_SUCCESS;

out_put_mtd:
	put_mtd_device(mtd);

	return ret;
}

#ifdef CONFIG_CMD_MTD_MARKBAD
static int do_mtd_markbad(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	struct mtd_info *mtd;
	loff_t off;
	int ret = 0;

	if (argc < 3)
		return CMD_RET_USAGE;

	mtd = get_mtd_by_name(argv[1]);
	if (IS_ERR_OR_NULL(mtd))
		return CMD_RET_FAILURE;

	if (!mtd_can_have_bb(mtd)) {
		printf("Only NAND-based devices can have bad blocks\n");
		goto out_put_mtd;
	}

	argc -= 2;
	argv += 2;
	while (argc > 0) {
		off = hextoul(argv[0], NULL);
		if (!mtd_is_aligned_with_block_size(mtd, off)) {
			printf("Offset not aligned with a block (0x%x)\n",
			       mtd->erasesize);
			ret = CMD_RET_FAILURE;
			goto out_put_mtd;
		}

		ret = mtd_block_markbad(mtd, off);
		if (ret) {
			printf("block 0x%08llx NOT marked as bad! ERROR %d\n",
			       off, ret);
			ret = CMD_RET_FAILURE;
		} else {
			printf("block 0x%08llx successfully marked as bad\n",
			       off);
		}
		--argc;
		++argv;
	}

out_put_mtd:
	put_mtd_device(mtd);

	return ret;
}
#endif

#ifdef CONFIG_CMD_MTD_TORTURE
/**
 * nand_check_pattern:
 *
 * Check if buffer contains only a certain byte pattern.
 *
 * @param buf buffer to check
 * @param patt the pattern to check
 * @param size buffer size in bytes
 * Return: 1 if there are only patt bytes in buf
 *         0 if something else was found
 */
static int nand_check_pattern(const u_char *buf, u_char patt, int size)
{
	int i;

	for (i = 0; i < size; i++)
		if (buf[i] != patt)
			return 0;
	return 1;
}

/**
 * nand_torture:
 *
 * Torture a block of NAND flash.
 * This is useful to determine if a block that caused a write error is still
 * good or should be marked as bad.
 *
 * @param mtd nand mtd instance
 * @param offset offset in flash
 * Return: 0 if the block is still good
 */
static int nand_torture(struct mtd_info *mtd, loff_t offset)
{
	u_char patterns[] = {0xa5, 0x5a, 0x00};
	struct erase_info instr = {
		.mtd = mtd,
		.addr = offset,
		.len = mtd->erasesize,
	};
	size_t retlen;
	int err, ret = -1, i, patt_count;
	u_char *buf;

	if ((offset & (mtd->erasesize - 1)) != 0) {
		puts("Attempt to torture a block at a non block-aligned offset\n");
		return -EINVAL;
	}

	if (offset + mtd->erasesize > mtd->size) {
		puts("Attempt to torture a block outside the flash area\n");
		return -EINVAL;
	}

	patt_count = ARRAY_SIZE(patterns);

	buf = malloc_cache_aligned(mtd->erasesize);
	if (buf == NULL) {
		puts("Out of memory for erase block buffer\n");
		return -ENOMEM;
	}

	for (i = 0; i < patt_count; i++) {
		err = mtd_erase(mtd, &instr);
		if (err) {
			printf("%s: erase() failed for block at 0x%llx: %d\n",
			       mtd->name, instr.addr, err);
			goto out;
		}

		/* Make sure the block contains only 0xff bytes */
		err = mtd_read(mtd, offset, mtd->erasesize, &retlen, buf);
		if ((err && err != -EUCLEAN) || retlen != mtd->erasesize) {
			printf("%s: read() failed for block at 0x%llx: %d\n",
			       mtd->name, instr.addr, err);
			goto out;
		}

		err = nand_check_pattern(buf, 0xff, mtd->erasesize);
		if (!err) {
			printf("Erased block at 0x%llx, but a non-0xff byte was found\n",
			       offset);
			ret = -EIO;
			goto out;
		}

		/* Write a pattern and check it */
		memset(buf, patterns[i], mtd->erasesize);
		err = mtd_write(mtd, offset, mtd->erasesize, &retlen, buf);
		if (err || retlen != mtd->erasesize) {
			printf("%s: write() failed for block at 0x%llx: %d\n",
			       mtd->name, instr.addr, err);
			goto out;
		}

		err = mtd_read(mtd, offset, mtd->erasesize, &retlen, buf);
		if ((err && err != -EUCLEAN) || retlen != mtd->erasesize) {
			printf("%s: read() failed for block at 0x%llx: %d\n",
			       mtd->name, instr.addr, err);
			goto out;
		}

		err = nand_check_pattern(buf, patterns[i], mtd->erasesize);
		if (!err) {
			printf("Pattern 0x%.2x checking failed for block at "
			       "0x%llx\n", patterns[i], offset);
			ret = -EIO;
			goto out;
		}
	}

	ret = 0;

out:
	free(buf);
	return ret;
}

static int do_mtd_torture(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	struct mtd_info *mtd;
	loff_t off, len;
	int ret = 0;
	unsigned int failed = 0, passed = 0;

	if (argc < 2)
		return CMD_RET_USAGE;

	mtd = get_mtd_by_name(argv[1]);
	if (IS_ERR_OR_NULL(mtd))
		return CMD_RET_FAILURE;

	if (!mtd_can_have_bb(mtd)) {
		printf("Only NAND-based devices can be tortured\n");
		goto out_put_mtd;
	}

	argc -= 2;
	argv += 2;

	off = argc > 0 ? hextoul(argv[0], NULL) : 0;
	len = argc > 1 ? hextoul(argv[1], NULL) : mtd->size;

	if (!mtd_is_aligned_with_block_size(mtd, off)) {
		printf("Offset not aligned with a block (0x%x)\n",
		       mtd->erasesize);
		ret = CMD_RET_FAILURE;
		goto out_put_mtd;
	}

	if (!mtd_is_aligned_with_block_size(mtd, len)) {
		printf("Size not a multiple of a block (0x%x)\n",
		       mtd->erasesize);
		ret = CMD_RET_FAILURE;
		goto out_put_mtd;
	}

	printf("\nNAND torture: device '%s' offset 0x%llx size 0x%llx (block size 0x%x)\n",
	       mtd->name, off, len, mtd->erasesize);
	while (len > 0) {
		printf("\r  block at %llx ", off);
		if (mtd_block_isbad(mtd, off)) {
			printf("marked bad, skipping\n");
		} else {
			ret = nand_torture(mtd, off);
			if (ret) {
				failed++;
				printf("failed\n");
			} else {
				passed++;
			}
		}
		off += mtd->erasesize;
		len -= mtd->erasesize;
	}
	printf("\n Passed: %u, failed: %u\n", passed, failed);
	if (failed != 0)
		ret = CMD_RET_FAILURE;

out_put_mtd:
	put_mtd_device(mtd);

	return ret;
}
#endif

#ifdef CONFIG_CMD_MTD_NANDTEST
enum nandtest_status {
	NANDTEST_STATUS_UNKNOWN = 0,
	NANDTEST_STATUS_NONECC_READ_FAIL,
	NANDTEST_STATUS_ECC_READ_FAIL,
	NANDTEST_STATUS_BAD_BLOCK,
	NANDTEST_STATUS_BITFLIP_ABOVE_MAX,
	NANDTEST_STATUS_BITFLIP_MISMATCH,
	NANDTEST_STATUS_BITFLIP_MAX,
	NANDTEST_STATUS_OK,
};

static enum nandtest_status nandtest_block_check(struct mtd_info *mtd,
						 loff_t off, size_t blocksize)
{
	struct mtd_oob_ops	ops = {};
	u_char			*buf;
	int			i, d, ret, len, pos, cnt, max;

	if (blocksize % mtd->writesize != 0) {
		printf("\r  block at %llx: bad block size\n", off);
		return NANDTEST_STATUS_UNKNOWN;
	}

	buf = malloc_cache_aligned(2 * blocksize);
	if (buf == NULL) {
		printf("\r  block at %llx: can't allocate memory\n", off);
		return NANDTEST_STATUS_UNKNOWN;
	}

	ops.mode = MTD_OPS_RAW;
	ops.len = blocksize;
	ops.datbuf = buf;
	ops.ooblen = 0;
	ops.oobbuf = NULL;

	if (mtd->_read_oob)
		ret = mtd->_read_oob(mtd, off, &ops);
	else
		ret = mtd->_read(mtd, off, ops.len, &ops.retlen, ops.datbuf);

	if (ret != 0) {
		free(buf);
		printf("\r  block at %llx: non-ecc reading error %d\n",
		       off, ret);
		return NANDTEST_STATUS_NONECC_READ_FAIL;
	}

	ops.mode = MTD_OPS_AUTO_OOB;
	ops.datbuf = buf + blocksize;

	if (mtd->_read_oob)
		ret = mtd->_read_oob(mtd, off, &ops);
	else
		ret = mtd->_read(mtd, off, ops.len, &ops.retlen, ops.datbuf);

	if (ret == -EBADMSG) {
		free(buf);
		printf("\r  block at %llx: bad block\n", off);
		return NANDTEST_STATUS_BAD_BLOCK;
	}

	if (ret < 0) {
		free(buf);
		printf("\r  block at %llx: ecc reading error %d\n", off, ret);
		return NANDTEST_STATUS_ECC_READ_FAIL;
	}

	if (mtd->ecc_strength == 0) {
		free(buf);
		return NANDTEST_STATUS_OK;
	}

	if (ret > mtd->ecc_strength) {
		free(buf);
		printf("\r  block at %llx: returned bit-flips value %d "
		       "is above maximum value %d\n",
		       off, ret, mtd->ecc_strength);
		return NANDTEST_STATUS_BITFLIP_ABOVE_MAX;
	}

	max = 0;
	pos = 0;
	len = blocksize;
	while (len > 0) {
		cnt = 0;
		for (i = 0; i < mtd->ecc_step_size; i++) {
			d = buf[pos + i] ^ buf[blocksize + pos + i];
			if (d == 0)
				continue;

			while (d > 0) {
				d &= (d - 1);
				cnt++;
			}
		}
		if (cnt > max)
			max = cnt;

		len -= mtd->ecc_step_size;
		pos += mtd->ecc_step_size;
	}

	free(buf);

	if (max > ret) {
		printf("\r  block at %llx: bitflip mismatch, "
		       "read %d but actual %d\n", off, ret, max);
		return NANDTEST_STATUS_BITFLIP_MISMATCH;
	}

	if (ret == mtd->ecc_strength) {
		printf("\r  block at %llx: max bitflip reached, "
		       "block is unreliable\n", off);
		return NANDTEST_STATUS_BITFLIP_MAX;
	}

	return NANDTEST_STATUS_OK;
}

static int do_mtd_nandtest(struct cmd_tbl *cmdtp, int flag, int argc,
			   char *const argv[])
{
	struct mtd_info		*mtd;
	loff_t			off, len;
	int			stat[NANDTEST_STATUS_OK + 1];
	enum nandtest_status	ret;
	size_t			block_size;

	if (argc < 2)
		return CMD_RET_USAGE;

	mtd = get_mtd_by_name(argv[1]);
	if (IS_ERR_OR_NULL(mtd))
		return CMD_RET_FAILURE;

	if (!mtd_can_have_bb(mtd)) {
		printf("Only NAND-based devices can be checked\n");
		goto out_put_mtd;
	}

	off = 0;
	len = mtd->size;
	block_size = mtd->erasesize;

	printf("ECC strength:     %d\n",   mtd->ecc_strength);
	printf("ECC step size:    %d\n",   mtd->ecc_step_size);
	printf("Erase block size: 0x%x\n", mtd->erasesize);
	printf("Total blocks:     %d\n",   (u32)len / mtd->erasesize);

	memset(stat, 0, sizeof(stat));
	while (len > 0) {
		if (mtd_is_aligned_with_block_size(mtd, off))
			printf("\r  block at %llx ", off);

		ret = nandtest_block_check(mtd, off, block_size);
		stat[ret]++;

		switch (ret) {
		case NANDTEST_STATUS_BAD_BLOCK:
		case NANDTEST_STATUS_BITFLIP_MAX:
			if (!mtd_block_isbad(mtd, off))
				printf("\r  block at %llx: should be marked "
				       "as BAD\n", off);
			break;

		case NANDTEST_STATUS_OK:
			if (mtd_block_isbad(mtd, off))
				printf("\r  block at %llx: marked as BAD, but "
				       "probably is GOOD\n", off);
			break;

		default:
			break;
		}

		off += block_size;
		len -= block_size;
	}

out_put_mtd:
	put_mtd_device(mtd);
	printf("\n");
	printf("results:\n");
	printf("  Good blocks:            %d\n", stat[NANDTEST_STATUS_OK]);
	printf("  Physically bad blocks:  %d\n", stat[NANDTEST_STATUS_BAD_BLOCK]);
	printf("  Unreliable blocks:      %d\n", stat[NANDTEST_STATUS_BITFLIP_MAX]);
	printf("  Non checked blocks:     %d\n", stat[NANDTEST_STATUS_UNKNOWN]);
	printf("  Failed to check blocks: %d\n", stat[NANDTEST_STATUS_NONECC_READ_FAIL] +
						 stat[NANDTEST_STATUS_ECC_READ_FAIL]);
	printf("  Suspictious blocks:     %d\n", stat[NANDTEST_STATUS_BITFLIP_ABOVE_MAX] +
						 stat[NANDTEST_STATUS_BITFLIP_MISMATCH]);
	return CMD_RET_SUCCESS;
}
#endif

static int do_mtd_bad(struct cmd_tbl *cmdtp, int flag, int argc,
		      char *const argv[])
{
	struct mtd_info *mtd;
	loff_t off;

	if (argc < 2)
		return CMD_RET_USAGE;

	mtd = get_mtd_by_name(argv[1]);
	if (IS_ERR_OR_NULL(mtd))
		return CMD_RET_FAILURE;

	if (!mtd_can_have_bb(mtd)) {
		printf("Only NAND-based devices can have bad blocks\n");
		goto out_put_mtd;
	}

	printf("MTD device %s bad blocks list:\n", mtd->name);
	for (off = 0; off < mtd->size; off += mtd->erasesize) {
		if (mtd_block_isbad(mtd, off))
			printf("\t0x%08llx\n", off);
	}

out_put_mtd:
	put_mtd_device(mtd);

	return CMD_RET_SUCCESS;
}

#ifdef CONFIG_AUTO_COMPLETE
static int mtd_name_complete(int argc, char *const argv[], char last_char,
			     int maxv, char *cmdv[])
{
	int len = 0, n_found = 0;
	struct mtd_info *mtd;

	argc--;
	argv++;

	if (argc > 1 ||
	    (argc == 1 && (last_char == '\0' || isblank(last_char))))
		return 0;

	if (argc)
		len = strlen(argv[0]);

	mtd_for_each_device(mtd) {
		if (argc &&
		    (len > strlen(mtd->name) ||
		     strncmp(argv[0], mtd->name, len)))
			continue;

		if (n_found >= maxv - 2) {
			cmdv[n_found++] = "...";
			break;
		}

		cmdv[n_found++] = mtd->name;
	}

	cmdv[n_found] = NULL;

	return n_found;
}
#endif /* CONFIG_AUTO_COMPLETE */

#ifdef CONFIG_SYS_LONGHELP
static char mtd_help_text[] =
	"- generic operations on memory technology devices\n\n"
	"mtd list\n"
	"mtd read[.raw][.oob]                  <name> <addr> [<off> [<size>]]\n"
	"mtd dump[.raw][.oob]                  <name>        [<off> [<size>]]\n"
	"mtd write[.raw][.oob][.dontskipff]    <name> <addr> [<off> [<size>]]\n"
	"mtd erase[.dontskipbad]               <name>        [<off> [<size>]]\n"
	"\n"
	"Specific functions:\n"
	"mtd bad                               <name>\n"
#ifdef CONFIG_CMD_MTD_MARKBAD
	"mtd markbad                           <name>       <off> [<off> ...]\n"
#endif
#ifdef CONFIG_CMD_MTD_TORTURE
	"mtd torture                           <name>        [<off> [<size>]]\n"
#endif
#ifdef CONFIG_CMD_MTD_NANDTEST
	"mtd nandtest                          <name>\n"
#endif
	"\n"
	"With:\n"
	"\t<name>: NAND partition/chip name (or corresponding DM device name or OF path or dev:part)\n"
	"\t<addr>: user address from/to which data will be retrieved/stored\n"
	"\t<off>: offset in <name> in bytes (default: start of the part)\n"
	"\t\t* must be block-aligned for erase\n"
	"\t\t* must be page-aligned otherwise\n"
	"\t<size>: length of the operation in bytes (default: the entire device)\n"
	"\t\t* must be a multiple of a block for erase\n"
	"\t\t* must be a multiple of a page otherwise (special case: default is a page with dump)\n"
	"\n"
	"The .dontskipff option forces writing empty pages, don't use it if unsure.\n";
#endif

U_BOOT_CMD_WITH_SUBCMDS(mtd, "MTD utils", mtd_help_text,
		U_BOOT_SUBCMD_MKENT(list, 1, 1, do_mtd_list),
		U_BOOT_SUBCMD_MKENT_COMPLETE(read, 5, 0, do_mtd_io,
					     mtd_name_complete),
		U_BOOT_SUBCMD_MKENT_COMPLETE(write, 5, 0, do_mtd_io,
					     mtd_name_complete),
		U_BOOT_SUBCMD_MKENT_COMPLETE(dump, 4, 0, do_mtd_io,
					     mtd_name_complete),
		U_BOOT_SUBCMD_MKENT_COMPLETE(erase, 4, 0, do_mtd_erase,
					     mtd_name_complete),
#ifdef CONFIG_CMD_MTD_MARKBAD
		U_BOOT_SUBCMD_MKENT_COMPLETE(markbad, 20, 0, do_mtd_markbad,
					     mtd_name_complete),
#endif
#ifdef CONFIG_CMD_MTD_TORTURE
		U_BOOT_SUBCMD_MKENT_COMPLETE(torture, 4, 0, do_mtd_torture,
					     mtd_name_complete),
#endif
#ifdef CONFIG_CMD_MTD_NANDTEST
		U_BOOT_SUBCMD_MKENT_COMPLETE(nandtest, 2, 0, do_mtd_nandtest,
					     mtd_name_complete),
#endif
		U_BOOT_SUBCMD_MKENT_COMPLETE(bad, 2, 1, do_mtd_bad,
					     mtd_name_complete));
