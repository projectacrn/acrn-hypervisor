/*
 * Copyright (C) 2020-2024 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifdef CONFIG_IVSHMEM_ENABLED
#include <asm/guest/vm.h>
#include <asm/mmu.h>
#include <asm/guest/ept.h>
#include <logmsg.h>
#include <errno.h>
#include <ivshmem.h>
#include <ivshmem_cfg.h>
#include "vpci_priv.h"

/**
 * @addtogroup vp-dm_vperipheral
 *
 * @{
 */

/**
 * @file
 * @brief Implementation of Inter-VM shared memory device (ivshmem).
 *
 * This file defines marcos, data structure and functions to support ivshmem devices. It also implements necessary
 * functions to model a ivshmem device as a PCI device.
 */

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

static struct ivshmem_device ivshmem_dev[IVSHMEM_DEV_NUM];
static spinlock_t ivshmem_dev_lock = { .head = 0U, .tail = 0U, };

/**
 * @brief Initialize the shared memory regions for all ivshmem devices.
 *
 * An ivshmem device is used to transfer data between VMs based on shared memory region. Basic ivshmem information is
 * configured in scenario file. After compilation, every shared memory region is stored in struct ivshmem_shm_region.
 * This function initializes all shared memory regions for ivshmem devices and it is usually called before all VMs are
 * created.
 *
 * IVSHMEM_SHM_SIZE is the sum of all ivshmem shared memory regions in bytes. It rounds IVSHMEM_SHM_SIZE up to PDE_SIZE
 * (1 GiB) and allocates a contiguous block of memory for these memory regions from host e820. For detailed allocation
 * operations, refer to e820_alloc_memory(). The function then iterates over the memory regions and assigns the
 * allocated physical addresses to each region.
 *
 * @return None
 *
 * @pre N/A
 *
 * @post N/A
 */
void init_ivshmem_shared_memory()
{
	uint32_t i;
	uint64_t addr;

	addr = e820_alloc_memory(roundup(IVSHMEM_SHM_SIZE, PDE_SIZE), MEM_SIZE_MAX);
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
	/*
	 * Clear ivshmem_device mmio to ensure the same initial
	 * states after VM reboot.
	 */
	memset(&ivshmem_dev[i].mmio, 0U, sizeof(uint32_t) * 4);
}

/**
 * @brief Handle MMIO (Memory-Mapped I/O) operations for the ivshmem device.
 *
 * BAR0 is used for device registers. This function handles MMIO read and write operations to the ivshmem device BAR0.
 *
 * Per the specification, the access offset within should be 4-byte aligned, the access size should be 4 bytes, and the
 * access offset exceeds 16 bytes are reserved. So the request needs to meet these conditions, otherwise, it does
 * nothing and directly returns 0.
 * - For a read operation, the read value is stored in the input mmio request structure:
 *   - Doorbell register is a write-only register, so it sets the read value to 0.
 *   - Otherwise, it reads specified register value of the ivshmem device.
 * - For a write operation:
 *   - IVPosition register is a read-only register, so it does nothing if writing to IVPosition.
 *   - Writing to the Doorbell register requests to interrupt a peer. It extracts the peer ID and vector index from the
 *     input mmio value. If the peer is valid (peer ivshmem device exists, MSI-X is enabled, the MSI-X table entry
 *     corresponding to the vector index exists and is not masked), it injects an MSI to the peer VM. For more details
 *     about the MSI injection, refer to vlapic_inject_msi().
 *   - Otherwise, it writes the value to the specified register of the ivshmem device.
 * - Finally, it returns 0.
 *
 * @param[inout] io_req Pointer to the I/O request structure that contains the MMIO request information.
 * @param[inout] data Pointer to the pci_vdev structure that is treated as an ivshmem device.
 *
 * @return Always return 0.
 *
 * @pre io_req != NULL
 * @pre data != NULL
 * @pre data->priv_data != NULL
 *
 * @post retval == 0
 */
static int32_t ivshmem_mmio_handler(struct io_request *io_req, void *data)
{
	union ivshmem_doorbell doorbell;
	struct acrn_mmio_request *mmio = &io_req->reqs.mmio_request;
	struct pci_vdev *vdev = (struct pci_vdev *) data;
	struct ivshmem_device *ivs_dev = (struct ivshmem_device *) vdev->priv_data;
	uint64_t offset = mmio->address - vdev->vbars[IVSHMEM_MMIO_BAR].base_gpa;

	/* ivshmem spec define the BAR0 offset > 16 are reserved */
	if ((mmio->size == 4U) && ((offset & 0x3U) == 0U) &&
		(offset < sizeof(ivs_dev->mmio))) {
		/*
		 * IVSHMEM_IRQ_MASK_REG and IVSHMEM_IRQ_STA_REG are R/W registers
		 * they are useless for ivshmem Rev.1.
		 * IVSHMEM_IV_POS_REG is Read-Only register and IVSHMEM_DOORBELL_REG
		 * is Write-Only register, they are used for interrupt.
		 */
		if (mmio->direction == ACRN_IOREQ_DIR_READ) {
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

/**
 * @brief Read the PCI configuration space of the ivshmem device.
 *
 * This function reads the configuration space of the specified virtual PCI device that is configured as a ivshmem
 * device. It is used to retrieve the configuration data of the ivshmem device for further processing or validation.
 *
 * It directly reads the configuration space of the ivshmem device by calling pci_vdev_read_vcfg().
 *
 * @param[in] vdev Pointer to the virtual PCI device whose configuration is to be read.
 * @param[in] offset Offset within the configuration space to start reading from.
 * @param[in] bytes Number of bytes to read from the configuration space.
 * @param[inout] val Pointer to the buffer where the read configuration data will be stored.
 *
 * @return Always return 0.
 *
 * @pre vdev != NULL
 * @pre val != NULL
 * @pre offset + bytes <= 0x1000
 *
 * @post retval == 0
 */
static int32_t read_ivshmem_vdev_cfg(struct pci_vdev *vdev, uint32_t offset, uint32_t bytes, uint32_t *val)
{
	*val = pci_vdev_read_vcfg(vdev, offset, bytes);

	return 0;
}

/**
 * @brief Unmap the specified BAR for the ivshmem device.
 *
 * This function unmaps the specified BAR for the ivshmem device. It is typically called during the destroy phase of the
 * ivshmem device or when guest updates the BAR register.
 *
 * - BAR0 and BAR1 are used for device registers and MSI-X table and PBA, respectively. If the specified idx is 0 or 1
 *   and the field base_gpa in the specified vBAR is not 0, it unregisters the mmio range handler for the BAR by calling
 *   unregister_mmio_emulation_handler().
 * - BAR2 maps the shared memory object. If the specified idx is 2 and the field base_gpa in vBAR2 is not 0, it releases
 *   the ept memory mapping for the shared memory region by calling ept_del_mr().
 * - Otherwise, it does nothing.
 *
 * @param[inout] vdev Pointer to the PCI device that is treated as an ivshmem device.
 * @param[in] idx Index of the BAR to be unmapped.
 *
 * @return None
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre idx < PCI_BAR_COUNT
 *
 * @post N/A
 */
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

/**
 * @brief Map the virtual BAR for the ivshmem device.
 *
 * This function maps the specified virtual BAR for the ivshmem device. It is typically called when guest updates the
 * BAR register.
 *
 * - BAR0 is used for device registers. If the specified idx is 0 and the field base_gpa in the specified vBAR is not 0,
 *   it registers the mmio range handler (via the callback ivshmem_mmio_handler) for the BAR and deletes the 4KB ept
 *   memory mapping for the BAR by calling ept_del_mr().
 * - BAR1 is used for MSI-X table and PBA. If the specified idx is 1 and the field base_gpa in the specified vBAR is not
 *   0, it registers the mmio range handler (via the callback vmsix_handle_table_mmio_access) for the BAR and deletes
 *   the ept memory mapping for the BAR by calling ept_del_mr(). It also sets the mmio_gpa field in the vdev->msix to
 *   the GPA of the BAR for MSI-X table access.
 * - BAR2 maps the shared memory object. If the specified idx is 2, the field base_gpa in vBAR2 is not 0 and the field
 *   base_hpa in vBAR2 is not INVALID_HPA, it adds the ept memory mapping as (EPT_RD|EPT_WR|EPT_WB|EPT_IGNORE_PAT) for
 *   the BAR by calling ept_add_mr().
 * - Otherwise, it does nothing.
 *
 * @param[inout] vdev Pointer to the PCI device that is treated as an ivshmem device.
 * @param[in] idx Index of the BAR to be mapped.
 *
 * @return None
 *
 * @pre vdev != NULL
 * @pre vdev->priv_data != NULL
 * @pre msix->table_offset == 0U
 * @pre bar_idx < PCI_BAR_COUNT
 *
 * @post N/A
 */
static void ivshmem_vbar_map(struct pci_vdev *vdev, uint32_t idx)
{
	struct acrn_vm *vm = vpci2vm(vdev->vpci);
	struct pci_vbar *vbar = &vdev->vbars[idx];

	if ((idx == IVSHMEM_SHM_BAR) && (vbar->base_hpa != INVALID_HPA) && (vbar->base_gpa != 0UL)) {
		ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, vbar->base_hpa,
				vbar->base_gpa, vbar->size, EPT_RD | EPT_WR | EPT_WB | EPT_IGNORE_PAT);
	} else if ((idx == IVSHMEM_MMIO_BAR) && (vbar->base_gpa != 0UL)) {
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

/**
 * @brief Write to the virtual ivshmem device configuration space.
 *
 * This function handles writes to the configuration space of the specified virtual PCI device that is configured as an
 * ivshmem device. It is typically called when the guest writes to the ivshmem device's configuration space.
 *
 * - If the write request is for a BAR register, it updates the BAR with the provided value. It also needs to update the
 *   ept mapping and mmio emulation handler based on the bar information. For detailed operations, refer to
 *   vpci_update_one_vbar(), ivshmem_vbar_map() and ivshmem_vbar_unmap().
 * - If the write request is for the MSI-X capability register, it specially handles the write request. For detailed
 *   operations, refer to write_vmsix_cap_reg().
 * - Otherwise, the function writes the provided value to the specified configuration space register.
 * - Finally, the function returns 0.
 *
 * @param[inout] vdev Pointer to the virtual PCI device whose configuration is to be written.
 * @param[in] offset Offset within the configuration space to start writing to.
 * @param[in] bytes Number of bytes to write.
 * @param[in] val The value to be written to the register.
 *
 * @return Always return 0.
 *
 * @pre vdev != NULL
 * @pre offset + bytes <= 0x1000
 *
 * @post retval == 0
 */
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

/**
 * @brief Initialize the specified BAR for the ivshmem device.
 *
 * The ivshmem PCI device has three BARs: BAR0, BAR1, and BAR2. BAR0/BAR1 is a 32-bit memory BAR and BAR2 is a 64-bit
 * memory BAR. This function initializes a specified BAR for the ivshmem device. It is typically called during the
 * initialization phase of the ivshmem device.
 *
 * - If bar_idx exceeds 2, the function does nothing.
 * - For BAR2, it finds the shared memory region based on the shared memory region name. If the shared memory region is
 *   not found, the function does nothing.
 * - It updates corresponding fields in the pci_vbar structure of specified bar_idx.
 * - It configures the Base Address Register in the device's configuration space.
 * - For a 64-bit memory BAR (BAR2 for now), it also sets up the next Base Address Register as the high 32 bits.
 *
 * @param[inout] vdev Pointer to the PCI device that is treated as an ivshmem device.
 * @param[in] bar_idx Index of the BAR to be initialized.
 *
 * @return None
 *
 * @pre vdev != NULL
 * @pre vdev->pci_dev_config != NULL
 * @pre bar_idx < PCI_BAR_COUNT
 *
 * @post N/A
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

/**
 * @brief Initialize a virtual ivshmem device.
 *
 * This function initializes the specified virtual PCI device as an ivshmem device. It sets up the device to follow the
 * specifications. Because the ivshmem is introduced by QEMU, the spec link is
 * https://www.qemu.org/docs/master/specs/ivshmem-spec.html. This function is usually used in the initialization phase
 * of VM.
 *
 * - It sets the pcidev field in ivshmem_device (all ivshmem devices emulated by hypervisor are static stored based on
 *   the configuration) to the vdev and sets the priv_data field in the specified vdev to new ivshmem_device structure
 *   data, indicating the association between the virtual PCI device and the ivshmem device.
 * - Per the ivshmem specification and PCI Express Base Specification, it initializes the ivshmem device configuration
 *   space with appropriate values:
 *   - The device ID and Vendor ID is 0x11101af4.
 *   - It sets subsystem vendor ID to 0x8086 (Intel) and subsystem ID to the region ID of the shared memory region.
 *   - It sets up the MSI-X capability with 8 MSI-X table entries and maps the table and PBA into BAR1. For detailed
 *     operations, refer to add_vmsix_capability().
 *   - It initializes BAR0 for the device to hold device registers (256 Byte MMIO).
 *   - It initializes BAR1 for the device to hold MSI-X table and PBA.
 *   - It initializes BAR2 for the device to map the shared memory object. Because BAR2 is a 64-bit memory BAR, it also
 *     sets up the next Base Address Register as the high 32 bits and the total number of bars is set to 4.
 *   - It binds the device to the ivshmem server (hosts in hypervisor) for inter-VM communication.
 * - Finally, it sets the user field to vdev, indicating that this ivshmem is used by a VM.
 *
 * @param[inout] vdev Pointer to the virtual PCI device to be initialized.
 *
 * @return None
 *
 * @pre vdev != NULL
 * @pre vdev->pci_dev_config != NULL
 * @pre vdev->pci != NULL
 *
 * @post N/A
 */
static void init_ivshmem_vdev(struct pci_vdev *vdev)
{
	struct acrn_vm_pci_dev_config *dev_config = vdev->pci_dev_config;
	struct ivshmem_shm_region *region = find_shm_region(dev_config->shm_region_name);

	create_ivshmem_device(vdev);

	/* initialize ivshmem config */
	pci_vdev_write_vcfg(vdev, PCIR_VENDOR, 2U, IVSHMEM_VENDOR_ID);
	pci_vdev_write_vcfg(vdev, PCIR_DEVICE, 2U, IVSHMEM_DEVICE_ID);
	pci_vdev_write_vcfg(vdev, PCIR_REVID, 1U, IVSHMEM_REV);
	pci_vdev_write_vcfg(vdev, PCIR_CLASS, 1U, IVSHMEM_CLASS);
	pci_vdev_write_vcfg(vdev, PCIR_HDRTYPE, 1U,
		PCIM_HDRTYPE_NORMAL | ((vdev->bdf.bits.f == 0U) ? PCIM_MFDEV : 0U));

	pci_vdev_write_vcfg(vdev, PCIV_SUB_VENDOR_ID, 2U, IVSHMEM_INTEL_SUBVENDOR_ID);
	if (region != NULL) {
		pci_vdev_write_vcfg(vdev, PCIV_SUB_SYSTEM_ID, 2U, region->region_id);
	}

	add_vmsix_capability(vdev, MAX_IVSHMEM_MSIX_TBL_ENTRY_NUM, IVSHMEM_MSIX_BAR);

	/* initialize ivshmem bars */
	vdev->nr_bars = 4U;
	init_ivshmem_bar(vdev, IVSHMEM_MMIO_BAR);
	init_ivshmem_bar(vdev, IVSHMEM_MSIX_BAR);
	init_ivshmem_bar(vdev, IVSHMEM_SHM_BAR);
	ivshmem_server_bind_peer(vdev);

	vdev->user = vdev;
}

/**
 * @brief Deinitialize a virtual ivshmem device.
 *
 * This function deinitializes the specified virtual PCI device that was previously initialized as an ivshmem device.
 *
 * - It unbinds the device from the ivshmem server (hosts in hypervisor).
 * - It sets the priv_data field in the specified vdev to NULL and sets the pcidev field in ivshmem_device to NULL,
 *   indicating the disassociation between the virtual PCI device and the ivshmem device.
 * - It sets the user field to NULL, indicating that this virtual device is not owned by any VM.
 *
 * @param[inout] vdev Pointer to the virtual PCI device to be deinitialized.
 *
 * @return None
 *
 * @pre vdev != NULL
 * @pre vdev->priv_data != NULL
 * @pre vdev->pci != NULL
 *
 * @post N/A
 */
static void deinit_ivshmem_vdev(struct pci_vdev *vdev)
{
	struct ivshmem_device *ivs_dev = (struct ivshmem_device *) vdev->priv_data;

	ivshmem_server_unbind_peer(vdev);

	spinlock_obtain(&ivshmem_dev_lock);
	vdev->priv_data = NULL;
	vdev->user = NULL;
	ivs_dev->pcidev = NULL;
	spinlock_release(&ivshmem_dev_lock);
}

/**
 * @brief Create a virtual ivshmem device based on the specified device information.
 *
 * Basic ivshmem information is configured in the scenario file. After compilation, some device configurations of every
 * ivshmem PCI device are stored in struct acrn_vm_pci_dev_config and every shared memory region is stored in struct
 * ivshmem_shm_region. This function creates one virtual ivshmem device based on the input device information. The
 * user-space tool(such as acrn-dm) may add an ivshmem device for a post-launch VM and this device is emulated in
 * hypervisor. This function is used for the case for now and it is usually used in the initialization phase of a
 * post-launch VM.
 *
 * - Per the ivshmem specification, BAR2 maps the shared memory object. For the ivshmem device to be created, the shared
 *   memory region name is stored in dev->args and the size of the shared memory region is stored in
 *   dev->io_size[IVSHMEM_SHM_BAR].
 * - It traverses all configured PCI devices of the specified VM. Based on the input shared memory region name, it finds
 *   corresponding acrn_vm_pci_dev_config and ivshmem_shm_region.
 * - If the acrn_vm_pci_dev_config is not found or the ivshmem_shm_region is not found or the size of ivshmem_shm_region
 *   is not equal to the size specified in dev->io_size[IVSHMEM_SHM_BAR], the function returns -EINVAL.
 * - Otherwise, update the acrn_vm_pci_dev_config with input device information specified in dev and initializes a new
 *   virtual PCI device as an ivshmem device. For detailed operations, refer to vpci_init_vdev(). The function returns
 *   -EINVAL if the vpci_init_vdev() fails.
 * - Finally, it returns 0 on success.
 *
 * @param[inout] vm Pointer to the VM that owns the ivshmem device.
 * @param[in] dev Pointer to the device information to create an ivshmem device.
 *
 * @return A int32_t value to indicate the status of the ivshmem device creation.
 *
 * @retval 0 On success.
 * @retval -EINVAL If the ivshmem device creation fails.
 *
 * @pre vm != NULL
 * @pre dev != NULL
 *
 * @post retval <= 0
 */
int32_t create_ivshmem_vdev(struct acrn_vm *vm, struct acrn_vdev *dev)
{
	uint32_t i;
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	struct acrn_vm_pci_dev_config *dev_config = NULL;
	struct pci_vdev *vdev = NULL;
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
				vdev = vpci_init_vdev(&vm->vpci, dev_config, NULL);
				spinlock_release(&vm->vpci.lock);
				if (vdev != NULL) {
					ret = 0;
				}
			}
			break;
		}
	}

	if (ret != 0) {
		pr_warn("%s, failed to create ivshmem device %x:%x.%x\n", __func__,
			dev->slot >> 8U, (dev->slot >> 3U) & 0x1fU, dev->slot & 0x7U);
	}
	return ret;
}

/**
 * @brief Destroy the virtual ivshmem device.
 *
 * This function is the counterpart of create_ivshmem_vdev(). This function destroys the specified virtual PCI device
 * that was previously initialized as an ivshmem device. It is usually used for a post-launch VM to destroy the ivshmem
 * device.
 *
 * - It updates all BARs of the specified vdev. For detailed operations, refer to vpci_update_one_vbar() and the
 *   function ivshmem_vbar_unmap().
 * - It deinitializes the specified virtual PCI device. For detailed operations, refer to vpci_deinit_vdev().
 * - Finally, it returns 0.
 *
 * @param[inout] vdev Pointer to the virtual PCI device to be destroyed.
 *
 * @return Always return 0.
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 *
 * @post retval == 0
 */
int32_t destroy_ivshmem_vdev(struct pci_vdev *vdev)
{
	uint32_t i;
	struct acrn_vpci *vpci = vdev->vpci;

	for (i = 0U; i < vdev->nr_bars; i++) {
		vpci_update_one_vbar(vdev, i, 0U, NULL, ivshmem_vbar_unmap);
	}

	spinlock_obtain(&vpci->lock);
	vpci_deinit_vdev(vdev);
	spinlock_release(&vpci->lock);

	return 0;
}

/**
 * @brief Data structure implementation for virtual Inter-VM shared memory device (ivshmem) operations.
 *
 * The ivshmem is actually first introduced by QEMU to share a memory region between multiple VMs and host. It is
 * modeled as a PCI device exposing said memory to the VM as a PCI BAR. ACRN also introduces it to transfer data between
 * VMs based on the shared memory region. Struct pci_vdev_ops is used to define the operations of virtual PCI device and
 * definition here is used to support ivshmem device.
 *
 * @consistency N/A
 * @alignment N/A
 *
 * @remark N/A
 */
const struct pci_vdev_ops vpci_ivshmem_ops = {
	.init_vdev	= init_ivshmem_vdev,
	.deinit_vdev	= deinit_ivshmem_vdev,
	.write_vdev_cfg	= write_ivshmem_vdev_cfg,
	.read_vdev_cfg	= read_ivshmem_vdev_cfg,
};
/**
 * @}
 */
#endif
