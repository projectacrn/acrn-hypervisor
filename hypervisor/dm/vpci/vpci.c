/*-
* Copyright (c) 2011 NetApp, Inc.
* Copyright (c) 2018-2025 Intel Corporation.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*
* $FreeBSD$
*/

#include <errno.h>
#include <ptdev.h>
#include <asm/guest/vm.h>
#include <asm/vtd.h>
#include <io.h>
#include <asm/mmu.h>
#include <vacpi.h>
#include <logmsg.h>
#include "vpci_priv.h"
#include <asm/pci_dev.h>
#include <hash.h>
#include <board_info.h>
#include <atomic.h>


static int32_t vpci_init_vdevs(struct acrn_vm *vm);
static int32_t vpci_read_cfg(struct acrn_vpci *vpci, union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t *val);
static int32_t vpci_write_cfg(struct acrn_vpci *vpci, union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val);
static struct pci_vdev *find_available_vdev(struct acrn_vpci *vpci, union pci_bdf bdf);

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool vpci_pio_cfgaddr_read(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes)
{
	uint32_t val = ~0U;
	struct acrn_vpci *vpci = &vcpu->vm->vpci;
	union pci_cfg_addr_reg *cfg_addr = &vpci->addr;
	struct acrn_pio_request *pio_req = &vcpu->req.reqs.pio_request;

	if ((addr == (uint16_t)PCI_CONFIG_ADDR) && (bytes == 4U)) {
		val = cfg_addr->value;
	}

	pio_req->value = val;

	return true;
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 *
 * @retval true on success.
 * @retval false. (ACRN will deliver this IO request to DM to handle for post-launched VM)
 */
static bool vpci_pio_cfgaddr_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes, uint32_t val)
{
	bool ret = true;
	struct acrn_vpci *vpci = &vcpu->vm->vpci;
	union pci_cfg_addr_reg *cfg_addr = &vpci->addr;
	union pci_bdf vbdf;

	if ((addr == (uint16_t)PCI_CONFIG_ADDR) && (bytes == 4U)) {
		/* unmask reserved fields: BITs 24-30 and BITs 0-1 */
		cfg_addr->value = val & (~0x7f000003U);

		if (is_postlaunched_vm(vcpu->vm)) {
			const struct pci_vdev *vdev;

			vbdf.value = cfg_addr->bits.bdf;
			vdev = find_available_vdev(vpci, vbdf);
			/* For post-launched VM, ACRN HV will only handle PT device,
			 * all virtual PCI device and QUIRK PT device
			 * still need to deliver to ACRN DM to handle.
			 */
			if ((vdev == NULL) || is_quirk_ptdev(vdev)) {
				ret = false;
			}
		}
	}

	return ret;
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre vcpu->vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre (get_vm_config(vcpu->vm->vm_id)->load_order == PRE_LAUNCHED_VM)
 *	|| (get_vm_config(vcpu->vm->vm_id)->load_order == SERVICE_VM)
 *
 * @retval true on success.
 * @retval false. (ACRN will deliver this IO request to DM to handle for post-launched VM)
 */
static bool vpci_pio_cfgdata_read(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes)
{
	int32_t ret = 0;
	struct acrn_vm *vm = vcpu->vm;
	struct acrn_vpci *vpci = &vm->vpci;
	union pci_cfg_addr_reg cfg_addr;
	union pci_bdf bdf;
	uint32_t val = ~0U;
	struct acrn_pio_request *pio_req = &vcpu->req.reqs.pio_request;

	cfg_addr.value = atomic_readandclear32(&vpci->addr.value);
	if (cfg_addr.bits.enable != 0U) {
		uint32_t offset = (uint16_t)cfg_addr.bits.reg_num + (addr - PCI_CONFIG_DATA);
		if (pci_is_valid_access(offset, bytes)) {
			bdf.value = cfg_addr.bits.bdf;
			ret = vpci_read_cfg(vpci, bdf, offset, bytes, &val);
		}
	}

	pio_req->value = val;
	return (ret == 0);
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre vcpu->vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre (get_vm_config(vcpu->vm->vm_id)->load_order == PRE_LAUNCHED_VM)
 *	|| (get_vm_config(vcpu->vm->vm_id)->load_order == SERVICE_VM)
 *
 * @retval true on success.
 * @retval false. (ACRN will deliver this IO request to DM to handle for post-launched VM)
 */
static bool vpci_pio_cfgdata_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes, uint32_t val)
{
	int32_t ret = 0;
	struct acrn_vm *vm = vcpu->vm;
	struct acrn_vpci *vpci = &vm->vpci;
	union pci_cfg_addr_reg cfg_addr;
	union pci_bdf bdf;

	cfg_addr.value = atomic_readandclear32(&vpci->addr.value);
	if (cfg_addr.bits.enable != 0U) {
		uint32_t offset = (uint16_t)cfg_addr.bits.reg_num + (addr - PCI_CONFIG_DATA);
		if (pci_is_valid_access(offset, bytes)) {
			bdf.value = cfg_addr.bits.bdf;
			ret = vpci_write_cfg(vpci, bdf, offset, bytes, val);
		}
	}

	return (ret == 0);
}

/**
 * @pre io_req != NULL && private_data != NULL
 *
 * @retval 0 on success.
 * @retval other on false. (ACRN will deliver this MMIO request to DM to handle for post-launched VM)
 */
static int32_t vpci_mmio_cfg_access(struct io_request *io_req, void *private_data)
{
	int32_t ret = 0;
	struct acrn_mmio_request *mmio = &io_req->reqs.mmio_request;
	struct acrn_vpci *vpci = (struct acrn_vpci *)private_data;
	uint64_t pci_mmcofg_base = vpci->pci_mmcfg.address;
	uint64_t address = mmio->address;
	uint32_t reg_num = (uint32_t)(address & 0xfffUL);
	union pci_bdf bdf;

	/**
	 * Enhanced Configuration Address Mapping
	 * A[(20+n-1):20] Bus Number 1 ≤ n ≤ 8
	 * A[19:15] Device Number
	 * A[14:12] Function Number
	 * A[11:8] Extended Register Number
	 * A[7:2] Register Number
	 * A[1:0] Along with size of the access, used to generate Byte Enables
	 */
	bdf.value = (uint16_t)((address - pci_mmcofg_base) >> 12U);

	if (mmio->direction == ACRN_IOREQ_DIR_READ) {
		uint32_t val = ~0U;

		if (pci_is_valid_access(reg_num, (uint32_t)mmio->size)) {
			ret = vpci_read_cfg(vpci, bdf, reg_num, (uint32_t)mmio->size, &val);
		}
		mmio->value = val;
	} else {
		if (pci_is_valid_access(reg_num, (uint32_t)mmio->size)) {
			ret = vpci_write_cfg(vpci, bdf, reg_num, (uint32_t)mmio->size, (uint32_t)mmio->value);
		}
	}

	return ret;
}

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 */
int32_t init_vpci(struct acrn_vm *vm)
{
	struct vm_io_range pci_cfgaddr_range = {
		.base = PCI_CONFIG_ADDR,
		.len = 1U
	};

	struct vm_io_range pci_cfgdata_range = {
		.base = PCI_CONFIG_DATA,
		.len = 4U
	};

	struct acrn_vm_config *vm_config;
	struct pci_mmcfg_region *pci_mmcfg;
	int32_t ret = 0;

	vm->iommu = create_iommu_domain(vm->vm_id, hva2hpa(vm->arch_vm.nworld_eptp), 48U);

	vm_config = get_vm_config(vm->vm_id);
	/* virtual PCI MMCONFIG for Service VM is same with the physical value */
	if (vm_config->load_order == SERVICE_VM) {
		pci_mmcfg = get_mmcfg_region();
		vm->vpci.pci_mmcfg = *pci_mmcfg;
		vm->vpci.res32.start = MMIO32_START;
		vm->vpci.res32.end = MMIO32_END;
		vm->vpci.res64.start = MMIO64_START;
		vm->vpci.res64.end = MMIO64_END;
	} else {
		vm->vpci.pci_mmcfg.address = USER_VM_VIRT_PCI_MMCFG_BASE;
		vm->vpci.pci_mmcfg.start_bus = USER_VM_VIRT_PCI_MMCFG_START_BUS;
		vm->vpci.pci_mmcfg.end_bus = USER_VM_VIRT_PCI_MMCFG_END_BUS;
		vm->vpci.res32.start = USER_VM_VIRT_PCI_MEMBASE32;
		vm->vpci.res32.end = USER_VM_VIRT_PCI_MEMLIMIT32;
		vm->vpci.res64.start = USER_VM_VIRT_PCI_MEMBASE64;
		vm->vpci.res64.end = USER_VM_VIRT_PCI_MEMLIMIT64;
	}

	/* Build up vdev list for vm */
	ret = vpci_init_vdevs(vm);

	if (ret == 0) {
		register_mmio_emulation_handler(vm, vpci_mmio_cfg_access, vm->vpci.pci_mmcfg.address,
			vm->vpci.pci_mmcfg.address + get_pci_mmcfg_size(&vm->vpci.pci_mmcfg), &vm->vpci, false);

		/* Intercept and handle I/O ports CF8h */
		register_pio_emulation_handler(vm, PCI_CFGADDR_PIO_IDX, &pci_cfgaddr_range,
			vpci_pio_cfgaddr_read, vpci_pio_cfgaddr_write);

		/* Intercept and handle I/O ports CFCh -- CFFh */
		register_pio_emulation_handler(vm, PCI_CFGDATA_PIO_IDX, &pci_cfgdata_range,
			vpci_pio_cfgdata_read, vpci_pio_cfgdata_write);

		spinlock_init(&vm->vpci.lock);
	}

	return ret;
}

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 */
void deinit_vpci(struct acrn_vm *vm)
{
	struct pci_vdev *vdev, *parent_vdev;
	uint32_t i;

	for (i = 0U; i < CONFIG_MAX_PCI_DEV_NUM; i++) {
		vdev = (struct pci_vdev *) &(vm->vpci.pci_vdevs[i]);

		/* Only deinit the VM's own devices */
		if (vdev->user == vdev) {
			parent_vdev = vdev->parent_user;

			vdev->vdev_ops->deinit_vdev(vdev);

			if (parent_vdev != NULL) {
				spinlock_obtain(&parent_vdev->vpci->lock);
				parent_vdev->vdev_ops->init_vdev(parent_vdev);
				spinlock_release(&parent_vdev->vpci->lock);
			}
		}
	}

	ptdev_release_all_entries(vm);
	(void)memset(&vm->vpci, 0U, sizeof(struct acrn_vpci));

	/* Free iommu */
	destroy_iommu_domain(vm->iommu);
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vpci2vm(vdev->vpci)->iommu != NULL
 */
static void assign_vdev_pt_iommu_domain(struct pci_vdev *vdev)
{
	int32_t ret;
	struct acrn_vm *vm = vpci2vm(vdev->vpci);

	ret = move_pt_device(NULL, vm->iommu, (uint8_t)vdev->pdev->bdf.bits.b,
		(uint8_t)(vdev->pdev->bdf.value & 0xFFU));
	if (ret != 0) {
		panic("failed to assign iommu device!");
	}
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vpci2vm(vdev->vpci)->iommu != NULL
 */
static void remove_vdev_pt_iommu_domain(const struct pci_vdev *vdev)
{
	int32_t ret;
	const struct acrn_vm *vm = vpci2vm(vdev->vpci);

	ret = move_pt_device(vm->iommu, NULL, (uint8_t)vdev->pdev->bdf.bits.b,
		(uint8_t)(vdev->pdev->bdf.value & 0xFFU));
	if (ret != 0) {
		/*
		 *TODO
		 * panic needs to be removed here
		 * Currently unassign_pt_device can fail for multiple reasons
		 * Once all the reasons and methods to avoid them can be made sure
		 * panic here is not necessary.
		 */
		panic("failed to unassign iommu device!");
	}
}

/**
 * @brief Find an available vdev structure with BDF from a specified vpci structure.
 *        If the vdev's vpci is the same as the specified vpci, the vdev is available.
 *        If the vdev's vpci is not the same as the specified vpci, the vdev has already
 *        been assigned and it is unavailable for Service VM.
 *        If the vdev's vpci is NULL, the vdev is a orphan/zombie instance, it can't
 *        be accessed by any vpci.
 *
 * @param vpci Pointer to a specified vpci structure
 * @param bdf  Indicate the vdev's BDF
 *
 * @pre vpci != NULL
 *
 * @return Return a available vdev instance, otherwise return NULL
 */
static struct pci_vdev *find_available_vdev(struct acrn_vpci *vpci, union pci_bdf bdf)
{
	struct pci_vdev *vdev = pci_find_vdev(vpci, bdf);

	if ((vdev != NULL) && (vdev->user != vdev)) {
		if (vdev->user != NULL) {
			/* the Service VM is able to access, if and only if the Service VM has higher severity than the User VM. */
			if (get_vm_severity(vpci2vm(vpci)->vm_id) <
					get_vm_severity(vpci2vm(vdev->user->vpci)->vm_id)) {
				vdev = NULL;
			}
		} else {
			vdev = NULL;
		}
	}

	return vdev;
}

static void vpci_init_pt_dev(struct pci_vdev *vdev)
{
	vdev->parent_user = NULL;
	vdev->user = vdev;

	/*
	 * Here init_vdev_pt() needs to be called after init_vmsix_pt() for the following reason:
	 * init_vdev_pt() will indirectly call has_msix_cap(), which
	 * requires init_vmsix_pt() to be called first.
	 */
	init_vmsi(vdev);
	init_vmsix_pt(vdev);
	init_vsriov(vdev);
	init_vdev_pt(vdev, false);

	assign_vdev_pt_iommu_domain(vdev);
}

static void vpci_deinit_pt_dev(struct pci_vdev *vdev)
{
	deinit_vdev_pt(vdev);
	remove_vdev_pt_iommu_domain(vdev);
	deinit_vmsix_pt(vdev);
	deinit_vmsi(vdev);

	vdev->user = NULL;
	vdev->parent_user = NULL;
}

struct cfg_header_perm {
	/* For each 4-byte register defined in PCI config space header,
	 * there is one bit dedicated for it in pt_mask and ro_mask.
	 * For example, bit 0 for CFG Vendor ID and Device ID register,
	 * Bit 1 for CFG register Command and Status register, and so on.
	 *
	 * For each mask, only low 16-bits takes effect.
	 *
	 * When guest read:
	 * If bit x is set the pt_mask, it indicates that the corresponding 4 Bytes register for bit x is pass through to guest.
	 * Otherwise, bit x is not set the pt_mask, it's virtualized.
	 *
	 * When guest write:
	 * If bit x is set the ro_mask, it indicates that the corresponding 4 Bytes register for bit x is not writable.
	 * Otherwise, that is, bit x is not set the ro_mask,
	 * If bit x is set the pt_mask, it indicates that the corresponding 4 Bytes register for bit x is pass through to guest.
	 * If bit x is not set the pt_mask, it's virtualized.
	 */
	/* For type 0 device */
	uint32_t type0_pt_mask;
	uint32_t type0_ro_mask;
	/* For type 1 device */
	uint32_t type1_pt_mask;
	uint32_t type1_ro_mask;
};

static const struct cfg_header_perm cfg_hdr_perm = {
	/* Only Command (0x04-0x05) and Status (0x06-0x07) Registers are pass through */
	.type0_pt_mask = 0x0002U,
	/* Command (0x04-0x05) and Status (0x06-0x07) Registers and
	 * Base Address Registers (0x10-0x27) are writable */
	.type0_ro_mask = (uint16_t)~0x03f2U,
	/* Command (0x04-0x05) and Status (0x06-0x07) Registers and
	 * from Primary Bus Number to I/O Base Limit 16 Bits (0x18-0x33)
	 * are pass through
	 */
	.type1_pt_mask = 0x1fc2U,
	/* Command (0x04-0x05) and Status (0x06-0x07) Registers and
	 * Base Address Registers (0x10-0x17) and
	 * Secondary Status (0x1e-0x1f) are writable
	 * Note: should handle I/O Base (0x1c) specially
	 */
	.type1_ro_mask = (uint16_t)~0xb2U
};


/*
 * @pre offset + bytes < PCI_CFG_HEADER_LENGTH
 */
static int32_t read_cfg_header(const struct pci_vdev *vdev,
		uint32_t offset, uint32_t bytes, uint32_t *val)
{
	int32_t ret = 0;
	uint32_t pt_mask;

	if ((offset == PCIR_BIOS) && is_quirk_ptdev(vdev)) {
		/* the access of PCIR_BIOS is emulated for quirk_ptdev */
		ret = -ENODEV;
	} else if (vbar_access(vdev, offset)) {
		/* bar access must be 4 bytes and offset must also be 4 bytes aligned */
		if ((bytes == 4U) && ((offset & 0x3U) == 0U)) {
			*val = pci_vdev_read_vcfg(vdev, offset, bytes);
		} else {
			*val = ~0U;
		}
	} else {
		if (is_bridge(vdev->pdev)) {
			pt_mask = cfg_hdr_perm.type1_pt_mask;
		} else {
			pt_mask = cfg_hdr_perm.type0_pt_mask;
		}

		if (bitmap32_test(((uint16_t)offset) >> 2U, &pt_mask)) {
			*val = pci_pdev_read_cfg(vdev->pdev->bdf, offset, bytes);

			/* MSE(Memory Space Enable) bit always be set for an assigned VF */
			if ((vdev->phyfun != NULL) && (offset == PCIR_COMMAND) &&
					(vdev->vpci != vdev->phyfun->vpci)) {
				*val |= PCIM_CMD_MEMEN;
			}
		} else {
			*val = pci_vdev_read_vcfg(vdev, offset, bytes);
		}
	}
	return ret;
}

/*
 * @pre offset + bytes < PCI_CFG_HEADER_LENGTH
 */
static int32_t write_cfg_header(struct pci_vdev *vdev,
		uint32_t offset, uint32_t bytes, uint32_t val)
{
	bool dev_is_bridge = is_bridge(vdev->pdev);
	int32_t ret = 0;
	uint32_t pt_mask, ro_mask;

	if ((offset == PCIR_BIOS) && is_quirk_ptdev(vdev)) {
		/* the access of PCIR_BIOS is emulated for quirk_ptdev */
		ret = -ENODEV;
	} else if (vbar_access(vdev, offset)) {
		/* bar write access must be 4 bytes and offset must also be 4 bytes aligned */
		if ((bytes == 4U) && ((offset & 0x3U) == 0U)) {
			vdev_pt_write_vbar(vdev, pci_bar_index(offset), val);
		}
	} else {
		if (offset == PCIR_COMMAND) {
#define PCIM_SPACE_EN (PCIM_CMD_PORTEN | PCIM_CMD_MEMEN)
			uint16_t phys_cmd = (uint16_t)pci_pdev_read_cfg(vdev->pdev->bdf, PCIR_COMMAND, 2U);

			if (((phys_cmd & PCIM_SPACE_EN) == 0U) && ((val & PCIM_SPACE_EN) != 0U)) {
				/* check whether need to restore BAR because some kind of reset */
				if (pdev_need_bar_restore(vdev->pdev)) {
					pdev_restore_bar(vdev->pdev);
				}

				/* check whether need to restore bridge mem/IO related registers because some kind of reset */
				if (dev_is_bridge) {
					vdev_bridge_pt_restore_space(vdev);
				}
			}
			/* check whether need to restore Primary/Secondary/Subordinate Bus Number registers because some kind of reset */
			if (dev_is_bridge && ((phys_cmd & PCIM_CMD_BUSEN) == 0U) && ((val & PCIM_CMD_BUSEN) != 0U)) {
				vdev_bridge_pt_restore_bus(vdev);
			}
		}

		if (dev_is_bridge) {
			ro_mask = cfg_hdr_perm.type1_ro_mask;
			pt_mask = cfg_hdr_perm.type1_pt_mask;
		} else {
			ro_mask = cfg_hdr_perm.type0_ro_mask;
			pt_mask = cfg_hdr_perm.type0_pt_mask;
		}

		if (!bitmap32_test(((uint16_t)offset) >> 2U, &ro_mask)) {
			if (bitmap32_test(((uint16_t)offset) >> 2U, &pt_mask)) {
				/* I/O Base (0x1c) and I/O Limit (0x1d) are read-only */
				if (!((offset == PCIR_IO_BASE) && (bytes <= 2)) && (offset != PCIR_IO_LIMIT)) {
					uint32_t value = val;
					if ((offset == PCIR_IO_BASE) && (bytes == 4U)) {
						uint16_t phys_val = (uint16_t)pci_pdev_read_cfg(vdev->pdev->bdf, offset, 2U);
						value = (val & PCIR_SECSTATUS_LINE_MASK) | phys_val;
					}
					pci_pdev_write_cfg(vdev->pdev->bdf, offset, bytes, value);
				}
			} else {
				pci_vdev_write_vcfg(vdev, offset, bytes, val);
			}
		}

		/* According to PCIe Spec, for a RW register bits, If the optional feature
		 * that is associated with the bits is not implemented, the bits are permitted
		 * to be hardwired to 0b. However Zephyr would use INTx Line Register as writable
		 * even this PCI device has no INTx, so emulate INTx Line Register as writable.
		 */
		if (offset == PCIR_INTERRUPT_LINE) {
			pci_vdev_write_vcfg(vdev, offset, bytes, (val & 0xfU));
		}

	}
	return ret;
}

static int32_t write_pt_dev_cfg(struct pci_vdev *vdev, uint32_t offset,
		uint32_t bytes, uint32_t val)
{
	int32_t ret = 0;

	if (cfg_header_access(offset)) {
		ret = write_cfg_header(vdev, offset, bytes, val);
	} else if (msicap_access(vdev, offset)) {
		write_vmsi_cap_reg(vdev, offset, bytes, val);
	} else if (msixcap_access(vdev, offset)) {
		if (vdev->msix.is_vmsix_on_msi) {
			write_vmsix_cap_reg_on_msi(vdev, offset, bytes, val);
		} else {
			write_pt_vmsix_cap_reg(vdev, offset, bytes, val);
		}
	} else if (sriovcap_access(vdev, offset)) {
		write_sriov_cap_reg(vdev, offset, bytes, val);
	} else {
		if (offset != vdev->pdev->sriov.pre_pos) {
			if (!is_quirk_ptdev(vdev)) {
				if ((vdev->pdev->bdf.value != CONFIG_IGD_SBDF) || (offset != PCIR_ASLS_CTL)) {
					/* passthru to physical device */
					pci_pdev_write_cfg(vdev->pdev->bdf, offset, bytes, val);
				}
			} else {
				ret = -ENODEV;
			}
		}
	}

	return ret;
}

static int32_t read_pt_dev_cfg(struct pci_vdev *vdev, uint32_t offset,
		uint32_t bytes, uint32_t *val)
{
	int32_t ret = 0;

	if (cfg_header_access(offset)) {
		ret = read_cfg_header(vdev, offset, bytes, val);
	} else if (msicap_access(vdev, offset)) {
		*val = pci_vdev_read_vcfg(vdev, offset, bytes);
	} else if (msixcap_access(vdev, offset)) {
		read_pt_vmsix_cap_reg(vdev, offset, bytes, val);
	} else if (sriovcap_access(vdev, offset)) {
		read_sriov_cap_reg(vdev, offset, bytes, val);
	} else {
		if ((offset == vdev->pdev->sriov.pre_pos) && (vdev->pdev->sriov.hide_sriov)) {
			*val = pci_vdev_read_vcfg(vdev, offset, bytes);
		} else if (!is_quirk_ptdev(vdev)) {
			/* passthru to physical device */
			*val = pci_pdev_read_cfg(vdev->pdev->bdf, offset, bytes);
			if ((vdev->pdev->bdf.value == CONFIG_IGD_SBDF) && (offset == PCIR_ASLS_CTL)) {
				*val = pci_vdev_read_vcfg(vdev, offset, bytes);
			}
		} else {
			ret = -ENODEV;
		}
	}

	return ret;
}

static const struct pci_vdev_ops pci_pt_dev_ops = {
	.init_vdev	= vpci_init_pt_dev,
	.deinit_vdev	= vpci_deinit_pt_dev,
	.write_vdev_cfg	= write_pt_dev_cfg,
	.read_vdev_cfg	= read_pt_dev_cfg,
};

/**
 * @pre vpci != NULL
 */
static int32_t vpci_read_cfg(struct acrn_vpci *vpci, union pci_bdf bdf,
	uint32_t offset, uint32_t bytes, uint32_t *val)
{
	int32_t ret = 0;
	struct pci_vdev *vdev;

	spinlock_obtain(&vpci->lock);
	vdev = find_available_vdev(vpci, bdf);
	if (vdev != NULL) {
		ret = vdev->vdev_ops->read_vdev_cfg(vdev, offset, bytes, val);
	} else {
		if (is_postlaunched_vm(vpci2vm(vpci))) {
			ret = -ENODEV;
		} else if (is_plat_hidden_pdev(bdf)) {
			/* expose and pass through platform hidden devices */
			*val = pci_pdev_read_cfg(bdf, offset, bytes);
		} else {
			/* no action: e.g., PCI scan */
		}
	}
	spinlock_release(&vpci->lock);
	return ret;
}

/**
 * @pre vpci != NULL
 */
static int32_t vpci_write_cfg(struct acrn_vpci *vpci, union pci_bdf bdf,
	uint32_t offset, uint32_t bytes, uint32_t val)
{
	int32_t ret = 0;
	struct pci_vdev *vdev;

	spinlock_obtain(&vpci->lock);
	vdev = find_available_vdev(vpci, bdf);
	if (vdev != NULL) {
		ret = vdev->vdev_ops->write_vdev_cfg(vdev, offset, bytes, val);
	} else {
		if (is_postlaunched_vm(vpci2vm(vpci))) {
			ret = -ENODEV;
		} else if (is_plat_hidden_pdev(bdf)) {
			/* expose and pass through platform hidden devices */
			pci_pdev_write_cfg(bdf, offset, bytes, val);
		} else {
			pr_acrnlog("%s %x:%x.%x not found! off: 0x%x, val: 0x%x\n", __func__,
				bdf.bits.b, bdf.bits.d, bdf.bits.f, offset, val);
		}
	}
	spinlock_release(&vpci->lock);
	return ret;
}

/**
 * @brief Initialize a vdev structure.
 *
 * The function vpci_init_vdev is used to initialize a vdev structure with a PCI device configuration(dev_config)
 * on a specified vPCI bus(vpci). If the function vpci_init_vdev initializes a SRIOV Virtual Function(VF) vdev structure,
 * the parameter parent_pf_vdev is the VF associated Physical Function(PF) vdev structure, otherwise the parameter parent_pf_vdev is NULL.
 * The caller of the function vpci_init_vdev should guarantee execution atomically.
 *
 * @param vpci              Pointer to a vpci structure
 * @param dev_config        Pointer to a dev_config structure of the vdev
 * @param parent_pf_vdev    If the parameter def_config points to a SRIOV VF vdev, this parameter parent_pf_vdev indicates the parent PF vdev.
 *                          Otherwise, it is NULL.
 *
 * @pre vpci != NULL
 *
 * @return If there's a successfully initialized vdev structure return it, otherwise return NULL;
 */
struct pci_vdev *vpci_init_vdev(struct acrn_vpci *vpci, struct acrn_vm_pci_dev_config *dev_config, struct pci_vdev *parent_pf_vdev)
{
	struct pci_vdev *vdev = NULL;
	uint32_t id = (uint32_t)ffz64_ex(vpci->vdev_bitmaps, CONFIG_MAX_PCI_DEV_NUM);

	if (id < CONFIG_MAX_PCI_DEV_NUM) {
		bitmap_set_non_atomic((id & 0x3FU), &vpci->vdev_bitmaps[id >> 6U]);

		vdev = &vpci->pci_vdevs[id];
		vdev->id = id;
		vdev->vpci = vpci;
		vdev->bdf.value = dev_config->vbdf.value;
		vdev->pdev = dev_config->pdev;
		vdev->pci_dev_config = dev_config;
		vdev->phyfun = parent_pf_vdev;

		hlist_add_head(&vdev->link, &vpci->vdevs_hlist_heads[hash64(dev_config->vbdf.value, VDEV_LIST_HASHBITS)]);
		if (dev_config->vdev_ops != NULL) {
			vdev->vdev_ops = dev_config->vdev_ops;
		} else {
			vdev->vdev_ops = &pci_pt_dev_ops;
			ASSERT(dev_config->emu_type == PCI_DEV_TYPE_PTDEV,
				"Only PCI_DEV_TYPE_PTDEV could not configure vdev_ops");
			ASSERT(dev_config->pdev != NULL, "PCI PTDev is not present on platform!");
		}
		vdev->vdev_ops->init_vdev(vdev);
	}
	return vdev;
}

/**
 * @brief Deinitialize a vdev structure.
 * 
 * The caller of the function vpci_init_vdev should guarantee execution atomically.
 *
 * @param vdev              Pointer to a vdev structure
 *
 * @pre vpci != NULL
 * @pre vdev->vpci != NULL
 */
void vpci_deinit_vdev(struct pci_vdev *vdev)
{
	vdev->vdev_ops->deinit_vdev(vdev);

	hlist_del(&vdev->link);
	bitmap_clear_non_atomic((vdev->id & 0x3FU), &vdev->vpci->vdev_bitmaps[vdev->id >> 6U]);
	memset(vdev, 0U, sizeof(struct pci_vdev));
}

/**
 * @pre vm != NULL
 */
static int32_t vpci_init_vdevs(struct acrn_vm *vm)
{
	uint16_t idx;
	struct pci_vdev *vdev;
	struct acrn_vpci *vpci = &(vm->vpci);
	const struct acrn_vm_config *vm_config = get_vm_config(vpci2vm(vpci)->vm_id);
	int32_t ret = 0;

	for (idx = 0U; idx < vm_config->pci_dev_num; idx++) {
		/* the vdev whose vBDF is unassigned will be created by hypercall */
		if ((!is_postlaunched_vm(vm)) || (vm_config->pci_devs[idx].vbdf.value != UNASSIGNED_VBDF)) {
			vdev = vpci_init_vdev(vpci, &vm_config->pci_devs[idx], NULL);
			if (vdev == NULL) {
				pr_err("%s: failed to initialize vpci, increase MAX_PCI_DEV_NUM in scenario!\n", __func__);
				break;
			}
			ret = check_pt_dev_pio_bars(&vpci->pci_vdevs[idx]);
			if (ret != 0) {
				break;
			}
		}
	}

	return ret;
}

/**
 * @brief assign a PCI device from Service VM to target post-launched VM.
 *
 * @pre tgt_vm != NULL
 * @pre pcidev != NULL
 */
int32_t vpci_assign_pcidev(struct acrn_vm *tgt_vm, struct acrn_pcidev *pcidev)
{
	int32_t ret = 0;
	uint32_t idx;
	struct pci_vdev *vdev_in_service_vm, *vdev;
	struct acrn_vpci *vpci;
	union pci_bdf bdf;
	struct acrn_vm *service_vm;

	bdf.value = pcidev->phys_bdf;
	service_vm = get_service_vm();
	spinlock_obtain(&service_vm->vpci.lock);
	vdev_in_service_vm = pci_find_vdev(&service_vm->vpci, bdf);
	if ((vdev_in_service_vm != NULL) && (vdev_in_service_vm->user == vdev_in_service_vm) &&
			(vdev_in_service_vm->pdev != NULL) &&
			!is_host_bridge(vdev_in_service_vm->pdev) && !is_bridge(vdev_in_service_vm->pdev)) {

		/* ToDo: Each PT device must support one type reset */
		if (!vdev_in_service_vm->pdev->has_pm_reset && !vdev_in_service_vm->pdev->has_flr &&
				!vdev_in_service_vm->pdev->has_af_flr) {
			pr_fatal("%s %x:%x.%x not support FLR or not support PM reset\n",
				__func__, bdf.bits.b,  bdf.bits.d,  bdf.bits.f);
		} else {
			/* DM will reset this device before assigning it */
			pdev_restore_bar(vdev_in_service_vm->pdev);
		}

		vdev_in_service_vm->vdev_ops->deinit_vdev(vdev_in_service_vm);

		vpci = &(tgt_vm->vpci);

		spinlock_obtain(&tgt_vm->vpci.lock);
		vdev = vpci_init_vdev(vpci, vdev_in_service_vm->pci_dev_config, vdev_in_service_vm->phyfun);
		if (vdev != NULL) {
			pci_vdev_write_vcfg(vdev, PCIR_INTERRUPT_LINE, 1U, pcidev->intr_line);
			pci_vdev_write_vcfg(vdev, PCIR_INTERRUPT_PIN, 1U, pcidev->intr_pin);
			for (idx = 0U; idx < vdev->nr_bars; idx++) {
				/* VF is assigned to a User VM */
				if (vdev->phyfun != NULL) {
					vdev->vbars[idx] = vdev_in_service_vm->vbars[idx];
					if (has_msix_cap(vdev) && (idx == vdev->msix.table_bar)) {
						vdev->msix.mmio_hpa = vdev->vbars[idx].base_hpa;
						vdev->msix.mmio_size = vdev->vbars[idx].size;
					}
				}
				pci_vdev_write_vbar(vdev, idx, pcidev->bar[idx]);
			}

			ret = check_pt_dev_pio_bars(vdev);

			if (ret == 0) {
				vdev->flags |= pcidev->type;
				vdev->bdf.value = pcidev->virt_bdf;
				/*We should re-add the vdev to hashlist since its vbdf has changed */
				hlist_del(&vdev->link);
				hlist_add_head(&vdev->link, &vpci->vdevs_hlist_heads[hash64(vdev->bdf.value, VDEV_LIST_HASHBITS)]);
				vdev->parent_user = vdev_in_service_vm;
				vdev_in_service_vm->user = vdev;
			} else {
				vdev->vdev_ops->deinit_vdev(vdev);
				vdev_in_service_vm->vdev_ops->init_vdev(vdev_in_service_vm);
			}
		} else {
			pr_fatal("%s, Failed to initialize PCI device %x:%x.%x for vm [%d]\n", __func__,
				pcidev->phys_bdf >> 8U, (pcidev->phys_bdf >> 3U) & 0x1fU, pcidev->phys_bdf & 0x7U,
				tgt_vm->vm_id);
			ret = -EFAULT;
		}
		spinlock_release(&tgt_vm->vpci.lock);
	} else {
		pr_fatal("%s, can't find PCI device %x:%x.%x for vm[%d] %x:%x.%x\n", __func__,
			pcidev->phys_bdf >> 8U, (pcidev->phys_bdf >> 3U) & 0x1fU, pcidev->phys_bdf & 0x7U,
			tgt_vm->vm_id,
			pcidev->virt_bdf >> 8U, (pcidev->virt_bdf >> 3U) & 0x1fU, pcidev->virt_bdf & 0x7U);
		ret = -ENODEV;
	}
	spinlock_release(&service_vm->vpci.lock);

	return ret;
}

/**
 * @brief deassign a PCI device from target post-launched VM to Service VM.
 *
 * @pre tgt_vm != NULL
 * @pre pcidev != NULL
 */
int32_t vpci_deassign_pcidev(struct acrn_vm *tgt_vm, struct acrn_pcidev *pcidev)
{
	int32_t ret = 0;
	struct pci_vdev *parent_vdev, *vdev;
	struct acrn_vpci *vpci;
	union pci_bdf bdf;

	bdf.value = pcidev->virt_bdf;
	vdev = pci_find_vdev(&tgt_vm->vpci, bdf);
	if ((vdev != NULL) && (vdev->user == vdev) && (vdev->pdev != NULL) &&
			(vdev->pdev->bdf.value == pcidev->phys_bdf)) {
		vpci = vdev->vpci;
		parent_vdev = vdev->parent_user;

		spinlock_obtain(&vpci->lock);
		vpci_deinit_vdev(vdev);
		spinlock_release(&vpci->lock);

		if (parent_vdev != NULL) {
			spinlock_obtain(&parent_vdev->vpci->lock);
			parent_vdev->vdev_ops->init_vdev(parent_vdev);
			spinlock_release(&parent_vdev->vpci->lock);
		}
	} else {
		pr_fatal("%s, can't find PCI device %x:%x.%x for vm[%d] %x:%x.%x\n", __func__,
			pcidev->phys_bdf >> 8U, (pcidev->phys_bdf >> 3U) & 0x1fU, pcidev->phys_bdf & 0x7U,
			tgt_vm->vm_id,
			pcidev->virt_bdf >> 8U, (pcidev->virt_bdf >> 3U) & 0x1fU, pcidev->virt_bdf & 0x7U);
		ret = -ENODEV;
	}

	return ret;
}

/*
 * @pre unmap_cb != NULL
 */
void vpci_update_one_vbar(struct pci_vdev *vdev, uint32_t bar_idx, uint32_t val,
		map_pcibar map_cb, unmap_pcibar unmap_cb)
{
	struct pci_vbar *vbar = &vdev->vbars[bar_idx];
	uint32_t update_idx = bar_idx;

	if (vbar->is_mem64hi) {
		update_idx -= 1U;
	}
	unmap_cb(vdev, update_idx);
	pci_vdev_write_vbar(vdev, bar_idx, val);
	if ((map_cb != NULL) && (vdev->vbars[update_idx].base_gpa != 0UL)) {
		map_cb(vdev, update_idx);
	}
}

/**
 * @brief Add emulated legacy PCI capability support for virtual PCI device
 *
 * @param vdev     Pointer to vdev data structure
 * @param capdata  Pointer to buffer that holds the capability data to be added.
 * @param caplen   Length of buffer that holds the capability data to be added.
 *
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 */
uint32_t vpci_add_capability(struct pci_vdev *vdev, uint8_t *capdata, uint8_t caplen)
{
#define CAP_START_OFFSET PCI_CFG_HEADER_LENGTH

	uint8_t capoff, reallen;
	uint32_t sts;
	uint32_t ret = 0U;

	reallen = roundup(caplen, 4U); /* dword aligned */

	sts = pci_vdev_read_vcfg(vdev, PCIR_STATUS, 2U);
	if ((sts & PCIM_STATUS_CAPPRESENT) == 0U) {
		capoff = CAP_START_OFFSET;
	} else {
		capoff = vdev->free_capoff;
	}

	/* Check if we have enough space */
	if (((uint16_t)capoff + reallen) <= PCI_CONFIG_SPACE_SIZE) {
		/* Set the previous capability pointer */
		if ((sts & PCIM_STATUS_CAPPRESENT) == 0U) {
			pci_vdev_write_vcfg(vdev, PCIR_CAP_PTR, 1U, capoff);
			pci_vdev_write_vcfg(vdev, PCIR_STATUS, 2U, sts|PCIM_STATUS_CAPPRESENT);
		} else {
			pci_vdev_write_vcfg(vdev, vdev->prev_capoff + 1U, 1U, capoff);
		}

		/* Copy the capability */
		(void)memcpy_s((void *)&vdev->cfgdata.data_8[capoff], caplen, (void *)capdata, caplen);

		/* Set the next capability pointer */
		pci_vdev_write_vcfg(vdev, capoff + 1U, 1U, 0U);

		vdev->prev_capoff = capoff;
		vdev->free_capoff = capoff + reallen;
		ret = capoff;
	}

	return ret;
}

bool vpci_vmsix_enabled(const struct pci_vdev *vdev)
{
	uint32_t msgctrl;
	bool ret = false;

	if (vdev->msix.capoff != 0U) {
		msgctrl = pci_vdev_read_vcfg(vdev, vdev->msix.capoff + PCIR_MSIX_CTRL, 2U);
		if (((msgctrl & PCIM_MSIXCTRL_MSIX_ENABLE) != 0U) &&
			((msgctrl & PCIM_MSIXCTRL_FUNCTION_MASK) == 0U)) {
			ret = true;
		}
	}
	return ret;
}
