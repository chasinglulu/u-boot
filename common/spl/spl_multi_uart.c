// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 */

#include <common.h>
#include <gzip.h>
#include <image.h>
#include <log.h>
#include <spl.h>
#include <xyzModem.h>
#include <asm/u-boot.h>
#include <linux/libfdt.h>

#define BUF_SIZE 1024

int __weak spl_board_get_image_info(int index,
           const char **namep, int *type)
{
	static struct images_info {
		const char *name;
		int type;
		bool required;
	} info[] = {
		{ "bl31.img", IH_OS_ARM_TRUSTED_FIRMWARE, true },
		{ "tee.img", IH_OS_TEE, false },
		{ "u-boot.img", IH_OS_U_BOOT, true },
	};

	if (index >= ARRAY_SIZE(info))
		return -EINVAL;

	if (namep)
		*namep = info[index].name;

	if (type)
		*type = info[index].type;

	return info[index].required;
}

/*
 * Information required to load image using ymodem.
 *
 * @image_read: Now of bytes read from the image.
 * @buf: pointer to the previous read block.
 */
struct ymodem_info {
	int image_read;
	char *buf;
};

static int getcymodem(void) {
	if (tstc())
		return (getchar());
	return -1;
}

static ulong spl_uart_load_ymodem(struct spl_load_info *load, ulong offset,
                     ulong size, void *addr)
{
	int res, err, buf_offset;
	struct ymodem_info *info = load->priv;
	char *buf = info->buf;
	ulong copy_size = size;

	while (info->image_read < offset) {
		res = xyzModem_stream_read(buf, BUF_SIZE, &err);
		if (res <= 0)
			break;

		info->image_read += res;
	}

	if (info->image_read > offset) {
		res = info->image_read - offset;
		if (info->image_read % BUF_SIZE)
			buf_offset = (info->image_read % BUF_SIZE);
		else
			buf_offset = BUF_SIZE;

		if (res > copy_size) {
			memcpy(addr, &buf[buf_offset - res], copy_size);
			goto done;
		}
		memcpy(addr, &buf[buf_offset - res], res);
		addr = addr + res;
		copy_size -= res;
	}

	while (info->image_read < offset + size) {
		res = xyzModem_stream_read(buf, BUF_SIZE, &err);
		if (res <= 0)
			break;

		info->image_read += res;
		if (res > copy_size) {
			memcpy(addr, buf, copy_size);
			goto done;
		}
		memcpy(addr, buf, res);
		addr += res;
		copy_size -= res;
	}

done:
	return size;
}

static int spl_uart_load_image(struct spl_image_info *spl_image,
                 struct spl_boot_device *bootdev)
{
	connection_info_t conn;
	ulong size = 0;
	int err, res, ret;
	char buf[BUF_SIZE];
	struct ymodem_info info;

	conn.mode = xyzModem_ymodem;
	ret = xyzModem_stream_open(&conn, &err);
	if (ret) {
		printf("spl: ymodem err - %s\n", xyzModem_error(err));
		return ret;
	}

	res = xyzModem_stream_read(buf, BUF_SIZE, &err);
	if (res <= 0)
		goto end_stream;

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
	    image_get_magic((struct image_header *)buf) == FDT_MAGIC) {
		struct spl_load_info load;

		debug("Found FIT\n");
		load.dev = NULL;
		load.priv = (void *)&info;
		load.filename = NULL;
		load.bl_len = 1;
		info.buf = buf;
		info.image_read = BUF_SIZE;
		load.read = spl_uart_load_ymodem;
		ret = spl_load_simple_fit(spl_image, &load, 0, (void *)buf);

	} else if (IS_ENABLED(CONFIG_SPL_LEGACY_IMAGE_FORMAT) &&
	            image_get_magic((struct image_header *)buf) == IH_MAGIC) {
		struct spl_load_info load;

		debug("Found legacy image\n");
		load.dev = NULL;
		load.priv = (void *)&info;
		load.filename = NULL;
		load.bl_len = 1;
		info.buf = buf;
		info.image_read = BUF_SIZE;
		load.read = spl_uart_load_ymodem;
		ret = spl_load_legacy_img(spl_image, bootdev, &load, 0);
	}

	size = info.image_read;
	/* drop padding bytes of end */
	while ((res = xyzModem_stream_read(buf, BUF_SIZE, &err)) > 0)
		size += res;

end_stream:
	xyzModem_stream_close(&err);
	xyzModem_stream_terminate(false, &getcymodem);

	printf("Loaded %lu bytes file, included image %u bytes\n", size, info.image_read);
	debug("load_addr 0x%lx entry_point: 0x%lx size: %u\n",
	          spl_image->load_addr, spl_image->entry_point, spl_image->size);

	return ret;
}

static int spl_uart_load_multi_images(struct spl_image_info *spl_image,
                        struct spl_boot_device *bootdev)
{
	struct spl_image_info image_info;
	bool has_uboot = false;
	bool has_optee = false;
	bool has_atf = false;
	const char *name;
	int type;
	int err = 0;
	int i;

	for (i = 0; spl_board_get_image_info(i, NULL, NULL) >= 0; i++) {
		memset(&image_info, 0, sizeof(image_info));
		name = NULL;
		type =0;

		if (!spl_board_get_image_info(i, &name, &type))
			continue;
		err = spl_uart_load_image(&image_info, bootdev);
		if (err)
			continue;

		if (image_info.os != type) {
			pr_err("Not expected image (name %s type %d)\n", name, type);
			continue;
		}

		if (image_info.os == IH_OS_ARM_TRUSTED_FIRMWARE
			|| (image_info.os == IH_OS_U_BOOT && !has_atf)) {
			spl_image->name = image_info.name;
			spl_image->load_addr = image_info.load_addr;
			spl_image->entry_point = image_info.entry_point;
		}

		switch (image_info.os) {
		case IH_OS_ARM_TRUSTED_FIRMWARE:
			spl_image->os = IH_OS_ARM_TRUSTED_FIRMWARE;
			has_atf = true;
			break;
		case IH_OS_U_BOOT:
			has_uboot = true;
#if CONFIG_IS_ENABLED(LOAD_FIT) || CONFIG_IS_ENABLED(LOAD_FIT_FULL)
			spl_image->fdt_addr = image_info.fdt_addr;
#endif

			if (has_atf)
				break;

			spl_image->os = IH_OS_U_BOOT;
			break;
		case IH_OS_TEE:
			/* SPL does not support direct jump to optee */
			has_optee = true;
			break;
		default:
			pr_err("Invaild Image: %d\n", image_info.os);
			return -EINVAL;
		}
	}

	if (!has_uboot)
		return -ENODATA;

#if CONFIG_IS_ENABLED(LOAD_STRICT_CHECK)
	if (!has_atf || !has_optee)
		return -ENODATA;
#endif

	return err;
}
SPL_LOAD_IMAGE_METHOD("UART", 0, BOOT_DEVICE_UART, spl_uart_load_multi_images);