/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <schedule.h>
#include <hypercall.h>
#include <version.h>
#include <reloc.h>

#define ACRN_DBG_HYCALL	6U

bool is_hypercall_from_ring0(void)
{
	uint16_t cs_sel;

	cs_sel = exec_vmread16(VMX_GUEST_CS_SEL);
	/* cs_selector[1:0] is CPL */
	if ((cs_sel & 0x3U) == 0U) {
		return true;
	}

	return false;
}

int32_t hcall_sos_offline_cpu(struct vm *vm, uint64_t lapicid)
{
	struct vcpu *vcpu;
	int i;

	if (!is_vm0(vm))
		return -1;

	pr_info("sos offline cpu with lapicid %lld", lapicid);

	foreach_vcpu(i, vm, vcpu) {
		if (vlapic_get_apicid(vcpu->arch_vcpu.vlapic) == lapicid) {
			/* should not offline BSP */
			if (vcpu->vcpu_id == 0)
				return -1;
			pause_vcpu(vcpu, VCPU_ZOMBIE);
			reset_vcpu(vcpu);
			destroy_vcpu(vcpu);
		}
	}

	return 0;
}

int32_t hcall_get_api_version(struct vm *vm, uint64_t param)
{
	struct hc_api_version version;

	if (!is_vm0(vm)) {
		return -1;
	}

	version.major_version = HV_API_MAJOR_VERSION;
	version.minor_version = HV_API_MINOR_VERSION;

	if (copy_to_gpa(vm, &version, param, sizeof(version)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	return 0;
}

static int32_t
handle_vpic_irqline(struct vm *vm, uint32_t irq, enum irq_mode mode)
{
	int32_t ret = -1;

	if (vm == NULL) {
		return ret;
	}

	switch (mode) {
	case IRQ_ASSERT:
		ret = vpic_assert_irq(vm, irq);
		break;
	case IRQ_DEASSERT:
		ret = vpic_deassert_irq(vm, irq);
		break;
	case IRQ_PULSE:
		ret = vpic_pulse_irq(vm, irq);
	default:
		break;
	}

	return ret;
}

static int32_t
handle_vioapic_irqline(struct vm *vm, uint32_t irq, enum irq_mode mode)
{
	int32_t ret = -1;

	if (vm == NULL) {
		return ret;
	}

	switch (mode) {
	case IRQ_ASSERT:
		ret = vioapic_assert_irq(vm, irq);
		break;
	case IRQ_DEASSERT:
		ret = vioapic_deassert_irq(vm, irq);
		break;
	case IRQ_PULSE:
		ret = vioapic_pulse_irq(vm, irq);
		break;
	default:
		break;
	}
	return ret;
}

static int32_t
handle_virt_irqline(struct vm *vm, uint16_t target_vmid,
		struct acrn_irqline *param, enum irq_mode mode)
{
	int32_t ret = 0;
	uint32_t intr_type;
	struct vm *target_vm = get_vm_from_vmid(target_vmid);

	if ((vm == NULL) || (param == NULL)) {
		return -1;
	}

	intr_type = param->intr_type;

	switch (intr_type) {
	case ACRN_INTR_TYPE_ISA:
		/* Call vpic for pic injection */
		ret = handle_vpic_irqline(target_vm, param->pic_irq, mode);

		/* call vioapic for ioapic injection if ioapic_irq != ~0U*/
		if (param->ioapic_irq != (~0U)) {
			/* handle IOAPIC irqline */
			ret = handle_vioapic_irqline(target_vm,
				param->ioapic_irq, mode);
		}
		break;
	case ACRN_INTR_TYPE_IOAPIC:
		/* handle IOAPIC irqline */
		ret = handle_vioapic_irqline(target_vm,
				param->ioapic_irq, mode);
		break;
	default:
		dev_dbg(ACRN_DBG_HYCALL, "vINTR inject failed. type=%d",
				intr_type);
		ret = -1;
	}
	return ret;
}

int32_t hcall_create_vm(struct vm *vm, uint64_t param)
{
	int32_t ret = 0;
	struct vm *target_vm = NULL;
	/* VM are created from hv_main() directly
	 * Here we just return the vmid for DM
	 */
	struct acrn_create_vm cv;
	struct vm_description vm_desc;

	(void)memset((void *)&cv, 0U, sizeof(cv));
	if (copy_from_gpa(vm, &cv, param, sizeof(cv)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	(void)memset(&vm_desc, 0U, sizeof(vm_desc));
	vm_desc.sworld_enabled =
		((cv.vm_flag & (SECURE_WORLD_ENABLED)) != 0U);
	(void)memcpy_s(&vm_desc.GUID[0], 16U, &cv.GUID[0], 16U);
	ret = create_vm(&vm_desc, &target_vm);

	if (ret != 0) {
		dev_dbg(ACRN_DBG_HYCALL, "HCALL: Create VM failed");
		cv.vmid = ACRN_INVALID_VMID;
		ret = -1;
	} else {
		cv.vmid = target_vm->attr.id;
		ret = 0;
	}

	if (copy_to_gpa(vm, &cv.vmid, param, sizeof(cv.vmid)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	return ret;
}

int32_t hcall_destroy_vm(uint16_t vmid)
{
	int32_t ret = 0;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL) {
		return -1;
	}

	ret = shutdown_vm(target_vm);
	return ret;
}

int32_t hcall_resume_vm(uint16_t vmid)
{
	int32_t ret = 0;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL) {
		return -1;
	}
	if (target_vm->sw.io_shared_page == NULL) {
		ret = -1;
	} else {
		ret = start_vm(target_vm);
	}

	return ret;
}

int32_t hcall_pause_vm(uint16_t vmid)
{
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL) {
		return -1;
	}

	pause_vm(target_vm);

	return 0;
}

int32_t hcall_create_vcpu(struct vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret;
	uint16_t pcpu_id;
	struct acrn_create_vcpu cv;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if ((target_vm == NULL) || (param == 0U)) {
		return -1;
	}

	if (copy_from_gpa(vm, &cv, param, sizeof(cv)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	pcpu_id = allocate_pcpu();
	if (pcpu_id == INVALID_CPU_ID) {
		pr_err("%s: No physical available\n", __func__);
		return -1;
	}

	ret = prepare_vcpu(target_vm, pcpu_id);

	return ret;
}

int32_t hcall_assert_irqline(struct vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret = 0;
	struct acrn_irqline irqline;

	if (copy_from_gpa(vm, &irqline, param, sizeof(irqline)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}
	ret = handle_virt_irqline(vm, vmid, &irqline, IRQ_ASSERT);

	return ret;
}

int32_t hcall_deassert_irqline(struct vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret = 0;
	struct acrn_irqline irqline;

	if (copy_from_gpa(vm, &irqline, param, sizeof(irqline)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}
	ret = handle_virt_irqline(vm, vmid, &irqline, IRQ_DEASSERT);

	return ret;
}

int32_t hcall_pulse_irqline(struct vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret = 0;
	struct acrn_irqline irqline;

	if (copy_from_gpa(vm, &irqline, param, sizeof(irqline)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}
	ret = handle_virt_irqline(vm, vmid, &irqline, IRQ_PULSE);

	return ret;
}

int32_t hcall_inject_msi(struct vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret = 0;
	struct acrn_msi_entry msi;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL) {
		return -1;
	}

	(void)memset((void *)&msi, 0U, sizeof(msi));
	if (copy_from_gpa(vm, &msi, param, sizeof(msi)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}
	ret = vlapic_intr_msi(target_vm, msi.msi_addr, msi.msi_data);

	return ret;
}

int32_t hcall_set_ioreq_buffer(struct vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret = 0;
	uint64_t hpa = 0UL;
	struct acrn_set_ioreq_buffer iobuf;
	struct vm *target_vm = get_vm_from_vmid(vmid);
	union vhm_request_buffer *req_buf;
	uint16_t i;

	if (target_vm == NULL) {
		return -1;
	}

	(void)memset((void *)&iobuf, 0U, sizeof(iobuf));

	if (copy_from_gpa(vm, &iobuf, param, sizeof(iobuf)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	dev_dbg(ACRN_DBG_HYCALL, "[%d] SET BUFFER=0x%p",
			vmid, iobuf.req_buf);

	hpa = gpa2hpa(vm, iobuf.req_buf);
	if (hpa == 0UL) {
		pr_err("%s: invalid GPA.\n", __func__);
		target_vm->sw.io_shared_page = NULL;
		return -EINVAL;
	}

	target_vm->sw.io_shared_page = HPA2HVA(hpa);

	req_buf = target_vm->sw.io_shared_page;
	for (i = 0U; i < VHM_REQUEST_MAX; i++) {
		atomic_store32(&req_buf->req_queue[i].processed, REQ_STATE_FREE);
	}

	return ret;
}

int32_t hcall_notify_ioreq_finish(uint16_t vmid, uint16_t vcpu_id)
{
	struct vcpu *vcpu;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	/* make sure we have set req_buf */
	if ((target_vm == NULL) || target_vm->sw.io_shared_page == NULL) {
		pr_err("%s, invalid parameter\n", __func__);
		return -EINVAL;
	}

	dev_dbg(ACRN_DBG_HYCALL, "[%d] NOTIFY_FINISH for vcpu %d",
			vmid, vcpu_id);

	vcpu = vcpu_from_vid(target_vm, vcpu_id);
	if (vcpu == NULL) {
		pr_err("%s, failed to get VCPU %d context from VM %d\n",
			__func__, vcpu_id, target_vm->attr.id);
		return -EINVAL;
	}

	emulate_io_post(vcpu);

	return 0;
}

static int32_t _set_vm_memory_region(struct vm *vm,
	struct vm *target_vm, struct vm_memory_region *region)
{
	uint64_t hpa, base_paddr;
	uint64_t prot;

	if ((region->size & (CPU_PAGE_SIZE - 1UL)) != 0UL) {
		pr_err("%s: [vm%d] map size 0x%x is not page aligned",
			__func__, target_vm->attr.id, region->size);
		return -EINVAL;
	}

	hpa = gpa2hpa(vm, region->vm0_gpa);
	dev_dbg(ACRN_DBG_HYCALL, "[vm%d] gpa=0x%x hpa=0x%x size=0x%x",
		target_vm->attr.id, region->gpa, hpa, region->size);

	base_paddr = get_hv_image_base();
	if (((hpa <= base_paddr) &&
		((hpa + region->size) > base_paddr)) ||
		((hpa >= base_paddr) &&
		(hpa < (base_paddr + CONFIG_RAM_SIZE)))) {
		pr_err("%s: overlap the HV memory region.", __func__);
		return -EFAULT;
	}

	if (region->type != MR_DEL) {
		prot = 0UL;
		/* access right */
		if ((region->prot & MEM_ACCESS_READ) != 0U) {
			prot |= EPT_RD;
		}
		if ((region->prot & MEM_ACCESS_WRITE) != 0U) {
			prot |= EPT_WR;
		}
		if ((region->prot & MEM_ACCESS_EXEC) != 0U) {
			prot |= EPT_EXE;
		}
		/* memory type */
		if ((region->prot & MEM_TYPE_WB) != 0U) {
			prot |= EPT_WB;
		} else if ((region->prot & MEM_TYPE_WT) != 0U) {
			prot |= EPT_WT;
		} else if ((region->prot & MEM_TYPE_WC) != 0U) {
			prot |= EPT_WC;
		} else if ((region->prot & MEM_TYPE_WP) != 0U) {
			prot |= EPT_WP;
		} else {
			prot |= EPT_UNCACHED;
		}
		/* create gpa to hpa EPT mapping */
		return ept_mr_add(target_vm, hpa,
				region->gpa, region->size, prot);
	} else {
		return ept_mr_del(target_vm,
				(uint64_t *)target_vm->arch_vm.nworld_eptp,
				region->gpa, region->size);
	}

}

int32_t hcall_set_vm_memory_region(struct vm *vm, uint16_t vmid, uint64_t param)
{
	struct vm_memory_region region;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if ((vm == NULL) || (target_vm == NULL)) {
		return -EINVAL;
	}

	(void)memset((void *)&region, 0U, sizeof(region));

	if (copy_from_gpa(vm, &region, param, sizeof(region)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -EFAULT;
	}

	if (!is_vm0(vm)) {
		pr_err("%s: Not coming from service vm", __func__);
		return -EPERM;
	}

	if (is_vm0(target_vm)) {
		pr_err("%s: Targeting to service vm", __func__);
		return -EPERM;
	}

	return _set_vm_memory_region(vm, target_vm, &region);
}

int32_t hcall_set_vm_memory_regions(struct vm *vm, uint64_t param)
{
	struct set_regions set_regions;
	struct vm_memory_region *regions;
	struct vm *target_vm;
	uint32_t idx;

	if (!is_vm0(vm)) {
		pr_err("%s: Not coming from service vm", __func__);
		return -EPERM;
	}

	(void)memset((void *)&set_regions, 0U, sizeof(set_regions));

	if (copy_from_gpa(vm, &set_regions, param, sizeof(set_regions)) != 0) {
		pr_err("%s: Unable copy param from vm\n", __func__);
		return -EFAULT;
	}

	target_vm = get_vm_from_vmid(set_regions.vmid);
	if (is_vm0(target_vm)) {
		pr_err("%s: Targeting to service vm", __func__);
		return -EFAULT;
	}

	idx = 0U;
	/*TODO: use copy_from_gpa for this buffer page */
	regions = GPA2HVA(vm, set_regions.regions_gpa);
	while (idx < set_regions.mr_num) {
		/* the force pointer change below is for back compatible
		 * to struct vm_memory_region, it will be removed in the future
		 */
		int ret = _set_vm_memory_region(vm, target_vm, &regions[idx]);
		if (ret < 0) {
			return ret;
		}
		idx++;
	}
	return 0;
}

static int32_t write_protect_page(struct vm *vm, struct wp_data *wp)
{
	uint64_t hpa, base_paddr;
	uint64_t prot_set;
	uint64_t prot_clr;

	hpa = gpa2hpa(vm, wp->gpa);
	dev_dbg(ACRN_DBG_HYCALL, "[vm%d] gpa=0x%x hpa=0x%x",
			vm->attr.id, wp->gpa, hpa);

	base_paddr = get_hv_image_base();
	if (((hpa <= base_paddr) && (hpa + CPU_PAGE_SIZE > base_paddr)) ||
			((hpa >= base_paddr) &&
			(hpa < base_paddr + CONFIG_RAM_SIZE))) {
		pr_err("%s: overlap the HV memory region.", __func__);
		return -EINVAL;
	}

	prot_set = (wp->set != 0U) ? 0UL : EPT_WR;
	prot_clr = (wp->set != 0U) ? EPT_WR : 0UL;

	return ept_mr_modify(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
		wp->gpa, CPU_PAGE_SIZE, prot_set, prot_clr);
}

int32_t hcall_write_protect_page(struct vm *vm, uint16_t vmid, uint64_t wp_gpa)
{
	struct wp_data wp;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if ((vm == NULL) || (target_vm == NULL)) {
		return -EINVAL;
	}

	if (!is_vm0(vm)) {
		pr_err("%s: Not coming from service vm", __func__);
		return -EPERM;
	}

	if (is_vm0(target_vm)) {
		pr_err("%s: Targeting to service vm", __func__);
		return -EINVAL;
	}

	(void)memset((void *)&wp, 0U, sizeof(wp));

	if (copy_from_gpa(vm, &wp, wp_gpa, sizeof(wp)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -EFAULT;
	}

	return write_protect_page(target_vm, &wp);
}

int32_t hcall_remap_pci_msix(struct vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret = 0;
	struct acrn_vm_pci_msix_remap remap;
	struct ptdev_msi_info info;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL) {
		return -1;
	}

	(void)memset((void *)&remap, 0U, sizeof(remap));

	if (copy_from_gpa(vm, &remap, param, sizeof(remap)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	if (!is_vm0(vm)) {
		ret = -1;
	} else {
		info.msix = remap.msix;
		info.msix_entry_index = remap.msix_entry_index;
		info.vmsi_ctl = remap.msi_ctl;
		info.vmsi_addr = remap.msi_addr;
		info.vmsi_data = remap.msi_data;

		ret = ptdev_msix_remap(target_vm,
				remap.virt_bdf, &info);
		remap.msi_data = info.pmsi_data;
		remap.msi_addr = info.pmsi_addr;

		if (copy_to_gpa(vm, &remap, param, sizeof(remap)) != 0) {
			pr_err("%s: Unable copy param to vm\n", __func__);
			return -1;
		}
	}

	return ret;
}

int32_t hcall_gpa_to_hpa(struct vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret = 0;
	struct vm_gpa2hpa v_gpa2hpa;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL) {
		return -1;
	}

	(void)memset((void *)&v_gpa2hpa, 0U, sizeof(v_gpa2hpa));

	if (copy_from_gpa(vm, &v_gpa2hpa, param, sizeof(v_gpa2hpa)) != 0) {
		pr_err("HCALL gpa2hpa: Unable copy param from vm\n");
		return -1;
	}
	v_gpa2hpa.hpa = gpa2hpa(target_vm, v_gpa2hpa.gpa);
	if (copy_to_gpa(vm, &v_gpa2hpa, param, sizeof(v_gpa2hpa)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	return ret;
}

int32_t hcall_assign_ptdev(struct vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret;
	uint16_t bdf;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL) {
		pr_err("%s, vm is null\n", __func__);
		return -EINVAL;
	}

	if (copy_from_gpa(vm, &bdf, param, sizeof(bdf)) != 0) {
		pr_err("%s: Unable copy param from vm %d\n",
			__func__, vm->attr.id);
		return -EIO;
	}

	/* create a iommu domain for target VM if not created */
	if (target_vm->iommu_domain == NULL) {
		if (target_vm->arch_vm.nworld_eptp == NULL) {
			pr_err("%s, EPT of VM not set!\n",
				__func__, target_vm->attr.id);
			return -EPERM;
		}
		/* TODO: how to get vm's address width? */
		target_vm->iommu_domain = create_iommu_domain(vmid,
				HVA2HPA(target_vm->arch_vm.nworld_eptp), 48U);
		if (target_vm->iommu_domain == NULL) {
			return -ENODEV;
		}

	}
	ret = assign_iommu_device(target_vm->iommu_domain,
			(uint8_t)(bdf >> 8), (uint8_t)(bdf & 0xffU));

	return ret;
}

int32_t hcall_deassign_ptdev(struct vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret = 0;
	uint16_t bdf;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL) {
		return -1;
	}

	if (copy_from_gpa(vm, &bdf, param, sizeof(bdf)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}
	ret = unassign_iommu_device(target_vm->iommu_domain,
			(uint8_t)(bdf >> 8), (uint8_t)(bdf & 0xffU));

	return ret;
}

int32_t hcall_set_ptdev_intr_info(struct vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret = 0;
	struct hc_ptdev_irq irq;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL) {
		return -1;
	}

	(void)memset((void *)&irq, 0U, sizeof(irq));

	if (copy_from_gpa(vm, &irq, param, sizeof(irq)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	if (irq.type == IRQ_INTX) {
		ret = ptdev_add_intx_remapping(target_vm,
				irq.virt_bdf, irq.phys_bdf,
				irq.is.intx.virt_pin, irq.is.intx.phys_pin,
				irq.is.intx.pic_pin);
	} else if (irq.type == IRQ_MSI || irq.type == IRQ_MSIX) {
		ret = ptdev_add_msix_remapping(target_vm,
				irq.virt_bdf, irq.phys_bdf,
				irq.is.msix.vector_cnt);
	} else {
		pr_err("%s: Invalid irq type: %u\n", __func__, irq.type);
		ret = -1;
	}

	return ret;
}

int32_t
hcall_reset_ptdev_intr_info(struct vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret = 0;
	struct hc_ptdev_irq irq;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL) {
		return -1;
	}

	(void)memset((void *)&irq, 0U, sizeof(irq));

	if (copy_from_gpa(vm, &irq, param, sizeof(irq)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	if (irq.type == IRQ_INTX) {
		ptdev_remove_intx_remapping(target_vm,
				irq.is.intx.virt_pin,
				irq.is.intx.pic_pin);
	} else if (irq.type == IRQ_MSI || irq.type == IRQ_MSIX) {
		ptdev_remove_msix_remapping(target_vm,
				irq.virt_bdf,
				irq.is.msix.vector_cnt);
	} else {
		pr_err("%s: Invalid irq type: %u\n", __func__, irq.type);
		ret = -1;
	}

	return ret;
}

int32_t hcall_setup_sbuf(struct vm *vm, uint64_t param)
{
	struct sbuf_setup_param ssp;
	uint64_t *hva;

	(void)memset((void *)&ssp, 0U, sizeof(ssp));

	if (copy_from_gpa(vm, &ssp, param, sizeof(ssp)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	if (ssp.gpa != 0U) {
		hva = (uint64_t *)GPA2HVA(vm, ssp.gpa);
	} else {
		hva = (uint64_t *)NULL;
	}

	return sbuf_share_setup(ssp.pcpu_id, ssp.sbuf_id, hva);
}

int32_t hcall_get_cpu_pm_state(struct vm *vm, uint64_t cmd, uint64_t param)
{
	uint16_t target_vm_id;
	struct vm *target_vm;

	target_vm_id = (uint16_t)((cmd & PMCMD_VMID_MASK) >> PMCMD_VMID_SHIFT);
	target_vm = get_vm_from_vmid(target_vm_id);

	if (target_vm == NULL) {
		return -1;
	}

	switch (cmd & PMCMD_TYPE_MASK) {
	case PMCMD_GET_PX_CNT: {

		if (target_vm->pm.px_cnt == 0U) {
			return -1;
		}

		if (copy_to_gpa(vm, &(target_vm->pm.px_cnt), param,
					sizeof(target_vm->pm.px_cnt)) != 0) {
			pr_err("%s: Unable copy param to vm\n", __func__);
			return -1;
		}
		return 0;
	}
	case PMCMD_GET_PX_DATA: {
		int32_t pn;
		struct cpu_px_data *px_data;

		/* For now we put px data as per-vm,
		 * If it is stored as per-cpu in the future,
		 * we need to check PMCMD_VCPUID_MASK in cmd.
		 */
		if (target_vm->pm.px_cnt == 0U) {
			return -1;
		}

		pn = (cmd & PMCMD_STATE_NUM_MASK) >> PMCMD_STATE_NUM_SHIFT;
		if (pn >= target_vm->pm.px_cnt) {
			return -1;
		}

		px_data = target_vm->pm.px_data + pn;
		if (copy_to_gpa(vm, px_data, param,
						sizeof(struct cpu_px_data)) != 0) {
			pr_err("%s: Unable copy param to vm\n", __func__);
			return -1;
		}

		return 0;
	}
	case PMCMD_GET_CX_CNT: {

		if (target_vm->pm.cx_cnt == 0U) {
			return -1;
		}

		if (copy_to_gpa(vm, &(target_vm->pm.cx_cnt), param,
					sizeof(target_vm->pm.cx_cnt)) != 0) {
			pr_err("%s: Unable copy param to vm\n", __func__);
			return -1;
		}
		return 0;
	}
	case PMCMD_GET_CX_DATA: {
		uint8_t cx_idx;
		struct cpu_cx_data *cx_data;

		if (target_vm->pm.cx_cnt == 0U) {
			return -1;
		}

		cx_idx = (uint8_t)
			((cmd & PMCMD_STATE_NUM_MASK) >> PMCMD_STATE_NUM_SHIFT);
		if ((cx_idx == 0U) || (cx_idx > target_vm->pm.cx_cnt)) {
			return -1;
		}

		cx_data = target_vm->pm.cx_data + cx_idx;

		if (copy_to_gpa(vm, cx_data, param,
						sizeof(struct cpu_cx_data)) != 0) {
			pr_err("%s: Unable copy param to vm\n", __func__);
			return -1;
		}

		return 0;
	}
	default:
		return -1;

	}
}
