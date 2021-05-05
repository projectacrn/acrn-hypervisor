/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <errno.h>
#include <stddef.h>
#include <sys/queue.h>
#include <stdlib.h>

#include "pcireg.h"
#include "pciaccess.h"
#include "pci_core.h"
#include "pci_util.h"
#include "log.h"

#define MAX_LEN			(PCI_BUSMAX + 1)

TAILQ_HEAD(ALL_PCI_DEVICES, pci_device_info);
static struct ALL_PCI_DEVICES pci_device_q;

static int pci_scanned;

/* map bus to bridge
 * assume only one pci domain in the system so the
 * bus # in the pci hierarchy is unique.
 */
static struct pci_device_info *bus_map[MAX_LEN];

/* find position of specified pci capability register*/
int pci_find_cap(struct pci_device *pdev, const int cap_id)
{
	uint8_t cap_pos, cap_data;
	uint16_t status = 0;

	pci_device_cfg_read_u16(pdev, &status, PCIR_STATUS);
	if (status & PCIM_STATUS_CAPPRESENT) {
		pci_device_cfg_read_u8(pdev, &cap_pos, PCIR_CAP_PTR);

		while (cap_pos != 0 && cap_pos != 0xff) {
			pci_device_cfg_read_u8(pdev, &cap_data,
					cap_pos + PCICAP_ID);

			if (cap_data == cap_id)
				return cap_pos;

			pci_device_cfg_read_u8(pdev, &cap_pos,
					cap_pos + PCICAP_NEXTPTR);
		}
	}

	return 0;
}

/* find extend capability register position from cap_id */
int pci_find_ext_cap(struct pci_device *pdev, int cap_id)
{
	int offset = 0;
	uint32_t data = 0;

	offset = PCIR_EXTCAP;

	do {
		/* PCI Express Extended Capability must have 4 bytes header */
		pci_device_cfg_read_u32(pdev, &data, offset);

		if (PCI_EXTCAP_ID(data) == cap_id)
			break;

		offset = PCI_EXTCAP_NEXTPTR(data);
	} while (offset != 0);

	return offset;
}

/* find pci-e device type */
int pci_get_pcie_type(struct pci_device *dev)
{
	uint8_t data = 0;
	int pcie_type;
	int pos = 0;

	if (dev == NULL)
		return -EINVAL;

	pos = pci_find_cap(dev, PCIY_EXPRESS);
	if (!pos)
		return -EINVAL;

	pci_device_cfg_read_u8(dev, &data, pos + PCIER_FLAGS);
	pcie_type = data & PCIEM_FLAGS_TYPE;

	return pcie_type;
}

/* check whether pdev is a pci root port */
bool is_root_port(struct pci_device *pdev)
{
	int pcie_type;

	pcie_type = pci_get_pcie_type(pdev);

	return (pcie_type == PCIEM_TYPE_ROOT_PORT);
}

/* check whether pdev is a bridge */
bool is_bridge(struct pci_device *pdev)
{
	uint8_t hdr_type;

	pci_device_cfg_read_u8(pdev, &hdr_type, PCIR_HDRTYPE);

	return ((hdr_type & PCIM_HDRTYPE) == PCIM_HDRTYPE_BRIDGE);
}

/* add new_child to parent's child list */
static inline void
add_child(struct pci_device_info *new_child,
			struct pci_device_info *parent)
{
	new_child->clist = parent->clist;
	parent->clist = new_child;

	// it is important to reset the new_child's clist to null
	new_child->clist = NULL;
}

/* get number of children on bus */
int get_device_count_on_bus(int bus)
{
	int count = 0;
	struct pci_device_info *bridge = bus_map[bus];
	struct pci_device_info *next;

	if (bridge == NULL)
		return 0;

	next = bridge->clist;

	while (next != NULL) {
		if (next->is_bridge)
			count += get_device_count_on_bridge(bridge);
		else
			count++;

		next = next->clist;
	}

	return count;
}

/* get number of children on bridge */
int get_device_count_on_bridge(const struct pci_device_info *bridge_info)
{
	int count = 0;
	int i, ret;

	if (bridge_info == NULL)
		return 0;

	for (i = bridge_info->secondary_bus; i <= bridge_info->subordinate_bus; i++) {
		ret = get_device_count_on_bus(i);
		pr_info("%s: device_count on bus[%x]=%d.\n", __func__, i, ret);
		count += ret;
	}

	return count;
}

/* build and cache pci hierarchy in one pci domain
 * @pre: only one pci domain in the system.  Need revisit
 * if there is more than one domain in the system.
 */
int scan_pci(void)
{
	int error = 0;
	struct pci_device_iterator *iter;
	struct pci_device *dev;
	int i;

	if (pci_scanned)
		return 0;

	TAILQ_INIT(&pci_device_q);

	pci_scanned = 1;

	iter = pci_slot_match_iterator_create(NULL);
	while ((dev = pci_device_next(iter)) != NULL) {
		struct pci_device_info *pci_info = calloc(1, sizeof(struct pci_device_info));

		if (pci_info == NULL) {
			error = -ENOMEM;
			pr_err("%s, %s: calloc", __func__, __LINE__);
			goto done;
		}

		pci_info->parent = NULL;
		pci_info->clist = NULL;

		pci_info->bdf = PCI_BDF(dev->bus, dev->dev, dev->func);

		TAILQ_INSERT_TAIL(&pci_device_q, pci_info, PCI_DEVICE_Q);

		if (pci_info->bdf == 0) { /* host bridge */
			bus_map[0] = pci_info;
			continue;
		}

		if (is_bridge(dev)) {
			pci_info->is_bridge = true;

			pci_device_get_bridge_buses(dev, &(pci_info->primary_bus),
						&(pci_info->secondary_bus), &(pci_info->subordinate_bus));

			// put bus and bridge into bus_map
			for (i = pci_info->secondary_bus; i <= pci_info->subordinate_bus; i++)
				bus_map[i] = pci_info;
		} else
			pci_info->is_bridge = false;

		// set its parent bridge
		pci_info->parent = bus_map[dev->bus];

		// add itself to its parent bridge's child list
		add_child(pci_info, pci_info->parent);
	}

done:
	if (error < 0)
	{
		pr_err("%s: failed to build pci hierarchy.\n", __func__);
		clean_pci_cache();
		pci_scanned = false;
	}

	pci_iterator_destroy(iter);

	return error;
}

/* find bdf of pci device pdev's root port */
struct pci_device * pci_find_root_port(const struct pci_device *pdev)
{
	struct pci_device *bdev;
	int bus, dev, func;

	struct pci_device_info *brdg = bus_map[pdev->bus];

	for (;;) {
		if (brdg == NULL)
			return NULL;

		bus = (brdg->bdf >> 8) & 0xFF;
		dev = (brdg->bdf >> 3) & 0x1F;
		func = brdg->bdf & 0x7;

		bdev = pci_device_find_by_slot(0, bus, dev, func);
		if (is_root_port(bdev))
			return bdev;

		brdg = brdg->parent;
	}

	return NULL;
}

/* clean cache of pci device: free all allocated memory */
void clean_pci_cache(void)
{
	if (!pci_scanned)
		return;

	while (!TAILQ_EMPTY(&pci_device_q)) {
		struct pci_device_info *p = TAILQ_FIRST(&pci_device_q);

		TAILQ_REMOVE(&pci_device_q, p, PCI_DEVICE_Q);
	}

	pci_scanned = 0;
}

#ifdef SCAN_PCI_TEST
static void
scan_pci_test(void)
{
	struct pci_device_info *device_info;
	struct pci_device *pdev;
	int bdf, parent_bdf;

	printf("%s enter.\n", __func__);
	if (!pci_scanned) {
		printf("%s: no pci hierarchy to test.\n", __func__);
		return;
	}

	TAILQ_FOREACH(device_info, &pci_device_q, PCI_DEVICE_Q) {
		bdf = device_info->bdf;

		if (bdf == 0) {
			printf("%s: host bridge.\n", __func__);
			continue;
		}

		parent_bdf = device_info->parent->bdf;

		printf("%s: [%x:%x.%x]: [%x:%x.%x]\n", __func__, (bdf >> 8) & 0xFF, (bdf >> 3) & 0x1F, bdf & 0x7,
			(parent_bdf >> 8) & 0xFF, (parent_bdf >> 3) & 0x1F, parent_bdf & 0x7);

		pdev = pci_device_find_by_slot(0, (bdf >> 8) & 0xFF, (bdf >> 3) & 0x1F, bdf & 0x7);

		if (is_bridge(pdev)) {
			struct pci_device_info *child;
			child = device_info->clist;
			while(child != NULL) {
				printf("\t\t: child: [%x:%x.%x]\n", (child->bdf >> 8) & 0xFF, (child->bdf >> 3) & 0x1F, child->bdf & 0x7);
				child = child->clist;
			}
		}
	}
}
#endif
