// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Charleye <wangkart@aliyun.com>
 *
 */

#include <clk.h>
#include <common.h>
#include <dm.h>
#include <init.h>
#include <malloc.h>
#include <asm/arch/clk.h>
#include <dm/device-internal.h>
#include <dm/device_compat.h>

#if defined(CONFIG_CLK_AXERA_V2)
/**
 * soc_clk_dump() - Print clock frequencies
 * Returns zero on success
 *
 * Implementation for the clk dump command.
 */
int soc_clk_dump(void)
{
	struct udevice *dev;
	struct uclass *uc;
	int ret, count, i;
	char *unit = "Hz";

	ret = uclass_get(UCLASS_CLK, &uc);
	if (ret)
		return ret;

	printf("      Rate            Name                       Index\n");
	uclass_foreach_dev(dev, uc) {
		if (device_probe(dev)) {
			pr_err("Failed to probe device %s\n", dev->name);
			return -ENXIO;
		}
		count = dev_read_string_count(dev, "clock-output-names");
		printf("---------------------------------------------------------\n");
		printf("'%s' output clocks information:\n\n", dev->name);
		for (i = 0; i < count; i++) {
			struct clk clk;
			unsigned long rate;
			const char *name;

			clk.id = i;
			ret = clk_request(dev, &clk);
			if (ret < 0)
				return ret;

			rate = clk_get_rate(&clk);
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

			clk_free(&clk);

			dev_read_string_index(dev, "clock-output-names", i, &name);

			if ((rate == (unsigned long)-ENOSYS) ||
			    (rate == (unsigned long)-ENXIO))
				printf(" %10s           ", "unknown");
			else
				printf(" %6lu %s        ", rate, unit);

			printf("%s", name);
			printf("%*d\n", (int)(32 - strlen(name)), i);
		}
	}

	return 0;
}

int soc_clk_setfreq(struct udevice *dev, int idx, long rate)
{
	struct clk clk;
	int ret;
	long freq;

	if (device_probe(dev))
		return -ENXIO;

	clk.id = idx;

	ret = clk_request(dev, &clk);
	if (ret < 0)
		return ret;

	freq = clk_set_rate(&clk, rate);
	if (freq < 0)
		dev_err(dev, "old = %lu, new = %ld\n", rate, freq);
	else
		dev_err(dev, "old = %lu, new = %lu\n", rate, freq);;

	clk_free(&clk);

	return rate;
}
#endif