// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2013 Xilinx, Inc.
 */
#include <common.h>
#include <command.h>
#include <clk.h>
#if defined(CONFIG_DM) && defined(CONFIG_CLK)
#include <dm.h>
#include <dm/device.h>
#include <dm/root.h>
#include <dm/device-internal.h>
#include <linux/clk-provider.h>
#endif

#if defined(CONFIG_DM) && defined(CONFIG_CLK)
static void show_clks(struct udevice *dev, int depth, int last_flag)
{
	int i, is_last;
	struct udevice *child;
	struct clk *clkp, *parent;
	u32 rate;
	char *unit = "Hz";

	clkp = dev_get_clk_ptr(dev);
	if (device_get_uclass_id(dev) == UCLASS_CLK && clkp) {
		parent = clk_get_parent(clkp);
		if (!IS_ERR(parent) && depth == -1)
			return;
		depth++;
		rate = clk_get_rate(clkp);

		if (rate > 1000000000 && rate % 1000000000 == 0) {
			rate /= 1000000000;
			unit = "GHz";
		} else if (rate > 1000000 && rate % 1000000 == 0) {
			rate /= 1000000;
			unit = "MHz";
		} else if (rate > 1000) {
			rate /= 1000;
			unit = "KHz";
		}

		printf(" %6u%s  %8d        ", rate, unit, clkp->enable_count);

		for (i = depth; i >= 0; i--) {
			is_last = (last_flag >> i) & 1;
			if (i) {
				if (is_last)
					printf("    ");
				else
					printf("|   ");
			} else {
				if (is_last)
					printf("`-- ");
				else
					printf("|-- ");
			}
		}

		printf("%s\n", dev->name);
	}

	list_for_each_entry(child, &dev->child_head, sibling_node) {
		if (child == dev)
			continue;

		is_last = list_is_last(&child->sibling_node, &dev->child_head);
		show_clks(child, depth, (last_flag << 1) | is_last);
	}
}

int __weak soc_clk_dump(void)
{
	struct udevice *dev;
	struct uclass *uc;
	int ret;

	ret = uclass_get(UCLASS_CLK, &uc);
	if (ret)
		return ret;

	printf("      Rate       Usecnt      Name\n");
	printf("---------------------------------------------\n");

	uclass_foreach_dev(dev, uc)
		show_clks(dev, -1, 0);

	return 0;
}

int __weak soc_clk_setfreq(struct udevice *dev, int idx, long rate)
{
	struct clk *clk;

	clk = dev_get_clk_ptr(dev);
	if (!clk) {
		printf("clock '%s' not found.\n", dev->name);
		return -EINVAL;
	}

	rate = clk_set_rate(clk, rate);
	if (rate < 0) {
		printf("set_rate failed: %ld\n", rate);
		return -ENXIO;
	}

	printf("set_rate returns %lu\n", rate);
	return 0;
}
#else
int __weak soc_clk_dump(void)
{
	puts("Not implemented\n");
	return 1;
}

int __weak soc_clk_setfreq(struct udevice *dev, int idx, long rate)
{
	puts("Not implemented\n");
	return 1;
}
#endif

static int do_clk_dump(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	int ret;

	ret = soc_clk_dump();
	if (ret < 0) {
		printf("Clock dump error %d\n", ret);
		ret = CMD_RET_FAILURE;
	}

	return ret;
}

static int do_clk_setfreq(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	struct udevice *dev;
	long freq;
	int ret, idx = 0;
	const char *p;
	int len;
	char *name;

	if (argc != 3)
		return CMD_RET_USAGE;

	p = strchr(argv[1], ':');
	if (p) {
		len = p - argv[1] + 1;
		idx = dectoul(p + 1, NULL);
	} else
		len = strlen(argv[1]) + 1;

	name = strndup(argv[1], len);
	name[len - 1] = '\0';

	if (uclass_get_device_by_name(UCLASS_CLK, name, &dev)) {
		printf("Invaild device name '%s'\n", name);
		return CMD_RET_FAILURE;
	}

	freq = dectoul(argv[2], NULL);
	ret = soc_clk_setfreq(dev, idx, freq);
	if (ret < 0) {
		printf("Clock setfreq error %d\n", ret);
		return CMD_RET_FAILURE;
	}

	return 0;
}

static struct cmd_tbl cmd_clk_sub[] = {
	U_BOOT_CMD_MKENT(dump, 1, 1, do_clk_dump, "", ""),
	U_BOOT_CMD_MKENT(setfreq, 3, 1, do_clk_setfreq, "", ""),
};

static int do_clk(struct cmd_tbl *cmdtp, int flag, int argc,
		  char *const argv[])
{
	struct cmd_tbl *c;

	if (argc < 2)
		return CMD_RET_USAGE;

	/* Strip off leading 'clk' command argument */
	argc--;
	argv++;

	c = find_cmd_tbl(argv[0], &cmd_clk_sub[0], ARRAY_SIZE(cmd_clk_sub));

	if (c)
		return c->cmd(cmdtp, flag, argc, argv);
	else
		return CMD_RET_USAGE;
}

#ifdef CONFIG_SYS_LONGHELP
static char clk_help_text[] =
	"dump - Print clock frequencies\n"
	"clk setfreq [clk:index] [freq] - Set indexed clock frequency";
#endif

U_BOOT_CMD(clk, 4, 1, do_clk, "CLK sub-system", clk_help_text);
