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
static struct bootloader_message bcb __aligned(ARCH_DMA_MINALIGN) = { { 0 } };

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
	case BCB_CMD_FIELD_SET:
		if (argc != 3)
			goto err;
		break;
	case BCB_CMD_FIELD_TEST:
		if (argc != 4)
			goto err;
		break;
	case BCB_CMD_FIELD_CLEAR:
		if (argc != 1 && argc != 2)
			goto err;
		break;
	case BCB_CMD_STORE:
		if (argc != 1 && argc != 3)
			goto err;
		break;
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

static int bcb_load(struct blk_desc *dev_desc, struct disk_partition *part_info)
{
	ulong blk_offset, blkcnt, offset;
	char buf[sizeof(struct bootloader_message_ab)];
	int ret;

	offset = offsetof(struct bootloader_message_ab, message);
	if (offset >= part_info->blksz)
		blk_offset = offset / part_info->blksz;
	else
		blk_offset = 0;

	blkcnt = DIV_ROUND_UP(sizeof(struct bootloader_message), part_info->blksz);
	if (unlikely(blkcnt > part_info->size)) {
		pr_err("Partition size too small\n");
		ret = -EINVAL;
		goto err;
	}

	ret = blk_dread(dev_desc, part_info->start + blk_offset,
					blkcnt, buf);
	if (ret != blkcnt) {
		pr_err("Unable to read BCB from part '%s' on '%s' device.\n",
		                 part_info->name, dev_desc->bdev->name);
		ret = -EIO;
		goto err;
	}

	if (blk_offset == 0)
		memcpy(&bcb, buf + offset, sizeof(struct bootloader_message));
	else
		memcpy(&bcb, buf, sizeof(struct bootloader_message));

	bcb_dev = dev_desc->devnum;
	printf("BCB loaded from part '%s' on '%s' device successfully.\n",
	                    part_info->name, dev_desc->bdev->name);

	return 0;

err:
	bcb_dev = -1;
	bcb_part = -1;
	bcb_dev_desc = NULL;
	memset(&bcb_part_info, 0, sizeof(bcb_part_info));
	return ret;
}

static int do_bcb_load(struct cmd_tbl *cmdtp, int flag, int argc,
                       char * const argv[])
{
	int ret;

	if (argc != 2) {
		pr_err("Invalid number of arguments\n");
		return CMD_RET_USAGE;
	}

	/* Lookup the "misc" partition */
	ret = part_get_info_by_dev_and_name_or_num(argv[0], argv[1],
	      &bcb_dev_desc, &bcb_part_info,
	      false);
	if (ret < 0) {
		pr_err("Unable to get device and partition information\n");
		return CMD_RET_FAILURE;
	}

	ret = bcb_load(bcb_dev_desc, &bcb_part_info);
	if (ret < 0) {
		pr_err("Could not load BCB part '%s' on '%s' device\n",
		            bcb_part_info.name, bcb_dev_desc->bdev->name);
		return CMD_RET_FAILURE;
	}
	bcb_dev = bcb_dev_desc->devnum;
	bcb_part = ret;

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

	return CMD_RET_SUCCESS;
}

static int do_bcb_set(struct cmd_tbl *cmdtp, int flag, int argc,
                      char * const argv[])
{
	if (bcb_dev == -1 || bcb_part == -1) {
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

	if (bcb_dev == -1 || bcb_part == -1) {
		pr_err("\n ** No loaded BCB metadata **\n\n");
		return CMD_RET_USAGE;
	}

	if (argc == 0) {
		memset(&bcb, 0, sizeof(bcb));
		return CMD_RET_SUCCESS;
	}

	if (bcb_field_get(argv[0], &field, &size))
		return CMD_RET_FAILURE;

	memset(field, 0, size);

	return CMD_RET_SUCCESS;
}

static int do_bcb_test(struct cmd_tbl *cmdtp, int flag, int argc,
                       char *const argv[])
{
	int size;
	char *field;
	char *op = argv[1];

	if (bcb_dev == -1 || bcb_part == -1) {
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

	if (bcb_dev == -1 || bcb_part == -1) {
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

static int bcb_store(struct bootloader_message *bcb)
{
	ulong offset, blkcnt, blk_offset;
	char buf[sizeof(struct bootloader_message_ab)];
	int ret;

	if (unlikely(!bcb)) {
		pr_err("Invalid BCB metadata\n");
		return -EINVAL;
	}

	if (bcb_dev < 0 || bcb_part < 0) {
		pr_err("Invalid device or partition\n");
		return -EINVAL;
	}

	offset = offsetof(struct bootloader_message_ab, message);
	if (offset >= bcb_part_info.blksz)
		blk_offset = offset / bcb_part_info.blksz;
	else
		blk_offset = 0;
	blkcnt = DIV_ROUND_UP(sizeof(struct bootloader_message), bcb_part_info.blksz);

	ret = blk_dread(bcb_dev_desc, bcb_part_info.start + blk_offset,
	                            blkcnt, buf);
	if (IS_ERR_VALUE(ret)) {
		pr_err("Could not read from the '%s' partition\n", bcb_part_info.name);
		return -EIO;
	}

	if (blk_offset == 0)
		memcpy(buf + offset, bcb, sizeof(struct bootloader_message));
	else
		memcpy(buf, bcb, sizeof(struct bootloader_message));

	ret = blk_dwrite(bcb_dev_desc, bcb_part_info.start + blk_offset,
	                    blkcnt, buf);
	if (IS_ERR_VALUE(ret)) {
		pr_err("Could not write back the '%s' partition\n", bcb_part_info.name);
		return -EIO;
	}

	return 0;
}

static int do_bcb_store(struct cmd_tbl *cmdtp, int flag, int argc,
                        char * const argv[])
{
	int ret;

	if (argc != 0 && argc != 2) {
		pr_err("Invalid number of arguments\n");
		goto err;
	}

	if (argc ==0 && (bcb_dev < 0 || bcb_part < 0)) {
		pr_err("Please load BCB first.\n");
		return CMD_RET_FAILURE;
	}

	if (argc == 0)
		goto store_bcb;

	ret = part_get_info_by_dev_and_name_or_num(argv[0], argv[1],
	      &bcb_dev_desc, &bcb_part_info,
	      false);
	if (ret < 0) {
		pr_err("Unable to get partition information\n");
		goto err;
	}
	bcb_part = ret;
	bcb_dev = bcb_dev_desc->devnum;

store_bcb:
	ret = bcb_store(&bcb);
	if (ret < 0) {
		pr_err("Could not store BCB into part '%s' on '%s' device\n",
		                  bcb_part_info.name, bcb_dev_desc->bdev->name);
		goto err;
	}

	printf("BCB stored into part '%s' on '%s' device successfully.\n",
	                bcb_part_info.name, bcb_dev_desc->bdev->name);
	return CMD_RET_SUCCESS;

err:
	bcb_dev_desc = NULL;
	memset(&bcb_part_info, 0, sizeof(bcb_part_info));
	bcb_dev = -1;
	bcb_part = -1;

	return CMD_RET_FAILURE;
}

int bcb_write_reboot_reason(const char *ifname, int devnum,
                            const char *part_name, const char *reasonp)
{
	int ret;
	char dev_part_str[64];

	sprintf(dev_part_str, "%d#%s",devnum, part_name);
	debug("%s: Writing reboot reason '%s' to BCB on '%s %s'\n",
	            __func__, reasonp, ifname, dev_part_str);
	ret = part_get_info_by_dev_and_name_or_num(ifname, dev_part_str,
	      &bcb_dev_desc, &bcb_part_info,
	      false);
	if (ret < 0) {
		pr_err("Unable to get device and partition information (ret = %d)\n", ret);
		return CMD_RET_FAILURE;
	}

	ret = bcb_load(bcb_dev_desc, &bcb_part_info);
	if (ret != CMD_RET_SUCCESS)
		return ret;

	ret = __bcb_set("command", reasonp);
	if (ret != CMD_RET_SUCCESS)
		return ret;

	ret = bcb_store(&bcb);
	if (ret != CMD_RET_SUCCESS)
		return ret;

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

	return c->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(
	bcb, CONFIG_SYS_MAXARGS, 1, do_bcb,
	"Load/store/set/clear/test/dump Android BCB fields",
	"load <interface> <dev[:part|#part_name]>\n"
	"    - load BCB from 'part' on device type 'interface' instance 'dev'\n"
	"bcb store [<interface>] [<dev[:part|#part_name]>]\n"
	"    - store BCB to 'part' on device type 'interface' instance 'dev';\n"
	"      if not specified, reuses the location from 'bcb load'\n"
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
