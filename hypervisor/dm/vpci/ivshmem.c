/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifdef CONFIG_IVSHMEM_ENABLED
#include <vm.h>
#include <mmu.h>
#include <ept.h>
#include <logmsg.h>
#include <ivshmem.h>
#include <ivshmem_cfg.h>
#include "vpci_priv.h"

/* config space of ivshmem device */
#define	IVSHMEM_VENDOR_ID	0x1af4U
#define	IVSHMEM_DEVICE_ID	0x1110U
#define	IVSHMEM_CLASS		0x05U
#define	IVSHMEM_REV		0x01U

/* ivshmem device only supports bar0 and bar2 */
#define IVSHMEM_MMIO_BAR	0U
#define IVSHMEM_SHM_BAR	2U

/* IVSHMEM_SHM_SIZE is provided by offline tool */
static uint8_t ivshmem_base[IVSHMEM_SHM_SIZE] __aligned(PDE_SIZE);

void init_ivshmem_shared_memory()
{
	uint32_t i;
	uint64_t addr = hva2hpa(&ivshmem_base);

	for (i = 0U; i < ARRAY_SIZE(mem_regions); i++) {
		mem_regions[i].hpa = addr;
		addr += mem_regions[i].size;
	}
}

/*
 * @pre name != NULL
 */
static struct ivshmem_shm_region *find_shm_region(const char *name)
{
	uint32_t i, num = ARRAY_SIZE(mem_regions);

	for (i = 0U; i < num; i++) {
		if (strncmp(name, mem_regions[i].name, sizeof(mem_regions[0].name)) == 0) {
			break;
		}
	}
	return ((i < num) ? &mem_regions[i] : NULL);
}

static int32_t read_ivshmem_vdev_cfg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	/* Implementation in next patch */
	(void) vdev;
	(void) offset;
	(void) bytes;
	(void) val;
	return 0;
}

static int32_t write_ivshmem_vdev_cfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	/* Implementation in next patch */
	(void) vdev;
	(void) offset;
	(void) bytes;
	(void) val;
	return 0;
}
/*
 * @pre vdev != NULL
 * @pre bar_idx < PCI_BAR_COUNT
 */
static void init_ivshmem_bar(struct pci_vdev *vdev, uint32_t bar_idx)
{
	struct pci_vbar *vbar;
	enum pci_bar_type type;
	uint64_t addr, mask, size = 0UL;
	struct acrn_vm_pci_dev_config *dev_config = vdev->pci_dev_config;

	addr = dev_config->vbar_base[bar_idx];
	type = pci_get_bar_type((uint32_t) addr);
	mask = (type == PCIBAR_IO_SPACE) ? PCI_BASE_ADDRESS_IO_MASK : PCI_BASE_ADDRESS_MEM_MASK;
	vbar = &vdev->vbars[bar_idx];
	vbar->type = type;
	if (bar_idx == IVSHMEM_SHM_BAR) {
		struct ivshmem_shm_region *region = find_shm_region(dev_config->shm_region_name);
		if (region != NULL) {
			size = region->size;
			vbar->base_hpa = region->hpa;
		} else {
			pr_err("%s ivshmem device %x:%x.%x has no memory region\n",
				__func__, vdev->bdf.bits.b, vdev->bdf.bits.d, vdev->bdf.bits.f);
		}
	} else if (bar_idx == IVSHMEM_MMIO_BAR) {
		size = 0x100UL;
	}
	if (size != 0UL) {
		vbar->size = size;
		vbar->mask = (uint32_t) (~(size - 1UL));
		vbar->fixed = (uint32_t) (addr & (~mask));
		pci_vdev_write_vbar(vdev, bar_idx, (uint32_t) addr);
		if (type == PCIBAR_MEM64) {
			vbar = &vdev->vbars[bar_idx + 1U];
			vbar->type = PCIBAR_MEM64HI;
			vbar->mask = (uint32_t) ((~(size - 1UL)) >> 32U);
			pci_vdev_write_vbar(vdev, (bar_idx + 1U), ((uint32_t)(addr >> 32U)));
		}
	}
}

static void init_ivshmem_vdev(struct pci_vdev *vdev)
{
	/* initialize ivshmem config */
	pci_vdev_write_vcfg(vdev, PCIR_VENDOR, 2U, IVSHMEM_VENDOR_ID);
	pci_vdev_write_vcfg(vdev, PCIR_DEVICE, 2U, IVSHMEM_DEVICE_ID);
	pci_vdev_write_vcfg(vdev, PCIR_REVID, 1U, IVSHMEM_REV);
	pci_vdev_write_vcfg(vdev, PCIR_CLASS, 1U, IVSHMEM_CLASS);

	/* initialize ivshmem bars */
	vdev->nr_bars = PCI_BAR_COUNT;
	init_ivshmem_bar(vdev, IVSHMEM_MMIO_BAR);
	init_ivshmem_bar(vdev, IVSHMEM_SHM_BAR);

	vdev->user = vdev;
}

static void deinit_ivshmem_vdev(struct pci_vdev *vdev)
{
	vdev->user = NULL;
}

const struct pci_vdev_ops vpci_ivshmem_ops = {
	.init_vdev	= init_ivshmem_vdev,
	.deinit_vdev	= deinit_ivshmem_vdev,
	.write_vdev_cfg	= write_ivshmem_vdev_cfg,
	.read_vdev_cfg	= read_ivshmem_vdev_cfg,
};
#endif
