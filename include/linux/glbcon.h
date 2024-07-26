/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __LAGUNA_GLBCON_H
#define __LAGUNA_GLBCON_H

#include <dm/ofnode.h>
#include <linux/err.h>

struct ofnode_phandle_args;
struct udevice;

struct subsysctl {
	struct udevice *dev;
	/*
	 * Written by of_xlate. In the future, we might add more fields here.
	 */
	uint16_t shift;
	uint16_t width;
	uint64_t data;
};

struct subsys_ctl_bulk {
	struct subsysctl *resets;
	unsigned int count;
};


#if CONFIG_IS_ENABLED(DM_GLBCON)
/**
 * subsysctl_assert - Assert a syscon signal.
 *
 * This function will assert the specified syscon signal, thus controlling the
 * affected HW module(s). Depending on the syscon controller hardware, the syscon
 * signal will stay asserted, or the hardware may autonomously clear the syscon signal itself.
 *
 * @subsys_ctl:	A syscon control struct that was previously successfully
 *		requested by reset_get_by_*().
 * Return: 0 if OK, or a negative error code.
 */
int subsysctl_assert(struct subsysctl *subsys_ctl);
/**
 * subsysctl_status - Check syscon signal status.
 *
 * @subsys_ctl:	The syscon signal to check.
 * Return: 0 if asserted, positive if deasserted, or a negative
 *           error code.
 */
int subsysctl_status(struct subsysctl *subsys_ctl);

/**
 * devm_subsys_control_get - resource managed subsysctl_get_by_name()
 * @dev: device to be asserted by the controller
 * @id: reset line name
 *
 * Managed subsysctl_get_by_name(). For syscon controllers returned
 * from this function, subsysctl_free() is called automatically on driver
 * detach.
 *
 * Returns a struct subsysctl or IS_ERR() condition containing errno.
 */
struct subsysctl *devm_subsys_control_get(struct udevice *dev, const char *id);

/**
 * subsysctl_get_by_name - Get/request a syscon signal by name.
 *
 * This looks up and requests a syscon signal. The name is relative to the
 * client device; each device is assumed to have n syscon signals associated
 * with it somehow, and this function finds and requests one of them. The
 * mapping of client device syscon signal names to provider syscon signal may be
 * via device-tree properties, board-provided mapping tables, or some other
 * mechanism.
 *
 * @dev:	The client device.
 * @name:	The name of the syscon signal to request, within the client's
 *		list of syscon signals.
 * @reset_ctl:	A pointer to a syscon struct to initialize.
 * Return: 0 if OK, or a negative error code.
 */
int subsysctl_get_by_name(struct udevice *dev, const char *name, struct subsysctl *subsys_ctl);

/**
 * subsysctl_get_by_index_nodev - Get/request a syscon signal by integer index
 * without a device.
 *
 * This is a version of subsysctl_get_by_index() that does not use a device.
 *
 * @node:	The client ofnode.
 * @index:	The index of the syscon signal to request, within the client's
 *		list of syscon signals.
 * @subsys_ctl	A pointer to a syscon struct to initialize.
 * Return: 0 if OK, or a negative error code.
 */
int subsysctl_get_by_index_nodev(ofnode node, int index, struct subsysctl *subsys_ctl);

/**
 * subsysctl_get_by_index - Get/request a syscon signal by integer index.
 *
 * This looks up and requests a syscon signal. The index is relative to the
 * client device; each device is assumed to have n syscon signals associated
 * with it somehow, and this function finds and requests one of them. The
 * mapping of client device syscon signal indices to provider syscon signals may
 * be via device-tree properties, board-provided mapping tables, or some other
 * mechanism.
 *
 * @dev:	The client device.
 * @index:	The index of the syscon signal to request, within the client's
 *		list of syscon signals.
 * @subsys_ctl	A pointer to a syscon struct to initialize.
 * Return: 0 if OK, or a negative error code.
 */
int subsysctl_get_by_index(struct udevice *dev, int index, struct subsysctl *subsys_ctl);
#else
static inline int subsysctl_assert(struct subsysctl *subsys_ctl)
{
	return 0;
}

static inline int subsysctl_status(struct subsysctl *subsys_ctl)
{
	return -ENOTSUPP;
}

static inline struct subsysctl *devm_subsys_control_get(struct udevice *dev, const char *id)
{
	return ERR_PTR(-ENOTSUPP);
}

static inline int subsysctl_get_by_name(struct udevice *dev, const char *name,
			    struct subsysctl *subsys_ctl)
{
	return -ENOTSUPP;
}

static inline int subsysctl_get_by_index_nodev(ofnode node, int index,
			    struct subsysctl *subsys_ctl)
{
	return -ENOTSUPP;
}

static inline int subsysctl_get_by_index(struct udevice *dev, int index,
			    struct subsysctl *subsys_ctl)
{
	return -ENOTSUPP;
}
#endif

static inline bool subsysctl_valid(struct subsysctl *subsys_ctl)
{
	return !!subsys_ctl->dev;
}

#endif