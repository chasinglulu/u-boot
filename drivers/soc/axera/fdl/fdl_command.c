// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <env.h>
#include <part.h>
#include <fdl.h>
#include <hexdump.h>
#include <linux/err.h>
#include <linux/sizes.h>
#include <linux/delay.h>

/**
 * image_size - final FDL image size
 */
static u32 image_size;

/**
 * fdl_part - FDL image partition name
 */
static char fdl_part[FDL_PART_NAME_LEN];

/**
 * fdl_bytes_received - number of bytes received in the current download
 */
static u32 fdl_bytes_received;

/**
 * fdl_bytes_expected - number of bytes expected in the current download
 */
static u32 fdl_bytes_expected;

/*
 * exec_cmd_cnt - number of execute command received
 */
uint32_t exec_cmd_cnt = 0;
/*
 * exec_addr - binary image start address of execute command
 */
uint64_t exec_addr;

static const struct {
	const char *response;
	uint16_t bytecode;
} responses[FDL_RESPONSE_COUNT] = {
	[FDL_RESPONSE_ACK] = {
		.response = "okay",
		.bytecode = 0x80,
	},
	[FDL_RESPONSE_VERSION] = {
		.response = "version",
		.bytecode = 0x81,
	},
	[FDL_RESPONSE_INVAILD_CMD] = {
		.response = "invaild command",
		.bytecode = 0x82,
	},
	[FDL_RESPONSE_UNKNOW_CMD] = {
		.response = "unknown command",
		.bytecode = 0x83,
	},
	[FDL_RESPONSE_OPERATION_FAIL] = {
		.response = "operation failure",
		.bytecode = 0x84,
	},
	[FDL_RESPONSE_SECUREBOOT_VERIFY_ERROR] = {
		.response = "signature verification failure",
		.bytecode = 0x88,
	},
	[FDL_RESPONSE_INVALID_DEST_ADDR] = {
		.response = "invaild destination address",
		.bytecode = 0x89,
	},
	[FDL_RESPONSE_INVALID_SIZE] = {
		.response = "invaild download or read size",
		.bytecode = 0x8A,
	},
	[FDL_RESPONSE_VERIFY_CHECKSUM_FAIL] = {
		.response = "checksum verification failure",
		.bytecode = 0x8B,
	},
	[FDL_RESPONSE_INVALID_PARTITION] = {
		.response = "invaild partition ID",
		.bytecode = 0x8C,
	},
	[FDL_RESPONSE_FLASH_DATA] = {
		.response = "returned flash data",
		.bytecode = 0x93,
	},
	[FDL_RESPONSE_CHIP_ID] = {
		.response = "returned chip ID",
		.bytecode = 0x94,
	},
	[FDL_RESPONSE_CHIP_UID] = {
		.response = "returned UID",
		.bytecode = 0x95,
	},
	[FDL_RESPONSE_COMPATIBILITY_DATA] = {
		.response = "returned compatibility data",
		.bytecode = 0x96,
	},
};

inline uint16_t fdl_get_resp_bytecode(int tag)
{
	if (tag < 0 || tag > FDL_RESPONSE_COUNT)
		return 0;

	return responses[tag].bytecode;
}

/**
 * fdl_data_remaining() - return bytes remaining in current transfer
 *
 * Return: Number of bytes left in the current download
 */
u32 fdl_data_remaining(void)
{
	return fdl_bytes_expected - fdl_bytes_received;
}

/**
 * fdl_data_download() - Copy image data to fdl_buf_addr.
 *
 * @fdl_data: Pointer to received fastboot data
 * @fdl_data_len: Length of received fastboot data
 * @response: Pointer to fastboot response buffer
 *
 * Copies image data from fdl_data to fdl_buf_addr. Writes to
 * response. fdl_bytes_received is updated to indicate the number
 * of bytes that have been transferred.
 *
 * On completion sets image_size and ${filesize} to the total size of the
 * downloaded image.
 */
int fdl_data_download(const void *fdl_data,
                unsigned int fdl_data_len,
                char *response)
{
#define BYTES_PER_DOT	0x1000
	u32 pre_dot_num, now_dot_num;

	if (fdl_data_len == 0 ||
	    (fdl_bytes_received + fdl_data_len) >
	                 fdl_bytes_expected) {
		debug("Received invalid data length\n");
		fdl_fail_null(FDL_RESPONSE_INVALID_SIZE, response);
		return -EINVAL;
	}

	/* Download data to fdl_buf_addr */
	memcpy(fdl_buf_addr + fdl_bytes_received,
	                   fdl_data, fdl_data_len);

	pre_dot_num = fdl_bytes_received / BYTES_PER_DOT;
	fdl_bytes_received += fdl_data_len;
	now_dot_num = fdl_bytes_received / BYTES_PER_DOT;

	if (pre_dot_num != now_dot_num) {
		putc('.');
		if (!(now_dot_num % 74))
			putc('\n');
	}

	return 0;
}

/**
 * fdl_data_complete() - Mark current transfer complete
 *
 * @response: Pointer to FDL response buffer
 *
 * Set image_size and ${filesize} to the total size of the downloaded image.
 */
int fdl_data_complete(char *response)
{
	int ret;

	printf("\ndownloading of %d bytes finished\n", fdl_bytes_received);
	image_size = fdl_bytes_received;
	fdl_bytes_expected = 0;
	fdl_bytes_received = 0;

#ifndef CONFIG_FDL_FDL2_PROTO
	if (exec_cmd_cnt != 2)
		return 0;
#endif

	env_set_hex("filesize", image_size);
	ret = fdl_blk_write_data(fdl_part, image_size);
	if (ret < 0) {
		debug("Unable to write date\n");
		fdl_fail_null(FDL_RESPONSE_OPERATION_FAIL, response);
		return -EINVAL;
	}

	return 0;
}

static int handshake(struct fdl_struct *fdl, char *response)
{
	char str[64];
	char *type = NULL;

	if (unlikely(!response))
		return -EIO;

#if defined(CONFIG_FDL_DEBUG)
	switch (exec_cmd_cnt) {
	case 0:
		type = FDL_HANDSHAKE_ROMCODE;
		break;
	case 1:
		type = FDL_HANDSHAKE_FDL1;
		break;
	}
#endif

#if defined(CONFIG_FDL_ROMCODE_PROTO)
	type = FDL_HANDSHAKE_ROMCODE;
#elif defined(CONFIG_FDL_FDL1_PROTO)
	type = FDL_HANDSHAKE_FDL1;
#elif defined(CONFIG_FDL_FDL2_PROTO)
	type = FDL_HANDSHAKE_FDL2;
#endif

	snprintf(str, sizeof(str), "%s %s", type, FDL_VERSION);

#if defined(CONFIG_FDL_RAW_DATA_DL)
	strncat(str, ";raw", 5);
#endif

	fdl_okay(FDL_RESPONSE_VERSION, str, response);

	return 0;
}

static int connect(struct fdl_struct *fdl, char *response)
{
	const char *str = "okay";

	if (unlikely(!response))
		return -EIO;

	fdl_okay(FDL_RESPONSE_ACK, str, response);

	return 0;
}

static int start_tx_msg(struct fdl_struct *fdl, char *response)
{
	const char *str = "okay";
	char part_name[FDL_PART_NAME_LEN];

	if (unlikely(!response))
		return -EIO;

	if (unlikely(!fdl))
		return -EINVAL;

#ifdef CONFIG_FDL_DEBUG
	struct fdl_tx_msg_romcode *romcode;
	struct fdl_tx_msg_fdl1 *fdl1;
	struct fdl_tx_msg_fdl2 *fdl2;

	switch (exec_cmd_cnt) {
	case 0:
		romcode = (void *)fdl->payload_data;
		debug("address: 0x%X size: 0x%X\n", romcode->addr, romcode->size);
		fdl_bytes_expected = romcode->size;
		exec_addr = romcode->addr;
		break;
	case 1:
		fdl1 = (void *)fdl->payload_data;
		debug("address: 0x%llX size: 0x%llX\n", fdl1->addr, fdl1->size);
		fdl_bytes_expected = fdl1->size;
		exec_addr = fdl1->addr;
		break;
	case 2:
		fdl2 = (void *)fdl->payload_data;
		char *name = (void *)fdl2;
		for (int i = 0; i < FDL_PART_NAME_LEN; i++) {
			uint16_t ch16 = *(uint16_t *)(name + 2 * i);
			if (!ch16) {
				part_name[i] = '\0';
				break;
			}
			part_name[i] = (char)ch16;
		}
		memcpy(fdl_part, part_name, FDL_PART_NAME_LEN);
		debug("name: %s size: 0x%llX\n", part_name, fdl2->size);
		fdl_bytes_expected = fdl2->size;
		exec_addr = 0;
		break;
	}
#else
	struct fdl_tx_msg *tx = NULL;

	tx = (void *)fdl->payload_data;

#if defined(CONFIG_FDL_ROMCODE_PROTO)
	debug("address: 0x%X size: 0x%X\n", tx->addr, tx->size);
	exec_addr = tx->addr;
#elif defined(CONFIG_FDL_FDL1_PROTO)
	debug("address: 0x%llX size: 0x%llX\n", tx->addr, tx->size);
	exec_addr = tx->addr;
#elif defined(CONFIG_FDL_FDL2_PROTO)
	char *name = (void *)tx;

	for (int i = 0; i < FDL_PART_NAME_LEN; i++) {
		uint16_t ch16 = *(uint16_t *)(name + 2 * i);
		if (!ch16) {
			part_name[i] = '\0';
			break;
		}
		part_name[i] = (char)ch16;
	}
	memcpy(fdl_part, part_name, FDL_PART_NAME_LEN);
	debug("name: %s size: 0x%llX\n", part_name, tx->size);
#endif

	fdl_bytes_expected = tx->size;
#endif /* CONFIG_FDL_DEBUG */

	fdl_bytes_received = 0;
	if (fdl_bytes_expected == 0) {
		debug("Expected nonzero image size\n");
		fdl_fail_null(FDL_RESPONSE_INVALID_SIZE, response);
		return 1;
	}

	debug("fdl_bytes_expected: 0x%X fdl_buf_size: 0x%X\n",
	                    fdl_bytes_expected, fdl_buf_size);
	if (fdl_bytes_expected > fdl_buf_size) {
		debug("FDL download buffer too small\n");
		fdl_fail_null(FDL_RESPONSE_INVALID_SIZE, response);
		return 1;
	} else {
		printf("Starting download of %d bytes\n",
		       fdl_bytes_expected);
	}

	fdl_okay(FDL_RESPONSE_ACK, str, response);

	return 0;
}

static int tx_data(struct fdl_struct *fdl, char *response)
{
	struct fdl_raw_tx *tx;

	if (unlikely(!response))
		return -EIO;

	if (!fdl)
		return -EINVAL;

	tx = (void *)fdl->payload_data;
	debug("tx data len: 0x%X en: 0x%X checksum: 0x%X\n",
	                 tx->len, tx->chksum_en, tx->chksum);

	fdl_response_data(FDL_RESPONSE_ACK, response,
	                  (void *)tx, sizeof(*tx));

	return 0;
}

static int terminate_tx(struct fdl_struct *fdl, char *response)
{
	struct fdl_packet *packet = NULL;
	struct fdl_raw_tx *tx;

	if (unlikely(!response))
		return -EIO;

	if (!fdl)
		return -EINVAL;

	packet = (void *)response;
	tx = (void *)packet->payload;
	memset(tx, 0, sizeof(*tx));

	fdl_response_data(FDL_RESPONSE_ACK, response,
	                  (void *)tx, sizeof(*tx));

	return 0;
}

static int execute(struct fdl_struct *fdl, char *response)
{
	const char *str = "okay";

	if (unlikely(!response))
		return -EIO;

	exec_cmd_cnt++;

	fdl_okay(FDL_RESPONSE_ACK, str, response);

	return 0;
}

static int setbrg(struct fdl_struct *fdl, char *response)
{
	const char *str = "okay";

	if (unlikely(!response))
		return -EIO;

	if (unlikely(!fdl))
		return -EINVAL;

	fdl_okay(FDL_RESPONSE_ACK, str, response);

	return 0;
}

static int reboot(struct fdl_struct *fdl, char *response)
{
	const char *str = "okay";

	if (unlikely(!response))
		return -EIO;

	if (unlikely(!fdl))
		return -EINVAL;

	fdl_okay(FDL_RESPONSE_ACK, str, response);

	return 0;
}

static int poweroff(struct fdl_struct *fdl, char *response)
{
	const char *str = "okay";

	if (unlikely(!response))
		return -EIO;

	if (unlikely(!fdl))
		return -EINVAL;

	fdl_okay(FDL_RESPONSE_ACK, str, response);

	return 0;
}

static int read_chip_id(struct fdl_struct *fdl, char *response)
{;
	const char *str = "okay";

	if (unlikely(!response))
		return -EIO;

	if (unlikely(!fdl))
		return -EINVAL;

	fdl_okay(FDL_RESPONSE_ACK, str, response);

	return 0;
}

static int erase(struct fdl_struct *fdl, char *response)
{
	struct fdl_erase *erase = NULL;
	const char *str = "okay";
	char part_name[FDL_PART_NAME_LEN];
	uint16_t ch16;
	int i;

	if (unlikely(!response))
		return -EIO;

	if (unlikely(!fdl))
		return -EINVAL;

	erase = (void *)fdl->payload_data;
	for (i = 0; i < FDL_PART_NAME_LEN; i++) {
		ch16 = *(uint16_t *)(erase->name + 2 * i);
		if (!ch16) {
			part_name[i] = '\0';
			break;
		}
		part_name[i] = (char)ch16;
	}
	debug("flags: 0x%llx name: %s size: 0x%llx\n",
	                 erase->flag, part_name, erase->size);

	if (erase->flag)
		fdl_blk_erase(NULL, -1);
	else
		fdl_blk_erase(part_name, erase->size);

	fdl_okay(FDL_RESPONSE_ACK, str, response);

	return 0;
}

static int partition(struct fdl_struct *fdl, char *response)
{
	struct fdl_part_header *h = NULL;
	struct fdl_part *part = NULL;
	struct fdl_part_table *tab = NULL;
	const char *str = "okay";
	char part_name[FDL_PART_NAME_LEN];
	size_t start = 0;
	uint32_t unit;
	int i, j, ret;

	if (unlikely(!response))
		return -EIO;

	if (unlikely(!fdl))
		return -EINVAL;

	h = (void *)fdl->payload_data;
	debug("magic: 0x%x, version: %d, unit: %d, count: %d\n",
	              h->magic, h->version, h->unit, h->count);

	switch (h->unit) {
	case FDL_PART_UNIT_MiB:
		unit = SZ_1M;
		break;
	case FDL_PART_UNIT_512KiB:
		unit = SZ_512K;
		break;
	case FDL_PART_UNIT_KiB:
		unit = SZ_1K;
		break;
	case FDL_PART_UNIT_BYTE:
		unit = SZ_1;
		break;
	case FDL_PART_UNIT_SECTOR:
		unit = SZ_512;
		break;
	default:
		debug("Invaild unit argument\n");
		return -EINVAL;
	}

	tab = malloc(sizeof(*tab) + sizeof(struct disk_partition) * h->count);
	if (!tab) {
		debug("Out of memory\n");
		return -ENOMEM;
	}
	tab->number = h->count;

	part = (void *)h + sizeof(*h);
	for (i = 0; i < h->count; i++, part++) {
		char *name = (void *)part;
		struct disk_partition *dp = &tab->part[i];

		for (j = 0; j < FDL_PART_NAME_LEN; j++) {
			uint16_t ch16 = *(uint16_t *)(name + 2 * j);
			if (!ch16) {
				part_name[j] = '\0';
				break;
			}
			part_name[j] = (char)ch16;
		}
		if (strlen(part_name) > PART_NAME_LEN) {
			ret = -EINVAL;
			goto failed;
		}
		strncpy((void *)dp->name, part_name, PART_NAME_LEN);
		dp->blksz = SZ_512;
		dp->start = DIV_ROUND_UP(start, dp->blksz);
		dp->size = DIV_ROUND_UP((part->size * unit), dp->blksz);
		start += ((part->size + part->gap) * unit);

		debug("Part %d:\n", i);
		debug("    Raw Part    : size = 0x%llx gap = 0x%llx\n", part->size, part->gap);
		debug("    Written Part: %s start = 0x%lx size = 0x%lx\n",
		                  dp->name, dp->start, dp->size);
	}

	ret = fdl_blk_write_partition(tab);
	if (ret < 0)
		debug("Unable to write partitions\n");

failed:
	if (tab)
		free(tab);

	if (ret < 0)
		return ret;

	fdl_okay(FDL_RESPONSE_ACK, str, response);
	return 0;
}

static int read_chip_uid(struct fdl_struct *fdl, char *response)
{
	const char *str = "okay";

	if (unlikely(!response))
		return -EIO;

	fdl_okay(FDL_RESPONSE_ACK, str, response);

	return 0;
}

static int start_rx_msg(struct fdl_struct *fdl, char *response)
{
	const char *str = "okay";

	if (unlikely(!response))
		return -EIO;

	fdl_okay(FDL_RESPONSE_ACK, str, response);

	return 0;
}

static int rx_data(struct fdl_struct *fdl, char *response)
{
	const char *str = "okay";

	if (unlikely(!response))
		return -EIO;

	fdl_okay(FDL_RESPONSE_ACK, str, response);

	return 0;
}

static int terminate_rx(struct fdl_struct *fdl, char *response)
{
	const char *str = "okay";

	if (unlikely(!response))
		return -EIO;

	fdl_okay(FDL_RESPONSE_ACK, str, response);

	return 0;
}

static const struct {
	const char *command;
	uint16_t bytecode;
	int (*dispatch)(struct fdl_struct *fdl, char *response);
} commands[FDL_COMMAND_COUNT] = {
	[FDL_COMMAND_HANDSHAKE] = {
		.command = "start to handshake",
		.bytecode = 0x3C,
		.dispatch = handshake,
	},
	[FDL_COMMAND_CONNECT] = {
		.command = "request to setup connection",
		.bytecode = 0x00,
		.dispatch = connect,
	},
	[FDL_COMMAND_START_DATA] = {
		.command = "start to transmit data",
		.bytecode = 0x01,
		.dispatch = start_tx_msg,
	},
	[FDL_COMMAND_MIDST_DATA] = {
		.command = "repeat to transmit data",
		.bytecode = 0x02,
		.dispatch = tx_data,
	},
	[FDL_COMMAND_END_DATA] = {
		.command = "terminate to transmit data",
		.bytecode = 0x03,
		.dispatch = terminate_tx,
	},
	[FDL_COMMAND_EXECUTE] = {
		.command = "execute",
		.bytecode = 0x04,
		.dispatch = execute,
	},
	[FDL_COMMAND_CHANGE_BAUD] = {
		.command = "setbrg",
		.bytecode = 0x09,
		.dispatch = setbrg,
	},
	[FDL_COMMAND_REBOOT] = {
		.command = "reboot",
		.bytecode = 0x05,
		.dispatch = reboot,
	},
	[FDL_COMMAND_POWER_OFF] = {
		.command = "poweroff",
		.bytecode = 0x06,
		.dispatch = poweroff,
	},
	[FDL_COMMAND_READ_CHIP_ID] = {
		.command = "read chip ID",
		.bytecode = 0x07,
		.dispatch = read_chip_id,
	},
	[FDL_COMMAND_ERASE_FLASH] = {
		.command = "erase flash",
		.bytecode = 0x0A,
		.dispatch = erase,
	},
	[FDL_COMMAND_REPARTITION] = {
		.command = "flash repartition",
		.bytecode = 0x0B,
		.dispatch = partition,
	},
	[FDL_COMMAND_READ_CHIP_UID] = {
		.command = "read chip UID",
		.bytecode = 0x0C,
		.dispatch = read_chip_uid,
	},
	[FDL_COMMAND_START_READ_FLASH] = {
		.command = "start to receive data",
		.bytecode = 0x10,
		.dispatch = start_rx_msg,
	},
	[FDL_COMMAND_MIDST_READ_FLASH] = {
		.command = "repeat to receive data",
		.bytecode = 0x11,
		.dispatch = rx_data,
	},
	[FDL_COMMAND_END_READ_FLASH] = {
		.command = "terminate to receive data",
		.bytecode = 0x12,
		.dispatch = terminate_rx,
	},
};

inline uint16_t fdl_get_comm_bytecode(int tag)
{
	if (tag < 0 || tag > FDL_COMMAND_COUNT)
		return 0;

	return commands[tag].bytecode;
}

inline bool fdl_compare_comm_bytecode(struct fdl_struct *fdl, int tag)
{
	if (tag < 0 || tag > FDL_COMMAND_COUNT)
		return false;
	if (IS_ERR_OR_NULL(fdl))
		return false;

	return fdl->head.bytecode == commands[tag].bytecode;
}

int fdl_handle_command(struct fdl_struct *fdl, char *response)
{
	struct fdl_header *comm_head, *resp_head;
	const char *resp = "unrecognized command";
	int i, ret;

	if (unlikely(!fdl)) {
		fdl_fail_null(FDL_RESPONSE_INVAILD_CMD, response);
		return -EINVAL;
	}

	comm_head = (void *)fdl;
	resp_head = (void *)response;
	for (i = 0; i < FDL_COMMAND_COUNT; i++) {
		if (commands[i].bytecode == comm_head->bytecode &&
		                  commands[i].dispatch) {
			debug("FDL Command: 0x%x\n", comm_head->bytecode);
			ret = commands[i].dispatch(fdl, response);
			if (ret < 0) {
				fdl_fail_null(FDL_RESPONSE_OPERATION_FAIL, response);
				return ret;
			}

			return 0;
		}
	}

	debug("FDL Command 0x%x not recognized.\n", comm_head->bytecode);
	fdl_fail(FDL_RESPONSE_UNKNOW_CMD, resp, response);

	return -EAGAIN;
}