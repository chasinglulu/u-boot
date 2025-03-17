// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 * managing A/B metadata
 */

#include <common.h>
#include <command.h>
#include <log.h>
#include <part.h>
#include <memalign.h>
#include <u-boot/crc.h>
#include <linux/err.h>
#include <ctype.h>

#include <android_bootloader_message.h>

#define MAX_SLOTS      4

static int abc_dev                   = -1;
static int abc_part                  = -1;
static struct blk_desc *abc_dev_desc           = NULL;
static struct disk_partition abc_part_info     = { 0 };
static struct bootloader_control abc_metadata  = { 0 };

static int abc_store(struct bootloader_control *abc)
{
	ulong abc_offset, abc_blocks;
	char buf[sizeof(struct bootloader_message_ab)];
	uint32_t crc32_le;
	ulong abc_pos;
	int ret;

	if (unlikely(!abc)) {
		pr_err("Invalid A/B metadata\n");
		return -EINVAL;
	}

	if (abc_dev < 0 || abc_part < 0) {
		pr_err("Invalid device or partition\n");
		return -EINVAL;
	}

	abc_pos = offsetof(struct bootloader_message_ab, slot_suffix);
	if (abc_pos >= abc_part_info.blksz)
		abc_offset = abc_pos / abc_part_info.blksz;
	else
		abc_offset = 0;
	abc_blocks = DIV_ROUND_UP(sizeof(struct bootloader_control), abc_part_info.blksz);

	ret = blk_dread(abc_dev_desc, abc_part_info.start + abc_offset,
	                            abc_blocks, buf);
	if (IS_ERR_VALUE(ret)) {
		pr_err("Could not read from the misc partition\n");
		return -EIO;
	}

	crc32_le = crc32(0, (void *)abc, offsetof(typeof(*abc), crc32_le));
	abc->crc32_le = crc32_le;

	if (abc_offset == 0)
		memcpy(buf + abc_pos, abc, sizeof(struct bootloader_control));
	else
		memcpy(buf, abc, sizeof(struct bootloader_control));

	ret = blk_dwrite(abc_dev_desc, abc_part_info.start + abc_offset,
	                    abc_blocks, buf);
	if (IS_ERR_VALUE(ret)) {
		pr_err("Could not write back the misc partition\n");
		return -EIO;
	}

	return 0;
}

static int abc_load_from_blk(struct blk_desc *dev_desc, struct disk_partition *part_info)
{
	ulong abc_offset, abc_blocks, abc_pos;
	char buf[sizeof(struct bootloader_message_ab)];
	int ret;

	abc_pos = offsetof(struct bootloader_message_ab, slot_suffix);
	if (abc_pos >= part_info->blksz)
		abc_offset = abc_pos / part_info->blksz;
	else
		abc_offset = 0;

	abc_blocks = DIV_ROUND_UP(sizeof(struct bootloader_control), part_info->blksz);
	if (unlikely(abc_blocks > part_info->size)) {
		pr_err("partition size too small\n");
		ret = -EINVAL;
		goto err;
	}

	ret = blk_dread(dev_desc, part_info->start + abc_offset,
					abc_blocks, buf);
	if (ret != abc_blocks) {
		pr_err("failed to read A/B metadata from mmc %d#%s\n", dev_desc->devnum, part_info->name);
		ret = -EIO;
		goto err;
	}

	if (abc_offset == 0)
		memcpy(&abc_metadata, buf + abc_pos, sizeof(struct bootloader_control));
	else
		memcpy(&abc_metadata, buf, sizeof(struct bootloader_control));

	abc_dev = dev_desc->devnum;
	printf("Successfully load A/B metadata from %s %d:%d (%s)\n",
	             blk_get_if_type_name(dev_desc->if_type),
	             abc_dev, abc_part, part_info->name);

	return 0;

err:
	abc_dev = -1;
	abc_part = -1;
	return ret;
}

static int abc_validate(struct bootloader_control *abc)
{
	uint32_t crc32_le;
	uint32_t len;

	len = offsetof(typeof(*abc), crc32_le);
	crc32_le = crc32(0, (void *)abc, len);
	if (crc32_le != abc->crc32_le) {
		pr_err("Invalid CRC32 (expected %.8x, found %.8x)\n",
		       crc32_le, abc->crc32_le);
		return -EINVAL;
	}

	if (abc->magic != BOOT_CTRL_MAGIC) {
		pr_err("Invalid A/B metadata magic\n");
		return -ENODATA;
	}

	if (abc->version > BOOT_CTRL_VERSION) {
		pr_err("Unsupported A/B metadata version\n");
		return -ENODATA;
	}

	if (abc->nb_slot > MAX_SLOTS) {
		pr_err("Invalid number of slots\n");
		return -EINVAL;
	}

	return 0;
}

static int abc_load(struct blk_desc *dev_desc, struct disk_partition *part_info)
{
	int ret;

	if (abc_dev_desc == NULL || abc_part == -1) {
		pr_err("Invalid device descriptor or partition infomation\n");
		return -ENODATA;
	}

	ret = abc_load_from_blk(dev_desc, part_info);
	if (ret < 0) {
		pr_err("Failed to load A/B metadata from %s %d#%s\n",
		                 blk_get_if_type_name(dev_desc->if_type),
		                 dev_desc->devnum, part_info->name);
		return ret;
	}

	ret = abc_validate(&abc_metadata);
	if (ret < 0) {
		pr_err("Invalid A/B metadata\n");
		return ret;
	}

	return 0;
}

static int do_abc_load(struct cmd_tbl *cmdtp, int flag, int argc,
                       char * const argv[])
{
	int ret;

	if (argc != 2) {
		pr_err("Invalid number of arguments\n");
		goto err;
	}

	/* Lookup the "misc" partition */
	ret = part_get_info_by_dev_and_name_or_num(argv[0], argv[1],
	      &abc_dev_desc, &abc_part_info,
	      false);
	if (ret < 0) {
		pr_err("Failed to get A/B metadata partition information\n");
		goto err;
	}
	abc_part = ret;

	ret = abc_load(abc_dev_desc, &abc_part_info);
	if (ret < 0) {
		pr_err("Failed to load A/B metadata from <%s> <%s>\n", argv[0], argv[1]);
		goto err;
	}

	return CMD_RET_SUCCESS;

err:
	abc_dev_desc = NULL;
	memset(&abc_part_info, 0, sizeof(abc_part_info));
	abc_dev = -1;
	abc_part = -1;

	return CMD_RET_FAILURE;
}

static int abc_get_active_slot(void)
{
	int i, slot;

	if (abc_dev == -1 || abc_part == -1) {
		pr_err("\n ** No loaded A/B metadata **\n\n");
		return -ENODATA;
	}

	for (i = 0; i < MAX_SLOTS; i++) {
		if (abc_metadata.slot_suffix[i])
			break;
	}

	if (i == MAX_SLOTS)
		return -EINVAL;

	slot = toupper(abc_metadata.slot_suffix[i]) - 'A';

	return slot;
}

static int do_abc_save(struct cmd_tbl *cmdtp, int flag, int argc,
                       char * const argv[])
{
	int ret;

	if (argc != 2) {
		pr_err("Invalid number of arguments\n");
		goto err;
	}

	ret = part_get_info_by_dev_and_name_or_num(argv[0], argv[1],
	      &abc_dev_desc, &abc_part_info,
	      false);
	if (ret < 0) {
		pr_err("Failed to get partition information\n");
		goto err;
	}
	abc_part = ret;
	abc_dev = abc_dev_desc->devnum;

	ret = abc_store(&abc_metadata);
	if (ret < 0) {
		pr_err("Failed to store A/B metadata into <%s> <%s>\n", argv[0], argv[1]);
		goto err;
	}

	printf("Successfully store A/B metadata into %s %d:%d (%s)\n", argv[0], abc_dev, abc_part, abc_part_info.name);
	return CMD_RET_SUCCESS;

err:
	abc_dev_desc = NULL;
	memset(&abc_part_info, 0, sizeof(abc_part_info));
	abc_dev = -1;
	abc_part = -1;

	return CMD_RET_FAILURE;
}

static int do_abc_get_slots(struct cmd_tbl *cmdtp, int flag, int argc,
                            char * const argv[])
{
	if (abc_dev == -1 || abc_part == -1) {
		pr_err("\n ** No loaded A/B metadata **\n\n");
		return CMD_RET_USAGE;
	}

	printf("%d\n", abc_metadata.nb_slot);
	return CMD_RET_SUCCESS;
}

static int do_abc_get_cur_slot(struct cmd_tbl *cmdtp, int flag, int argc,
                               char * const argv[])
{
	int slot;

	slot = abc_get_active_slot();
	if (slot < 0)
		return CMD_RET_FAILURE;

	printf("Current active slot: %c\n", 'A' + slot);
	return CMD_RET_SUCCESS;
}

static int abc_prepare_slot(int argc, char * const argv[])
{
	char token;
	int slot, ret;

	if (abc_dev == -1 || abc_part == -1) {
		pr_err("\n ** No loaded A/B metadata **\n\n");
		return -ENODATA;
	}

	ret = abc_load(abc_dev_desc, &abc_part_info);
	if (ret < 0) {
		pr_err("Failed to load A/B metadata\n");
		return ret;
	}

	if (argc > 0) {
		token = toupper(argv[0][0]);
		if (!isalnum(token) || (token != '0' && token != '1' &&
		     token != 'A' && token != 'B') ) {
			pr_err("Invalid SLOT\n");
			return -EINVAL;
		}
		slot = isdigit(token) ? token - '0' : token - 'A';
	} else {
		slot = abc_get_active_slot();
		if (slot < 0)
			return slot;
	}

	return slot;
}

static int do_abc_mark_successful(struct cmd_tbl *cmdtp, int flag, int argc,
                       char * const argv[])
{
	struct bootloader_control *metadata = &abc_metadata;
	struct slot_metadata *slotp;
	int slot;

	slot = abc_prepare_slot(argc, argv);
	if (slot < 0)
		return CMD_RET_FAILURE;

	slotp = &metadata->slot_info[slot];
	slotp->successful_boot = 1;
	slotp->tries_remaining = 7;

	if (abc_store(metadata) < 0) {
		pr_err("Failed to store A/B metadata with successful mark\n");
		return CMD_RET_FAILURE;
	}

	printf("Slot %c marked as successful\n", 'A' + slot);
	return CMD_RET_SUCCESS;
}

static int do_abc_set_active_boot(struct cmd_tbl *cmdtp, int flag, int argc,
                                  char * const argv[])
{
	struct bootloader_control *metadata = &abc_metadata;
	struct slot_metadata *slotp;
	int slot, slot1;

	slot = abc_prepare_slot(argc, argv);
	if (slot < 0)
		return CMD_RET_FAILURE;

	if (unlikely(slot > 2)) {
		pr_err("Wrong slot value\n");
		return -EINVAL;
	}

	slotp = &metadata->slot_info[slot];
	slotp->priority = 15;
	slotp->tries_remaining = 7;
	slotp->successful_boot = 0;

	slot1 = slot ? 0 : 1;
	slotp = &metadata->slot_info[slot1];
	if(slotp->priority == 15)
		slotp->priority = 15 - 1;

	if (abc_store(metadata) < 0) {
		pr_err("Failed to store A/B metadata with active slot setting\n");
		return CMD_RET_FAILURE;
	}

	pr_err("Slot %c marked as next active \n", 'A' + slot);
	return CMD_RET_SUCCESS;
}

static int do_abc_set_unbootable(struct cmd_tbl *cmdtp, int flag, int argc,
                                 char * const argv[])
{
	struct bootloader_control *metadata = &abc_metadata;
	struct slot_metadata *slotp;
	int slot;

	slot = abc_prepare_slot(argc, argv);
	if (slot < 0)
		return CMD_RET_FAILURE;

	slotp = &metadata->slot_info[slot];
	slotp->successful_boot = 0;
	slotp->priority = 0;
	slotp->tries_remaining = 0;

	if (abc_store(metadata) < 0) {
		pr_err("Failed to store A/B metadata with unbootable setting\n");
		return CMD_RET_FAILURE;
	}

	printf("Slot %c marked as unbootable\n", 'A' + slot);
	return CMD_RET_SUCCESS;
}

static int do_abc_check_bootable(struct cmd_tbl *cmdtp, int flag, int argc,
                                 char * const argv[])
{
	struct bootloader_control *metadata = &abc_metadata;
	struct slot_metadata *slotp;
	int slot;

	slot = abc_prepare_slot(argc, argv);
	if (slot < 0)
		return CMD_RET_FAILURE;

	if (slot > 2) {
		pr_err("Wrong slot value\n");
		return -EINVAL;
	}

	slotp = &metadata->slot_info[slot];

	printf("Slot %c marked as %s\n", 'A' + slot, slotp->priority != 0 ? "bootable" : "unbootable");
	return CMD_RET_SUCCESS;
}

static int do_abc_check_bootup_status(struct cmd_tbl *cmdtp, int flag, int argc,
                               char * const argv[])
{
	struct bootloader_control *metadata = &abc_metadata;
	struct slot_metadata *slotp;
	int slot;

	slot = abc_prepare_slot(argc, argv);
	if (slot < 0)
		return CMD_RET_FAILURE;

	if (slot > 2) {
		pr_err("Wrong slot value\n");
		return -EINVAL;
	}

	slotp = &metadata->slot_info[slot];

	printf("Slot %c marked as %s\n", 'A' + slot, slotp->successful_boot ? "suceessful" : "unsuccessful");
	return CMD_RET_SUCCESS;
}

static int do_abc_get_suffix(struct cmd_tbl *cmdtp, int flag, int argc,
                             char * const argv[])
{
	static const char* suffix[2] = {"_a", "_b"};
	int slot;

	slot = abc_prepare_slot(argc, argv);
	if (slot < 0)
		return CMD_RET_FAILURE;

	if (unlikely(slot > 2)) {
		pr_err("Wrong SLOT\n");
		return -EINVAL;
	}

	if (argc > 1 && argv[1])
		env_set(argv[1], suffix[slot]);

	printf("%s\n", suffix[slot]);
	return CMD_RET_SUCCESS;
}

static struct cmd_tbl abc_commands[] = {
	U_BOOT_CMD_MKENT(load, CONFIG_SYS_MAXARGS, 1, do_abc_load, "", ""),
	U_BOOT_CMD_MKENT(save, CONFIG_SYS_MAXARGS, 1, do_abc_save, "", ""),
	U_BOOT_CMD_MKENT(get-number-slots, 1, 1, do_abc_get_slots, "", ""),
	U_BOOT_CMD_MKENT(get-current-slot, 1, 1, do_abc_get_cur_slot, "", ""),
	U_BOOT_CMD_MKENT(mark-boot-successful, 1, 1, do_abc_mark_successful, "", ""),
	U_BOOT_CMD_MKENT(set-active-boot-slot, 2, 1, do_abc_set_active_boot, "", ""),
	U_BOOT_CMD_MKENT(set-slot-as-unbootable, 2, 1, do_abc_set_unbootable, "", ""),
	U_BOOT_CMD_MKENT(is-slot-bootable, 2, 1, do_abc_check_bootable, "", ""),
	U_BOOT_CMD_MKENT(is-slot-marked-successful, 2, 1, do_abc_check_bootup_status, "", ""),
	U_BOOT_CMD_MKENT(get-suffix, 2, 1, do_abc_get_suffix, "", ""),
};

static int do_abc(struct cmd_tbl *cmdtp, int flag, int argc,
		   char *const argv[])
{
	struct cmd_tbl *abc_cmd;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	abc_cmd = find_cmd_tbl(argv[1], abc_commands,
				ARRAY_SIZE(abc_commands));
	argc -= 2;
	argv += 2;


	if ((!abc_cmd || argc > abc_cmd->maxargs))
		return CMD_RET_USAGE;

	ret = abc_cmd->cmd(abc_cmd, flag, argc, argv);
	if (ret < 0) {
		pr_err("Command failed with error %d\n", ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	abc, CONFIG_SYS_MAXARGS, 1, do_abc,
	"Manage A/B metadata",
	"load <interface> <dev[:part|#part_name]>\n"
	"    - load A/B metadata from 'part' on device type 'interface' instance 'dev'\n"
	"save <interface> <dev[:part|#part_name]>\n"
	"    - save A/B metadata to 'part' on device type 'interface' instance 'dev'\n"
	"abc get-number-slots\n"
	"    - print number of slots\n"
	"abc get-current-slot\n"
	"    - print currently running SLOT\n"
	"abc mark-boot-successful [SLOT]\n"
	"    - mark current or <SLOT> slot as GOOD\n"
	"abc set-active-boot-slot [SLOT]\n"
	"    - On next boot, load and execute SLOT\n"
	"abc set-slot-as-unbootable [SLOT]\n"
	"    - mark SLOT as invalid\n"
	"abc is-slot-bootable [SLOT]\n"
	"    - returns 0 only if SLOT is bootable\n"
	"abc is-slot-marked-successful [SLOT]\n"
	"    - returns 0 only if SLOT is marked GOOD\n"
	"abc get-suffix [SLOT] [varname]\n"
	"    - prints suffix for SLOT and store the slot suffix in the 'varname' variable\n"
);
