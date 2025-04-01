// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024, Charleye <wangkart@aliyun.com>
 *
 */

#include <common.h>
#include <blk.h>
#include <command.h>
#include <linux/err.h>
#include <dm.h>
#include <mapmem.h>
#if defined(CONFIG_DM_MTD)
#include <mtd.h>
#endif

static int blk_dev = -1;
static int if_devnum = -1;
static enum if_type if_type = IF_TYPE_UNKNOWN;

static void block_display_header(void)
{
	printf(" seq      name                    type     devnum\n");
	printf("------------------------------------------------------\n");
}

static void block_display_line(struct udevice *dev)
{
	struct blk_desc *desc;

	printf("%3i     %c %-16s  ", dev_seq(dev),
	       dev_get_flags(dev) & DM_FLAG_ACTIVATED ? '*' : ' ',
	       dev->name);

	desc = dev_get_uclass_plat(dev);
	printf("      %s         %u",
	         blk_get_if_type_name(desc->if_type),
	         desc->devnum);
	puts("\n");
}

static int do_block(struct cmd_tbl *cmdtp, int flag, int argc,
                        char *const argv[])
{
	struct udevice *blk, *parent;
	struct udevice *dev;
	struct uclass *uc;
	int devnum, id;
	char **params;
	int ret, i = 0;

	if (argc < 2)
		return CMD_RET_USAGE;

	params = malloc(argc * sizeof(char *));
	if (IS_ERR_OR_NULL(params))
		return CMD_RET_FAILURE;
	for (i = 0; i < argc; i++)
		params[i] = strdup(argv[i]);

	switch (argc) {
	case 2:
		if (strncmp(argv[1], "list", 4) == 0) {
			block_display_header();
			uclass_id_foreach_dev(UCLASS_BLK, dev, uc)
				block_display_line(dev);

			return CMD_RET_SUCCESS;
		}
		break;
	case 3:
		ret = CMD_RET_USAGE;
		if (strncmp(argv[1], "inf", 3) == 0)
			devnum = (int)dectoul(argv[2], NULL);
		else if (strncmp(argv[1], "dev", 3) == 0)
			devnum = (int)dectoul(argv[2], NULL);
		else if (strncmp(argv[1], "part", 4) == 0)
			devnum = (int)dectoul(argv[2], NULL);
		else
			goto failed;

		if (!uclass_get_device(UCLASS_BLK, devnum, &blk)) {
			parent = dev_get_parent(blk);
			if (IS_ERR_OR_NULL(parent)) {
				ret = CMD_RET_FAILURE;
				goto failed;
			}
			id = device_get_uclass_id(parent);
			switch (id) {
			case UCLASS_MMC:
				if_type = IF_TYPE_MMC;
				break;
			case UCLASS_MTD:
				if_type = IF_TYPE_MTD;
				break;
			default:
				debug("Not supported uclass_id (%d)\n", id);
				ret = CMD_RET_FAILURE;
				goto failed;
			}
			blk_dev = devnum;
			if_devnum = dev_seq(parent);

			if (strncmp(argv[1], "inf", 3) == 0) {
				free(params[1]);
				params[1] = strdup("dev");
			}
			params[2] = strdup(simple_itoa(if_devnum));
			printf("%s device %d is now current block\n",
			            blk_get_if_type_name(if_type), if_devnum);
		} else {
			ret = CMD_RET_FAILURE;
			goto failed;
		}
	}

	if (blk_dev < 0 || !if_type) {
		printf("\n ** Invaild device number or interface type **\n\n");
		ret = CMD_RET_USAGE;
		goto failed;
	}

	switch ((int)if_type) {
	case IF_TYPE_MTD:
#if defined(CONFIG_DM_MTD)
		mtd_probe_devices();
#endif
	/* fall through */
	case IF_TYPE_MMC:
		ret = blk_common_cmd(argc, params, if_type, &if_devnum);
		break;
	default:
		ret = CMD_RET_FAILURE;
		debug("Not supported if_type (%d)", if_type);
	}

failed:
	for (i = 0; i < argc; i++)
		free(params[i]);
	free(params);
	return ret;
}

U_BOOT_CMD(
	block, 8, 1, do_block,
	"block devices sub-system",
	"list - list all available block devices\n"
	"block info [dev] - show one or all devices of a specified if_type\n"
	"block dev [dev] - show or set current device of a specified if_type\n"
	"block part [dev] - print partition table for one or all devices of a specified if_type\n"
	"block read addr blk# cnt - read `cnt' blocks starting at block\n"
	"     `blk#' to memory address `addr'\n"
	"block write addr blk# cnt - write `cnt' blocks starting at block\n"
	"     `blk#' from memory address `addr'\n"
	"block erase blk# cnt - erase `cnt' blocks starting at block `blk#'\n"
);