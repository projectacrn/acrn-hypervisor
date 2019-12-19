/*-
* Copyright (c) 2011 NetApp, Inc.
* Copyright (c) 2018 Intel Corporation
* All rights reserved.
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

#include <vm.h>
#include <vtd.h>
#include <mmu.h>
#include <logmsg.h>
#include "vpci_priv.h"
#include "pci_dev.h"

static void vpci_init_vdevs(struct acrn_vm *vm);
static void deinit_prelaunched_vm_vpci(struct acrn_vm *vm);
static void deinit_postlaunched_vm_vpci(struct acrn_vm *vm);
static void read_cfg(struct acrn_vpci *vpci, union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t *val);
static void write_cfg(struct acrn_vpci *vpci, union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val);

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool pci_cfgaddr_io_read(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes)
{
	uint32_t val = ~0U;
	struct acrn_vpci *vpci = &vcpu->vm->vpci;
	union pci_cfg_addr_reg *cfg_addr = &vpci->addr;
	struct pio_request *pio_req = &vcpu->req.reqs.pio;

	if ((addr == (uint16_t)PCI_CONFIG_ADDR) && (bytes == 4U)) {
		val = cfg_addr->value;
	}

	pio_req->value = val;

	return true;
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool pci_cfgaddr_io_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes, uint32_t val)
{
	struct acrn_vpci *vpci = &vcpu->vm->vpci;
	union pci_cfg_addr_reg *cfg_addr = &vpci->addr;

	if ((addr == (uint16_t)PCI_CONFIG_ADDR) && (bytes == 4U)) {
		/* unmask reserved fields: BITs 24-30 and BITs 0-1 */
		cfg_addr->value = val & (~0x7f000003U);
	}

	return true;
}

static inline bool vpci_is_valid_access_offset(uint32_t offset, uint32_t bytes)
{
	return ((offset & (bytes - 1U)) == 0U);
}

static inline bool vpci_is_valid_access_byte(uint32_t bytes)
{
	return ((bytes == 1U) || (bytes == 2U) || (bytes == 4U));
}

static inline bool vpci_is_valid_access(uint32_t offset, uint32_t bytes)
{
	return (vpci_is_valid_access_byte(bytes) && vpci_is_valid_access_offset(offset, bytes));
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre vcpu->vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre (get_vm_config(vcpu->vm->vm_id)->load_order == PRE_LAUNCHED_VM)
 *	|| (get_vm_config(vcpu->vm->vm_id)->load_order == SOS_VM)
 */
static bool pci_cfgdata_io_read(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes)
{
	struct acrn_vm *vm = vcpu->vm;
	struct acrn_vpci *vpci = &vm->vpci;
	union pci_cfg_addr_reg cfg_addr;
	union pci_bdf bdf;
	uint16_t offset = addr - PCI_CONFIG_DATA;
	uint32_t val = ~0U;
	struct acrn_vm_config *vm_config;
	struct pio_request *pio_req = &vcpu->req.reqs.pio;

	cfg_addr.value = atomic_readandclear32(&vpci->addr.value);
	if (cfg_addr.bits.enable != 0U) {
		if (vpci_is_valid_access(cfg_addr.bits.reg_num + offset, bytes)) {
			vm_config = get_vm_config(vm->vm_id);

			switch (vm_config->load_order) {
			case PRE_LAUNCHED_VM:
			case SOS_VM:
				bdf.value = cfg_addr.bits.bdf;
				read_cfg(vpci, bdf, cfg_addr.bits.reg_num + offset, bytes, &val);
				break;

			default:
				ASSERT(false, "Error, pci_cfgdata_io_read should only be called for PRE_LAUNCHED_VM and SOS_VM");
				break;
			}
		}
	}

	pio_req->value = val;

	return true;
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre vcpu->vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre (get_vm_config(vcpu->vm->vm_id)->load_order == PRE_LAUNCHED_VM)
 *	|| (get_vm_config(vcpu->vm->vm_id)->load_order == SOS_VM)
 */
static bool pci_cfgdata_io_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes, uint32_t val)
{
	struct acrn_vm *vm = vcpu->vm;
	struct acrn_vpci *vpci = &vm->vpci;
	union pci_cfg_addr_reg cfg_addr;
	union pci_bdf bdf;
	uint16_t offset = addr - PCI_CONFIG_DATA;
	struct acrn_vm_config *vm_config;

	cfg_addr.value = atomic_readandclear32(&vpci->addr.value);
	if (cfg_addr.bits.enable != 0U) {
		if (vpci_is_valid_access(cfg_addr.bits.reg_num + offset, bytes)) {
			vm_config = get_vm_config(vm->vm_id);

			switch (vm_config->load_order) {
			case PRE_LAUNCHED_VM:
			case SOS_VM:
				bdf.value = cfg_addr.bits.bdf;
				write_cfg(vpci, bdf, cfg_addr.bits.reg_num + offset, bytes, val);
				break;

			default:
				ASSERT(false, "Error, pci_cfgdata_io_write should only be called for PRE_LAUNCHED_VM and SOS_VM");
				break;
			}
		}
	}

	return true;
}

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 */
void vpci_init(struct acrn_vm *vm)
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

	vm->vpci.vm = vm;
	vm->iommu = create_iommu_domain(vm->vm_id, hva2hpa(vm->arch_vm.nworld_eptp), 48U);
	/* Build up vdev list for vm */
	vpci_init_vdevs(vm);

	vm_config = get_vm_config(vm->vm_id);
	switch (vm_config->load_order) {
	case PRE_LAUNCHED_VM:
	case SOS_VM:
		/*
		 * SOS: intercept port CF8 only.
		 * UOS or pre-launched VM: register handler for CF8 only and I/O requests to CF9/CFA/CFB are
		 * not handled by vpci.
		 */
		register_pio_emulation_handler(vm, PCI_CFGADDR_PIO_IDX, &pci_cfgaddr_range,
			pci_cfgaddr_io_read, pci_cfgaddr_io_write);

		/* Intercept and handle I/O ports CFC -- CFF */
		register_pio_emulation_handler(vm, PCI_CFGDATA_PIO_IDX, &pci_cfgdata_range,
			pci_cfgdata_io_read, pci_cfgdata_io_write);
		break;

	default:
		/* Nothing to do for other vm types */
		break;
	}

	spinlock_init(&vm->vpci.lock);
}

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 */
void vpci_cleanup(struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config;

	vm_config = get_vm_config(vm->vm_id);
	switch (vm_config->load_order) {
	case PRE_LAUNCHED_VM:
	case SOS_VM:
		/* deinit function for both SOS and pre-launched VMs (consider sos also as pre-launched) */
		deinit_prelaunched_vm_vpci(vm);
		break;

	case POST_LAUNCHED_VM:
		deinit_postlaunched_vm_vpci(vm);
		break;

	default:
		/* Unsupported VM type - Do nothing */
		break;
	}
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 * @pre vdev->vpci->vm->iommu != NULL
 */
static void assign_vdev_pt_iommu_domain(const struct pci_vdev *vdev)
{
	int32_t ret;
	struct acrn_vm *vm = vdev->vpci->vm;

	ret = move_pt_device(NULL, vm->iommu, (uint8_t)vdev->pdev->bdf.bits.b,
		(uint8_t)(vdev->pdev->bdf.value & 0xFFU));
	if (ret != 0) {
		panic("failed to assign iommu device!");
	}
}

/**
 * @pre vdev != NULL
 * @pre vdev->vpci != NULL
 * @pre vdev->vpci->vm != NULL
 * @pre vdev->vpci->vm->iommu != NULL
 */
static void remove_vdev_pt_iommu_domain(const struct pci_vdev *vdev)
{
	int32_t ret;
	struct acrn_vm *vm = vdev->vpci->vm;

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
 * @pre vpci != NULL
 * @pre vpci->vm != NULL
 */
static struct pci_vdev *find_vdev(struct acrn_vpci *vpci, union pci_bdf bdf)
{
	struct pci_vdev *vdev = pci_find_vdev(vpci, bdf);

	if ((vdev != NULL) && (vdev->vpci != vpci)) {
		vdev = vdev->new_owner;
	}

	return vdev;
}

static void vpci_init_pt_dev(struct pci_vdev *vdev)
{
	/*
	 * Here init_vdev_pt() needs to be called after init_vmsix() for the following reason:
	 * init_vdev_pt() will indirectly call has_msix_cap(), which
	 * requires init_vmsix() to be called first.
	 */
	init_vmsi(vdev);
	init_vmsix(vdev);
	init_vdev_pt(vdev);

	assign_vdev_pt_iommu_domain(vdev);
}

static void vpci_deinit_pt_dev(struct pci_vdev *vdev)
{
	remove_vdev_pt_iommu_domain(vdev);
	deinit_vmsix(vdev);
	deinit_vmsi(vdev);
}

static int32_t vpci_write_pt_dev_cfg(struct pci_vdev *vdev, uint32_t offset,
		uint32_t bytes, uint32_t val)
{
	if (vbar_access(vdev, offset)) {
		/* bar write access must be 4 bytes and offset must also be 4 bytes aligned */
		if ((bytes == 4U) && ((offset & 0x3U) == 0U)) {
			vdev_pt_write_vbar(vdev, pci_bar_index(offset), val);
		}
	} else if (msicap_access(vdev, offset)) {
		vmsi_write_cfg(vdev, offset, bytes, val);
	} else if (msixcap_access(vdev, offset)) {
		vmsix_write_cfg(vdev, offset, bytes, val);
	} else if ((vdev->has_flr && ((vdev->pcie_capoff + PCIR_PCIE_DEVCTRL) == offset) &&
				((val & PCIM_PCIE_FLR) != 0U)) || (vdev->has_af_flr &&
				((vdev->af_capoff + PCIR_AF_CTRL) == offset) && ((val & PCIM_AF_FLR) != 0U))) {
			/* Assume that guest write FLR must be 4 bytes aligned */
			pdev_do_flr(vdev->pdev->bdf, offset, bytes, val);
	} else {
		/* passthru to physical device */
		pci_pdev_write_cfg(vdev->pdev->bdf, offset, bytes, val);
	}

	return 0;
}

static int32_t vpci_read_pt_dev_cfg(const struct pci_vdev *vdev, uint32_t offset,
		uint32_t bytes, uint32_t *val)
{
	if (vbar_access(vdev, offset)) {
		/* bar access must be 4 bytes and offset must also be 4 bytes aligned */
		if ((bytes == 4U) && ((offset & 0x3U) == 0U)) {
			*val = pci_vdev_read_bar(vdev, pci_bar_index(offset));
		} else {
			*val = ~0U;
		}
	} else if (msicap_access(vdev, offset)) {
		vmsi_read_cfg(vdev, offset, bytes, val);
	} else if (msixcap_access(vdev, offset)) {
		vmsix_read_cfg(vdev, offset, bytes, val);
	} else {
		/* passthru to physical device */
		*val = pci_pdev_read_cfg(vdev->pdev->bdf, offset, bytes);
	}

	return 0;
}

static const struct pci_vdev_ops pci_pt_dev_ops = {
	.init_vdev	= vpci_init_pt_dev,
	.deinit_vdev	= vpci_deinit_pt_dev,
	.write_vdev_cfg	= vpci_write_pt_dev_cfg,
	.read_vdev_cfg	= vpci_read_pt_dev_cfg,
};

/**
 * @pre vpci != NULL
 */
static void read_cfg(struct acrn_vpci *vpci, union pci_bdf bdf,
	uint32_t offset, uint32_t bytes, uint32_t *val)
{
	struct pci_vdev *vdev;

	spinlock_obtain(&vpci->lock);
	vdev = find_vdev(vpci, bdf);
	if (vdev != NULL) {
		vdev->vdev_ops->read_vdev_cfg(vdev, offset, bytes, val);
	}
	spinlock_release(&vpci->lock);
}

/**
 * @pre vpci != NULL
 */
static void write_cfg(struct acrn_vpci *vpci, union pci_bdf bdf,
	uint32_t offset, uint32_t bytes, uint32_t val)
{
	struct pci_vdev *vdev;

	spinlock_obtain(&vpci->lock);
	vdev = find_vdev(vpci, bdf);
	if (vdev != NULL) {
		vdev->vdev_ops->write_vdev_cfg(vdev, offset, bytes, val);
	}
	spinlock_release(&vpci->lock);
}

/**
 * @pre vpci != NULL
 * @pre vpci.pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 */
static void vpci_init_vdev(struct acrn_vpci *vpci, struct acrn_vm_pci_dev_config *dev_config)
{
	struct pci_vdev *vdev = &vpci->pci_vdevs[vpci->pci_vdev_cnt];

	vpci->pci_vdev_cnt++;
	vdev->vpci = vpci;
	vdev->bdf.value = dev_config->vbdf.value;
	vdev->pdev = dev_config->pdev;
	vdev->pci_dev_config = dev_config;

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

/**
 * @pre vm != NULL
 */
static void vpci_init_vdevs(struct acrn_vm *vm)
{
	uint32_t idx;
	struct acrn_vpci *vpci = &(vm->vpci);
	const struct acrn_vm_config *vm_config = get_vm_config(vpci->vm->vm_id);

	for (idx = 0U; idx < vm_config->pci_dev_num; idx++) {
		vpci_init_vdev(vpci, &vm_config->pci_devs[idx]);
	}
}

/**
 * @pre vm != NULL
 * @pre vm->vpci.pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 * @pre is_sos_vm(vm) || is_prelaunched_vm(vm)
 */
static void deinit_prelaunched_vm_vpci(struct acrn_vm *vm)
{
	struct pci_vdev *vdev;
	uint32_t i;

	for (i = 0U; i < vm->vpci.pci_vdev_cnt; i++) {
		vdev = (struct pci_vdev *) &(vm->vpci.pci_vdevs[i]);

		vdev->vdev_ops->deinit_vdev(vdev);
	}
}

/**
 * @pre vm != NULL && Pointer vm shall point to SOS_VM
 * @pre vm->vpci.pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 * @pre is_postlaunched_vm(vm) == true
 */
static void deinit_postlaunched_vm_vpci(struct acrn_vm *vm)
{
	struct acrn_vm *sos_vm;
	uint32_t i;
	struct pci_vdev *vdev, *target_vdev;
	int32_t ret;
	/* PCI resources
	 * 1) IOMMU domain switch
	 * 2) Relese UOS MSI host IRQ/IRTE
	 * 3) Update vdev info in SOS vdev
	 * Cleanup mentioned above is  taken care when DM releases UOS resources
	 * during a UOS reboot or shutdown
	 * In the following cases, where DM does not get chance to cleanup
	 * 1) DM crash/segfault
	 * 2) SOS triple fault/hang
	 * 3) SOS reboot before shutting down POST_LAUNCHED_VMs
	 * ACRN must cleanup
	 */
	sos_vm = get_sos_vm();
	spinlock_obtain(&sos_vm->vpci.lock);
	for (i = 0U; i < sos_vm->vpci.pci_vdev_cnt; i++) {
		vdev = (struct pci_vdev *)&(sos_vm->vpci.pci_vdevs[i]);

		if (vdev->vpci->vm == vm) {
			spinlock_obtain(&vm->vpci.lock);
			target_vdev = vdev->new_owner;
			ret = move_pt_device(vm->iommu, sos_vm->iommu, (uint8_t)target_vdev->pdev->bdf.bits.b,
					(uint8_t)(target_vdev->pdev->bdf.value & 0xFFU));
			if (ret != 0) {
				panic("failed to assign iommu device!");
			}

			deinit_vmsi(target_vdev);

			deinit_vmsix(target_vdev);

			/* Move vdev pointers back to SOS*/
			vdev->vpci = (struct acrn_vpci *) &sos_vm->vpci;
			vdev->new_owner = NULL;
			spinlock_release(&vm->vpci.lock);
		}
	}
	spinlock_release(&sos_vm->vpci.lock);
}

/**
 * @pre target_vm != NULL && Pointer target_vm shall point to SOS_VM
 */
void vpci_set_ptdev_intr_info(struct acrn_vm *target_vm, uint16_t vbdf, uint16_t pbdf)
{
	struct pci_vdev *vdev, *target_vdev;
	struct acrn_vpci *target_vpci;
	union pci_bdf bdf;
	struct acrn_vm *sos_vm;

	bdf.value = pbdf;
	sos_vm = get_sos_vm();
	spinlock_obtain(&sos_vm->vpci.lock);
	vdev = pci_find_vdev(&sos_vm->vpci, bdf);
	if (vdev == NULL) {
		pr_err("%s, can't find PCI device for vm%d, vbdf (0x%x) pbdf (0x%x)", __func__,
			target_vm->vm_id, vbdf, pbdf);
	} else {
		if (vdev->vpci->vm == sos_vm) {
			spinlock_obtain(&target_vm->vpci.lock);
			target_vpci = &(target_vm->vpci);
			vdev->vpci = target_vpci;

			target_vdev = &target_vpci->pci_vdevs[target_vpci->pci_vdev_cnt];
			target_vpci->pci_vdev_cnt++;
			(void)memcpy_s((void *)target_vdev, sizeof(struct pci_vdev),
					(void *)vdev, sizeof(struct pci_vdev));
			target_vdev->bdf.value = vbdf;

			vdev->new_owner = target_vdev;
			spinlock_release(&target_vm->vpci.lock);
		}
	}
	spinlock_release(&sos_vm->vpci.lock);
}

/**
 * @pre target_vm != NULL && Pointer target_vm shall point to SOS_VM
 */
void vpci_reset_ptdev_intr_info(struct acrn_vm *target_vm, uint16_t vbdf, uint16_t pbdf)
{
	struct pci_vdev *vdev;
	union pci_bdf bdf;
	struct acrn_vm *sos_vm;

	bdf.value = pbdf;
	sos_vm = get_sos_vm();
	spinlock_obtain(&sos_vm->vpci.lock);
	vdev = pci_find_vdev(&sos_vm->vpci, bdf);
	if (vdev == NULL) {
		pr_err("%s, can't find PCI device for vm%d, vbdf (0x%x) pbdf (0x%x)", __func__,
			target_vm->vm_id, vbdf, pbdf);
	} else {
		/* Return this PCI device to SOS */
		if (vdev->vpci->vm == target_vm) {
			spinlock_obtain(&target_vm->vpci.lock);
			vdev->vpci = &sos_vm->vpci;
			vdev->new_owner = NULL;
			spinlock_release(&target_vm->vpci.lock);
		}
	}
	spinlock_release(&sos_vm->vpci.lock);
}
