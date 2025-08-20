// SPDX-License-Identifier: GPL-2.0+
/*
 * FDL over USB
 *
 * Copyright (C) 2025 Charleye <wangkart@aliyun.com>
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
#include <hexdump.h>
#include <malloc.h>

#include <asm/arch/fdl_usb_drv.h>

static int maxpacket;

static void fdl_response_tx(struct udevice *dev, const char *response)
{
	int len = fdl_get_resp_size(response);
	int ret;

	FDL_DUMP("RESPONSE: ", response, len);
	ret = fdl_usb_drv_write(dev, response, len);
	if (ret < 0)
		fdl_err("Unable to send response: %d\n", ret);
}

int fdl_usb_rx_timeout(struct udevice *dev, char *buf, size_t len, uint32_t timeout)
{
	uint64_t ticks, elapsed = 0;
	int ret;

	while (true) {
		ticks = get_timer(0);
		ret = fdl_usb_drv_read(dev, buf, len);
		elapsed += get_timer(ticks);

		if (elapsed > timeout) {
			ret = -ETIMEDOUT;
			break;
		}

		if (likely(!ret))
			break;
	}

	return ret;
}

int fdl_usb_data_download(struct udevice *dev, char *data, size_t size)
{
	uint32_t timeout = CONFIG_VAL(FDL_TIMEOUT);

	return fdl_usb_rx_timeout(dev, data, size, timeout);
}

static int fdl_packet_rx(struct udevice *dev, char **packet)
{
	uint32_t timeout = CONFIG_VAL(FDL_TIMEOUT);
	struct fdl_header *head = NULL;
	struct fdl_packet *pp = NULL;
	uint32_t magic, size;
	char *start, *buffer;
	int drop_len, remaining;
	int total, rx_max_len;
	int handshake = 3;
	int ch, ret;

	if (unlikely(!packet))
		return -EINVAL;

	buffer = malloc(maxpacket);
	if (!buffer)
		return -ENOMEM;

restart:
	start = buffer;
	ret = fdl_usb_rx_timeout(dev, buffer, maxpacket, timeout);
	if (ret < 0)
		goto failed;

	/* Drop leading unrecognized character */
	while (true) {
		ch = *start;

		if (ch == 0x9F || ch == 0x3C)
			break;

		start++;
		if (start - buffer >= maxpacket)
			goto restart;
	}

	drop_len = start - buffer;
	remaining = maxpacket - drop_len;
	if (unlikely(drop_len)) {
		memcpy(buffer, start, remaining);
		memset(buffer + remaining, 0, drop_len);
		start = buffer;
	}

	/* handshake packet */
	if (ch == 0x3C) {
		handshake--;
		while (handshake--) {
			ch = *start++;
			if (ch != 0x3C) {
				fdl_err("handshake failed\n");
				handshake = 3;
				goto restart;
			}
		}

		fdl_command(FDL_COMMAND_HANDSHAKE, buffer);
		*packet = buffer;
		return 0;
	}

	/* command packet */
	pp = (void *)buffer;
	head = (void *)pp;
	magic = head->magic;
	if (unlikely(magic != FDL_MAGIC)) {
		fdl_err("Wrong magic (0x%X)\n", magic);
		goto restart;
	}

	size = head->size;
	total = size + FDL_PROTO_FIXED_LEN;
	if (unlikely(total > remaining)) {
		rx_max_len = ROUND(total, maxpacket);
		start = malloc(rx_max_len);
		if (!start) {
			fdl_err("Out of memory\n");
			ret = -ENOMEM;
			goto failed;
		}
		memcpy(start, buffer, remaining);
		free(buffer);
		buffer = start;

		ret = fdl_usb_rx_timeout(dev, buffer + remaining,
		                 total - remaining, timeout);
		if (ret < 0)
			goto failed;
	}

	*packet = buffer;
	return total;

failed:
	if (buffer)
		free(buffer);

	return ret;
}

static int fdl_usb_process(struct udevice *dev, bool timeout, bool need_ack)
{
	char *cmd_packet = NULL;
	char response[FDL_RESPONSE_LEN];
	struct fdl_info fdl;
	struct fdl_header *head;
	struct fdl_tx_info __maybe_unused *payload;
	char __maybe_unused *fdl_data = NULL;
	uint32_t __maybe_unused fdl_data_len;
	uint32_t __maybe_unused checksum;
	int ret;

	fdl_init(NULL, 0);
	memset(response, 0, sizeof(response));

	fdl_usb_drv_open(dev);
	maxpacket = fdl_usb_drv_get_maxpacket(dev);

	if (unlikely(need_ack)) {
		fdl_debug("Acknowledge the previous execute command\n");
		fdl_okay_null(FDL_RESPONSE_ACK, response);
		fdl_response_tx(dev, response);;
	}

	while (true) {
		ret = fdl_packet_rx(dev, &cmd_packet);
		if (ret < 0) {
			if (!timeout && ret == -ETIMEDOUT) {
				fdl_err("Timeout to receive command packet\n");
				continue;
			}

			fdl_err("Failed to receive command packet [ret %d]\n", ret);
			return CMD_RET_FAILURE;
		}
		FDL_DUMP("COMMAND: ", cmd_packet, ret);

		ret = fdl_packet_check(&fdl, cmd_packet, response);
		if (ret < 0)
			goto failed;

		ret = fdl_handle_command(&fdl, response);
		if (ret < 0) {
			head = (void *)&fdl;
			fdl_err("Unable to handle 0x%X command\n", head->bytecode);
			goto failed;
		}
		if (fdl_compare_command_by_tag(&fdl, FDL_COMMAND_END_DATA)) {
			ret = fdl_data_complete(response);
			if (ret < 0)
				goto failed;
		}
		fdl_response_tx(dev, response);

#ifdef CONFIG_FDL_RAW_DATA_DL
		if (!fdl_compare_command_by_tag(&fdl, FDL_COMMAND_MIDST_DATA)) {
			if (cmd_packet) {
				free(cmd_packet);
				cmd_packet = NULL;
			}
			continue;
		}

		payload = (void *)&fdl_tx_payload_info;
		fdl_data_len = ROUND(payload->len, maxpacket);
		fdl_data = malloc(fdl_data_len);
		if (unlikely(!fdl_data)) {
			fdl_debug("Out of memory\n");
			fdl_fail_null(FDL_RESPONSE_OPERATION_FAIL, response);
			goto failed;
		}
		fdl_debug("fdl_data_len: %d\n", fdl_data_len);

		ret = fdl_usb_data_download(dev, fdl_data, fdl_data_len);
		if (ret < 0) {
			fdl_fail_null(FDL_RESPONSE_OPERATION_FAIL, response);
			goto failed;
		}
		FDL_DUMP("PAYLOAD DATA: ", fdl_data, fdl_data_len);

		if (payload->chksum_en) {
			checksum = fdl_rawdata_checksum((void *)fdl_data, payload->len);
			if (checksum != payload->chksum) {
				fdl_debug("Invalid raw data checksum (expected %.8x, found %.8x)\n", checksum, payload->chksum);
				ret = -EINVAL;
				fdl_fail_null(FDL_RESPONSE_VERIFY_CHECKSUM_FAIL, response);
				goto failed;
			}
		}

		ret = fdl_data_download(fdl_data, payload->len, response);
		if (ret < 0)
			goto failed;

		fdl_debug("Payload data download over USB completed\n");
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

static int do_fdl_usb(struct cmd_tbl *cmdtp, int flag, int argc,
                        char *const argv[])
{
	struct udevice *dev;
	int ret;

	fdl_usb_drv_get(&dev);
	if (IS_ERR_OR_NULL(dev)) {
		fdl_err("Not found FDL USB device\n");
		return CMD_RET_FAILURE;
	}
	fdl_debug("device name: %s\n", dev->name);

	// fdl_set_loglevel(FDL_LOGLEVEL_DEBUG);
	ret = fdl_usb_process(dev, true, false);
	if (ret < 0)
		return CMD_RET_FAILURE;

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	fdl_usb, 1, 1, do_fdl_usb,
	"FDL over USB",
	"\n"
	"    - Using FDL USB device to download\n"
);

int fdl_usb_download(int dev_idx, bool timeout)
{
	struct udevice *dev;
	int ret;

	fdl_usb_drv_get(&dev);
	if (IS_ERR_OR_NULL(dev)) {
		fdl_err("Not found FDL USB device\n");
		return -ENODEV;
	}
	fdl_debug("device name: %s\n", dev->name);

	// fdl_set_loglevel(FDL_LOGLEVEL_DEBUG);
	ret = fdl_usb_process(dev, timeout, true);
	if (ret < 0)
		return ret;

	return 0;
}