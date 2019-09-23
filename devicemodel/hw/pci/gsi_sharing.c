/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <sys/queue.h>
#include <pciaccess.h>

#include "pci_core.h"
#include "mevent.h"

#define MAX_DEV_PER_GSI 4

/* physical info of the GSI sharing group */
struct gsi_sharing_group {
	uint8_t gsi;
	/* the number of devices that are sharing the same GSI */
	int shared_dev_num;
	struct {
		char *dev_name;
		int assigned_to_this_vm;
	} dev[MAX_DEV_PER_GSI];

	LIST_ENTRY(gsi_sharing_group) gsg_list;
};
static LIST_HEAD(gsg_struct, gsi_sharing_group) gsg_head;

static int
update_gsi_sharing_info(char *dev_name, uint8_t gsi)
{
	int gsi_shared = 0;
	struct gsi_sharing_group *group = NULL;

	LIST_FOREACH(group, &gsg_head, gsg_list) {
		if (gsi != group->gsi)
			continue;

		if (group->shared_dev_num >= MAX_DEV_PER_GSI) {
			warnx("max %d devices share one GSI", MAX_DEV_PER_GSI);
			return -EINVAL;
		}

		gsi_shared = 1;
		break;
	}

	if (gsi_shared == 0) {
		group = calloc(1, sizeof(struct gsi_sharing_group));
		if (!group) {
			warnx("%s: calloc FAIL!", __func__);
			return -ENOMEM;
		}

		group->gsi = gsi;
		group->shared_dev_num = 0;

		LIST_INSERT_HEAD(&gsg_head, group, gsg_list);
	}

	if (group != NULL) {
		group->dev[group->shared_dev_num].dev_name = dev_name;
		group->dev[group->shared_dev_num].assigned_to_this_vm = 0;
		group->shared_dev_num++;
	}

	return 0;
}

/*
 * return 1 if MSI/MSI-x is supported
 * return 0 if MSI/MSI-x is NOT supported
 */
static int
check_msi_capability(char *dev_name)
{
	int bus, slot, func;
	uint16_t status;
	uint8_t cap_ptr, cap_id;
	struct pci_device *phys_dev;

	/* only check the MSI/MSI-x capability for PCI device */
	if (parse_bdf(dev_name, &bus, &slot, &func, 16) != 0)
		return 0;

	phys_dev = pci_device_find_by_slot(0, bus, slot, func);
	if (!phys_dev)
		return 0;

	pci_device_cfg_read_u16(phys_dev, &status, PCIR_STATUS);
	if (status & PCIM_STATUS_CAPPRESENT) {
		pci_device_cfg_read_u8(phys_dev, &cap_ptr, PCIR_CAP_PTR);

		while (cap_ptr != 0 && cap_ptr != 0xff) {
			pci_device_cfg_read_u8(phys_dev, &cap_id,
						cap_ptr + PCICAP_ID);

			if (cap_id == PCIY_MSI)
				return 1;
			else if (cap_id == PCIY_MSIX)
				return 1;

			pci_device_cfg_read_u8(phys_dev, &cap_ptr,
						cap_ptr + PCICAP_NEXTPTR);
		}
	}
	return 0;
}

int
create_gsi_sharing_groups(void)
{
	uint8_t gsi;
	char *dev_name;
	int i, error, msi_support;
	struct gsi_sharing_group *group = NULL, *temp = NULL;

	error = pciaccess_init();
	if (error < 0)
		return error;

	for (i = 0; i < num_gsi_dev_mapping_tables; i++) {
		dev_name = gsi_dev_mapping_tables[i].dev_name;
		gsi = gsi_dev_mapping_tables[i].gsi;

		/* skip the devices that support MSI/MSI-x */
		msi_support = check_msi_capability(dev_name);
		if (msi_support == 1)
			continue;

		/* insert the device into gsi_sharing_group */
		error = update_gsi_sharing_info(dev_name, gsi);
		if (error < 0)
			return error;
	}

	pciaccess_cleanup();

	/*
	 * clean up gsg_head - the list for gsi_sharing_group
	 * delete the element without gsi sharing condition (shared_dev_num < 2)
	 */
	list_foreach_safe(group, &gsg_head, gsg_list, temp) {
		if (group->shared_dev_num < 2) {
			LIST_REMOVE(group, gsg_list);
			free(group);
		}
	}

	return 0;
}

/*
 * update passthrough info in gsi_sharing_group
 * set assigned_to_this_vm as 1 if the PCI device is assigned to current VM
 */
void
update_pt_info(uint16_t phys_bdf)
{
	char *name;
	int i, bus, slot, func;
	struct gsi_sharing_group *group;

	LIST_FOREACH(group, &gsg_head, gsg_list) {
		for (i = 0; i < (group->shared_dev_num); i++) {
			name = group->dev[i].dev_name;
			if (parse_bdf(name, &bus, &slot, &func, 16) != 0)
				continue;

			if (phys_bdf == (PCI_BDF(bus, slot, func)))
				group->dev[i].assigned_to_this_vm = 1;
		}
	}
}

/* check if all devices in one GSI sharing group are assigned to same VM */
int
check_gsi_sharing_violation(void)
{
	struct gsi_sharing_group *group, *temp;
	int i, error, violation;

	error = 0;
	LIST_FOREACH(group, &gsg_head, gsg_list) {
		/*
		 * All the PCI devices that are sharing the same GSI should be
		 * assigned to same VM to avoid physical GSI sharing between
		 * multiple VMs.
		 *
		 * If the value of 'assigned_to_this_vm' for each device in one
		 * gsi_sharing_group are all the same, either all 0 or all 1, it
		 * indicates that there is no GSI sharing between multiple VMs.
		 * All 0 means that all devices are NOT assigned to current VM.
		 * All 1 means that all devices are assigned to current VM.
		 *
		 * Otherwise, the passthrough will be rejected due to violation.
		 */
		violation = 0;
		for (i = 1; i < (group->shared_dev_num); i++)  {
			if (group->dev[i].assigned_to_this_vm !=
				group->dev[i - 1].assigned_to_this_vm) {
				violation = 1;
				break;
			}
		}

		if (violation == 0)
			continue;

		/* reject the passthrough since gsi sharing violation occurs */
		warnx("GSI SHARING VIOLATION!");
		warnx("following physical devices are sharing same GSI, please "
			"assign them to same VM to avoid physical GSI sharing "
			"between multiple VMs");
		for (i = 0; i < (group->shared_dev_num); i++) {
			warnx("device %s \t assigned_to_this_vm %d",
				group->dev[i].dev_name,
				group->dev[i].assigned_to_this_vm);
		}
		error = -EINVAL;
		break;
	}

	/* destroy the gsg_head after all the checks have been done */
	list_foreach_safe(group, &gsg_head, gsg_list, temp) {
		LIST_REMOVE(group, gsg_list);
		free(group);
	}

	return error;
}
