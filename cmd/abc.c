/*
 * SPDX-License-Identifier: GPL-2.0+
 *
 * managing A/B metadata
 *
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 */

#include <common.h>
#include <command.h>
#include <log.h>
#include <part.h>
#include <memalign.h>
#include <u-boot/crc.h>
#include <linux/err.h>
#include <ctype.h>
#include <dm/device.h>

#include <android_ab.h>
#include <android_bootloader_message.h>

#define SLOT_NAME(slot_num)          ('A' + (slot_num))
#define MAX_SLOTS                    4

static int abc_dev                   = -1;
static int abc_part                  = -1;
static struct blk_desc *abc_dev_desc           = NULL;
static struct disk_partition abc_part_info     = { 0 };
static struct bootloader_control abc_metadata  = { 0 };
static struct bootloader_message_ab bl_msg_ab  = { 0 };
static bool abc_metadata_loaded = false;
static bool need_stored = false;

__weak int abc_board_setup(enum ab_slot_mark type, int slot)
{
	/* Overridden by board-specific code */
	return 0;
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

static int abc_load_from_disk(const char *ifname, const char *dev_part_str)
{
	struct bootloader_control *abc = NULL;
	struct blk_desc *dev_desc = NULL;
	struct disk_partition part_info = { 0 };
	int part = -1;
	int ret;

	/* Lookup the "misc" partition */
	ret = part_get_info_by_dev_and_name_or_num(ifname, dev_part_str,
	      &dev_desc, &part_info,
	      false);
	if (ret < 0) {
		pr_err("Unable to get device and partition information\n");
		return ret;
	}
	part = ret;

	ret = ab_control_create_from_disk(dev_desc, &part_info, &abc);
	if (ret < 0) {
		pr_err("Could not load A/B metadata from '%s %s'\n",
		                             ifname, dev_part_str);
		return ret;
	}

	ret = abc_validate(abc);
	if (ret < 0) {
		free(abc);
		pr_err("Unable to validate A/B metadata\n");
		return ret;
	}

	memcpy(&abc_metadata, abc, sizeof(abc_metadata));
	abc_metadata_loaded = true;
	free(abc);

	abc_dev_desc = dev_desc;
	abc_part_info = part_info;
	abc_part = part;
	abc_dev = dev_desc->devnum;

	printf("A/B metadata loaded from '%s %s' successfully.\n",
	                             ifname, dev_part_str);
	return 0;
}

static int abc_load_from_buffer(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(struct bootloader_message_ab, buffer, 1);
	ALLOC_CACHE_ALIGN_BUFFER(struct bootloader_control, abc, 1);
	int ret;

	ret = bootloader_message_ab_load(buffer);
	if (ret < 0) {
		pr_err("Could not load AB-specific bootloader message.\n");
		return ret;
	}

	ret = ab_control_create_from_buffer(buffer, abc);
	if (ret < 0) {
		pr_err("Unable to create A/B metadata from buffer\n");
		return ret;
	}

	ret = abc_validate(abc);
	if (ret < 0) {
		pr_err("Unable to validate A/B metadata from buffer\n");
		return ret;
	}

	memcpy(&bl_msg_ab, buffer, sizeof(bl_msg_ab));
	memcpy(&abc_metadata, abc, sizeof(abc_metadata));
	abc_metadata_loaded = true;
	printf("A/B metadata loaded from buffer successfully.\n");
	return 0;
}

static int do_abc_load(struct cmd_tbl *cmdtp, int flag, int argc,
                       char * const argv[])
{
	int ret;

	if (argc != 0 && argc != 2) {
		pr_err("Invalid number of arguments\n");
		return CMD_RET_USAGE;
	}

	switch (argc) {
	case 0:
		ret = abc_load_from_buffer();
		break;
	case 2:
		ret = abc_load_from_disk(argv[0], argv[1]);
		break;
	}

	if (ret < 0) {
		pr_err("Failed to load A/B metadata\n");
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int abc_store_into_disk(struct blk_desc *dev_desc,
                               struct disk_partition *part_info,
                               struct bootloader_control *abc)
{
	int ret;

	abc->crc32_le = crc32(0, (void *)abc, offsetof(typeof(*abc), crc32_le));
	ret = ab_control_store(dev_desc, part_info, abc);
	if (ret < 0) {
		pr_err("Could not store A/B metadata into partition '%s' on '%s' device\n",
		       part_info->name, dev_desc->bdev->name);
		return ret;
	}

	printf("A/B metadata stored into partition '%s' on '%s' device successfully.\n",
	                 part_info->name, dev_desc->bdev->name);
	return 0;
}

static int abc_store_with_dev_part_str(const char *ifname,
								 const char *dev_part_str,
								 struct bootloader_control *abc)
{
	struct blk_desc *dev_desc = NULL;
	struct disk_partition part_info = { 0 };
	int ret;

	ret = part_get_info_by_dev_and_name_or_num(ifname, dev_part_str,
	      &dev_desc, &part_info,
	      false);
	if (ret < 0) {
		pr_err("Unable to get partition information\n");
		return ret;
	}

	ret = abc_store_into_disk(dev_desc, &part_info, abc);
	if (ret < 0) {
		pr_err("Failed to store A/B metadata into '%s %s'\n",
		       ifname, dev_part_str);
		return ret;
	}

	printf("A/B metadata stored into '%s %s' successfully.\n",
	       ifname, dev_part_str);
	return 0;
}

static int abc_store_into_buffer(struct bootloader_message_ab *buffer,
                                 struct bootloader_control *abc)
{
	int ret;

	abc->crc32_le = crc32(0, (void *)abc, offsetof(typeof(*abc), crc32_le));
	ret = ab_control_store_into_buffer(buffer, abc);
	if (ret < 0) {
		pr_err("Unable to store A/B metadata into buffer\n");
		return ret;
	}

	ret = bootloader_message_ab_store(buffer);
	if (ret < 0) {
		pr_err("Could not store AB-specific bootloader message.\n");
		return ret;
	}

	printf("A/B metadata stored into buffer successfully.\n");
	return 0;
}

static int do_abc_store(struct cmd_tbl *cmdtp, int flag, int argc,
                       char * const argv[])
{
	int ret;

	if (argc != 0 && argc != 2) {
		pr_err("Invalid number of arguments\n");
		return CMD_RET_USAGE;
	}

	if (!abc_metadata_loaded) {
		pr_err("No A/B metadata loaded. Please load it first.\n");
		return CMD_RET_FAILURE;
	}

	switch (argc) {
	case 0:
		if (abc_part < 0 || abc_dev < 0)
			ret = abc_store_into_buffer(&bl_msg_ab, &abc_metadata);
		else
			ret = abc_store_into_disk(abc_dev_desc, &abc_part_info,
			                      &abc_metadata);
		break;
	case 2:
		ret = abc_store_with_dev_part_str(argv[0], argv[1],
		                       &abc_metadata);
		break;
	}

	if (ret < 0) {
		pr_err("Failed to store A/B metadata\n");
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int abc_get_active_slot(void)
{
	int i, slot;

	if (!abc_metadata_loaded) {
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

static int do_abc_get_number(struct cmd_tbl *cmdtp, int flag, int argc,
                            char * const argv[])
{
	if (!abc_metadata_loaded) {
		pr_err("\n ** No loaded A/B metadata **\n\n");
		return CMD_RET_USAGE;
	}

	printf("%d\n", abc_metadata.nb_slot);
	return CMD_RET_SUCCESS;
}

static int do_abc_get_current(struct cmd_tbl *cmdtp, int flag, int argc,
                               char * const argv[])
{
	int slot;

	slot = abc_get_active_slot();
	if (slot < 0)
		return CMD_RET_FAILURE;

	printf("Current active slot: %c\n", SLOT_NAME(slot));
	return CMD_RET_SUCCESS;
}

static int abc_prepare_slot(int argc, char * const argv[])
{
	char token;
	int slot;

	if (!abc_metadata_loaded) {
		pr_err("\n ** No loaded A/B metadata **\n\n");
		return -ENODATA;
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

static int do_abc_dump(struct cmd_tbl *cmdtp, int flag, int argc,
                       char * const argv[])
{
	struct bootloader_control *metadata = &abc_metadata;
	struct slot_metadata *slotp;
	int i;

	if (!abc_metadata_loaded) {
		pr_err("\n ** No loaded A/B metadata **\n\n");
		return -ENODATA;
	}

	printf("Slot Info:\n");
	for (i = 0; i < abc_metadata.nb_slot; i++) {
		slotp = &metadata->slot_info[i];
		printf("  Slot %c:\n", SLOT_NAME(i));
		printf("    Priority: %d\n", slotp->priority);
		printf("    Tries Remaining: %d\n", slotp->tries_remaining);
		printf("    Successful Boot: %d\n", slotp->successful_boot);
	}

	return 0;
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

	abc_board_setup(AB_MARK_SUCCESSFUL, slot);

	need_stored = true;
	printf("Slot %c marked as successful\n", SLOT_NAME(slot));
	return CMD_RET_SUCCESS;
}

static int do_abc_mark_active(struct cmd_tbl *cmdtp, int flag, int argc,
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

	abc_board_setup(AB_MARK_ACTIVE, slot);

	need_stored = true;
	pr_err("Slot %c marked as next active \n", SLOT_NAME(slot));
	return CMD_RET_SUCCESS;
}

static int do_abc_mark_unbootable(struct cmd_tbl *cmdtp, int flag, int argc,
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

	abc_board_setup(AB_MARK_UNBOOTABLE, slot);

	need_stored = true;
	printf("Slot %c marked as unbootable\n", SLOT_NAME(slot));
	return CMD_RET_SUCCESS;
}

static int abc_is_bootable(struct cmd_tbl *cmdtp, int flag, int argc,
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

	printf("Slot %c marked as %s\n", SLOT_NAME(slot),
	            slotp->priority != 0 ? "bootable" : "unbootable");
	return CMD_RET_SUCCESS;
}

static int abc_is_successful(struct cmd_tbl *cmdtp, int flag, int argc,
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

	printf("Slot %c marked as %s\n", SLOT_NAME(slot),
	       slotp->successful_boot ? "suceessful" : "unsuccessful");
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
	U_BOOT_CMD_MKENT(store, CONFIG_SYS_MAXARGS, 1, do_abc_store, "", ""),
	U_BOOT_CMD_MKENT(dump, 1, 1, do_abc_dump, "", ""),
	U_BOOT_CMD_MKENT(get-number, 1, 1, do_abc_get_number, "", ""),
	U_BOOT_CMD_MKENT(get-current, 1, 1, do_abc_get_current, "", ""),
	U_BOOT_CMD_MKENT(get-suffix, 2, 1, do_abc_get_suffix, "", ""),
	U_BOOT_CMD_MKENT(mark-successful, 1, 1, do_abc_mark_successful, "", ""),
	U_BOOT_CMD_MKENT(mark-active, 2, 1, do_abc_mark_active, "", ""),
	U_BOOT_CMD_MKENT(mark-unbootable, 2, 1, do_abc_mark_unbootable, "", ""),
	U_BOOT_CMD_MKENT(is-bootable, 2, 1, abc_is_bootable, "", ""),
	U_BOOT_CMD_MKENT(is-successful, 2, 1, abc_is_successful, "", ""),
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

	if (need_stored && abc_metadata_loaded) {
		/* Store the metadata if it has been modified */
		if (abc_part < 0 || abc_dev < 0) {
			ret = abc_store_into_buffer(&bl_msg_ab, &abc_metadata);
		} else {
			ret = abc_store_into_disk(abc_dev_desc, &abc_part_info,
			                      &abc_metadata);
		}
		if (ret < 0) {
			pr_err("Unable to store A/B metadata\n");
			return CMD_RET_FAILURE;
		}
		need_stored = false;
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	abc, CONFIG_SYS_MAXARGS, 1, do_abc,
	"Manage A/B metadata",
	"load [<interface>] [<dev[:part|#part_name]>]\n"
	"    - load A/B metadata from 'part' on device type 'interface' instance 'dev'\n"
	"      If not specified, the default interface, dev and part is used.\n"
	"abc store [<interface>] [<dev[:part|#part_name]>]\n"
	"    - store A/B metadata to 'part' on device type 'interface' instance 'dev'\n"
	"      if not specified, reuses the location from 'abc load' or use the default\n"
	"      location\n"
	"abc dump\n"
	"    - dump A/B metadata\n"
	"abc get-number\n"
	"    - print number of slots\n"
	"abc get-current\n"
	"    - print currently running SLOT\n"
	"abc get-suffix [SLOT] [varname]\n"
	"    - prints suffix for SLOT and store the slot suffix in the 'varname' variable\n"
	"abc mark-successful [SLOT]\n"
	"    - mark current or <SLOT> slot as GOOD\n"
	"abc mark-active [SLOT]\n"
	"    - On next boot, load and execute SLOT\n"
	"abc mark-unbootable [SLOT]\n"
	"    - mark SLOT as invalid\n"
	"abc is-bootable [SLOT]\n"
	"    - returns 0 only if SLOT is bootable\n"
	"abc is-successful [SLOT]\n"
	"    - returns 0 only if SLOT is marked GOOD\n"
);
