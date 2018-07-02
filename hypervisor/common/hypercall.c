/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <schedule.h>
#include <hypercall.h>
#include <version.h>

#define ACRN_DBG_HYCALL	6

bool is_hypercall_from_ring0(void)
{
	uint64_t cs_sel;

	cs_sel = exec_vmread(VMX_GUEST_CS_SEL);
	/* cs_selector[1:0] is CPL */
	if ((cs_sel & 0x3UL) == 0)
		return true;

	return false;
}

int64_t hcall_get_api_version(struct vm *vm, uint64_t param)
{
	struct hc_api_version version;

	if (!is_vm0(vm))
		return -1;

	version.major_version = HV_API_MAJOR_VERSION;
	version.minor_version = HV_API_MINOR_VERSION;

	if (copy_to_gpa(vm, &version, param, sizeof(version)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	return 0;
}

static int handle_vpic_irqline(struct vm *vm, int irq, enum irq_mode mode)
{
	int32_t ret = -1;

	if (vm == NULL)
		return ret;

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

static int
handle_vioapic_irqline(struct vm *vm, int irq, enum irq_mode mode)
{
	int32_t ret = -1;

	if (vm == NULL)
		return ret;

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

static int handle_virt_irqline(struct vm *vm, uint64_t target_vmid,
		struct acrn_irqline *param, enum irq_mode mode)
{
	int32_t ret = 0;
	uint32_t intr_type;
	struct vm *target_vm = get_vm_from_vmid(target_vmid);

	if ((vm == NULL) || (param == NULL))
		return -1;

	intr_type = param->intr_type;

	switch (intr_type) {
	case ACRN_INTR_TYPE_ISA:
		/* Call vpic for pic injection */
		ret = handle_vpic_irqline(target_vm, param->pic_irq, mode);

		/* call vioapic for ioapic injection if ioapic_irq != -1*/
		if (param->ioapic_irq != -1UL) {
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

int64_t hcall_create_vm(struct vm *vm, uint64_t param)
{
	int64_t ret = 0;
	struct vm *target_vm = NULL;
	/* VM are created from hv_main() directly
	 * Here we just return the vmid for DM
	 */
	struct acrn_create_vm cv;
	struct vm_description vm_desc;

	memset((void *)&cv, 0, sizeof(cv));
	if (copy_from_gpa(vm, &cv, param, sizeof(cv)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	memset(&vm_desc, 0, sizeof(vm_desc));
	vm_desc.sworld_enabled =
		(!!(cv.vm_flag & (SECURE_WORLD_ENABLED)));
	memcpy_s(&vm_desc.GUID[0], 16, &cv.GUID[0], 16);
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

int64_t hcall_destroy_vm(uint64_t vmid)
{
	int64_t ret = 0;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL)
		return -1;

	ret = shutdown_vm(target_vm);
	return ret;
}

int64_t hcall_resume_vm(uint64_t vmid)
{
	int64_t ret = 0;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL)
		return -1;
	if (target_vm->sw.io_shared_page == NULL)
		ret = -1;
	else
		ret = start_vm(target_vm);

	return ret;
}

int64_t hcall_pause_vm(uint64_t vmid)
{
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL)
		return -1;

	pause_vm(target_vm);

	return 0;
}

int64_t hcall_create_vcpu(struct vm *vm, uint64_t vmid, uint64_t param)
{
	int32_t ret;
	uint16_t pcpu_id;
	struct acrn_create_vcpu cv;

	struct vm *target_vm = get_vm_from_vmid(vmid);

	if ((target_vm == NULL) || (param == 0U))
		return -1;

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

int64_t hcall_assert_irqline(struct vm *vm, uint64_t vmid, uint64_t param)
{
	int64_t ret = 0;
	struct acrn_irqline irqline;

	if (copy_from_gpa(vm, &irqline, param, sizeof(irqline)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}
	ret = handle_virt_irqline(vm, vmid, &irqline, IRQ_ASSERT);

	return ret;
}

int64_t hcall_deassert_irqline(struct vm *vm, uint64_t vmid, uint64_t param)
{
	int64_t ret = 0;
	struct acrn_irqline irqline;

	if (copy_from_gpa(vm, &irqline, param, sizeof(irqline)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}
	ret = handle_virt_irqline(vm, vmid, &irqline, IRQ_DEASSERT);

	return ret;
}

int64_t hcall_pulse_irqline(struct vm *vm, uint64_t vmid, uint64_t param)
{
	int64_t ret = 0;
	struct acrn_irqline irqline;

	if (copy_from_gpa(vm, &irqline, param, sizeof(irqline)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}
	ret = handle_virt_irqline(vm, vmid, &irqline, IRQ_PULSE);

	return ret;
}

int64_t hcall_inject_msi(struct vm *vm, uint64_t vmid, uint64_t param)
{
	int32_t ret = 0;
	struct acrn_msi_entry msi;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL)
		return -1;

	memset((void *)&msi, 0, sizeof(msi));
	if (copy_from_gpa(vm, &msi, param, sizeof(msi)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}
	ret = vlapic_intr_msi(target_vm, msi.msi_addr, msi.msi_data);

	return ret;
}

int64_t hcall_set_ioreq_buffer(struct vm *vm, uint64_t vmid, uint64_t param)
{
	int64_t ret = 0;
	uint64_t hpa = 0;
	struct acrn_set_ioreq_buffer iobuf;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL)
		return -1;

	memset((void *)&iobuf, 0, sizeof(iobuf));

	if (copy_from_gpa(vm, &iobuf, param, sizeof(iobuf)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	dev_dbg(ACRN_DBG_HYCALL, "[%d] SET BUFFER=0x%p",
			vmid, iobuf.req_buf);

	hpa = gpa2hpa(vm, iobuf.req_buf);
	if (hpa == 0) {
		pr_err("%s: invalid GPA.\n", __func__);
		target_vm->sw.io_shared_page = NULL;
		return -EINVAL;
	}

	target_vm->sw.io_shared_page = HPA2HVA(hpa);

	return ret;
}

static void complete_request(struct vcpu *vcpu)
{
	/*
	 * If vcpu is in Zombie state and will be destroyed soon. Just
	 * mark ioreq done and don't resume vcpu.
	 */
	if (vcpu->state == VCPU_ZOMBIE) {
		union vhm_request_buffer *req_buf;

		req_buf = (union vhm_request_buffer *)
				vcpu->vm->sw.io_shared_page;
		req_buf->req_queue[vcpu->vcpu_id].valid = false;
		atomic_store(&vcpu->ioreq_pending, 0);

		return;
	}

	switch (vcpu->req.type) {
	case REQ_MMIO:
		request_vcpu_pre_work(vcpu, ACRN_VCPU_MMIO_COMPLETE);
		break;

	case REQ_PORTIO:
		dm_emulate_pio_post(vcpu);
		break;

	default:
		break;
	}

	resume_vcpu(vcpu);
}

int64_t hcall_notify_req_finish(uint64_t vmid, uint64_t vcpu_id)
{
	union vhm_request_buffer *req_buf;
	struct vhm_request *req;
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

	req_buf = (union vhm_request_buffer *)target_vm->sw.io_shared_page;
	req = req_buf->req_queue + vcpu_id;

	if ((req->valid != 0) &&
		((req->processed == REQ_STATE_SUCCESS) ||
		 (req->processed == REQ_STATE_FAILED)))
		complete_request(vcpu);

	return 0;
}

int64_t _set_vm_memmap(struct vm *vm, struct vm *target_vm,
	struct vm_set_memmap *memmap)
{
	uint64_t hpa;
	uint32_t attr, prot;

	if ((memmap->length & 0xFFFUL) != 0) {
		pr_err("%s: ERROR! [vm%d] map size 0x%x is not page aligned",
				__func__, target_vm->attr.id, memmap->length);
		return -1;
	}

	hpa = gpa2hpa(vm, memmap->vm0_gpa);
	dev_dbg(ACRN_DBG_HYCALL, "[vm%d] gpa=0x%x hpa=0x%x size=0x%x",
		target_vm->attr.id, memmap->remote_gpa, hpa, memmap->length);

	if (((hpa <= CONFIG_RAM_START) &&
		(hpa + memmap->length > CONFIG_RAM_START)) ||
		((hpa >= CONFIG_RAM_START) &&
		(hpa < CONFIG_RAM_START + CONFIG_RAM_SIZE))) {
		pr_err("%s: ERROR! overlap the HV memory region.", __func__);
		return -1;
	}

	/* Check prot */
	attr = 0;
	if (memmap->type != MAP_UNMAP) {
		prot = (memmap->prot != 0) ? memmap->prot : memmap->prot_2;
		if ((prot & MEM_ACCESS_READ) != 0U)
			attr |= IA32E_EPT_R_BIT;
		if ((prot & MEM_ACCESS_WRITE) != 0U)
			attr |= IA32E_EPT_W_BIT;
		if ((prot & MEM_ACCESS_EXEC) != 0U)
			attr |= IA32E_EPT_X_BIT;
		if ((prot & MEM_TYPE_WB) != 0U)
			attr |= IA32E_EPT_WB;
		else if ((prot & MEM_TYPE_WT) != 0U)
			attr |= IA32E_EPT_WT;
		else if ((prot & MEM_TYPE_WC) != 0U)
			attr |= IA32E_EPT_WC;
		else if ((prot & MEM_TYPE_WP) != 0U)
			attr |= IA32E_EPT_WP;
		else
			attr |= IA32E_EPT_UNCACHED;
	}

	/* create gpa to hpa EPT mapping */
	return ept_mmap(target_vm, hpa,
		memmap->remote_gpa, memmap->length, memmap->type, attr);
}

int64_t hcall_set_vm_memmap(struct vm *vm, uint64_t vmid, uint64_t param)
{
	struct vm_set_memmap memmap;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if ((vm == NULL) || (target_vm == NULL))
		return -1;

	memset((void *)&memmap, 0, sizeof(memmap));

	if (copy_from_gpa(vm, &memmap, param, sizeof(memmap)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	if (!is_vm0(vm)) {
		pr_err("%s: ERROR! Not coming from service vm", __func__);
		return -1;
	}

	if (is_vm0(target_vm)) {
		pr_err("%s: ERROR! Targeting to service vm", __func__);
		return -1;
	}

	return _set_vm_memmap(vm, target_vm, &memmap);
}

int64_t hcall_set_vm_memmaps(struct vm *vm, uint64_t param)
{
	struct set_memmaps set_memmaps;
	struct memory_map *regions;
	struct vm *target_vm;
	uint32_t idx;

	if (!is_vm0(vm)) {
		pr_err("%s: ERROR! Not coming from service vm",
				__func__);
		return -1;
	}

	memset((void *)&set_memmaps, 0, sizeof(set_memmaps));

	if (copy_from_gpa(vm, &set_memmaps, param, sizeof(set_memmaps)) != 0) {
		pr_err("%s: Unable copy param from vm\n", __func__);
		return -1;
	}

	target_vm = get_vm_from_vmid(set_memmaps.vmid);
	if (is_vm0(target_vm)) {
		pr_err("%s: ERROR! Targeting to service vm",
				__func__);
		return -1;
	}

	idx = 0U;
	/*TODO: use copy_from_gpa for this buffer page */
	regions = GPA2HVA(vm, set_memmaps.memmaps_gpa);
	while (idx < set_memmaps.memmaps_num) {
		/* the force pointer change below is for back compatible
		 * to struct vm_set_memmap, it will be removed in the future
		 */
		if (_set_vm_memmap(vm, target_vm,
			(struct vm_set_memmap *)&regions[idx]) < 0)
			return -1;
		idx++;
	}
	return 0;
}

int64_t hcall_remap_pci_msix(struct vm *vm, uint64_t vmid, uint64_t param)
{
	int64_t ret = 0;
	struct acrn_vm_pci_msix_remap remap;
	struct ptdev_msi_info info;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL)
		return -1;

	memset((void *)&remap, 0, sizeof(remap));

	if (copy_from_gpa(vm, &remap, param, sizeof(remap)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	if (!is_vm0(vm))
		ret = -1;
	else {
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

int64_t hcall_gpa_to_hpa(struct vm *vm, uint64_t vmid, uint64_t param)
{
	int64_t ret = 0;
	struct vm_gpa2hpa v_gpa2hpa;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL)
		return -1;

	memset((void *)&v_gpa2hpa, 0, sizeof(v_gpa2hpa));

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

int64_t hcall_assign_ptdev(struct vm *vm, uint64_t vmid, uint64_t param)
{
	int64_t ret;
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
		if (target_vm->arch_vm.nworld_eptp == 0) {
			pr_err("%s, EPT of VM not set!\n",
				__func__, target_vm->attr.id);
			return -EPERM;
		}
		/* TODO: how to get vm's address width? */
		target_vm->iommu_domain = create_iommu_domain(vmid,
				target_vm->arch_vm.nworld_eptp, 48);
		if (target_vm->iommu_domain == NULL)
			return -ENODEV;

	}
	ret = assign_iommu_device(target_vm->iommu_domain,
			(uint8_t)(bdf >> 8), (uint8_t)(bdf & 0xffU));

	return ret;
}

int64_t hcall_deassign_ptdev(struct vm *vm, uint64_t vmid, uint64_t param)
{
	int64_t ret = 0;
	uint16_t bdf;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL)
		return -1;

	if (copy_from_gpa(vm, &bdf, param, sizeof(bdf)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}
	ret = unassign_iommu_device(target_vm->iommu_domain,
			(uint8_t)(bdf >> 8), (uint8_t)(bdf & 0xffU));

	return ret;
}

int64_t hcall_set_ptdev_intr_info(struct vm *vm, uint64_t vmid, uint64_t param)
{
	int64_t ret = 0;
	struct hc_ptdev_irq irq;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL)
		return -1;

	memset((void *)&irq, 0, sizeof(irq));

	if (copy_from_gpa(vm, &irq, param, sizeof(irq)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	if (irq.type == IRQ_INTX)
		ret = ptdev_add_intx_remapping(target_vm,
				irq.virt_bdf, irq.phys_bdf,
				irq.is.intx.virt_pin, irq.is.intx.phys_pin,
				irq.is.intx.pic_pin);
	else if (irq.type == IRQ_MSI || irq.type == IRQ_MSIX)
		ret = ptdev_add_msix_remapping(target_vm,
				irq.virt_bdf, irq.phys_bdf,
				irq.is.msix.vector_cnt);

	return ret;
}

int64_t
hcall_reset_ptdev_intr_info(struct vm *vm, uint64_t vmid, uint64_t param)
{
	int64_t ret = 0;
	struct hc_ptdev_irq irq;
	struct vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL)
		return -1;

	memset((void *)&irq, 0, sizeof(irq));

	if (copy_from_gpa(vm, &irq, param, sizeof(irq)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	if (irq.type == IRQ_INTX)
		ptdev_remove_intx_remapping(target_vm,
				irq.is.intx.virt_pin,
				irq.is.intx.pic_pin);
	else if (irq.type == IRQ_MSI || irq.type == IRQ_MSIX)
		ptdev_remove_msix_remapping(target_vm,
				irq.virt_bdf,
				irq.is.msix.vector_cnt);

	return ret;
}

int64_t hcall_setup_sbuf(struct vm *vm, uint64_t param)
{
	struct sbuf_setup_param ssp;
	uint64_t *hva;

	memset((void *)&ssp, 0, sizeof(ssp));

	if (copy_from_gpa(vm, &ssp, param, sizeof(ssp)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	if (ssp.gpa != 0U)
		hva = (uint64_t *)GPA2HVA(vm, ssp.gpa);
	else
		hva = (uint64_t *)NULL;

	return sbuf_share_setup(ssp.pcpu_id, ssp.sbuf_id, hva);
}

int64_t hcall_get_cpu_pm_state(struct vm *vm, uint64_t cmd, uint64_t param)
{
	int32_t target_vm_id;
	struct vm *target_vm;

	target_vm_id = (cmd & PMCMD_VMID_MASK) >> PMCMD_VMID_SHIFT;
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

		if (target_vm->pm.cx_cnt == 0) {
			return -1;
		}

		cx_idx = (cmd & PMCMD_STATE_NUM_MASK) >> PMCMD_STATE_NUM_SHIFT;
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
