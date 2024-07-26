// SPDX-License-Identifier: GPL-2.0
/*
 * Global SYSCTL driver for Laguna SoCs
 *
 * Copyright (C) 2024 Charleye <wangkart@liyun.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/of_access.h>
#include <errno.h>
#include <regmap.h>
#include <linux/err.h>
#include <syscon.h>
#include <linux/glbcon.h>
#include <dm/devres.h>

struct subsysctl_ops {
	int (*of_xlate)(struct subsysctl *subsys_ctl,
			struct ofnode_phandle_args *args);
	int (*status)(struct subsysctl *subsys_ctl);
	int (*write)(struct subsysctl *subsys_ctl);
};

static inline struct subsysctl_ops *subsysctl_dev_ops(struct udevice *dev)
{
	return (struct subsysctl_ops *)dev->driver->ops;
}

static int subsysctl_of_xlate_default(struct subsysctl *subsys_ctl,
				  struct ofnode_phandle_args *args)
{
	debug("%s(subsys_ctl=%p)\n", __func__, subsys_ctl);

	if (args->args_count != 2) {
		debug("Invalid args_count: %d\n", args->args_count);
		return -EINVAL;
	}

	subsys_ctl->shift = args->args[0];
	subsys_ctl->data = args->args[1];

	return 0;
}

static int subsysctl_get_by_index_tail(int ret, ofnode node,
				   struct ofnode_phandle_args *args,
				   const char *list_name, int index,
				   struct subsysctl *subsys_ctl)
{
	struct udevice *dev_subsys;
	struct subsysctl_ops *ops;

	assert(subsys_ctl);
	subsys_ctl->dev = NULL;
	if (ret)
		return ret;

	ret = uclass_get_device_by_ofnode(UCLASS_NOP, args->node,
					  &dev_subsys);
	if (ret) {
		debug("%s: uclass_get_device_by_ofnode() failed: %d\n",
		      __func__, ret);
		debug("%s %d\n", ofnode_get_name(args->node), args->args[0]);
		return ret;
	}
	ops = subsysctl_dev_ops(dev_subsys);

	subsys_ctl->dev = dev_subsys;
	if (ops->of_xlate)
		ret = ops->of_xlate(subsys_ctl, args);
	else
		ret = subsysctl_of_xlate_default(subsys_ctl, args);
	if (ret) {
		debug("of_xlate() failed: %d\n", ret);
		return ret;
	}

	return 0;
}

int subsysctl_get_by_index(struct udevice *dev, int index,
				    struct subsysctl *subsys_ctl)
{
	struct ofnode_phandle_args args;
	int ret;

	ret = dev_read_phandle_with_args(dev, "syscons", "#syscon-cells", 0,
				    index, &args);

	return subsysctl_get_by_index_tail(ret, dev_ofnode(dev), &args, "syscons",
				    index > 0, subsys_ctl);
}

int subsysctl_get_by_index_nodev(ofnode node, int index,
				    struct subsysctl *subsys_ctl)
{
	struct ofnode_phandle_args args;
	int ret;

	ret = ofnode_parse_phandle_with_args(node, "syscons", "#syscon-cells", 0,
				    index, &args);

	return subsysctl_get_by_index_tail(ret, node, &args, "syscons",
				    index > 0, subsys_ctl);
}

int subsysctl_get_by_name(struct udevice *dev, const char *name,
				    struct subsysctl *subsys_ctl)
{
	int index;

	debug("%s(dev=%p, name=%s, subsys_ctl=%p)\n", __func__, dev, name,
				    subsys_ctl);
	memset(subsys_ctl, 0, sizeof(*subsys_ctl));

	index = dev_read_stringlist_search(dev, "syscon-names", name);
	if (index < 0) {
		debug("fdt_stringlist_search() failed: %d\n", index);
		return index;
	}

	return subsysctl_get_by_index(dev, index, subsys_ctl);
}

struct subsysctl *devm_subsys_control_get(struct udevice *dev, const char *id)
{
	int rc;
	struct subsysctl *subsys_ctl;

	subsys_ctl = devres_alloc(NULL, sizeof(struct subsysctl),
				    __GFP_ZERO);
	if (unlikely(!subsys_ctl))
		return ERR_PTR(-ENOMEM);

	rc = subsysctl_get_by_name(dev, id, subsys_ctl);
	if (rc)
		return ERR_PTR(rc);

	devres_add(dev, subsys_ctl);
	return subsys_ctl;
}

int subsysctl_assert(struct subsysctl *subsys_ctl)
{
	struct subsysctl_ops *ops = subsysctl_dev_ops(subsys_ctl->dev);

	debug("%s(subsys_ctl=%p)\n", __func__, subsys_ctl);

	return ops->write ? ops->write(subsys_ctl) : 0;
}

int subsysctl_status(struct subsysctl *subsys_ctl)
{
	struct subsysctl_ops *ops = subsysctl_dev_ops(subsys_ctl->dev);

	debug("%s(subsys_ctl=%p)\n", __func__, subsys_ctl);

	return ops->status ? ops->status(subsys_ctl) : 0;
}

struct lua_sysctl {
	struct udevice *dev;
	struct regmap *regmap;
	uint32_t offset;
	uint32_t mask;
	uint32_t field_width;
};

/**
 * lua_sysctl_probe() - Basic probe
 * @dev:	corresponding lua_sysctl device
 *
 * Return: 0 if all goes good, else appropriate error message.
 */
static int lua_sysctl_probe(struct udevice *dev)
{
	struct lua_sysctl *priv;

	priv = dev_get_priv(dev);
	priv->dev = dev;

	priv->regmap = syscon_regmap_lookup_by_phandle(dev, "regmap");
	if (IS_ERR(priv->regmap))
		return -ENODEV;

	priv->offset = dev_read_u32_default(dev, "offset", 0);
	priv->mask = dev_read_u32_default(dev, "mask", 0);
	priv->field_width = dev_read_u32_default(dev, "field-width", 0);

	return 0;
}

static int lua_sysctl_status(struct subsysctl *subsys_ctl)
{
	struct lua_sysctl *priv = dev_get_priv(subsys_ctl->dev);
	uint16_t width = subsys_ctl->width ?: priv->field_width;
	uint32_t val;
	uint32_t mask;

	mask = ((~0U) >> (32 - width)) << subsys_ctl->shift;
	if (!(priv->mask & mask))
		return -EINVAL;

	regmap_read(priv->regmap, priv->offset, &val);
	val &= mask;

	if ((val >> subsys_ctl->shift) == subsys_ctl->data)
		return 1;

	return 0;
}

static int lua_sysctl_write(struct subsysctl *subsys_ctl)
{
	struct lua_sysctl *priv = dev_get_priv(subsys_ctl->dev);
	uint16_t width = subsys_ctl->width ?: priv->field_width;
	uint32_t val;
	uint32_t mask;

	mask = ((~0U) >> (32 - width)) << subsys_ctl->shift;
	if (!(priv->mask & mask))
		return -EINVAL;

	regmap_read(priv->regmap, priv->offset, &val);
	val &= ~mask;
	val |= subsys_ctl->data << subsys_ctl->shift;

	regmap_write(priv->regmap, priv->offset, val);

	return 0;
}

static const struct subsysctl_ops lua_sysctl_ops = {
	.status = lua_sysctl_status,
	.write = lua_sysctl_write,
};

static const struct udevice_id lua_sysctl_ids[] = {
	{ .compatible = "axera,laguna-sysctl" },
	{}
};

U_BOOT_DRIVER(lua_sysctl) = {
	.name = "sysctl",
	.of_match = lua_sysctl_ids,
	.id = UCLASS_NOP,
	.probe = lua_sysctl_probe,
	.priv_auto = sizeof(struct lua_sysctl),
	.ops = &lua_sysctl_ops,
};
