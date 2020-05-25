/*
 * Copyright (C) 2020 HWTC Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "log.h"
#include "pci_core.h"
#include "acpi_device.h"

SET_DECLARE(acpi_device_set, struct acpi_device);

int acpi_device_bind(struct pci_vdev *dev, uint32_t bus_vendor, uint32_t bus_device)
{
	struct acpi_device **adpp, *adev;
	int ret = 0;

	SET_FOREACH(adpp, acpi_device_set) {
		adev = *adpp;
		if (adev->bus_vendor == bus_vendor && adev->bus_device == bus_device) {
			pr_info("bind %s to virt-%d:%d:%d\n", adev->name,
					dev->bus, dev->slot, dev->func);
			adev->bus_vdev = dev;

			if (adev->bind)
				ret = adev->bind(adev, dev);

			if (!ret)
				adev->valid = true;
		}
	}

	return ret;
}

void acpi_device_unbind(struct pci_vdev *dev)
{
	struct acpi_device **adpp, *adev;

	SET_FOREACH(adpp, acpi_device_set) {
		adev = *adpp;
		if (adev->bus_vdev == dev) {
			pr_info("unbind %s from virt-%d:%d:%d\n", adev->name,
					dev->bus, dev->slot, dev->func);

			adev->valid = false;

			if (adev->bind)
				adev->unbind(adev);

			adev->bus_vdev = NULL;
		}
	}
}

void acpi_device_write_dsdt(void)
{
	struct acpi_device **adpp, *adev;

	SET_FOREACH(adpp, acpi_device_set) {
		adev = *adpp;
		if (adev->valid && adev->write_dsdt)
			adev->write_dsdt(adev);
	}
}

/* this ensure at least one acpi device exist in acpi_device_set */
static struct acpi_device dummy = {
	.name       = "dummy",
};
DEFINE_ACPI_DEVICE(dummy);

