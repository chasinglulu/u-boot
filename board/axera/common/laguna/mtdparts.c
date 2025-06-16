/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <mtd.h>
#include <env.h>
#include <hexdump.h>

#include <asm/arch/bootparams.h>
#include <linux/string.h>
#include <linux/mtd/mtdparts.h>

#define MTDPARTS_LEN    512
#define MTDIDS_LEN      128

#if defined(CONFIG_LUA_MTDPARTS_READ)
loff_t board_mtdparts_offset(uint8_t mtdtype, bool backup)
{
	loff_t offset = 0;
	size_t sbl, metadata;

	switch (mtdtype) {
	case MTD_NORFLASH:
		sbl = CONFIG_LUA_SBL_SIZE_IN_NORFLASH;
		metadata = CONFIG_LUA_METADATA_SIZE_IN_NORFLASH;
		break;
	case MTD_NANDFLASH:
	case MTD_MLCNANDFLASH:
		sbl = CONFIG_LUA_SBL_SIZE_IN_NANDFLASH;
		metadata = CONFIG_LUA_METADATA_SIZE_IN_NANDFLASH;
		break;
	default:
		pr_err("Not supported MTD type\n");
		return -EAGAIN;
	}

	offset = sbl;
#ifdef CONFIG_LUA_AB
	/* A/B slot */
	offset <<= 1;
#endif
	if (backup)
		offset += metadata;

	return offset;
}

void board_mtdparts_part_info(struct blk_desc *dev_desc, bool backup,
                              lbaint_t *part_start, lbaint_t *part_size)
{
	struct mtd_info *mtd = blk_desc_to_mtd(dev_desc);
	size_t metadata_size;
	loff_t offset;

	if (!part_start || !part_size || !mtd) {
		pr_err("Invalid parameter\n");
		return;
	}

	switch (mtd->type) {
	case MTD_NORFLASH:
		metadata_size = CONFIG_LUA_METADATA_SIZE_IN_NORFLASH;
		break;
	case MTD_NANDFLASH:
	case MTD_MLCNANDFLASH:
		metadata_size = CONFIG_LUA_METADATA_SIZE_IN_NANDFLASH;
		break;
	default:
		pr_err("Not supported MTD type\n");
		return;
	}

	offset = board_mtdparts_offset(mtd->type, backup);
	*part_start = offset / dev_desc->blksz;
	*part_size = metadata_size / dev_desc->blksz;
}
#endif

static inline
int mtdpart_name_check(const char *name, const char **part_id, int len)
{
	int i;

	for (i = 0; i < len; i++)
		if (!strcmp(part_id[i], name))
			return i;

	return -1;
}

static __maybe_unused
int splitup_mtdparts(const char *full_mtdparts,
                      const char **main_mtdparts,
                      const char **safe_mtdparts)
{
	const char *p, *np, *comma, *bracket;
	static char substr1[MTDPARTS_LEN];
	static char substr2[MTDPARTS_LEN];
	const char *hashtag = NULL;
	int mtdpart_len;
	int name_len;
	char name[32];
	int ret;

	if (!full_mtdparts || !main_mtdparts || !safe_mtdparts)
		return -EINVAL;

	memset(substr1, 0, MTDPARTS_LEN);
	memset(substr2, 0, MTDPARTS_LEN);
	hashtag = strchr(full_mtdparts, '#');
	if (hashtag)
		p = hashtag + 1;
	else
		p = full_mtdparts;

	while ((comma = strchr(p, ',')) != NULL) {
		mtdpart_len = comma - p + 1;
		bracket = strchr(p, '(');
		np = ++bracket;
		bracket = strchr(np, ')');
		name_len = bracket - np;
		memcpy(name, np, name_len);
		name[name_len] = '\0';

		ret = mtdpart_name_check(name, safe_part_id,
		                        get_safe_part_id_count());
		if (ret >= 0)
			strncat(substr1, p, mtdpart_len);
		else
			strncat(substr2, p, mtdpart_len);
		p += mtdpart_len;
	}

	/* Remove the last comma */
	if (strlen(substr1))
		strrchr(substr1, ',')[0] = '\0';
	if (strlen(substr2))
		strrchr(substr2, ',')[0] = '\0';

	*main_mtdparts = substr2;
	*safe_mtdparts = substr1;
	debug("main mtdparts: %s\n", *main_mtdparts);
	debug("safe mtdparts: %s\n", *safe_mtdparts);

	return 0;
}

static __maybe_unused
int filter_out_mtdparts(const char *full_mtdparts,
                          bool is_main, const char **mtdparts)
{
	const char *p, *np, *comma, *bracket;
	static char substr[MTDPARTS_LEN];
	const char *hashtag = NULL;
	const char **part_id = NULL;
	int part_count;
	int mtdpart_len;
	int name_len;
	char name[32];
	int ret;

	if (!full_mtdparts || !mtdparts) {
		debug("Invaild arguments\n");
		return -EINVAL;
	}

	part_id = is_main ? main_part_id : safe_part_id;
	part_count = is_main ? get_main_part_id_count() :
	                      get_safe_part_id_count();

	memset(substr, 0, MTDPARTS_LEN);
	hashtag = strchr(full_mtdparts, '#');
	if (hashtag)
		p = hashtag + 1;
	else
		p = full_mtdparts;

	while ((comma = strchr(p, ',')) != NULL) {
		mtdpart_len = comma - p + 1;
		bracket = strchr(p, '(');
		np = ++bracket;
		bracket = strchr(np, ')');
		name_len = bracket - np;
		memcpy(name, np, name_len);
		name[name_len] = '\0';

		ret = mtdpart_name_check(name, part_id,
		                         part_count);
		if (ret >= 0)
			strncat(substr, p, mtdpart_len);

		p += mtdpart_len;
	}

	/* Remove the last comma */
	if (strlen(substr))
		strrchr(substr, ',')[0] = '\0';

	*mtdparts = substr;
	debug("Filter out %s mtdparts: %s\n",
	         is_main ? "main" : "safety", *mtdparts);

	return 0;
}

static __maybe_unused
void board_get_mtdparts(const char *dev, const char *partition,
			  char *mtdids, char *mtdparts)
{
	char dev_id[MTDIDS_LEN];
	char dev_prefix[MTDIDS_LEN];

	/* mtdids: "<dev>=<dev>, ...." */
	snprintf(dev_id, sizeof(dev_id), "%s=%s", dev, dev);
	if (!strstr(mtdids, dev_id)) {
		if (mtdids[0] != '\0')
			strcat(mtdids, ",");
		strcat(mtdids, dev_id);
	}

	/* mtdparts: "mtdparts=<dev>:<mtdparts_<dev>>;..." */
	snprintf(dev_prefix, sizeof(dev_prefix), "%s:", dev);
	if (strstr(mtdparts, dev_prefix)) {
		strncat(mtdparts, ",", MTDPARTS_LEN);
		strncat(mtdparts, partition, MTDPARTS_LEN);
	} else {
		if (mtdparts[0] != '\0') {
			strncat(mtdparts, ";", MTDPARTS_LEN);
		} else {
			strcat(mtdparts, "mtdparts=");
		}

		strncat(mtdparts, dev_prefix, MTDPARTS_LEN);
		strncat(mtdparts, partition, MTDPARTS_LEN);
	}
}

#if defined(CONFIG_LUA_MTDPARTS_FROM_MTDBLOCK)
static int __maybe_unused
parse_mtdparts_from_mtdblock(char *mtdids, char *mtdparts)
{
	struct blk_desc *blk_desc0 = NULL;
	struct blk_desc *blk_desc1 = NULL;
	struct blk_desc *found_dev = NULL;
	struct mtd_info *mtd0 = NULL;
	struct mtd_info *mtd1 = NULL;
	struct udevice *dev = NULL;
	const char *main_mtdparts = NULL;
	const char *safe_mtdparts = NULL;
	char *full_mtdparts;
	int main_mtd, safe_mtd;
	int bootdev;
	int ret;

	/* probe all MTD devices */
	uclass_foreach_dev_probe(UCLASS_MTD, dev)
		debug("MTD device name: %s\n", dev->name);

	bootdev = get_bootdevice(NULL);
	if (bootdev < 0) {
		debug("Wrong boot device '%d'\n", bootdev);
		return -EINVAL;
	}

	switch (bootdev) {
	case BOOTDEVICE_BOTH_NOR_NAND:
		main_mtd = env_get_ulong(env_get_name(ENV_MAIN_MTD), 10, ~0UL);
		safe_mtd = env_get_ulong(env_get_name(ENV_SAFE_MTD), 10, ~0UL);

		blk_desc0 = get_blk_by_devnum(IF_TYPE_MTD, safe_mtd);
		blk_desc1 = get_blk_by_devnum(IF_TYPE_MTD, main_mtd);
		if (IS_ERR_OR_NULL(blk_desc0) ||
		    IS_ERR_OR_NULL(blk_desc1)) {
			debug("Unable to get MTD block device\n");
			return -ENXIO;
		}
		found_dev = blk_desc0;
		mtd0 = blk_desc_to_mtd(blk_desc0);
		mtd1 = blk_desc_to_mtd(blk_desc1);
		break;
	case BOOTDEVICE_BOTH_NOR_EMMC:
		safe_mtd = env_get_ulong(env_get_name(ENV_SAFE_MTD), 10, ~0UL);

		blk_desc0 = get_blk_by_devnum(IF_TYPE_MTD, safe_mtd);
		if (IS_ERR_OR_NULL(blk_desc0))
			return -ENXIO;
		found_dev = blk_desc0;
		mtd0 = blk_desc_to_mtd(blk_desc0);
		break;
	case BOOTDEVICE_ONLY_NAND:
		main_mtd = env_get_ulong(env_get_name(ENV_MAIN_MTD), 10, ~0UL);

		blk_desc1 = get_blk_by_devnum(IF_TYPE_MTD, main_mtd);
		if (IS_ERR_OR_NULL(blk_desc1))
			return -ENXIO;
		found_dev = blk_desc1;
		mtd1 = blk_desc_to_mtd(blk_desc1);
		break;
	default:
		debug("Not supported bootdevice '%d'\n", bootdev);
		return -ENOTSUPP;
	}
	debug("MTDPARTS on MTD block device: %s\n", found_dev->bdev->name);

	ret = read_full_mtdparts(found_dev, &full_mtdparts);
	if (ret < 0) {
		debug("Unable to read mtdparts from mtdblock device\n");
		return ret;
	}
	debug("full mtdparts: %s\n", full_mtdparts);

	ret = splitup_mtdparts(full_mtdparts, &main_mtdparts,
	               &safe_mtdparts);
	if (ret < 0) {
		debug("Could not split up mtdparts string\n");
		free(full_mtdparts);
		return ret;
	}

	/* safe partitions on main MTD device */
	if (IS_ERR_OR_NULL(mtd0) && strlen(safe_mtdparts) &&
	    !IS_ERR_OR_NULL(mtd1))
		board_get_mtdparts(mtd1->name, safe_mtdparts,
		                          mtdids, mtdparts);

	/* part of partitions on safe MTD device */
	if (!IS_ERR_OR_NULL(mtd0) && strlen(safe_mtdparts))
	    board_get_mtdparts(mtd0->name, safe_mtdparts,
		                          mtdids, mtdparts);

	/* part of partitions on main MTD device */
	if (!IS_ERR_OR_NULL(mtd1) && strlen(main_mtdparts))
	    board_get_mtdparts(mtd1->name, main_mtdparts,
		                          mtdids, mtdparts);

	if (full_mtdparts)
		free(full_mtdparts);

	return 0;
}
#else
static inline int __maybe_unused
parse_mtdparts_from_mtdblock(char *mtdids, char *mtdparts)
{
	return -ENODATA;
}
#endif /* !CONFIG_LUA_MTDPARTS_FROM_MTDBLOCK */

#if defined(CONFIG_LUA_MTDPARTS_FROM_BUFFER)
static int __maybe_unused
parse_mtdparts_from_buffer(const char *buf, size_t buflen,
                           char *mtdids, char *mtdparts)
{
	struct blk_desc *blk_desc = NULL;
	struct mtd_info *mtd = NULL;
	struct udevice *dev = NULL;
	const char *main_mtdparts = NULL;
	const char *safe_mtdparts = NULL;
	bool safety_on_main = false;
	char *full_mtdparts;
	int main_mtd;
	int bootdev;
	int ret;

	if (!buf || !mtdids || !mtdparts) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	if (buflen < 1) {
		pr_err("Buffer length is too short\n");
		return -EINVAL;
	}

	/* probe all MTD devices */
	uclass_foreach_dev_probe(UCLASS_MTD, dev)
		debug("MTD device name: %s\n", dev->name);


	bootdev = get_bootdevice(NULL);
	if (bootdev < 0) {
		debug("Wrong boot device '%d'\n", bootdev);
		return -EINVAL;
	}

	switch (bootdev) {
	case BOOTDEVICE_BOTH_NOR_NAND:
	case BOOTDEVICE_ONLY_NAND:
		main_mtd = env_get_ulong(env_get_name(ENV_MAIN_MTD), 10, ~0UL);
		blk_desc = get_blk_by_devnum(IF_TYPE_MTD, main_mtd);
		if (IS_ERR_OR_NULL(blk_desc)) {
			debug("Unable to get MTD block device\n");
			return -ENXIO;
		}
		mtd = blk_desc_to_mtd(blk_desc);
		if (bootdev == BOOTDEVICE_ONLY_NAND)
			safety_on_main = true;
		break;
	default:
		debug("Not supported bootdevice '%d'\n", bootdev);
		return -ENOTSUPP;
	}
	debug("MTD block device: %s\n", blk_desc->bdev->name);
	if (IS_ERR_OR_NULL(mtd)) {
		debug("MTD device is not available\n");
		return -ENODEV;
	}

#ifdef DEBUG
	print_hex_dump("MTDPARTS BUFFER: ", DUMP_PREFIX_ADDRESS,
	            16, 64, buf, buflen, true);
#endif

	ret = read_full_mtdparts_from_buffer(buf, buflen, &full_mtdparts);
	if (ret < 0) {
		debug("Unable to read mtdparts from buffer 0x'%p'\n", buf);
		return ret;
	}
	debug("full mtdparts from buffer: %s\n", full_mtdparts);

	ret = filter_out_mtdparts(full_mtdparts, true, &main_mtdparts);
	if (ret < 0) {
		debug("Could not filter out main mtdparts string\n");
		free(full_mtdparts);
		return ret;
	}

	if (mtd && strlen(main_mtdparts))
		board_get_mtdparts(mtd->name, main_mtdparts,
		                          mtdids, mtdparts);

	/* safe partitions on main MTD device */
	if (safety_on_main && mtd) {
		ret = filter_out_mtdparts(full_mtdparts, false,
		                          &safe_mtdparts);
		if (ret < 0) {
			debug("Could not filter out safe mtdparts string\n");
			free(full_mtdparts);
			return ret;
		}
		board_get_mtdparts(mtd->name, safe_mtdparts,
		                          mtdids, mtdparts);
	}

	if (full_mtdparts)
		free(full_mtdparts);

	return 0;
}
#else
static inline int __maybe_unused
parse_mtdparts_from_buffer(const char *buf, size_t buflen,
                           char *mtdids, char *mtdparts)
{
	return -ENODATA;
}
#endif /* !CONFIG_LUA_MTDPARTS_FROM_BUFFER */

void board_mtdparts_default(const char **mtdids, const char **mtdparts)
{
	boot_params_t __maybe_unused *bp = NULL;
	const char __maybe_unused *mtd_parts;
	const char __maybe_unused *mtd_name;
	static char parts[3 * MTDPARTS_LEN + 1];
	static char ids[MTDIDS_LEN + 1];
	static bool mtd_initialized;
	int __maybe_unused bootstate = 0;
	int __maybe_unused ret;

	if (mtd_initialized) {
		*mtdids = ids;
		*mtdparts = parts;
		return;
	}

	memset(parts, 0, sizeof(parts));
	memset(ids, 0, sizeof(ids));

#if defined(CONFIG_LUA_MTDPARTS_READ)
	bootstate = get_bootstate();
	if (bootstate < 0) {
		pr_err("Unknown bootstate\n");
		return;
	}

	if (bootstate == BOOTSTATE_DOWNLOAD) {
		ret = parse_mtdparts_from_mtdblock(ids, parts);
		if (ret < 0) {
			debug("Unable to parse mtdparts from mtdblock device\n");
			return;
		}
	} else {
#if defined(CONFIG_LUA_MTDPARTS_FROM_BUFFER)
		/* read mtdparts from buffer */
		bp = boot_params_get_base();
		if (IS_ERR_OR_NULL(bp)) {
			debug("Wrong boot parameters base address\n");
			return;
		}

		ret = parse_mtdparts_from_buffer((void *)bp->mtdparts, sizeof(bp->mtdparts),
		                           ids, parts);
		if (ret < 0) {
			debug("Unable to parse mtdparts from buffer 0x'%p'\n", bp->mtdparts);
			return;
		}
#elif defined(CONFIG_LUA_MTDPARTS_FROM_MTDBLOCK)
		/* read mtdparts from mtd block device */
		ret = parse_mtdparts_from_mtdblock(ids, parts);
		if (ret < 0) {
			debug("Unable to parse mtdparts from mtdblock device\n");
			return;
		}
#else
		pr_err("Could not parse mtdparts, please check your configuration\n");
		return;
#endif
	}

	mtd_initialized = true;
#endif

#if defined(CONFIG_LUA_SPI_NOR_RUNTIME)
	/* probe all SPI NOR Flash devices */
	uclass_foreach_dev_probe(UCLASS_SPI_FLASH, dev) {
		debug("SPI NOR Flash device = %s\n", dev->name);
	}

#if defined(CONFIG_LUA_NOR0)
	mtd_parts = CONFIG_LUA_MTDPARTS_NOR0;

#if CONFIG_IS_ENABLED(SPI_FLASH_TINY)
	mtd_name = "spi-flash";
#else
	mtd_name = "nor0";
#endif

	mtd = get_mtd_device_nm(mtd_name);
	if (!IS_ERR_OR_NULL(mtd) && strlen(mtd_parts)) {
		board_get_mtdparts(mtd_name, mtd_parts, ids, parts);
		put_mtd_device(mtd);
		mtd_initialized = true;
	}
#endif

#if defined(CONFIG_LUA_NOR1)
	mtd_parts = CONFIG_LUA_MTDPARTS_NOR1;
	mtd_name = "nor1";
	mtd = get_mtd_device_nm(mtd_name);
	if (!IS_ERR_OR_NULL(mtd) && strlen(mtd_parts)) {
		board_get_mtdparts(mtd_name, mtd_parts, ids, parts);
		put_mtd_device(mtd);
		mtd_initialized = true;
	}
#endif
#endif /* CONFIG_LUA_SPI_NOR_RUNTIME */

#if defined(CONFIG_LUA_SPI_NAND_RUNTIME)
	/* probe all SPI NAND Flash devices */
	uclass_foreach_dev_probe(UCLASS_MTD, dev)
		debug("SPI NAND Flash device = %s\n", dev->name);

#if defined(CONFIG_LUA_NAND0)
	mtd_parts = CONFIG_LUA_MTDPARTS_NAND0;

#if CONFIG_IS_ENABLED(CMD_NAND) && CONFIG_IS_ENABLED(MTD_RAW_NAND)
	mtd_name = "nand0";
#else
	mtd_name = "spi-nand0";
#endif
	mtd = get_mtd_device_nm(mtd_name);
	if (!IS_ERR_OR_NULL(mtd) && strlen(mtd_parts)) {
		board_get_mtdparts(mtd_name, mtd_parts, ids, parts);
		put_mtd_device(mtd);
		mtd_initialized = true;
	}
#endif
#endif /* CONFIG_LUA_SPI_NAND_RUNTIME */

	*mtdids = ids;
	*mtdparts = parts;
	debug("%s:\nmtdids   : %s\nmtdparts : %s\n", __func__, ids, parts);
}
