// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 */

#include <android_bootloader_message.h>
#include <common.h>
#include <command.h>
#include <log.h>
#include <part.h>
#include <memalign.h>
#include <u-boot/crc.h>
#include <linux/err.h>

#define MAX_SLOTS    4

static int bootctrl_dev = -1;
static int bootctrl_part = -1;
static int bootctrl_cur_slot = -1;
static struct bootloader_control bootctrl __aligned(ARCH_DMA_MINALIGN) = { 0 };

static int __bootctrl_load_mmc(int devnum, const char *partp)
{
	struct blk_desc *desc;
	struct disk_partition info;
	u64 cnt, offset;
	char *endp;
	int part, ret;

	desc = blk_get_devnum_by_type(IF_TYPE_MMC, devnum);
	if (!desc) {
		ret = -ENODEV;
		goto err_read_fail;
	}

	part = simple_strtoul(partp, &endp, 0);
	if (*endp == '\0') {
		ret = part_get_info(desc, part, &info);
		if (ret)
			goto err_read_fail;
	} else {
		part = part_get_info_by_name(desc, partp, &info);
		if (part < 0) {
			ret = part;
			goto err_read_fail;
		}
	}

	cnt = DIV_ROUND_UP(sizeof(struct bootloader_control), info.blksz);
	if (cnt > info.size)
		goto err_too_small;

	offset = DIV_ROUND_UP(sizeof(struct bootloader_message), info.blksz);

	if (blk_dread(desc, info.start + offset,
					cnt, &bootctrl) != cnt) {
		ret = -EIO;
		goto err_read_fail;
	}

	bootctrl_dev = desc->devnum;
	bootctrl_part = part;
	debug("%s: Loaded from mmc %d:%d\n", __func__, bootctrl_dev, bootctrl_part);

	return CMD_RET_SUCCESS;
err_read_fail:
	printf("Error: mmc %d:%s read failed (%d)\n", devnum, partp, ret);
	goto err;
err_too_small:
	printf("Error: mmc %d:%s too small!", devnum, partp);
	goto err;
err:
	bootctrl_dev = -1;
	bootctrl_part = -1;

	return CMD_RET_FAILURE;
}

static int do_bootctrl_load(struct cmd_tbl *cmdtp, int flag, int argc,
		       char * const argv[])
{
	char *endp;
	int devnum = simple_strtoul(argv[1], &endp, 0);

	if (*endp != '\0') {
		printf("Error: Device id '%s' not a number\n", argv[1]);
		return CMD_RET_FAILURE;
	}

	return __bootctrl_load_mmc(devnum, argv[2]);
}

static int do_bootctrl_get_slots(struct cmd_tbl *cmdtp, int flag, int argc,
		       char * const argv[])
{
	if (bootctrl_dev == -1 || bootctrl_part == -1)
		return CMD_RET_USAGE;

	printf("%d\n", bootctrl.nb_slot);
	return CMD_RET_SUCCESS;
}

static int do_bootctrl_get_cur_slot(struct cmd_tbl *cmdtp, int flag, int argc,
		       char * const argv[])
{
	int i;

	if (bootctrl_dev == -1 || bootctrl_part == -1)
		return CMD_RET_USAGE;

	for (i = 0; i < MAX_SLOTS; i++) {
		if (bootctrl.slot_suffix[i])
			break;
	}
	bootctrl_cur_slot = i;

	printf("current slot: %d\n", bootctrl_cur_slot);
	return 0;
}

static int ab_control_store(struct bootloader_control *abc)
{
	ulong abc_offset, abc_blocks;
	int ret;
	struct blk_desc *dev_desc;
	struct disk_partition info;

	dev_desc = blk_get_devnum_by_type(IF_TYPE_MMC, bootctrl_dev);
	if (!dev_desc) {
		log_err("failed to get block[%d] descriptor\n", bootctrl_dev);
		return -ENODEV;
	}

	ret = part_get_info(dev_desc, bootctrl_part, &info);
	if (ret) {
		log_err("failed to get partition[%d] information\n", bootctrl_part);
		return ret;
	}

	abc_offset = offsetof(struct bootloader_message_ab, slot_suffix) /info.blksz;
	abc_blocks = DIV_ROUND_UP(sizeof(struct bootloader_control), info.blksz);
	ret = blk_dwrite(dev_desc, info.start + abc_offset, abc_blocks,
			 abc);
	if (IS_ERR_VALUE(ret)) {
		log_err("Could not write back the misc partition\n");
		return -EIO;
	}

	return 0;
}

static int do_bootctrl_mark(struct cmd_tbl *cmdtp, int flag, int argc,
		       char * const argv[])
{
	struct bootloader_control *bc = &bootctrl;
	struct slot_metadata *meta = &bc->slot_info[bootctrl_cur_slot];

	if (bootctrl_dev == -1 || bootctrl_part == -1 ||
		bootctrl_cur_slot == -1)
		return CMD_RET_USAGE;

	meta->successful_boot = 1;

	bc->crc32_le = crc32(0, (void *)bc, offsetof(typeof(*bc), crc32_le));

	if (ab_control_store(bc) < 0) {
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int do_bootctrl_set_active_boot(struct cmd_tbl *cmdtp, int flag, int argc,
		       char * const argv[])
{
	return 0;
}

static int do_bootctrl_set_unbootable(struct cmd_tbl *cmdtp, int flag, int argc,
		       char * const argv[])
{
	return 0;
}

static int do_bootctrl_check_bootable(struct cmd_tbl *cmdtp, int flag, int argc,
		       char * const argv[])
{
	return 0;
}

static int do_bootctrl_check_marked(struct cmd_tbl *cmdtp, int flag, int argc,
		       char * const argv[])
{
	return 0;
}

static int do_bootctrl_get_suffix(struct cmd_tbl *cmdtp, int flag, int argc,
		       char * const argv[])
{
	return 0;
}

static struct cmd_tbl bootctrl_commands[] = {
	U_BOOT_CMD_MKENT(load, CONFIG_SYS_MAXARGS, 1, do_bootctrl_load, "", ""),
	U_BOOT_CMD_MKENT(get-number-slots, 1, 1, do_bootctrl_get_slots, "", ""),
	U_BOOT_CMD_MKENT(get-current-slot, 1, 1, do_bootctrl_get_cur_slot, "", ""),
	U_BOOT_CMD_MKENT(mark-boot-successful, 1, 1, do_bootctrl_mark, "", ""),
	U_BOOT_CMD_MKENT(set-active-boot-slot, 2, 1, do_bootctrl_set_active_boot, "", ""),
	U_BOOT_CMD_MKENT(set-slot-as-unbootable, 2, 1, do_bootctrl_set_unbootable, "", ""),
	U_BOOT_CMD_MKENT(is-slot-bootable, 2, 1, do_bootctrl_check_bootable, "", ""),
	U_BOOT_CMD_MKENT(is-slot-marked-successful, 2, 1, do_bootctrl_check_marked, "", ""),
	U_BOOT_CMD_MKENT(get-suffix, 2, 1, do_bootctrl_get_suffix, "", ""),
};

static int do_bootctrl(struct cmd_tbl *cmdtp, int flag, int argc,
		   char *const argv[])
{
	struct cmd_tbl *bootctrl_cmd;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	bootctrl_cmd = find_cmd_tbl(argv[1], bootctrl_commands,
				ARRAY_SIZE(bootctrl_commands));
	argc -= 2;
	argv += 2;


	if ((!bootctrl_cmd || argc > bootctrl_cmd->maxargs))
		return CMD_RET_USAGE;

	ret = bootctrl_cmd->cmd(bootctrl_cmd, flag, argc, argv);

	return cmd_process_error(bootctrl_cmd, ret);
}

U_BOOT_CMD(
	bootctrl, CONFIG_SYS_MAXARGS, 1, do_bootctrl,
	"Manage A/B metadata",
	"load <interface> [<dev[:part]>\n"
	"    - Load bootctrl from <dev> on <interface>\n"
	"bootctrl get-number-slots\n"
	"    - Print number of slots\n"
	"bootctrl get-current-slot\n"
	"    - Print currently running SLOT\n"
	"bootctrl mark-boot-successful\n"
	"    - Mark current slot as GOOD\n"
	"bootctrl set-active-boot-slot SLOT\n"
	"    - On next boot, load and execute SLOT\n"
	"bootctrl set-slot-as-unbootable SLOT\n"
	"    - Mark SLOT as invalid\n"
	"bootctrl is-slot-bootable SLOT\n"
	"    - Returns 0 only if SLOT is bootable\n"
	"bootctrl is-slot-marked-successful SLOT\n"
	"    - Returns 0 only if SLOT is marked GOOD\n"
	"bootctrl get-suffix SLOT\n"
	"    - Prints suffix for SLOT\n"
);
