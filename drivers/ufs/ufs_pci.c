// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <ufs.h>
#include <dm.h>
#include <pci.h>
#include <dm/device_compat.h>

#include "ufs.h"

static int ufs_pci_bind(struct udevice *udev)
{
	static int ndev_num;
	char name[20];
	struct udevice *scsi_dev;

	sprintf(name, "ufs#%d", ndev_num++);
	device_set_name(udev, name);

	return ufs_scsi_bind(udev, &scsi_dev);
}

static int ufs_pci_probe(struct udevice *udev)
{
	int err = ufshcd_probe(udev, NULL);
	if (err)
		dev_err(udev, "ufshcd_probe() failed %d\n", err);

	return err;
}

U_BOOT_DRIVER(ufs_pci) = {
	.name	= "ufs_pci",
	.id	= UCLASS_UFS,
	.bind	= ufs_pci_bind,
	.probe	= ufs_pci_probe,
};

struct pci_device_id ufs_supported[] = {
	{ PCI_DEVICE_CLASS(PCI_CLASS_STORAGE_UFS, ~0) },
	{}
};

U_BOOT_PCI_DEVICE(ufs_pci, ufs_supported);