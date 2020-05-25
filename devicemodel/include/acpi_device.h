/*
 * Copyright (C) 2020 HWTC Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _ACPI_DEVICE_H_
#define _ACPI_DEVICE_H_

#include "types.h"

struct acpi_device {
	char name[32];
	struct pci_vdev *bus_vdev;
	uint32_t bus_vendor;
	uint32_t bus_device;

	int32_t phys_irq;
	int32_t virt_irq;

	int (*bind)(struct acpi_device *, struct pci_vdev *);
	void (*unbind)(struct acpi_device *);
	void (*write_dsdt)(struct acpi_device *);

	bool valid;
};

int acpi_device_bind(struct pci_vdev *dev, uint32_t bus_vendor, uint32_t bus_device);
void acpi_device_unbind(struct pci_vdev *dev);
void acpi_device_write_dsdt(void);

#define DEFINE_ACPI_DEVICE(d) DATA_SET(acpi_device_set, d)

#endif

