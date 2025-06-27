// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019 Eugeniu Rosca <rosca.eugeniu@gmail.com>
 * Charleye <wangkart@aliyun.com>
 *
 * Command to read/modify/write Android BCB fields
 */

#include <bcb.h>
#include <command.h>
#include <common.h>
#include <log.h>
#include <part.h>
#include <malloc.h>
#include <memalign.h>
#include <dm/device.h>
#include <linux/err.h>
#include <display_options.h>

#include <android_bootloader_message.h>

enum bcb_cmd {
	BCB_CMD_LOAD,
	BCB_CMD_FIELD_SET,
	BCB_CMD_FIELD_CLEAR,
	BCB_CMD_FIELD_TEST,
	BCB_CMD_FIELD_DUMP,
	BCB_CMD_STORE,
};

static int bcb_dev                             = -1;
static int bcb_part                            = -1;
static struct blk_desc *bcb_dev_desc           = NULL;
static struct disk_partition bcb_part_info     = { 0 };
static struct bootloader_message bcb           = { 0 };
static struct bootloader_message_ab bl_msg_ab  = { 0 };
bool bcb_loaded = false;
bool bcb_need_stored = false;

static int bcb_cmd_get(char *cmd)
{
	if (!strcmp(cmd, "load"))
		return BCB_CMD_LOAD;
	if (!strcmp(cmd, "set"))
		return BCB_CMD_FIELD_SET;
	if (!strcmp(cmd, "clear"))
		return BCB_CMD_FIELD_CLEAR;
	if (!strcmp(cmd, "test"))
		return BCB_CMD_FIELD_TEST;
	if (!strcmp(cmd, "store"))
		return BCB_CMD_STORE;
	if (!strcmp(cmd, "dump"))
		return BCB_CMD_FIELD_DUMP;
	else
		return -1;
}

static int bcb_is_misused(int argc, char *const argv[])
{
	int cmd = bcb_cmd_get(argv[0]);

	switch (cmd) {
	case BCB_CMD_LOAD:
	case BCB_CMD_STORE:
		if (argc != 1 && argc != 3)
			goto err;
		break;
	case BCB_CMD_FIELD_SET:
		if (argc != 3)
			goto err;
		break;
	case BCB_CMD_FIELD_TEST:
		if (argc != 4)
			goto err;
		break;
	case BCB_CMD_FIELD_CLEAR:
	case BCB_CMD_FIELD_DUMP:
		if (argc != 1 && argc != 2)
			goto err;
		break;
	default:
		printf("Error: 'bcb %s' not supported\n", argv[0]);
		return -1;
	}

	return 0;
err:
	printf("Error: Bad usage of 'bcb %s'\n", argv[0]);

	return -1;
}

static int bcb_field_get(char *name, char **fieldp, int *sizep)
{
	if (!strcmp(name, "command")) {
		*fieldp = bcb.command;
		*sizep = sizeof(bcb.command);
	} else if (!strcmp(name, "status")) {
		*fieldp = bcb.status;
		*sizep = sizeof(bcb.status);
	} else if (!strcmp(name, "recovery")) {
		*fieldp = bcb.recovery;
		*sizep = sizeof(bcb.recovery);
	} else if (!strcmp(name, "stage")) {
		*fieldp = bcb.stage;
		*sizep = sizeof(bcb.stage);
	} else if (!strcmp(name, "reserved")) {
		*fieldp = bcb.reserved;
		*sizep = sizeof(bcb.reserved);
	} else {
		printf("Error: Unknown bcb field '%s'\n", name);
		return -1;
	}

	return 0;
}

static int bcb_create_from_disk(struct blk_desc *dev_desc, struct disk_partition *part_info,
                              struct bootloader_message **bcbp)
{
	ulong blk_offset, blkcnt, offset;
	struct bootloader_message *bcb;
	char *buf;
	size_t size;
	int ret;

	offset = offsetof(struct bootloader_message_ab, message);
	if (offset >= part_info->blksz)
		blk_offset = offset / part_info->blksz;
	else
		blk_offset = 0;

	blkcnt = DIV_ROUND_UP(sizeof(struct bootloader_message), part_info->blksz);
	if (unlikely(blkcnt > part_info->size)) {
		pr_err("Partition size too small\n");
		return -EINVAL;
	}

	size = roundup(sizeof(struct bootloader_message_ab), part_info->blksz);
	buf = malloc_cache_aligned(size);
	if (!buf) {
		pr_err("Out of memory\n");
		return -ENOMEM;
	}

	ret = blk_dread(dev_desc, part_info->start + blk_offset,
					blkcnt, buf);
	if (ret != blkcnt) {
		pr_err("Unable to read BCB from partition '%s' on '%s' device.\n",
		                 part_info->name, dev_desc->bdev->name);
		free(buf);
		return -EIO;
	}

	size = roundup(sizeof(struct bootloader_message), part_info->blksz);
	bcb = malloc_cache_aligned(size);
	if (!bcb) {
		pr_err("Could not allocate BCB memory\n");
		free(buf);
		return -ENOMEM;
	}

	if (blk_offset == 0)
		memcpy(&bcb, buf + offset, sizeof(struct bootloader_message));
	else
		memcpy(&bcb, buf, sizeof(struct bootloader_message));

	debug("BCB loaded from part '%s' on '%s' device successfully.\n",
	                    part_info->name, dev_desc->bdev->name);

	*bcbp = bcb;
	free(buf);
	return 0;
}

static int bcb_create_from_buffer(struct bootloader_message_ab *buffer,
							  struct bootloader_message **bcbp)
{
	struct bootloader_message *bcb;
	loff_t offset;
	int size;

	if (!buffer || !bcbp) {
		pr_err("Invalid arguments\n");
		return -EINVAL;
	}

	size = roundup(sizeof(struct bootloader_message), sizeof(*buffer));
	bcb = malloc_cache_aligned(size);
	if (!bcb) {
		pr_err("Out of memory\n");
		return -ENOMEM;
	}

	offset = offsetof(struct bootloader_message_ab, message);
	memcpy(bcb, (void *)buffer + offset, sizeof(struct bootloader_message));
	*bcbp = bcb;

	debug("BCB loaded from buffer successfully.\n");
	return 0;
}

static int bcb_load_from_disk(const char *ifname, const char *dev_part_str)
{
	struct bootloader_message *bcb_metadata = NULL;
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

	ret = bcb_create_from_disk(dev_desc, &part_info, &bcb_metadata);
	if (ret < 0) {
		pr_err("Could not load BCB from '%s %s'\n",
		            ifname, dev_part_str);
		return ret;
	}

	memcpy(&bcb, bcb_metadata, sizeof(bcb));
	bcb_loaded = true;
	free(bcb_metadata);

	bcb_dev_desc = dev_desc;
	bcb_part_info = part_info;
	bcb_dev = dev_desc->devnum;
	bcb_part = part;

	printf("BCB loaded from '%s %s' successfully.\n",
	                             ifname, dev_part_str);
	return 0;
}

static int bcb_load_from_buffer(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(struct bootloader_message_ab, buffer, 1);
	struct bootloader_message *bcb_metadata = NULL;
	int ret;

	ret = bootloader_message_ab_load(buffer);
	if (ret < 0) {
		pr_err("Could not load BCB from buffer.\n");
		return ret;
	}

	ret = bcb_create_from_buffer(buffer, &bcb_metadata);
	if (ret < 0) {
		pr_err("Could not create BCB from buffer.\n");
		return ret;
	}

	memcpy(&bl_msg_ab, buffer, sizeof(bl_msg_ab));
	memcpy(&bcb, bcb_metadata, sizeof(bcb));
	bcb_loaded = true;
	free(bcb_metadata);

	printf("BCB loaded from buffer successfully.\n");
	return 0;
}

static int do_bcb_load(struct cmd_tbl *cmdtp, int flag, int argc,
                       char * const argv[])
{
	int ret;

	if (argc != 0 && argc != 2) {
		pr_err("Invalid number of arguments\n");
		return CMD_RET_USAGE;
	}

	switch (argc) {
	case 0:
		ret = bcb_load_from_buffer();
		break;
	case 2:
		ret = bcb_load_from_disk(argv[0], argv[1]);
		break;
	}

	if (ret < 0) {
		pr_err("Failed to load BCB\n");
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int __bcb_set(char *fieldp, const char *valp)
{
	int size, len;
	char *field, *str, *found, *tmp;

	if (bcb_field_get(fieldp, &field, &size))
		return CMD_RET_FAILURE;

	len = strlen(valp);
	if (len >= size) {
		printf("Error: sizeof('%s') = %d >= %d = sizeof(bcb.%s)\n",
		       valp, len, size, fieldp);
		return CMD_RET_FAILURE;
	}
	str = strdup(valp);
	if (!str) {
		printf("Error: Out of memory while strdup\n");
		return CMD_RET_FAILURE;
	}

	tmp = str;
	field[0] = '\0';
	while ((found = strsep(&tmp, ":"))) {
		if (field[0] != '\0')
			strcat(field, "\n");
		strcat(field, found);
	}
	free(str);

	bcb_need_stored = true;
	return CMD_RET_SUCCESS;
}

static int do_bcb_set(struct cmd_tbl *cmdtp, int flag, int argc,
                      char * const argv[])
{
	if (!bcb_loaded) {
		pr_err("\n ** No loaded BCB metadata **\n\n");
		return CMD_RET_USAGE;
	}

	return __bcb_set(argv[0], argv[1]);
}

static int do_bcb_clear(struct cmd_tbl *cmdtp, int flag, int argc,
                        char *const argv[])
{
	int size;
	char *field;

	if (!bcb_loaded) {
		pr_err("\n ** No loaded BCB metadata **\n\n");
		return CMD_RET_USAGE;
	}

	if (argc == 0) {
		memset(&bcb, 0, sizeof(bcb));
		bcb_need_stored = true;
		return CMD_RET_SUCCESS;
	}

	if (bcb_field_get(argv[0], &field, &size))
		return CMD_RET_FAILURE;

	memset(field, 0, size);
	bcb_need_stored = true;

	return CMD_RET_SUCCESS;
}

static int do_bcb_test(struct cmd_tbl *cmdtp, int flag, int argc,
                       char *const argv[])
{
	int size;
	char *field;
	char *op = argv[1];

	if (!bcb_loaded) {
		pr_err("\n ** No loaded BCB metadata **\n\n");
		return CMD_RET_USAGE;
	}

	if (bcb_field_get(argv[0], &field, &size))
		return CMD_RET_FAILURE;

	if (*op == '=' && *(op + 1) == '\0') {
		if (!strncmp(argv[2], field, size))
			return CMD_RET_SUCCESS;
		else
			return CMD_RET_FAILURE;
	} else if (*op == '~' && *(op + 1) == '\0') {
		if (!strstr(field, argv[2]))
			return CMD_RET_FAILURE;
		else
			return CMD_RET_SUCCESS;
	} else {
		printf("Error: Unknown operator '%s'\n", op);
	}

	return CMD_RET_FAILURE;
}

static int do_bcb_dump(struct cmd_tbl *cmdtp, int flag, int argc,
                       char *const argv[])
{
	int size;
	char *field;

	if (!bcb_loaded) {
		pr_err("\n ** No loaded BCB metadata **\n\n");
		return CMD_RET_USAGE;
	}

	if (argc ==0) {
		print_buffer(0, (void *)&bcb, 1, sizeof(bcb), 16);
		return CMD_RET_SUCCESS;
	}

	if (bcb_field_get(argv[0], &field, &size))
		return CMD_RET_FAILURE;

	print_buffer((ulong)field - (ulong)&bcb, (void *)field, 1, size, 16);

	return CMD_RET_SUCCESS;
}

static int bcb_write_into_disk(struct blk_desc *dev_desc,
                               const struct disk_partition *part_info,
                               struct bootloader_message *bcbp)
{
	ulong offset, blkcnt, blk_offset;
	size_t size;
	char *buf;
	int ret;

	offset = offsetof(struct bootloader_message_ab, message);
	if (offset >= bcb_part_info.blksz)
		blk_offset = offset / bcb_part_info.blksz;
	else
		blk_offset = 0;
	blkcnt = DIV_ROUND_UP(sizeof(struct bootloader_message), bcb_part_info.blksz);

	size = roundup(sizeof(struct bootloader_message_ab), part_info->blksz);
	buf = malloc_cache_aligned(size);
	if (!buf) {
		pr_err("Out of memory\n");
		return -ENOMEM;
	}

	ret = blk_dread(bcb_dev_desc, bcb_part_info.start + blk_offset,
	                            blkcnt, buf);
	if (IS_ERR_VALUE(ret)) {
		pr_err("Could not read from the '%s' partition\n", bcb_part_info.name);
		free(buf);
		return -EIO;
	}

	if (blk_offset == 0)
		memcpy(buf + offset, bcbp, sizeof(struct bootloader_message));
	else
		memcpy(buf, bcbp, sizeof(struct bootloader_message));

	ret = blk_dwrite(bcb_dev_desc, bcb_part_info.start + blk_offset,
	                    blkcnt, buf);
	if (IS_ERR_VALUE(ret)) {
		pr_err("Could not write into the '%s' partition\n", bcb_part_info.name);
		free(buf);
		return -EIO;
	}

	free(buf);
	return 0;
}

static int bcb_write_into_buffer(struct bootloader_message_ab *buffer,
							  struct bootloader_message *bcbp)
{
	ulong offset;

	if (!buffer || !bcbp) {
		pr_err("Invalid arguments\n");
		return -EINVAL;
	}

	offset = offsetof(struct bootloader_message_ab, message);
	if (offset >= sizeof(*buffer)) {
		pr_err("Offset is out of bounds\n");
		return -EINVAL;
	}

	memcpy((void *)buffer + offset, bcbp, sizeof(struct bootloader_message));
	return 0;
}

static int bcb_store_with_dev_part_str(const char *ifname,
									const char *dev_part_str,
									struct bootloader_message *bcbp)
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

	ret = bcb_write_into_disk(dev_desc, &part_info, bcbp);
	if (ret < 0) {
		pr_err("Failed to store BCB into '%s %s'\n",
		               ifname, dev_part_str);
		return ret;
	}

	printf("BCB stored into '%s %s' successfully.\n",
	                ifname, dev_part_str);
	return 0;
}

static int bcb_store_into_disk(struct blk_desc *dev_desc,
                               const struct disk_partition *part_info,
                               struct bootloader_message *bcbp)
{
	int ret;

	if (!dev_desc || !part_info || !bcbp) {
		pr_err("Invalid arguments\n");
		return -EINVAL;
	}

	ret = bcb_write_into_disk(dev_desc, part_info, bcbp);
	if (ret < 0) {
		pr_err("Could not store BCB into partition '%s' on '%s' device\n",
		                  part_info->name, dev_desc->bdev->name);
		return ret;
	}

	printf("BCB stored into partition '%s' on '%s' device successfully.\n",
	                part_info->name, dev_desc->bdev->name);
	return 0;
}

static int bcb_store_into_buffer(struct bootloader_message_ab *buffer,
							  struct bootloader_message *bcbp)
{
	int ret;

	if (!buffer || !bcbp) {
		pr_err("Invalid arguments\n");
		return -EINVAL;
	}

	ret = bcb_write_into_buffer(buffer, bcbp);
	if (ret < 0) {
		pr_err("Unable to write BCB into buffer\n");
		return ret;
	}

	ret = bootloader_message_ab_store(buffer);
	if (ret < 0) {
		pr_err("Could not store AB-specific bootloader message.\n");
		return ret;
	}

	printf("BCB stored into buffer successfully.\n");
	return 0;
}

static int do_bcb_store(struct cmd_tbl *cmdtp, int flag, int argc,
                        char * const argv[])
{
	int ret;

	if (argc != 0 && argc != 2) {
		pr_err("Invalid number of arguments\n");
		return CMD_RET_USAGE;
	}

	if (!bcb_loaded) {
		pr_err("No BCB loaded. Please load it first.\n");
		return CMD_RET_FAILURE;
	}

	switch (argc) {
	case 0:
		if (bcb_part < 0 || bcb_dev < 0)
			ret = bcb_store_into_buffer(&bl_msg_ab, &bcb);
		else
			ret = bcb_store_into_disk(bcb_dev_desc, &bcb_part_info,
			                      &bcb);
		break;
	case 2:
		ret = bcb_store_with_dev_part_str(argv[0], argv[1],
		                       &bcb);
		break;
	}

	if (ret < 0) {
		pr_err("Failed to store BCB\n");
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

int bcb_write_reboot_reason(const char *ifname, int devnum,
                            const char *part_name, const char *reasonp)
{
	char dev_part_str[PART_NAME_LEN];
	bool is_disk = false;
	int ret;

	if (!reasonp) {
		pr_err("Invalid reboot reason\n");
		return -EINVAL;
	}

	is_disk = (ifname != NULL && part_name != NULL && devnum >= 0);

	if (!is_disk) {
		ret = bcb_load_from_buffer();
	} else {
		snprintf(dev_part_str, PART_NAME_LEN, "%d#%s", devnum, part_name);
		ret = bcb_load_from_disk(ifname, dev_part_str);
	}

	if (ret < 0) {
		pr_err("Failed to load BCB\n");
		return ret;
	}

	ret = __bcb_set("command", reasonp);
	if (ret != CMD_RET_SUCCESS)
		return ret;

	if (!is_disk) {
		ret = bcb_store_into_buffer(&bl_msg_ab, &bcb);
	} else {
		ret = bcb_store_with_dev_part_str(ifname, dev_part_str, &bcb);
	}

	if (ret < 0) {
		pr_err("Failed to store BCB\n");
		return ret;
	}

	return 0;
}

static struct cmd_tbl cmd_bcb_sub[] = {
	U_BOOT_CMD_MKENT(load, CONFIG_SYS_MAXARGS, 1, do_bcb_load, "", ""),
	U_BOOT_CMD_MKENT(set, CONFIG_SYS_MAXARGS, 1, do_bcb_set, "", ""),
	U_BOOT_CMD_MKENT(clear, CONFIG_SYS_MAXARGS, 1, do_bcb_clear, "", ""),
	U_BOOT_CMD_MKENT(test, CONFIG_SYS_MAXARGS, 1, do_bcb_test, "", ""),
	U_BOOT_CMD_MKENT(dump, CONFIG_SYS_MAXARGS, 1, do_bcb_dump, "", ""),
	U_BOOT_CMD_MKENT(store, CONFIG_SYS_MAXARGS, 1, do_bcb_store, "", ""),
};

static int do_bcb(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct cmd_tbl *c;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], cmd_bcb_sub, ARRAY_SIZE(cmd_bcb_sub));
	if (!c)
		return CMD_RET_USAGE;

	if (bcb_is_misused(argc, argv)) {
		/*
		 * We try to improve the user experience by reporting the
		 * root-cause of misusage, so don't return CMD_RET_USAGE,
		 * since the latter prints out the full-blown help text
		 */
		return CMD_RET_FAILURE;
	}

	argc--;
	argv++;

	ret = c->cmd(cmdtp, flag, argc, argv);
	if (ret < 0) {
		pr_err("Command failed with error %d\n", ret);
		return CMD_RET_FAILURE;
	}

	if (bcb_need_stored && bcb_loaded) {
		/* Store the BCB if it has been modified */
		if (bcb_part < 0 || bcb_dev < 0) {
			ret = bcb_store_into_buffer(&bl_msg_ab, &bcb);
		} else {
			ret = bcb_store_into_disk(bcb_dev_desc, &bcb_part_info,
			                      &bcb);
		}
		if (ret < 0) {
			pr_err("Unable to store BCB\n");
			return CMD_RET_FAILURE;
		}
		bcb_need_stored = false;
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	bcb, CONFIG_SYS_MAXARGS, 1, do_bcb,
	"Load/store/set/clear/test/dump Android BCB fields",
	"load [<interface>] [<dev[:part|#part_name]>]\n"
	"    - load BCB from 'part' on device type 'interface' instance 'dev'\n"
	"      If not specified, the default interface, dev and part is used.\n"
	"bcb store [<interface>] [<dev[:part|#part_name]>]\n"
	"    - store BCB to 'part' on device type 'interface' instance 'dev';\n"
	"      if not specified, reuses the location from 'abc load' or use the default\n"
	"      location\n"
	"bcb set <field> <val>\n"
	"    - set BCB <field> to <val>\n"
	"bcb clear [<field>]\n"
	"    - clear BCB <field> or all fields\n"
	"bcb test <field> <op> <val>\n"
	"    - test BCB <field> against <val>\n"
	"bcb dump [<field>]\n"
	"    - dump BCB <field> or all fields\n"
	"\n"
	"Legend:\n"
	"<interface>  - block device interface type, e.g. 'mmc', 'mtd'\n"
	"<dev>        - block device index containing the BCB partition\n"
	"<part>       - block device partition index containing the BCB\n"
	"<part_name>  - block device partition name containing the BCB\n"
	"<field>      - one of {command,status,recovery,stage,reserved}\n"
	"<op>         - the binary operator used in 'bcb test':\n"
	"               '=' returns true if <val> matches the string stored in <field>\n"
	"               '~' returns true if <val> matches a subset of <field>'s string\n"
	"<val>        - string/text provided as input to bcb {set,test}\n"
	"               NOTE: any ':' character in <val> will be replaced by line feed\n"
	"               during 'bcb set' and used as separator by upper layers\n"
);
