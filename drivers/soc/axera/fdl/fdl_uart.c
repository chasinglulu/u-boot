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
#include <linux/err.h>
#include <fdl.h>
#include <malloc.h>

/* check if handshake is successful */
static bool connected = false;

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

	FDL_DUMP("RESPONSE: ", response, len);

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

static int fdl_uart_rx_timeout(struct udevice *dev, uint32_t timeout)
{
	uint32_t ticks, elapsed = 0;
	int ch;

	while (true) {
		ticks = get_timer(0);
		ch = _serial_getc(dev);
		elapsed += get_timer(ticks);

		if (elapsed > timeout)
			return -ETIMEDOUT;

		if (ch >= 0)
			break;
	}

	return ch;
}

static __maybe_unused
int fdl_uart_data_download(struct udevice *dev, char *data, size_t size)
{
	uint32_t timeout = CONFIG_VAL(FDL_TIMEOUT);
	int ret;

	while (size--) {
		ret = fdl_uart_rx_timeout(dev, timeout);
		if (unlikely(ret < 0))
			return ret;

		*data++ = ret;
	}

	return 0;
}

static int fdl_packet_rx(struct udevice *dev, char **packet)
{
	uint32_t timeout = CONFIG_VAL(FDL_TIMEOUT);
	int handshake = 3, rx_count = 0;
	uint32_t magic, size, bytecode;
	char *buffer, *pos;
	struct fdl_header *head;
	struct fdl_packet *pp;
	int ret;

	if (!packet)
		return -EINVAL;

	buffer = malloc(FDL_COMMAND_LEN);
	if (!buffer)
		return -ENOMEM;

retry:
	memset(buffer, 0, FDL_COMMAND_LEN);
	pos = buffer;
	/* Drop leading unrecognized character */
	while (true) {
		ret = fdl_uart_rx_timeout(dev, timeout);
		if (unlikely(ret < 0))
			goto failed;

		if (ret == 0x9F || (ret == 0x3C && !connected))
			break;
	}

	/* handshake packet */
	if (ret == 0x3C) {
		while (--handshake) {
			ret = fdl_uart_rx_timeout(dev, timeout);
			if (unlikely(ret < 0))
				goto failed;

			if (ret != 0x3C) {
				fdl_err("handshake failed\n");
				handshake = 3;
				goto retry;
			}
		}

		fdl_command(FDL_COMMAND_HANDSHAKE, buffer);
		*packet = buffer;

		return 0;
	}

	/* command packet */
	*pos++ = 0x9F;
	rx_count = 1;
	pp = (void *)buffer;
	head = (void *)pp;
	while (true) {
		ret = fdl_uart_rx_timeout(dev, timeout);
		if (unlikely(ret < 0))
			goto failed;

		rx_count++;
		*pos++ = ret;

		switch (rx_count) {
		case FDL_MAGIC_END:
			magic = head->magic;
			if (magic != FDL_MAGIC) {
				fdl_err("Wrong magic (0x%x)\n", magic);
				ret = -EINVAL;
				goto failed;
			}
			break;
		case FDL_SIZE_END:
			size = head->size;
			/* expand command buffer */
			if (size > FDL_COMMAND_LEN - FDL_PROTO_FIXED_LEN) {
				char *tmp = malloc(size + FDL_PROTO_FIXED_LEN);
				if (unlikely(!tmp)) {
					fdl_err("Out of memory\n");
					ret = -ENOMEM;
					goto failed;
				}
				memcpy(tmp, buffer, FDL_SIZE_END);
				pos = tmp + FDL_SIZE_END;
				free(buffer);
				buffer = tmp;
			}
			break;
		case FDL_BYTECODE_END:
			bytecode = head->bytecode;
			break;
		}

		if ((rx_count - FDL_PROTO_FIXED_LEN) == size)
			break;
	}

	*packet = buffer;
	return rx_count;

failed:
	if (buffer)
		free(buffer);

	return ret;
}

static int fdl_uart_process(struct udevice *dev, bool timeout, bool need_ack)
{
	char *cmd_packet = NULL;
	char response[FDL_RESPONSE_LEN];
	struct fdl_info fdl;
	struct fdl_header *head;
	struct fdl_packet __maybe_unused *packet;
	struct fdl_tx_info __maybe_unused *payload;
	uint32_t __maybe_unused fdl_data_len;
	uint32_t __maybe_unused checksum;
	char __maybe_unused *fdl_data = NULL;
	int ret;

	fdl_init(NULL, 0);
	memset(response, 0, sizeof(response));

	if (unlikely(need_ack)) {
		fdl_debug("Acknowledge the previous execute command\n");
		fdl_okay_null(FDL_RESPONSE_ACK, response);
		fdl_response_tx(dev, response);
	}

	while (true) {
		ret = fdl_packet_rx(dev, &cmd_packet);
		if (unlikely(ret < 0)) {
			if (!timeout && ret == -ETIMEDOUT)
				continue;

			fdl_err("Failed to receive command packet [ret %d]\n", ret);
			return ret;
		}
		FDL_DUMP("COMMAND: ", cmd_packet, ret);

		ret = fdl_packet_check(&fdl, cmd_packet, response);
		if (unlikely(ret < 0))
			goto failed;

		ret = fdl_handle_command(&fdl, response);
		if (unlikely(ret < 0)) {
			head = (void *)&fdl;
			fdl_err("Unable to handle 0x%X command\n", head->bytecode);
			goto failed;
		}

		if (fdl_compare_command_by_tag(&fdl, FDL_COMMAND_END_DATA)) {
			ret = fdl_data_complete(response);
			if (unlikely(ret < 0))
				goto failed;
		}

		if (fdl_compare_command_by_tag(&fdl, FDL_COMMAND_HANDSHAKE))
			connected = true;
		else if (fdl_compare_command_by_tag(&fdl, FDL_COMMAND_EXECUTE))
			connected = false;

		fdl_response_tx(dev, response);

#ifdef CONFIG_FDL_RAW_DATA_DL
		if (!fdl_compare_command_by_tag(&fdl, FDL_COMMAND_MIDST_DATA))
			continue;

		payload = (void *)&fdl_tx_payload_info;
		fdl_data_len = payload->len;
		fdl_data = malloc(fdl_data_len);
		if (unlikely(!fdl_data)) {
			fdl_debug("Out of memory\n");
			fdl_fail_null(FDL_RESPONSE_OPERATION_FAIL, response);
			goto failed;
		}

		ret = fdl_uart_data_download(dev, fdl_data, fdl_data_len);
		if (unlikely(ret < 0)) {
			fdl_debug("fdl_uart_data_download failed (ret = %d)\n", ret);
			fdl_fail_null(FDL_RESPONSE_OPERATION_FAIL, response);
			goto failed;
		}
		FDL_DUMP("PAYLOAD DATA: ", fdl_data, fdl_data_len);

		if (payload->chksum_en) {
			checksum = fdl_rawdata_checksum((void *)fdl_data, fdl_data_len);
			if (checksum != payload->chksum) {
				fdl_debug("Invalid raw data checksum (expected %.8x, found %.8x)\n", checksum, payload->chksum);
				ret = -EINVAL;
				fdl_fail_null(FDL_RESPONSE_VERIFY_CHECKSUM_FAIL, response);
				goto failed;
			}
		}

		ret = fdl_data_download(fdl_data, fdl_data_len, response);
		if (unlikely(ret < 0))
			goto failed;

		fdl_debug("Payload data download completed\n");
		fdl_okay_null(FDL_RESPONSE_ACK, response);
		fdl_response_tx(dev, response);
#endif

failed:
		if (fdl_data)
			free(fdl_data);
		if (cmd_packet)
			free(cmd_packet);
		if (ret < 0)
			fdl_response_tx(dev, response);
	}
}

static int do_fdl_uart(struct cmd_tbl *cmdtp, int flag, int argc,
                        char *const argv[])
{
	struct udevice *dev;
	int seq = 0;
	int ret;

	if (argc > 2)
		return CMD_RET_USAGE;

	if (argc == 2) {
		seq = simple_strtol(argv[1], NULL, 10);
		if (seq < 0)
			return CMD_RET_USAGE;
	}

	connected = false;
	// fdl_set_loglevel(FDL_LOGLEVEL_DEBUG);

	uclass_get_device_by_seq(UCLASS_SERIAL, seq, &dev);
	if (IS_ERR_OR_NULL(dev)) {
		fdl_err("Not found seq '%d' UART device\n", seq);
		return CMD_RET_FAILURE;
	}
	fdl_debug("device name: %s\n", dev->name);

	ret = fdl_uart_process(dev, true, false);
	if (ret < 0)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	fdl_uart, 2, 1, do_fdl_uart,
	"FDL over UART",
	"<dev_idx>\n"
	"    - Using sequence number 'dev_idx' UART device to download (default: 0)\n"
);

int fdl_uart_download(int dev_idx, bool timeout)
{
	struct udevice *dev;
	int ret;

	uclass_get_device_by_seq(UCLASS_SERIAL, dev_idx, &dev);
	if (IS_ERR_OR_NULL(dev)) {
		fdl_err("Not found seq '%d' UART device\n", dev_idx);
		return -ENODEV;
	}
	fdl_debug("FDL UART device name: %s\n", dev->name);

	// fdl_set_loglevel(FDL_LOGLEVEL_DEBUG);
	ret = fdl_uart_process(dev, timeout, true);
	if (ret < 0)
		return ret;

	return 0;
}
