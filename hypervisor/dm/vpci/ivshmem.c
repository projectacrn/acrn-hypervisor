/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifdef CONFIG_IVSHMEM_ENABLED
#include <x86/guest/vm.h>
#include <x86/mmu.h>
#include <x86/guest/ept.h>
#include <logmsg.h>
#include <errno.h>
#include <ivshmem.h>
#include <ivshmem_cfg.h>
#include "vpci_priv.h"

/* config space of ivshmem device */
#define	IVSHMEM_CLASS		0x05U
#define	IVSHMEM_REV		0x01U

/*
 * ivshmem device supports bar0, bar1 and bar2,
 * indexes of them shall begin with 0 and be continuous.
 */
#define IVSHMEM_MMIO_BAR	0U
#define IVSHMEM_MSIX_BAR	1U
#define IVSHMEM_SHM_BAR	2U

#define IVSHMEM_MMIO_BAR_SIZE 256UL

/* The device-specific registers of ivshmem device */
#define	IVSHMEM_IRQ_MASK_REG	0x0U
#define	IVSHMEM_IRQ_STA_REG	0x4U
#define	IVSHMEM_IV_POS_REG	0x8U
#define	IVSHMEM_DOORBELL_REG	0xcU

static struct ivshmem_shm_region mem_regions[8] = {
	IVSHMEM_SHM_REGIONS
};

union ivshmem_doorbell {
	uint32_t val;
	struct {
		uint16_t vector_index;
		uint16_t peer_id;
	} reg;
};

struct ivshmem_device {
	struct pci_vdev* pcidev;
	union {
		uint32_t data[4];
		struct {
			uint32_t irq_mask;
			uint32_t irq_state;
			/*
			 * If the device is not configured for interrupts,
			 * this is zero. Else, ivpos is the device's ID.
			 */
			uint32_t ivpos;

			/* Writing doorbell register requests to interrupt a peer */
			union ivshmem_doorbell doorbell;
		} regs;
	} mmio;
	struct ivshmem_shm_region *region;
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
 * @brief There are two ivshmem server implementation in HV-land and
 *	  DM-land, they're used for briding the notification channel
 *	  between ivshmem devices acrossed VMs.
 *
 * @pre vdev != NULL
 * @pre region->doorbell_peers[vm_id] = NULL
 */
static void ivshmem_server_bind_peer(struct pci_vdev *vdev)
{
	uint16_t vm_id;
	struct acrn_vm_pci_dev_config *dev_config = vdev->pci_dev_config;
	struct ivshmem_device *ivs_dev = (struct ivshmem_device *)vdev->priv_data;
	struct ivshmem_shm_region *region = find_shm_region(dev_config->shm_region_name);

	if (region != NULL) {
		vm_id = vpci2vm(vdev->vpci)->vm_id;
		/* Device ID equals to VM ID*/
		ivs_dev->mmio.regs.ivpos = vm_id;
		ivs_dev->region = region;
		region->doorbell_peers[vm_id] = ivs_dev;
	}
}

/*
 * @pre vdev != NULL
 */
static void ivshmem_server_unbind_peer(struct pci_vdev *vdev)
{
	struct ivshmem_shm_region *region = ((struct ivshmem_device *)vdev->priv_data)->region;

	region->doorbell_peers[vpci2vm(vdev->vpci)->vm_id] = NULL;
}

/*
 * @pre src_ivs_dev != NULL
 */
static void ivshmem_server_notify_peer(struct ivshmem_device *src_ivs_dev, uint16_t dest_peer_id, uint16_t vector_index)
{
	struct acrn_vm *dest_vm;
	struct ivshmem_device *dest_ivs_dev;
	struct msix_table_entry *entry;
	struct ivshmem_shm_region *region = src_ivs_dev->region;

	if (dest_peer_id < MAX_IVSHMEM_PEER_NUM) {

		dest_ivs_dev = region->doorbell_peers[dest_peer_id];
		if ((dest_ivs_dev != NULL) && vpci_vmsix_enabled(dest_ivs_dev->pcidev)
			&& (vector_index < dest_ivs_dev->pcidev->msix.table_count)) {

			entry = &(dest_ivs_dev->pcidev->msix.table_entries[vector_index]);
			if ((entry->vector_control & PCIM_MSIX_VCTRL_MASK) == 0U) {

				dest_vm = vpci2vm(dest_ivs_dev->pcidev->vpci);
				vlapic_inject_msi(dest_vm, entry->addr, entry->data);
			} else {
				pr_err("%s,target msix entry [%d] is masked.\n",
					__func__, vector_index);
			}
		} else {
			pr_err("%s,Invalid peer, ID = %d, vector index [%d] or MSI-X is disabled.\n",
				__func__, dest_peer_id, vector_index);
		}
	}
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
	union ivshmem_doorbell doorbell;
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
				if (offset == IVSHMEM_DOORBELL_REG) {
					doorbell.val = mmio->value;
					ivshmem_server_notify_peer(ivs_dev, doorbell.reg.peer_id,
						doorbell.reg.vector_index);
				} else {
					ivs_dev->mmio.data[offset >> 2U] = mmio->value;
				}
			}
		}
	}
	return 0;
}

static int32_t read_ivshmem_vdev_cfg(const struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	if (vbar_access(vdev, offset)) {
		*val = pci_vdev_read_vbar(vdev, pci_bar_index(offset));
	} else {
		*val = pci_vdev_read_vcfg(vdev, offset, bytes);
	}

	return 0;
}

static void ivshmem_vbar_unmap(struct pci_vdev *vdev, uint32_t idx)
{
	struct acrn_vm *vm = vpci2vm(vdev->vpci);
	struct pci_vbar *vbar = &vdev->vbars[idx];

	if ((idx == IVSHMEM_SHM_BAR) && (vbar->base_gpa != 0UL)) {
		ept_del_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, vbar->base_gpa, vbar->size);
	} else if (((idx == IVSHMEM_MMIO_BAR) || (idx == IVSHMEM_MSIX_BAR)) && (vbar->base_gpa != 0UL)) {
		unregister_mmio_emulation_handler(vm, vbar->base_gpa, (vbar->base_gpa + vbar->size));
	}
}

/*
 * @pre vdev->priv_data != NULL
 * @pre msix->table_offset == 0U
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
		ept_del_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, vbar->base_gpa, round_page_up(vbar->size));
	} else if ((idx == IVSHMEM_MSIX_BAR) && (vbar->base_gpa != 0UL)) {
		register_mmio_emulation_handler(vm, vmsix_handle_table_mmio_access, vbar->base_gpa,
			(vbar->base_gpa + vbar->size), vdev, false);
		ept_del_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, vbar->base_gpa, vbar->size);
		vdev->msix.mmio_gpa = vbar->base_gpa;
	}
}

static int32_t write_ivshmem_vdev_cfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t val)
{
	if (vbar_access(vdev, offset)) {
		vpci_update_one_vbar(vdev, pci_bar_index(offset), val,
			ivshmem_vbar_map, ivshmem_vbar_unmap);
	} else if (msixcap_access(vdev, offset)) {
		write_vmsix_cap_reg(vdev, offset, bytes, val);
	} else {
		pci_vdev_write_vcfg(vdev, offset, bytes, val);
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
	uint64_t addr, mask, size = 0UL;
	struct acrn_vm_pci_dev_config *dev_config = vdev->pci_dev_config;

	addr = dev_config->vbar_base[bar_idx];
	vbar = &vdev->vbars[bar_idx];
	vbar->bar_type.bits = addr;
	mask = is_pci_io_bar(vbar) ? PCI_BASE_ADDRESS_IO_MASK : PCI_BASE_ADDRESS_MEM_MASK;
	vbar->bar_type.bits &= (~mask);

	if (bar_idx == IVSHMEM_SHM_BAR) {
		struct ivshmem_shm_region *region = find_shm_region(dev_config->shm_region_name);
		if (region != NULL) {
			size = region->size;
			vbar->base_hpa = region->hpa;
		} else {
			pr_err("%s ivshmem device %x:%x.%x has no memory region\n",
				__func__, vdev->bdf.bits.b, vdev->bdf.bits.d, vdev->bdf.bits.f);
		}
	} else if (bar_idx == IVSHMEM_MSIX_BAR) {
		size = VMSIX_ENTRY_TABLE_PBA_BAR_SIZE;
	} else if (bar_idx == IVSHMEM_MMIO_BAR) {
		size = IVSHMEM_MMIO_BAR_SIZE;
	}
	if (size != 0UL) {
		vbar->size = size;
		vbar->mask = (uint32_t) (~(size - 1UL));
		pci_vdev_write_vbar(vdev, bar_idx, (uint32_t)addr);
		if (is_pci_mem64lo_bar(vbar)) {
			vbar = &vdev->vbars[bar_idx + 1U];
			vbar->is_mem64hi = true;
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
	add_vmsix_capability(vdev, MAX_IVSHMEM_MSIX_TBL_ENTRY_NUM, IVSHMEM_MSIX_BAR);

	/* initialize ivshmem bars */
	vdev->nr_bars = 4U;
	init_ivshmem_bar(vdev, IVSHMEM_MMIO_BAR);
	init_ivshmem_bar(vdev, IVSHMEM_MSIX_BAR);
	init_ivshmem_bar(vdev, IVSHMEM_SHM_BAR);
	ivshmem_server_bind_peer(vdev);

	vdev->user = vdev;
}

/*
 * @pre vdev->priv_data != NULL
 */
static void deinit_ivshmem_vdev(struct pci_vdev *vdev)
{
	struct ivshmem_device *ivs_dev = (struct ivshmem_device *) vdev->priv_data;

	ivshmem_server_unbind_peer(vdev);
	ivs_dev->pcidev = NULL;
	vdev->priv_data = NULL;
	vdev->user = NULL;
}

/**
 * @pre vm != NULL
 * @pre dev != NULL
 */
int32_t create_ivshmem_vdev(struct acrn_vm *vm, struct acrn_emul_dev *dev)
{
	uint32_t i;
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	struct acrn_vm_pci_dev_config *dev_config = NULL;
	int32_t ret = -EINVAL;

	for (i = 0U; i < vm_config->pci_dev_num; i++) {
		dev_config = &vm_config->pci_devs[i];
		if (strncmp(dev_config->shm_region_name, (char *)dev->args, sizeof(dev_config->shm_region_name)) == 0) {
			struct ivshmem_shm_region *region = find_shm_region(dev_config->shm_region_name);
			if ((region != NULL) && (region->size == dev->io_size[IVSHMEM_SHM_BAR])) {
				spinlock_obtain(&vm->vpci.lock);
				dev_config->vbdf.value = (uint16_t) dev->slot;
				dev_config->vbar_base[IVSHMEM_MMIO_BAR] = (uint64_t) dev->io_addr[IVSHMEM_MMIO_BAR];
				dev_config->vbar_base[IVSHMEM_MSIX_BAR] = (uint64_t) dev->io_addr[IVSHMEM_MSIX_BAR];
				dev_config->vbar_base[IVSHMEM_SHM_BAR] = (uint64_t) dev->io_addr[IVSHMEM_SHM_BAR];
				dev_config->vbar_base[IVSHMEM_SHM_BAR] |= ((uint64_t) dev->io_addr[IVSHMEM_SHM_BAR + 1U]) << 32U;
				(void) vpci_init_vdev(&vm->vpci, dev_config, NULL);
				spinlock_release(&vm->vpci.lock);
				ret = 0;
			} else {
				pr_warn("%s, failed to create ivshmem device %x:%x.%x\n", __func__,
				dev->slot >> 8U, (dev->slot >> 3U) & 0x1fU, dev->slot & 0x7U);
			}
			break;
		}
	}
	return ret;
}

int32_t destroy_ivshmem_vdev(struct pci_vdev *vdev)
{
	uint32_t i;

	for (i = 0U; i < vdev->nr_bars; i++) {
		vpci_update_one_vbar(vdev, i, 0U, NULL, ivshmem_vbar_unmap);
	}

	return 0;
}

const struct pci_vdev_ops vpci_ivshmem_ops = {
	.init_vdev	= init_ivshmem_vdev,
	.deinit_vdev	= deinit_ivshmem_vdev,
	.write_vdev_cfg	= write_ivshmem_vdev_cfg,
	.read_vdev_cfg	= read_ivshmem_vdev_cfg,
};
#endif
