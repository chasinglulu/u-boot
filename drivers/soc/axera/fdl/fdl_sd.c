/* SPDX-License-Identifier: GPL-2.0+
 *
 * FDL over SDCard
 *
 * Copyright (C) 2025 Charleye <wangkart@aliyun.com>
 *
 */

#include <common.h>
#include <dm/uclass.h>
#include <dm/device.h>
#include <mmc.h>
#include <command.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <fdl.h>
#include <malloc.h>
#include <fs.h>
#include <part.h>
#include <u-boot/crc.h>
#ifdef CONFIG_FDL_SHA1SUM
#include <u-boot/sha1.h>
#include <ctype.h>
#include <linux/string.h>
#endif

struct fdl_sd_partition {
	lbaint_t start;
	lbaint_t size;
	char name[PART_NAME_LEN];
	int bootable;
	int has_image;
};
_Static_assert(sizeof(struct fdl_sd_partition) == 56,
	   "fdl_sd_partition structure size must be 56 bytes");

struct __packed fdl_sd_part_tab {
	__le32 magic;
	__le32 version;
	__le32 crc32;
	__le32 blksz;
	__le32 number;
	struct fdl_sd_partition partitions[];
};

enum fdl_image_type {
	FDL_IMG_TYPE_PARTITION,
	FDL_IMG_TYPE_DATA,
};

#define FDL_SD_PARTITIONS_MAGIC       0x54524150

static struct blk_desc *fs_dev_desc;
static int fs_dev_part = 0;

static struct fdl_sd_part_tab *fdl_sd_table = NULL;
struct fdl_part_table *fdl_table = NULL;

__weak void fdl_sd_complete(void)
{
	/* Overridden by the board-specific code */
}

#ifdef CONFIG_FDL_SHA1SUM
#define MAX_SHA1_ENTRIES 32
#define SHA1_HEX_LEN (SHA1_SUM_LEN * 2)
#define MAX_FILENAME_LEN 64

struct fdl_sha1_entry {
	char filename[MAX_FILENAME_LEN];
	unsigned char sha1[SHA1_SUM_LEN];
};

static struct fdl_sha1_entry sha1_entries[MAX_SHA1_ENTRIES];
static int num_sha1_entries = 0;

static int parse_sha1sum_file(char *buffer, size_t size)
{
	char *line;
	char *buf_ptr = buffer;
	char *buf_end = buffer + size;
	int count = 0;

	num_sha1_entries = 0;

	while ((line = strsep(&buf_ptr, "\n")) != NULL && line < buf_end) {
		char sha1_hex[SHA1_HEX_LEN + 1];
		char *filename;
		int i = 0;
		char *p = line;

		/* Trim leading whitespace */
		while (*p && isspace((unsigned char)*p))
			p++;

		/* Skip empty or comment lines */
		if (!*p || *p == '#')
			continue;

		/* Read SHA1 hex string */
		while (*p && !isspace((unsigned char)*p) && i < SHA1_HEX_LEN)
			sha1_hex[i++] = *p++;
		sha1_hex[i] = '\0';

		if (i != SHA1_HEX_LEN) {
			fdl_err("Invalid SHA1 hash length for line: %s\n", line);
			continue; /* Skip malformed line */
		}

		/* Skip whitespace between hash and filename */
		while (*p && isspace((unsigned char)*p))
			p++;

		filename = p;
		/* Trim trailing whitespace from filename */
		p = filename + strlen(filename) - 1;
		while (p >= filename && isspace((unsigned char)*p))
			*p-- = '\0';

		if (!*filename) {
			fdl_err("Missing filename for line: %s\n", line);
			continue; /* Skip malformed line */
		}

		if (count >= MAX_SHA1_ENTRIES) {
			fdl_err("Exceeded maximum SHA1 entries (%d)\n", MAX_SHA1_ENTRIES);
			return -ENOSPC;
		}

		if (hex2bin(sha1_entries[count].sha1, sha1_hex, SHA1_SUM_LEN) < 0) {
			fdl_err("Invalid SHA1 hex string: %s\n", sha1_hex);
			continue;
		}

		strncpy(sha1_entries[count].filename, filename, MAX_FILENAME_LEN - 1);
		sha1_entries[count].filename[MAX_FILENAME_LEN - 1] = '\0';

		fdl_debug("SHA1 Entry %d: %s %s\n", count, sha1_hex, sha1_entries[count].filename);
		count++;
	}
	num_sha1_entries = count;
	return 0;
}

/* Gets the expected SHA1 hash for a given filename */
static const unsigned char *get_expected_sha1(const char *filename)
{
	int i;
	for (i = 0; i < num_sha1_entries; i++) {
		if (strcmp(sha1_entries[i].filename, filename) == 0) {
			return sha1_entries[i].sha1;
		}
	}
	return NULL;
}

static int verify_image_sha1sum(const char *image_name, const void *data, size_t data_size)
{
	unsigned char calculated_sha1[SHA1_SUM_LEN];
	const unsigned char *expected_sha1;

	/* Skip verification if no SHA1 entries are available */
	if (num_sha1_entries == 0)
		return 0;

	expected_sha1 = get_expected_sha1(image_name);
	if (!expected_sha1) {
		fdl_debug("No SHA1 entry for '%s', skipping verification.\n", image_name);
		return 0;
	}

	sha1_csum_wd(data, data_size, calculated_sha1, SHA1_SUM_LEN);

	if (memcmp(expected_sha1, calculated_sha1, SHA1_SUM_LEN) != 0) {
		char exp_hex[SHA1_HEX_LEN + 1];
		char calc_hex[SHA1_HEX_LEN + 1];
		bin2hex(exp_hex, (char *)expected_sha1, SHA1_SUM_LEN);
		bin2hex(calc_hex, (char *)calculated_sha1, SHA1_SUM_LEN);
		exp_hex[SHA1_HEX_LEN] = '\0';
		calc_hex[SHA1_HEX_LEN] = '\0';
		fdl_err("SHA1 mismatch for image '%s'\n", image_name);
		fdl_err("   Expected: %s\n", calc_hex);
		fdl_err("   Found   : %s\n", exp_hex);
		return -EBADMSG;
	}

	fdl_debug("Image '%s' SHA1 verification passed.\n", image_name);
	return 0;
}
#endif /* CONFIG_FDL_SHA1SUM */

static inline const char *get_filename(int part_idx)
{
	static char part_name[PART_NAME_LEN];
	struct fdl_sd_partition *part;
	if (!fdl_sd_table || part_idx < 0 ||
	    part_idx >= le32_to_cpu(fdl_sd_table->number)) {
		fdl_err("Invalid partition index: %d\n", part_idx);
		return NULL;
	}

	part = &fdl_sd_table->partitions[part_idx];
	if (le64_to_cpu(part->size) == 0)
		return NULL;
	if (part->has_image) {
		strncpy(part_name, part->name, PART_NAME_LEN);
		return strcat(part_name, CONFIG_VAL(FDL_IMAGE_SUFFIX));
	}

	return NULL;
}

static inline const char *get_image_name(enum fdl_image_type type, int idx)
{
	switch (type) {
	case FDL_IMG_TYPE_PARTITION:
		return CONFIG_VAL(FDL_PARTITION_IMAGE);
	case FDL_IMG_TYPE_DATA:
		return get_filename(idx);
	default:
		return NULL;
	}
}

static inline int _fs_exists(const char *image_name)
{
	int ret;

	ret = fs_set_blk_dev_with_part(fs_dev_desc, fs_dev_part);
	if (ret < 0)
		return ret;

	ret = fs_exists(image_name);
	if (ret < 0) {
		fdl_err("Image '%s' not found in partition '%d' on device '%s'\n",
			image_name, fs_dev_part, fs_dev_desc->bdev->name);
	}
	return ret;
}

static inline int _fs_size(const char *image_name, loff_t *size)
{
	int ret;

	ret = fs_set_blk_dev_with_part(fs_dev_desc, fs_dev_part);
	if (ret < 0)
		return ret;

	ret = fs_size(image_name, size);
	if (ret < 0) {
		fdl_err("Failed to get size of image '%s' (ret = %d)\n",
			      image_name, ret);
	}
	return ret;
}

static inline int _fs_read(const char *image_name, ulong addr, loff_t *actual_size)
{
	int ret;

	ret = fs_set_blk_dev_with_part(fs_dev_desc, fs_dev_part);
	if (ret < 0)
		return ret;

	/* fs_read: offset=0, len=0 means read the whole file */
	ret = fs_read(image_name, addr, 0, 0, actual_size);
	if (ret < 0) {
		fdl_err("Failed to read image '%s' from partition '%d' on device '%s'\n",
			image_name, fs_dev_part, fs_dev_desc->bdev->name);
	}
	return ret;
}

static int find_image_by_name(const char *image_name, char *buf, size_t *image_size)
{
	int ret;
	loff_t file_size;
	ulong read_addr;
	loff_t bytes_read;

	ret = _fs_exists(image_name);
	if (ret < 0)
		return ret;

	ret = _fs_size(image_name, &file_size);
	if (ret < 0)
		return ret;

	if (!buf && file_size >= fdl_buf_size) {
		fdl_err("Image '%s' size (%lld bytes) exceeds buffer size (%u bytes)\n",
			image_name, file_size, fdl_buf_size);
		return -ENOMEM;
	}

	read_addr = buf ? (ulong)buf : (ulong)fdl_buf_addr;

	ret = _fs_read(image_name, read_addr, &bytes_read);
	if (ret < 0)
		return ret;

	if (image_size)
		*image_size = (size_t)bytes_read;

	return 0;
}

static int get_part_table(const char *part_tab_image, struct fdl_part_table **fdl_part_tab)
{
	struct fdl_sd_part_tab *sd_tab = fdl_sd_table;
	struct fdl_part_table *fdl_tab;
	uint32_t calc_crc32, crc_len;
	size_t image_size;
	int i, ret = 0;

	if (!part_tab_image || !fdl_part_tab || !sd_tab) {
		fdl_err("Invalid partition table arguments\n");
		return -EINVAL;
	}

	ret = find_image_by_name(part_tab_image, (void *)sd_tab, &image_size);
	if (ret < 0) {
		fdl_err("Failed to find or read partition table image '%s'\n", part_tab_image);
		return ret;
	}

#ifdef DEBUG
	print_hex_dump("PARTITION TABLE: ", DUMP_PREFIX_ADDRESS, 16, 64,
		       sd_tab, image_size, true);
#endif

	if (le32_to_cpu(sd_tab->magic) != FDL_SD_PARTITIONS_MAGIC) {
		fdl_err("Invalid partition table magic (0x%x)\n", le32_to_cpu(sd_tab->magic));
		return -EINVAL;
	}

	crc_len = image_size - offsetof(struct fdl_sd_part_tab, blksz);
	calc_crc32 = crc32(0, (void *)&sd_tab->blksz, crc_len);
	if (calc_crc32 != le32_to_cpu(sd_tab->crc32)) {
		fdl_err("Invaild partition table CRC32 (expected 0x%x, found 0x%x)\n",
			calc_crc32, le32_to_cpu(sd_tab->crc32));
		return -EINVAL;
	}

	fdl_tab = malloc(sizeof(*fdl_tab) + sd_tab->number * sizeof(struct disk_partition));
	if (!fdl_tab) {
		fdl_err("Out of memory for FDL partition table\n");
		return -ENOMEM;
	}

	fdl_tab->number = le32_to_cpu(sd_tab->number);
	for (i = 0; i < fdl_tab->number; i++) {
		struct fdl_sd_partition *sd_part = &sd_tab->partitions[i];
		struct disk_partition *part = &fdl_tab->part[i];

#ifdef DEBUG
		print_hex_dump("PARTITION INFO: ", DUMP_PREFIX_ADDRESS, 16, 64,
		              sd_part, sizeof(*sd_part), true);
#endif

		part->blksz = le32_to_cpu(sd_tab->blksz);
		part->start = le64_to_cpu(sd_part->start);
		part->size = le64_to_cpu(sd_part->size);
		strncpy((void *)part->name, sd_part->name, PART_NAME_LEN - 1);
		part->name[PART_NAME_LEN - 1] = '\0';

		fdl_debug("Part %d:\n", i);
		fdl_debug("    Raw Part    : %s start = 0x%08lx size = 0x%08lx\n",
		                  sd_part->name, part->start, part->size);
		fdl_debug("    Written Part: %s start = 0x%08lx size = 0x%08lx\n",
		                  part->name, part->start, part->size);
	}
	*fdl_part_tab = fdl_tab;

	return 0;
}

static int fdl_sd_process(struct udevice *dev, bool timeout, bool need_ack)
{
	struct blk_desc *dev_desc;
	struct mmc *mmc;
	struct disk_partition part_info;
	bool found_partition = false;
	bool found_fs = false;
	const char *fs_type_name;
	const char *filename;
	const char *part_name;
	size_t filesize;
	int i, ret;
#ifdef CONFIG_FDL_SHA1SUM
	const char *sha1_filename;
	size_t sha1_filesize;
#endif

	fdl_init(NULL, 0);

	if (IS_ERR_OR_NULL(dev)) {
		fdl_err("Invalid MMC device\n");
		return -ENODEV;
	}

	mmc = mmc_get_mmc_dev(dev);
	dev_desc = mmc_get_blk_desc(mmc);
	if (IS_ERR_OR_NULL(dev_desc)) {
		fdl_err("MMC device '%s' is not ready\n", dev->name);
		return -ENODEV;
	}

	/* Check for partitions and filesystems */
	for (i = 1; i <= MAX_SEARCH_PARTITIONS; i++) {
		ret = part_get_info(dev_desc, i, &part_info);
		if (ret)
			continue;
		found_partition = true;

		ret = fs_set_blk_dev_with_part(dev_desc, i);
		if (ret < 0)
			continue;

		fs_type_name = fs_get_type_name();
		fs_close();

		if (fs_type_name && strcmp(fs_type_name, "unsupported") != 0) {
			fdl_debug("Found partition %d with filesystem '%s' on device '%s'\n",
			                     i, fs_type_name, dev->name);
			found_fs = true;
			break;  // Stop at the first supported filesystem found
		} else {
			fdl_debug("No filesystem found on partition %d of %s\n", i, dev->name);
			continue;
		}
	}

	if (!found_partition) {
		fdl_err("No partitions found on SDCard device '%s'\n", dev->name);
		return -ENODATA;
	}

	if (!found_fs) {
		fdl_err("No supported filesystem found on any partition of SDCard device '%s'\n",
			  dev->name);
		return -ENODATA;
	}

	fs_dev_desc = dev_desc;
	fs_dev_part = i;

#ifdef CONFIG_FDL_SHA1SUM
	num_sha1_entries = 0;
	sha1_filename = CONFIG_VAL(FDL_SHA1SUM_FILENAME);
	ret = find_image_by_name(sha1_filename, NULL, &sha1_filesize);
	if (ret == 0) {
		ret = parse_sha1sum_file((char *)fdl_buf_addr, sha1_filesize);
		if (ret < 0) {
			fdl_err("Failed to parse %s (ret = %d)\n", sha1_filename, ret);
			goto failed;
		}
		fdl_info("%s loaded and parsed, %d entries.\n", sha1_filename, num_sha1_entries);
	} else {
		fdl_warn("%s not found, skipping checksum verification.\n", sha1_filename);
	}
#endif

	fdl_sd_table = malloc(SZ_16K);
	if (!fdl_sd_table) {
		fdl_err("Out of memory\n");
		return -ENOMEM;
	}

	filename = get_image_name(FDL_IMG_TYPE_PARTITION, 0);
	ret = get_part_table(filename, &fdl_table);
	if (ret < 0) {
		fdl_err("Failed to get partition table (ret = %d)\n", ret);
		goto failed;
	}

	ret = fdl_blk_write_partition(fdl_table);
	if (ret < 0) {
		fdl_err("Failed to write FDL partition table (ret = %d)\n", ret);
		goto failed;
	}
	fdl_debug("FDL partition table written successfully\n");

	for (i = 0; i < fdl_sd_table->number; i++) {
		filename = get_image_name(FDL_IMG_TYPE_DATA, i);
		part_name = fdl_sd_table->partitions[i].name;

		if (!filename) {
			fdl_debug("No image for partition '%s'\n", part_name);
			continue;
		}

		fdl_info("Processing partition '%s': '%s'\n", part_name, filename);
		ret = find_image_by_name(filename, NULL, &filesize);
		if (ret < 0) {
			fdl_err("Failed to find image '%s' in partition %d (ret = %d)\n",
			                  filename, i, ret);
			goto failed;
		}
		fdl_debug("Found image '%s' with size %zu bytes\n", filename, filesize);

#ifdef CONFIG_FDL_SHA1SUM
		ret = verify_image_sha1sum(filename, (void *)fdl_buf_addr,
		                        filesize);
		if (ret < 0) {
			fdl_err("Image '%s' SHA1 verification failed.\n", filename);
			goto failed;
		}
#endif

		fdl_debug("Writing image '%s' to '%s' partition (i = %d)\n", filename, part_name, i);
		ret = fdl_blk_write_data(part_name, filesize);
		if (ret < 0) {
			fdl_err("Failed to write image '%s' to partition %d (ret = %d)\n",
			                  filename, i, ret);
			goto failed;
		}

		fdl_debug("Image '%s' written to '%s' partition successfully\n", filename, part_name);
	}

	fdl_debug("All images written successfully\n");
	fdl_sd_complete();

	ret = 0;
failed:
#ifdef CONFIG_FDL_SHA1SUM
	num_sha1_entries = 0;
#endif
	if (fdl_sd_table) {
		free(fdl_sd_table);
		fdl_sd_table = NULL;
	}
	if (fdl_table) {
		free(fdl_table);
		fdl_table = NULL;
	}

	return ret;
}

static int do_fdl_sd(struct cmd_tbl *cmdtp, int flag, int argc,
                        char *const argv[])
{
	struct udevice *dev = NULL;
	struct mmc *mmc;
	int seq = -1;
	int ret;

	if (argc > 2)
		return CMD_RET_USAGE;

	if (argc == 2) {
		seq = simple_strtol(argv[1], NULL, 10);
		if (seq < 0)
			return CMD_RET_USAGE;
	}

	// fdl_set_loglevel(FDL_LOGLEVEL_DEBUG);
	uclass_foreach_dev_probe(UCLASS_MMC, dev) {
		fdl_debug("Found MMC device: %s\n", dev->name);
		mmc = mmc_get_mmc_dev(dev);
		if (IS_ERR_OR_NULL(mmc)) {
			fdl_err("MMC device '%s' is not ready\n", dev->name);
			return CMD_RET_FAILURE;
		}

		mmc_init(mmc);

		if (seq > 0) {
			if (dev->seq_ == seq && IS_SD(mmc))
				break;
		} else {
			if (IS_SD(mmc))
				break;
		}
	}
	if (IS_ERR_OR_NULL(dev)) {
		fdl_err("Not found SDCard device\n");
		return CMD_RET_FAILURE;
	}
	fdl_debug("SDCard device name: '%s'\n", dev->name);

	ret = fdl_sd_process(dev, true, false);
	if (ret < 0)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	fdl_sd, 2, 1, do_fdl_sd,
	"FDL over SDCard",
	"[<dev_idx>]\n"
	"    - Download FDL image via SDCard.\n"
	"    - If <dev_idx> is specified, use the MMC device with that sequence number.\n"
	"    - If <dev_idx> is not specified, use the first available SDCard."
);

int fdl_sd_download(int dev_idx, bool timeout)
{
	struct udevice *dev = NULL;
	struct mmc *mmc;
	int ret;

	// fdl_set_loglevel(FDL_LOGLEVEL_DEBUG);
	uclass_foreach_dev_probe(UCLASS_MMC, dev) {
		fdl_debug("Found MMC device: %s\n", dev->name);
		mmc = mmc_get_mmc_dev(dev);
		if (IS_ERR_OR_NULL(mmc)) {
			fdl_err("MMC device '%s' is not ready\n", dev->name);
			return -EBUSY;
		}

		mmc_init(mmc);

		if (dev_idx > 0) {
			if (dev->seq_ == dev_idx && IS_SD(mmc))
				break;
		} else {
			if (IS_SD(mmc))
				break;
		}
	}
	if (IS_ERR_OR_NULL(dev)) {
		fdl_err("Not found SDCard device\n");
		return -ENODEV;
	}
	fdl_debug("SDCard device name: '%s'\n", dev->name);

	ret = fdl_sd_process(dev, timeout, true);
	if (ret < 0)
		return ret;

	return 0;
}