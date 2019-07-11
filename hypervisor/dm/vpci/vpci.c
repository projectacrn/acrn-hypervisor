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
#include <errno.h>
#include <logmsg.h>
#include "vpci_priv.h"

static void init_vdevs(const struct acrn_vm *vm);
static void deinit_prelaunched_vm_vpci(const struct acrn_vm *vm);
static void deinit_postlaunched_vm_vpci(const struct acrn_vm *vm);
static void read_cfg(const struct acrn_vpci *vpci, union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t *val);
static void write_cfg(const struct acrn_vpci *vpci, union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val);

/**
 * @pre pi != NULL
 */
static void pci_cfg_clear_cache(struct pci_addr_info *pi)
{
	pi->cached_bdf.value = 0xFFFFU;
	pi->cached_reg = 0U;
	pi->cached_enable = false;
}

/**
 * @pre vm != NULL
 * @pre vcpu != NULL
 */
static bool pci_cfgaddr_io_read(struct acrn_vm *vm, struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes)
{
	uint32_t val = ~0U;
	struct acrn_vpci *vpci = &vm->vpci;
	struct pci_addr_info *pi = &vpci->addr_info;
	struct pio_request *pio_req = &vcpu->req.reqs.pio;

	if ((addr == (uint16_t)PCI_CONFIG_ADDR) && (bytes == 4U)) {
		val = (uint32_t)pi->cached_bdf.value;
		val <<= 8U;
		val |= pi->cached_reg;
		if (pi->cached_enable) {
			val |= PCI_CFG_ENABLE;
		}
	}

	pio_req->value = val;

	return true;
}

/**
 * @pre vm != NULL
 */
static bool pci_cfgaddr_io_write(struct acrn_vm *vm, uint16_t addr, size_t bytes, uint32_t val)
{
	struct acrn_vpci *vpci = &vm->vpci;
	struct pci_addr_info *pi = &vpci->addr_info;

	if ((addr == (uint16_t)PCI_CONFIG_ADDR) && (bytes == 4U)) {
		pi->cached_bdf.value = (uint16_t)(val >> 8U);
		pi->cached_reg = val & PCI_REGMASK;
		pi->cached_enable = ((val & PCI_CFG_ENABLE) == PCI_CFG_ENABLE);
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
 * @pre vm != NULL
 * @pre vcpu != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre (get_vm_config(vm->vm_id)->load_order == PRE_LAUNCHED_VM) || (get_vm_config(vm->vm_id)->load_order == SOS_VM)
 */
static bool pci_cfgdata_io_read(struct acrn_vm *vm, struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes)
{
	struct acrn_vpci *vpci = &vm->vpci;
	struct pci_addr_info *pi = &vpci->addr_info;
	uint16_t offset = addr - PCI_CONFIG_DATA;
	uint32_t val = ~0U;
	struct acrn_vm_config *vm_config;
	struct pio_request *pio_req = &vcpu->req.reqs.pio;

	if (pi->cached_enable) {
		if (vpci_is_valid_access(pi->cached_reg + offset, bytes)) {
			vm_config = get_vm_config(vm->vm_id);

			switch (vm_config->load_order) {
			case PRE_LAUNCHED_VM:
			case SOS_VM:
				read_cfg(vpci, pi->cached_bdf, pi->cached_reg + offset, bytes, &val);
				break;

			default:
				ASSERT(false, "Error, pci_cfgdata_io_read should only be called for PRE_LAUNCHED_VM and SOS_VM");
				break;
			}
		}
		pci_cfg_clear_cache(pi);
	}

	pio_req->value = val;

	return true;
}

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre (get_vm_config(vm->vm_id)->load_order == PRE_LAUNCHED_VM) || (get_vm_config(vm->vm_id)->load_order == SOS_VM)
 */
static bool pci_cfgdata_io_write(struct acrn_vm *vm, uint16_t addr, size_t bytes, uint32_t val)
{
	struct acrn_vpci *vpci = &vm->vpci;
	struct pci_addr_info *pi = &vpci->addr_info;
	uint16_t offset = addr - PCI_CONFIG_DATA;
	struct acrn_vm_config *vm_config;


	if (pi->cached_enable) {
		if (vpci_is_valid_access(pi->cached_reg + offset, bytes)) {
			vm_config = get_vm_config(vm->vm_id);

			switch (vm_config->load_order) {
			case PRE_LAUNCHED_VM:
			case SOS_VM:
				write_cfg(vpci, pi->cached_bdf, pi->cached_reg + offset, bytes, val);
				break;

			default:
				ASSERT(false, "Error, pci_cfgdata_io_write should only be called for PRE_LAUNCHED_VM and SOS_VM");
				break;
			}
		}
		pci_cfg_clear_cache(pi);
	}

	return true;
}

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 */
void vpci_init(struct acrn_vm *vm)
{
	int32_t ret = -EINVAL;

	struct vm_io_range pci_cfgaddr_range = {
		.flags = IO_ATTR_RW,
		.base = PCI_CONFIG_ADDR,
		.len = 1U
	};

	struct vm_io_range pci_cfgdata_range = {
		.flags = IO_ATTR_RW,
		.base = PCI_CONFIG_DATA,
		.len = 4U
	};

	struct acrn_vm_config *vm_config;

	vm->vpci.vm = vm;

	vm_config = get_vm_config(vm->vm_id);
	switch (vm_config->load_order) {
	case PRE_LAUNCHED_VM:
	case SOS_VM:
		vm->iommu = create_iommu_domain(vm->vm_id, hva2hpa(vm->arch_vm.nworld_eptp), 48U);
		/* Build up vdev list for vm */
		init_vdevs(vm);
		ret = 0;
		break;

	default:
		/* Nothing to do for other vm types */
		break;
	}

	if (ret == 0) {
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
	}
}

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 */
void vpci_cleanup(const struct acrn_vm *vm)
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

static struct pci_vdev *find_vdev_for_sos(union pci_bdf bdf)
{
	struct acrn_vm *vm;

	vm = get_sos_vm();

	return pci_find_vdev_by_pbdf(&vm->vpci, bdf);
}

/**
 * @pre vpci != NULL
 * @pre vpci->vm != NULL
 */
static struct pci_vdev *find_vdev(const struct acrn_vpci *vpci, union pci_bdf bdf)
{
	struct pci_vdev *vdev;

	if (is_prelaunched_vm(vpci->vm)) {
		vdev = pci_find_vdev_by_vbdf(vpci, bdf);
	} else if (is_sos_vm(vpci->vm)) {
		vdev = find_vdev_for_sos(bdf);
	} else {
		vdev = NULL;
	}

	return vdev;
}

static void vpci_init_pt_dev(struct pci_vdev *vdev)
{
	init_vmsi(vdev);
	init_vmsix(vdev);

	/*
	 * Here init_vdev_pt() needs to be called after init_vmsix() for the following reason:
	 * init_vdev_pt() will indirectly call has_msix_cap(), which
	 * requires init_vmsix() to be called first.
	 */
	init_vdev_pt(vdev);
	assign_vdev_pt_iommu_domain(vdev);
}

static void vpci_deinit_pt_dev(struct pci_vdev *vdev)
{
	deinit_vmsi(vdev);
	deinit_vmsix(vdev);
	remove_vdev_pt_iommu_domain(vdev);
}

static int32_t vpci_write_pt_dev_cfg(struct pci_vdev *vdev, uint32_t offset,
		uint32_t bytes, uint32_t val)
{
	if (vbar_access(vdev, offset, bytes)) {
		(void)vdev_pt_write_cfg(vdev, offset, bytes, val);
	} else if (msicap_access(vdev, offset)) {
		(void)vmsi_write_cfg(vdev, offset, bytes, val);
	} else if (msixcap_access(vdev, offset)) {
		(void)vmsix_write_cfg(vdev, offset, bytes, val);
	} else {
		/* passthru to physical device */
		pci_pdev_write_cfg(vdev->pdev->bdf, offset, bytes, val);
	}

	return 0;
}

static int32_t vpci_read_pt_dev_cfg(struct pci_vdev *vdev, uint32_t offset,
		uint32_t bytes, uint32_t *val)
{
	if (vbar_access(vdev, offset, bytes)) {
		(void)vdev_pt_read_cfg(vdev, offset, bytes, val);
	} else if (msicap_access(vdev, offset)) {
		(void)vmsi_read_cfg(vdev, offset, bytes, val);
	} else if (msixcap_access(vdev, offset)) {
		(void)vmsix_read_cfg(vdev, offset, bytes, val);
	} else {
		/* passthru to physical device */
		*val = pci_pdev_read_cfg(vdev->pdev->bdf, offset, bytes);
	}

	return 0;
}

static struct pci_vdev_ops pci_pt_dev_ops = {
	.init_vdev	= vpci_init_pt_dev,
	.deinit_vdev	= vpci_deinit_pt_dev,
	.write_vdev_cfg	= vpci_write_pt_dev_cfg,
	.read_vdev_cfg	= vpci_read_pt_dev_cfg,
};

/**
 * @pre vpci != NULL
 */
static void read_cfg(const struct acrn_vpci *vpci, union pci_bdf bdf,
	uint32_t offset, uint32_t bytes, uint32_t *val)
{
	struct pci_vdev *vdev = find_vdev(vpci, bdf);

	if (vdev != NULL) {
		vdev->vdev_ops->read_vdev_cfg(vdev, offset, bytes, val);
	}
}

/**
 * @pre vpci != NULL
 */
static void write_cfg(const struct acrn_vpci *vpci, union pci_bdf bdf,
	uint32_t offset, uint32_t bytes, uint32_t val)
{
	struct pci_vdev *vdev = find_vdev(vpci, bdf);

	if (vdev != NULL) {
		vdev->vdev_ops->write_vdev_cfg(vdev, offset, bytes, val);
	}
}

/**
 * @pre vm_config != NULL
 * @pre vm_config->pci_ptdev_num <= CONFIG_MAX_PCI_DEV_NUM
 */
static struct acrn_vm_pci_ptdev_config *find_ptdev_config_by_pbdf(const struct acrn_vm_config *vm_config,
	union pci_bdf pbdf)
{
	struct acrn_vm_pci_ptdev_config *ptdev_config, *tmp;
	uint16_t i;

	ptdev_config = NULL;
	for (i = 0U; i < vm_config->pci_ptdev_num; i++) {
		tmp = &vm_config->pci_ptdevs[i];

		if (bdf_is_equal(&tmp->pbdf, &pbdf)) {
			ptdev_config = tmp;
			break;
		}
	}

	return ptdev_config;
}

/**
 * @pre pdev != NULL
 * @pre vm != NULL
 * @pre vm->vpci.pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 */
static void init_vdev_for_pdev(struct pci_pdev *pdev, const struct acrn_vm *vm)
{
	const struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	struct acrn_vpci *vpci = &(((struct acrn_vm *)vm)->vpci);
	struct acrn_vm_pci_ptdev_config *ptdev_config;

	ptdev_config = find_ptdev_config_by_pbdf(vm_config, pdev->bdf);

	if (((is_prelaunched_vm(vm) && (ptdev_config != NULL)) || is_sos_vm(vm))
		&& (vpci->pci_vdev_cnt < CONFIG_MAX_PCI_DEV_NUM)) {
		struct pci_vdev *vdev;

		vdev = &vpci->pci_vdevs[vpci->pci_vdev_cnt];
		vpci->pci_vdev_cnt++;

		vdev->vpci = vpci;
		vdev->pdev = pdev;
		vdev->ptdev_config = ptdev_config;

		if (ptdev_config != NULL) {
			/* vbdf is defined in vm_config */
			vdev->bdf.value = ptdev_config->vbdf.value;
		} else {
			/* vbdf is not defined in vm_config, set it to equal to pbdf */
			vdev->bdf.value = pdev->bdf.value;
		}

		if (is_hostbridge(vdev)) {
			vdev->vdev_ops = get_vhostbridge_ops();
		} else {
			vdev->vdev_ops = &pci_pt_dev_ops;
		}

		vdev->vdev_ops->init_vdev(vdev);
	}
}

/**
 * @pre vm != NULL
 */
static void init_vdevs(const struct acrn_vm *vm)
{
	uint32_t idx;

	for (idx = 0U; idx < num_pci_pdev; idx++) {
		init_vdev_for_pdev(&pci_pdev_array[idx], vm);
	}
}

/**
 * @pre vm != NULL
 * @pre vm->vpci.pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 * @pre is_sos_vm(vm) || is_prelaunched_vm(vm)
 */
static void deinit_prelaunched_vm_vpci(const struct acrn_vm *vm)
{
	struct pci_vdev *vdev;
	uint32_t i;

	for (i = 0U; i < vm->vpci.pci_vdev_cnt; i++) {
		vdev = (struct pci_vdev *) &(vm->vpci.pci_vdevs[i]);

		vdev->vdev_ops->deinit_vdev(vdev);
	}
}

/**
 * @pre vm != NULL
 * @pre vm->vpci.pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 * @pre is_postlaunched_vm(vm) == true
 */
static void deinit_postlaunched_vm_vpci(const struct acrn_vm *vm)
{
	struct acrn_vm *sos_vm;
	uint32_t i;
	struct pci_vdev *vdev;
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
	for (i = 0U; i < sos_vm->vpci.pci_vdev_cnt; i++) {
		vdev = (struct pci_vdev *)&(sos_vm->vpci.pci_vdevs[i]);

		if (vdev->vpci->vm == vm) {
			ret = move_pt_device(vm->iommu, sos_vm->iommu, (uint8_t)vdev->pdev->bdf.bits.b,
					(uint8_t)(vdev->pdev->bdf.value & 0xFFU));
			if (ret != 0) {
				panic("failed to assign iommu device!");
			}

			deinit_vmsi(vdev);

			deinit_vmsix(vdev);

			/* Move vdev pointers back to SOS*/
			vdev->vpci = (struct acrn_vpci *) &sos_vm->vpci;

			/* vbdf equals to pbdf in sos */
			vdev->bdf.value = vdev->pdev->bdf.value;
		}
	}
}

/**
 * @pre target_vm != NULL
 */
void vpci_set_ptdev_intr_info(const struct acrn_vm *target_vm, uint16_t vbdf, uint16_t pbdf)
{
	struct pci_vdev *vdev;
	union pci_bdf bdf;

	bdf.value = pbdf;
	vdev = find_vdev_for_sos(bdf);
	if (vdev == NULL) {
		pr_err("%s, can't find PCI device for vm%d, vbdf (0x%x) pbdf (0x%x)", __func__,
			target_vm->vm_id, vbdf, pbdf);
	} else {
		/* UOS may do BDF mapping */
		vdev->vpci = (struct acrn_vpci *)&(target_vm->vpci);
		vdev->bdf.value = vbdf;
		vdev->pdev->bdf.value = pbdf;
	}
}

/**
 * @pre target_vm != NULL
 */
void vpci_reset_ptdev_intr_info(const struct acrn_vm *target_vm, uint16_t vbdf, uint16_t pbdf)
{
	struct pci_vdev *vdev;
	struct acrn_vm *vm;
	union pci_bdf bdf;

	bdf.value = pbdf;
	vdev = find_vdev_for_sos(bdf);
	if (vdev == NULL) {
		pr_err("%s, can't find PCI device for vm%d, vbdf (0x%x) pbdf (0x%x)", __func__,
			target_vm->vm_id, vbdf, pbdf);
	} else {
		/* Return this PCI device to SOS */
		if (vdev->vpci->vm == target_vm) {
			vm = get_sos_vm();

			vdev->vpci = &vm->vpci;

			/* vbdf equals to pbdf in sos */
			vdev->bdf.value = vdev->pdev->bdf.value;
		}
	}
}
