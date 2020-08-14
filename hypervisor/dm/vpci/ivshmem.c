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

/* The device-specific registers of ivshmem device */
#define	IVSHMEM_IRQ_MASK_REG	0x0U
#define	IVSHMEM_IRQ_STA_REG	0x4U
#define	IVSHMEM_IV_POS_REG	0x8U
#define	IVSHMEM_DOORBELL_REG	0xcU

struct ivshmem_device {
	struct pci_vdev* pcidev;
	union {
		uint32_t data[4];
		struct {
			uint32_t irq_mask;
			uint32_t irq_state;
			uint32_t ivpos;
			uint32_t doorbell;
		} regs;
	} mmio;
};

/* IVSHMEM_SHM_SIZE is provided by offline tool */
static uint8_t ivshmem_base[IVSHMEM_SHM_SIZE] __aligned(PDE_SIZE);
static struct ivshmem_device ivshmem_dev[IVSHMEM_DEV_NUM];
static spinlock_t ivshmem_dev_lock = { .head = 0U, .tail = 0U, };

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

/*
 * @post vdev->priv_data != NULL
 */
static void create_ivshmem_device(struct pci_vdev *vdev)
{
	uint32_t i;

	spinlock_obtain(&ivshmem_dev_lock);
	for (i = 0U; i < IVSHMEM_DEV_NUM; i++) {
		if (ivshmem_dev[i].pcidev == NULL) {
			ivshmem_dev[i].pcidev = vdev;
			vdev->priv_data = &ivshmem_dev[i];
			break;
		}
	}
	spinlock_release(&ivshmem_dev_lock);
	ASSERT((i < IVSHMEM_DEV_NUM), "failed to find and set ivshmem device");
}

/*
 * @pre vdev->priv_data != NULL
 */
static int32_t ivshmem_mmio_handler(struct io_request *io_req, void *data)
{
	struct mmio_request *mmio = &io_req->reqs.mmio;
	struct pci_vdev *vdev = (struct pci_vdev *) data;
	struct ivshmem_device *ivs_dev = (struct ivshmem_device *) vdev->priv_data;
	uint64_t offset = mmio->address - vdev->vbars[IVSHMEM_MMIO_BAR].base_gpa;

	if ((mmio->size == 4U) && ((offset & 0x3U) == 0U)) {
		/*
		 * IVSHMEM_IRQ_MASK_REG and IVSHMEM_IRQ_STA_REG are R/W registers
		 * they are useless for ivshmem Rev.1.
		 * IVSHMEM_IV_POS_REG is Read-Only register and IVSHMEM_DOORBELL_REG
		 * is Write-Only register, they are used for interrupt.
		 */
		if (mmio->direction == REQUEST_READ) {
			if (offset != IVSHMEM_DOORBELL_REG) {
				mmio->value = ivs_dev->mmio.data[offset >> 2U];
			} else {
				mmio->value = 0UL;
			}
		} else {
			if (offset != IVSHMEM_IV_POS_REG) {
				ivs_dev->mmio.data[offset >> 2U] = mmio->value;
			}
		}
	}
	return 0;
}

static int32_t read_ivshmem_vdev_cfg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	if (cfg_header_access(offset)) {
		if (vbar_access(vdev, offset)) {
			*val = pci_vdev_read_vbar(vdev, pci_bar_index(offset));
		} else {
			*val = pci_vdev_read_vcfg(vdev, offset, bytes);
		}
	}

	return 0;
}

static void ivshmem_vbar_unmap(struct pci_vdev *vdev, uint32_t idx)
{
	struct acrn_vm *vm = vpci2vm(vdev->vpci);
	struct pci_vbar *vbar = &vdev->vbars[idx];

	if ((idx == IVSHMEM_SHM_BAR) && (vbar->base_gpa != 0UL)) {
		ept_del_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, vbar->base_gpa, vbar->size);
	} else if ((idx == IVSHMEM_MMIO_BAR) && (vbar->base_gpa != 0UL)) {
		unregister_mmio_emulation_handler(vm, vbar->base_gpa, (vbar->base_gpa + vbar->size));
	}
}

/*
 * @pre vdev->priv_data != NULL
 */
static void ivshmem_vbar_map(struct pci_vdev *vdev, uint32_t idx)
{
	struct acrn_vm *vm = vpci2vm(vdev->vpci);
	struct pci_vbar *vbar = &vdev->vbars[idx];
	struct ivshmem_device *ivs_dev = (struct ivshmem_device *) vdev->priv_data;

	if ((idx == IVSHMEM_SHM_BAR) && (vbar->base_hpa != INVALID_HPA) && (vbar->base_gpa != 0UL)) {
		ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, vbar->base_hpa,
				vbar->base_gpa, vbar->size, EPT_RD | EPT_WR | EPT_WB);
	} else if ((idx == IVSHMEM_MMIO_BAR) && (vbar->base_gpa != 0UL)) {
		(void)memset(&ivs_dev->mmio, 0U, sizeof(ivs_dev->mmio));
		register_mmio_emulation_handler(vm, ivshmem_mmio_handler, vbar->base_gpa,
				(vbar->base_gpa + vbar->size), vdev, false);
	}
}

static int32_t write_ivshmem_vdev_cfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	if (cfg_header_access(offset)) {
		if (vbar_access(vdev, offset)) {
			vpci_update_one_vbar(vdev, pci_bar_index(offset), val,
					ivshmem_vbar_map, ivshmem_vbar_unmap);
		} else {
			pci_vdev_write_vcfg(vdev, offset, bytes, val);
		}
	}

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
	create_ivshmem_device(vdev);

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

/*
 * @pre vdev->priv_data != NULL
 */
static void deinit_ivshmem_vdev(struct pci_vdev *vdev)
{
	struct ivshmem_device *ivs_dev = (struct ivshmem_device *) vdev->priv_data;

	ivs_dev->pcidev = NULL;
	vdev->priv_data = NULL;
	vdev->user = NULL;
}

const struct pci_vdev_ops vpci_ivshmem_ops = {
	.init_vdev	= init_ivshmem_vdev,
	.deinit_vdev	= deinit_ivshmem_vdev,
	.write_vdev_cfg	= write_ivshmem_vdev_cfg,
	.read_vdev_cfg	= read_ivshmem_vdev_cfg,
};
#endif
