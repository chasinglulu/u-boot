// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 Charleye <wangkart@aliyun.com>
 */

#include <common.h>
#include <dm.h>
#include <pci.h>
#include "ufs.h"

static int ufs_pci_bind(struct udevice *udev)
{
	static int ndev_num;
	char name[20];

    printf("%s\n", __func__);

	sprintf(name, "ufs#%d", ndev_num++);

	return device_set_name(udev, name);
}

static int ufs_pci_probe(struct udevice *udev)
{
    printf("%s\n", __func__);
	// struct nvme_dev *ndev = dev_get_priv(udev);
	// struct pci_child_plat *pplat;

	// pplat = dev_get_parent_plat(udev);
	// sprintf(ndev->vendor, "0x%.4x", pplat->vendor);

	// ndev->instance = trailing_strtol(udev->name);
	// ndev->bar = dm_pci_map_bar(udev, PCI_BASE_ADDRESS_0,
	// 		PCI_REGION_MEM);
	// return nvme_init(udev);
    return 0;
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