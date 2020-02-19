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

#include <errno.h>
#include <vm.h>
#include <vtd.h>
#include <io.h>
#include <mmu.h>
#include <logmsg.h>
#include "vpci_priv.h"
#include "pci_dev.h"

static void vpci_init_vdevs(struct acrn_vm *vm);
static void deinit_prelaunched_vm_vpci(struct acrn_vm *vm);
static void deinit_postlaunched_vm_vpci(struct acrn_vm *vm);
static int32_t read_cfg(struct acrn_vpci *vpci, union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t *val);
static int32_t write_cfg(struct acrn_vpci *vpci, union pci_bdf bdf, uint32_t offset, uint32_t bytes, uint32_t val);
static struct pci_vdev *find_vdev(struct acrn_vpci *vpci, union pci_bdf bdf);

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
 *
 * @retval true on success.
 * @retval false. (ACRN will deliver this IO request to DM to handle for post-launched VM)
 */
static bool pci_cfgaddr_io_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes, uint32_t val)
{
	bool ret = true;
	struct acrn_vpci *vpci = &vcpu->vm->vpci;
	union pci_cfg_addr_reg *cfg_addr = &vpci->addr;
	union pci_bdf vbdf;

	if ((addr == (uint16_t)PCI_CONFIG_ADDR) && (bytes == 4U)) {
		/* unmask reserved fields: BITs 24-30 and BITs 0-1 */
		cfg_addr->value = val & (~0x7f000003U);

		if (is_postlaunched_vm(vcpu->vm)) {
			vbdf.value = cfg_addr->bits.bdf;
			/* For post-launched VM, ACRN will only handle PT device, all virtual PCI device
			 * still need to deliver to ACRN DM to handle.
			 */
			if (find_vdev(vpci, vbdf) == NULL) {
				ret = false;
			}
		}
	}

	return ret;
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
 *
 * @retval true on success.
 * @retval false. (ACRN will deliver this IO request to DM to handle for post-launched VM)
 */
static bool pci_cfgdata_io_read(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes)
{
	int32_t ret = 0;
	struct acrn_vm *vm = vcpu->vm;
	struct acrn_vpci *vpci = &vm->vpci;
	union pci_cfg_addr_reg cfg_addr;
	union pci_bdf bdf;
	uint16_t offset = addr - PCI_CONFIG_DATA;
	uint32_t val = ~0U;
	struct pio_request *pio_req = &vcpu->req.reqs.pio;

	cfg_addr.value = atomic_readandclear32(&vpci->addr.value);
	if (cfg_addr.bits.enable != 0U) {
		if (vpci_is_valid_access(cfg_addr.bits.reg_num + offset, bytes)) {
			bdf.value = cfg_addr.bits.bdf;
			ret = read_cfg(vpci, bdf, cfg_addr.bits.reg_num + offset, bytes, &val);
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
 *	|| (get_vm_config(vcpu->vm->vm_id)->load_order == SOS_VM)
 *
 * @retval true on success.
 * @retval false. (ACRN will deliver this IO request to DM to handle for post-launched VM)
 */
static bool pci_cfgdata_io_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes, uint32_t val)
{
	int32_t ret = 0;
	struct acrn_vm *vm = vcpu->vm;
	struct acrn_vpci *vpci = &vm->vpci;
	union pci_cfg_addr_reg cfg_addr;
	union pci_bdf bdf;
	uint16_t offset = addr - PCI_CONFIG_DATA;

	cfg_addr.value = atomic_readandclear32(&vpci->addr.value);
	if (cfg_addr.bits.enable != 0U) {
		if (vpci_is_valid_access(cfg_addr.bits.reg_num + offset, bytes)) {
			bdf.value = cfg_addr.bits.bdf;
			ret = write_cfg(vpci, bdf, cfg_addr.bits.reg_num + offset, bytes, val);
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
static int32_t vpci_handle_mmconfig_access(struct io_request *io_req, void *private_data)
{
	int32_t ret = 0;
	struct mmio_request *mmio = &io_req->reqs.mmio;
	struct acrn_vpci *vpci = (struct acrn_vpci *)private_data;
	uint64_t pci_mmcofg_base = vpci->pci_mmcfg_base;
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

	if (mmio->direction == REQUEST_READ) {
		if (!is_plat_hidden_pdev(bdf)) {
			ret = read_cfg(vpci, bdf, reg_num, (uint32_t)mmio->size, (uint32_t *)&mmio->value);
		} else {
			/* expose and pass through platform hidden devices to SOS */
			mmio->value = (uint64_t)pci_pdev_read_cfg(bdf, reg_num, (uint32_t)mmio->size);
		}
	} else {
		if (!is_plat_hidden_pdev(bdf)) {
			ret = write_cfg(vpci, bdf, reg_num, (uint32_t)mmio->size, (uint32_t)mmio->value);
		} else {
			/* expose and pass through platform hidden devices to SOS */
			pci_pdev_write_cfg(bdf, reg_num, (uint32_t)mmio->size, (uint32_t)mmio->value);
		}
	}

	return ret;
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
	uint64_t pci_mmcfg_base;

	vm->vpci.vm = vm;
	vm->iommu = create_iommu_domain(vm->vm_id, hva2hpa(vm->arch_vm.nworld_eptp), 48U);
	/* Build up vdev list for vm */
	vpci_init_vdevs(vm);

	vm_config = get_vm_config(vm->vm_id);
	if (vm_config->load_order != PRE_LAUNCHED_VM) {
		/* PCI MMCONFIG for post-launched VM is fixed to 0xE0000000 */
		pci_mmcfg_base = (vm_config->load_order == SOS_VM) ? get_mmcfg_base() : 0xE0000000UL;
		vm->vpci.pci_mmcfg_base = pci_mmcfg_base;
		register_mmio_emulation_handler(vm, vpci_handle_mmconfig_access,
			pci_mmcfg_base, pci_mmcfg_base + PCI_MMCONFIG_SIZE, &vm->vpci, false);
	}

	/* Intercept and handle I/O ports CF8h */
	register_pio_emulation_handler(vm, PCI_CFGADDR_PIO_IDX, &pci_cfgaddr_range,
		pci_cfgaddr_io_read, pci_cfgaddr_io_write);

	/* Intercept and handle I/O ports CFCh -- CFFh */
	register_pio_emulation_handler(vm, PCI_CFGDATA_PIO_IDX, &pci_cfgdata_range,
		pci_cfgdata_io_read, pci_cfgdata_io_write);

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
static void assign_vdev_pt_iommu_domain(struct pci_vdev *vdev)
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
	const struct acrn_vm *vm = vdev->vpci->vm;

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
		/* If the device is assigned to other guest, we could not access it */
		vdev = NULL;
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
	} else if (offset == PCIR_COMMAND) {
		vdev_pt_write_command(vdev, (bytes > 2U) ? 2U : bytes, (uint16_t)val);
	} else {
		if (is_postlaunched_vm(vdev->vpci->vm) &&
				in_range(offset, PCIR_INTERRUPT_LINE, 4U)) {
			pci_vdev_write_cfg(vdev, offset, bytes, val);
		} else {
			/* passthru to physical device */
			pci_pdev_write_cfg(vdev->pdev->bdf, offset, bytes, val);
		}
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
		if (is_postlaunched_vm(vdev->vpci->vm) &&
				in_range(offset, PCIR_INTERRUPT_LINE, 4U)) {
			*val = pci_vdev_read_cfg(vdev, offset, bytes);
		} else {
			/* passthru to physical device */
			*val = pci_pdev_read_cfg(vdev->pdev->bdf, offset, bytes);
		}
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
static int32_t read_cfg(struct acrn_vpci *vpci, union pci_bdf bdf,
	uint32_t offset, uint32_t bytes, uint32_t *val)
{
	int32_t ret = 0;
	struct pci_vdev *vdev;

	spinlock_obtain(&vpci->lock);
	vdev = find_vdev(vpci, bdf);
	if (vdev != NULL) {
		vdev->vdev_ops->read_vdev_cfg(vdev, offset, bytes, val);
	} else {
		if (is_postlaunched_vm(vpci->vm)) {
			ret = -ENODEV;
		}
	}
	spinlock_release(&vpci->lock);
	return ret;
}

/**
 * @pre vpci != NULL
 */
static int32_t write_cfg(struct acrn_vpci *vpci, union pci_bdf bdf,
	uint32_t offset, uint32_t bytes, uint32_t val)
{
	int32_t ret = 0;
	struct pci_vdev *vdev;

	spinlock_obtain(&vpci->lock);
	vdev = find_vdev(vpci, bdf);
	if (vdev != NULL) {
		vdev->vdev_ops->write_vdev_cfg(vdev, offset, bytes, val);
	} else {
		if (!is_postlaunched_vm(vpci->vm)) {
			pr_acrnlog("%s %x:%x.%x not found! off: 0x%x, val: 0x%x\n", __func__,
				bdf.bits.b, bdf.bits.d, bdf.bits.f, offset, val);
		} else {
			ret = -ENODEV;
		}
	}
	spinlock_release(&vpci->lock);
	return ret;
}

/**
 * @pre vpci != NULL
 * @pre vpci.pci_vdev_cnt <= CONFIG_MAX_PCI_DEV_NUM
 */
static struct pci_vdev *vpci_init_vdev(struct acrn_vpci *vpci, struct acrn_vm_pci_dev_config *dev_config)
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

	return vdev;
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
		(void)vpci_init_vdev(vpci, &vm_config->pci_devs[idx]);
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
 * @brief assign a PCI device from SOS to target post-launched VM.
 *
 * @pre tgt_vm != NULL
 * @pre pcidev != NULL
 */
int32_t vpci_assign_pcidev(struct acrn_vm *tgt_vm, struct acrn_assign_pcidev *pcidev)
{
	int32_t ret = 0;
	uint32_t idx;
	struct pci_vdev *vdev_in_sos, *vdev;
	struct acrn_vpci *vpci;
	union pci_bdf bdf;
	struct acrn_vm *sos_vm;

	bdf.value = pcidev->phys_bdf;
	sos_vm = get_sos_vm();
	spinlock_obtain(&sos_vm->vpci.lock);
	vdev_in_sos = pci_find_vdev(&sos_vm->vpci, bdf);
	if ((vdev_in_sos != NULL) && (vdev_in_sos->vpci->vm == sos_vm) && (vdev_in_sos->pdev != NULL)) {
		/* ToDo: Each PT device must support one type reset */
		if (!vdev_in_sos->pdev->has_pm_reset && !vdev_in_sos->pdev->has_flr &&
				!vdev_in_sos->pdev->has_af_flr) {
			pr_fatal("%s %x:%x.%x not support FLR or not support PM reset\n",
				__func__, bdf.bits.b,  bdf.bits.d,  bdf.bits.f);
		} else {
			/* DM will reset this device before assigning it */
			pdev_restore_bar(vdev_in_sos->pdev);
		}

		remove_vdev_pt_iommu_domain(vdev_in_sos);
		if (ret == 0) {
			vpci = &(tgt_vm->vpci);
			vdev_in_sos->vpci = vpci;

			spinlock_obtain(&tgt_vm->vpci.lock);
			vdev = vpci_init_vdev(vpci, vdev_in_sos->pci_dev_config);
			pci_vdev_write_cfg_u8(vdev, PCIR_INTERRUPT_LINE, pcidev->intr_line);
			pci_vdev_write_cfg_u8(vdev, PCIR_INTERRUPT_PIN, pcidev->intr_pin);
			for (idx = 0U; idx < vdev->nr_bars; idx++) {
				pci_vdev_write_bar(vdev, idx, pcidev->bar[idx]);
			}

			vdev->bdf.value = pcidev->virt_bdf;
			spinlock_release(&tgt_vm->vpci.lock);
			vdev_in_sos->new_owner = vdev;
		}
	} else {
		pr_fatal("%s, can't find PCI device %x:%x.%x for vm[%d] %x:%x.%x\n", __func__,
			pcidev->phys_bdf >> 8U, (pcidev->phys_bdf >> 3U) & 0x1fU, pcidev->phys_bdf & 0x7U,
			tgt_vm->vm_id,
			pcidev->virt_bdf >> 8U, (pcidev->virt_bdf >> 3U) & 0x1fU, pcidev->virt_bdf & 0x7U);
		ret = -ENODEV;
	}
	spinlock_release(&sos_vm->vpci.lock);

	return ret;
}

/**
 * @brief deassign a PCI device from target post-launched VM to SOS.
 *
 * @pre tgt_vm != NULL
 * @pre pcidev != NULL
 */
int32_t vpci_deassign_pcidev(struct acrn_vm *tgt_vm, struct acrn_assign_pcidev *pcidev)
{
	int32_t ret = 0;
	struct pci_vdev *vdev_in_sos, *vdev;
	union pci_bdf bdf;
	struct acrn_vm *sos_vm;

	bdf.value = pcidev->phys_bdf;
	sos_vm = get_sos_vm();
	spinlock_obtain(&sos_vm->vpci.lock);
	vdev_in_sos = pci_find_vdev(&sos_vm->vpci, bdf);
	if ((vdev_in_sos != NULL) && (vdev_in_sos->vpci->vm == tgt_vm) && (vdev_in_sos->pdev != NULL)) {
		vdev = vdev_in_sos->new_owner;

		spinlock_obtain(&tgt_vm->vpci.lock);

		if (vdev != NULL) {
			ret = move_pt_device(tgt_vm->iommu, sos_vm->iommu, (uint8_t)(pcidev->phys_bdf >> 8U),
					(uint8_t)(pcidev->phys_bdf & 0xffU));
			if (ret != 0) {
				panic("failed to assign iommu device!");
			}

			deinit_vmsi(vdev);

			deinit_vmsix(vdev);

		}
		spinlock_release(&tgt_vm->vpci.lock);

		vdev_in_sos->vpci = &sos_vm->vpci;
		vdev_in_sos->new_owner = NULL;
	} else {
		pr_fatal("%s, can't find PCI device %x:%x.%x for vm[%d] %x:%x.%x\n", __func__,
			pcidev->phys_bdf >> 8U, (pcidev->phys_bdf >> 3U) & 0x1fU, pcidev->phys_bdf & 0x7U,
			tgt_vm->vm_id,
			pcidev->virt_bdf >> 8U, (pcidev->virt_bdf >> 3U) & 0x1fU, pcidev->virt_bdf & 0x7U);
		ret = -ENODEV;
	}
	spinlock_release(&sos_vm->vpci.lock);

	return ret;
}
