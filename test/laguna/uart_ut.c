// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 * Unit tests for Main Domain UART functions
 */

#include <common.h>
#include <test/laguna.h>
#include <test/test.h>
#include <test/ut.h>
#include <dm/uclass.h>
#include <dm/device.h>
#include <serial.h>

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

static void _serial_puts(struct udevice *dev, const char *str)
{
	while (*str)
		_serial_putc(dev, *str++);
}

static int __serial_getc(struct udevice *dev)
{
	struct dm_serial_ops *ops = serial_get_ops(dev);
	int err;
	int timeout = 1000;
	int elapsed = 0;
	unsigned int ticks;

	do {
		ticks = get_timer(0);
		err = ops->getc(dev);
		if (elapsed > timeout)
			return -ETIMEDOUT;
		elapsed += get_timer(ticks);
	} while (err == -EAGAIN);

	return err >= 0 ? err : 0;
}

static int _serial_getc(struct udevice *dev)
{
	return __serial_getc(dev);
}

static int test_uart_loopback1(struct unit_test_state *uts)
{
	struct udevice *dev2, *dev3;
	char buf[32] = "Hello World!";
	char dst[32];
	int ch;
	int count = strlen(buf);
	int off = 0;

	ut_assertok(console_record_reset_enable());
	ut_assertok(uclass_get_device_by_seq(UCLASS_SERIAL, 2, &dev2));
	printf("device name: %s\n", dev2->name);
	ut_assert_nextlinen("device name: uart@0E404000");
	ut_assert_console_end();

	ut_assertok(console_record_reset_enable());
	uclass_get_device_by_seq(UCLASS_SERIAL, 3, &dev3);
	printf("device name: %s\n", dev3->name);
	ut_assert_nextlinen("device name: uart@0E405000");
	ut_assert_console_end();

	/* Send data from UART2 into UART3 */
	_serial_puts(dev2, buf);
	while (count--) {
		ch = _serial_getc(dev3);
		if (ch > 0)
			sprintf(dst + off, "%c", ch);
		else
			ut_assertf(0, "timeout");
		off++;
	}
	dst[off] = '\0';
	ut_asserteq_str(buf, dst);

	/* Send data from UART3 into UART2 */
	count = strlen(buf);
	off = 0;
	memset(dst, 0x0, sizeof(dst));
	_serial_puts(dev3, buf);
	while (count--) {
		ch = _serial_getc(dev2);
		if (ch > 0)
			sprintf(dst + off, "%c", ch);
		else
			ut_assertf(0, "timeout");
		off++;
	}
	dst[off] = '\0';
	ut_asserteq_str(buf, dst);

	printf("%s: pass\n", __func__);
	return 0;
}
LAGUNA_TEST(test_uart_loopback1, UT_TESTF_CONSOLE_REC);

static int test_uart_loopback2(struct unit_test_state *uts)
{
	struct udevice *dev4, *dev5;
	char buf[32] = "Hello World!";
	char dst[32];
	int ch;
	int count = strlen(buf);
	int off = 0;

	ut_assertok(console_record_reset_enable());
	ut_assertok(uclass_get_device_by_seq(UCLASS_SERIAL, 4, &dev4));
	printf("device name: %s\n", dev4->name);
	ut_assert_nextlinen("device name: uart@0E410000");
	ut_assert_console_end();

	ut_assertok(console_record_reset_enable());
	uclass_get_device_by_seq(UCLASS_SERIAL, 5, &dev5);
	printf("device name: %s\n", dev5->name);
	ut_assert_nextlinen("device name: uart@0E411000");
	ut_assert_console_end();

	/* Send data from UART4 into UART5 */
	_serial_puts(dev4, buf);
	while (count--) {
		ch = _serial_getc(dev5);
		if (ch > 0)
			sprintf(dst + off, "%c", ch);
		else
			ut_assertf(0, "timeout");
		off++;
	}
	dst[off] = '\0';
	ut_asserteq_str(buf, dst);

	/* Send data from UART3 into UART2 */
	count = strlen(buf);
	off = 0;
	memset(dst, 0x0, sizeof(dst));
	_serial_puts(dev5, buf);
	while (count--) {
		ch = _serial_getc(dev4);
		if (ch > 0)
			sprintf(dst + off, "%c", ch);
		else
			ut_assertf(0, "timeout");
		off++;
	}
	dst[off] = '\0';
	ut_asserteq_str(buf, dst);

	printf("%s: pass\n", __func__);
	return 0;
}
LAGUNA_TEST(test_uart_loopback2, UT_TESTF_CONSOLE_REC);