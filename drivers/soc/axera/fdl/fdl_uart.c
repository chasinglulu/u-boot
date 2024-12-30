// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 */

#include <common.h>
#include <dm/uclass.h>
#include <dm/device.h>
#include <serial.h>
#include <command.h>
#include <linux/delay.h>
#include <fdl.h>
#include <hexdump.h>
#include <malloc.h>
#include <u-boot/crc.h>

static void _serial_putc(struct udevice *dev, char ch)
{
	struct dm_serial_ops *ops = serial_get_ops(dev);
	int err;

	if (ch == '\n')
		_serial_putc(dev, '\r');

	do {
		err = ops->putc(dev, ch);
	} while (err == -EAGAIN);
}

static int __serial_getc(struct udevice *dev)
{
	struct dm_serial_ops *ops = serial_get_ops(dev);
	int timeout = 500;
	int elapsed = 0;
	unsigned int ticks;
	int ch;

	do {
		ticks = get_timer(0);
		ch = ops->getc(dev);
		if (elapsed > timeout) {
			return -ETIMEDOUT;
		}
		elapsed += get_timer(ticks);
	} while (ch == -EAGAIN);

	return ch >= 0 ? ch : 0;
}

static int _serial_getc(struct udevice *dev)
{
	return __serial_getc(dev);
}

static void fdl_response_tx(struct udevice *dev, const char *response)
{
	int i = 0;
	int len = fdl_get_resp_size(response);

	// print_hex_dump("RESPONSE: ", DUMP_PREFIX_OFFSET, 16, 64, response, len, true);
	while(len--)
		_serial_putc(dev, response[i++]);
}

static __maybe_unused
void fdl_response_tx_by_len(struct udevice *dev, const char *response, uint32_t len)
{
	int i = 0;

	while(len--)
		_serial_putc(dev, response[i++]);
}

static __maybe_unused
int fdl_uart_data_download(struct udevice *dev, char *data, size_t size)
{
	debug("size: 0x%lx\n", size);
	while (size--)
		*data++ = _serial_getc(dev);

	return 0;
}

static int fdl_packet_rx(struct udevice *dev, char **packet)
{
	uint32_t ticks, elapsed = 0;
	uint32_t timeout = 10000;	// 10s
	int handshake = 3, rx_count = 0;
	uint32_t magic, size, bytecode;
	char *start, *buffer;
	struct fdl_header *head;
	struct fdl_packet *pp;
	bool first;
	int ret, ch;

	if (!packet)
		return -EINVAL;

	start = malloc(FDL_COMMAND_LEN);
	if (!start)
		return -ENOMEM;
	buffer = start;

	/* Drop leading unrecognized character */
	while (true) {
		ticks = get_timer(0);
		ch = _serial_getc(dev);
		elapsed += get_timer(ticks);

		if (elapsed > timeout) {
			ret = -ETIMEDOUT;
			goto failed;
		}

		if (ch < 0 || (ch != 0x9F && ch != 0x3C))
			continue;
		else
			break;
	}

	/* handshake packet */
	handshake--;
	if (ch == 0x3C) {
restart:
		while (handshake--) {
			ticks = get_timer(0);
			ch = _serial_getc(dev);
			elapsed += get_timer(ticks);

			if (elapsed > timeout) {
				pr_err("handshake timeout\n");
				ret = -ETIMEDOUT;
				goto failed;
			}

			if (ch < 0 || ch != 0x3C) {
				pr_err("handshake failed\n");
				handshake = 3;
				goto restart;
			}
		}

		fdl_command(FDL_COMMAND_HANDSHAKE, start);
		*packet = start;

		return 0;
	}

	/* command packet */
	*buffer++ = 0x9F;
	rx_count = 1;
	first = false;
	while (true) {
		ticks = get_timer(0);
		ch = _serial_getc(dev);
		elapsed += get_timer(ticks);

		if (elapsed > timeout) {
			pr_err("command timeout\n");
			ret = -ETIMEDOUT;
			goto failed;
		}

		if (ch < 0 || (first && ch != 0x9F)) {
			pr_err("command failed\n");
			goto next_packet;
		}
		rx_count++;
		*buffer++ = ch;

		pp = (void *)start;
		head = (void *)pp;
		switch (rx_count) {
		case FDL_STRUCT_1STF_LEN:
			magic = head->magic;
			if (magic != FDL_MAGIC) {
				pr_err("magic wrong\n");
				goto next_packet;
			}
			break;
		case FDL_STRUCT_2NDF_LEN:
			size = head->size;
			if (size > FDL_COMMAND_LEN - FDL_STRUCT_5THF_BUT4TH_LEN) {
				char *tmp = malloc(size + FDL_STRUCT_5THF_BUT4TH_LEN);
				if (!tmp) {
					pr_err("Out of memory\n");
					ret = -ENOMEM;
					goto failed;
				}
				memcpy(tmp, start, FDL_STRUCT_2NDF_LEN);
				buffer = tmp + FDL_STRUCT_2NDF_LEN;
				free(start);
				start = tmp;
			}
			break;
		case FDL_STRUCT_3RDF_LEN:
			bytecode = head->bytecode;
			break;
		}

		if ((rx_count - FDL_STRUCT_5THF_BUT4TH_LEN) == size)
			break;

		continue;
next_packet:
		memset(start, 0, rx_count);
		rx_count = 0;
		buffer = start;
		first = true;
	}

	*packet = start;
	return rx_count;

failed:
	if (start)
		free(start);

	return ret;
}

static int do_fdl_uart(struct cmd_tbl *cmdtp, int flag, int argc,
                        char *const argv[])
{
	struct udevice *dev;
	char *cmd_packet = NULL;
	char response[FDL_RESPONSE_LEN];
	struct fdl_struct fdl;
	struct fdl_header *head;
	struct fdl_packet __maybe_unused *packet;
	struct fdl_raw_tx __maybe_unused *tx;
	uint32_t __maybe_unused fdl_data_len;
	uint32_t __maybe_unused calc_chksum;
	int ret;

	fdl_init(NULL, 0);
	memset(response, 0, sizeof(response));

	uclass_get_device_by_seq(UCLASS_SERIAL, 2, &dev);
	printf("device name: %s\n", dev->name);

	while (true) {
		ret = fdl_packet_rx(dev, &cmd_packet);
		if (ret < 0) {
			pr_err("Failed to receive command packet [ret %d]\n", ret);
			return CMD_RET_FAILURE;
		}

		ret = fdl_packet_check(&fdl, cmd_packet, response);
		if (ret < 0)
			goto failed;

		ret = fdl_handle_command(&fdl, response);
		if (ret < 0) {
			head = (void *)&fdl;
			pr_err("Unable to handle 0x%X command\n", head->bytecode);
			goto failed;
		}
		mdelay(200);
		fdl_response_tx(dev, response);

#ifdef CONFIG_FDL_RAW_DATA_DL
		if (!fdl_compare_comm_bytecode(&fdl, FDL_COMMAND_MIDST_DATA) &&
		       !fdl_compare_comm_bytecode(&fdl, FDL_COMMAND_END_DATA))
			continue;

		packet = (void *)response;
		tx = (void *)packet->payload;
		fdl_data_len = tx->len;
		if (!fdl_data_len && !fdl_data_remaining()) {
			ret = fdl_data_complete(response);
			if (ret < 0)
				goto failed;
		} else {
			char *fdl_data = malloc(fdl_data_len);
			if (unlikely(!fdl_data)) {
				debug("Out of memory\n");
				fdl_fail_null(FDL_RESPONSE_OPERATION_FAIL, response);
				goto failed;
			}

			fdl_uart_data_download(dev, fdl_data, fdl_data_len);
			if (tx->chksum_en) {
				calc_chksum = fdl_rawdata_checksum((void *)fdl_data, fdl_data_len);
				debug("calc: 0x%x checksum: 0x%x\n", calc_chksum, tx->chksum);
				if (calc_chksum != tx->chksum) {
					ret = -EINVAL;
					fdl_fail_null(FDL_RESPONSE_VERIFY_CHECKSUM_FAIL, response);
					goto failed;
				}
			}

			ret = fdl_data_download(fdl_data, fdl_data_len, response);
			if (ret < 0)
				goto failed;

			if (fdl_data)
				free(fdl_data);
		}
		fdl_okay_null(FDL_RESPONSE_ACK, response);
		fdl_response_tx(dev, response);
#endif

failed:
		if (cmd_packet)
			free(cmd_packet);
		if (ret < 0)
			fdl_response_tx(dev, response);
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	fdl_uart, 1, 1, do_fdl_uart,
	"fdl uart test",
	""
);