/*
 * Microsoft Hyper-V emulation. See Microsoft's
 * Hypervisor Top Level Functional Specification for more information.
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <vm.h>
#include <logmsg.h>
#include <hyperv.h>

#define ACRN_DBG_HYPERV		6U

/* Hypercall MSRs (HV_X64_MSR_GUEST_OS_ID and HV_X64_MSR_HYPERCALL) */
#define CPUID3A_HYPERCALL_MSR		(1U << 5U)
/* Access virtual processor index MSR (HV_X64_MSR_VP_INDEX) */
#define CPUID3A_VP_INDEX_MSR		(1U << 6U)

static void
hyperv_setup_hypercall_page(const struct acrn_vcpu *vcpu, uint64_t val)
{
	union hyperv_hypercall_msr hypercall;
	uint64_t page_gpa;
	void *page_hva;

	/* asm volatile ("mov $2,%%eax; mov $0,%%edx; ret":::"eax","edx"); */
	const uint8_t inst[11] = {0xb8U, 0x02U, 0x0U, 0x0U, 0x0U, 0xbaU, 0x0U, 0x0U, 0x0U, 0x0U, 0xc3U};

	hypercall.val64 = val;

	if (hypercall.enabled != 0UL) {
		page_gpa = hypercall.gpfn << PAGE_SHIFT;
		page_hva = gpa2hva(vcpu->vm, page_gpa);
		if (page_hva != NULL) {
			stac();
			(void)memset(page_hva, 0U, PAGE_SIZE);
			(void)memcpy_s(page_hva, 11U, inst, 11U);
			clac();
		}
	}
}

int32_t
hyperv_wrmsr(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t wval)
{
	int32_t ret = 0;

	switch (msr) {
	case HV_X64_MSR_GUEST_OS_ID:
		vcpu->vm->arch_vm.hyperv.guest_os_id.val64 = wval;
		if (wval == 0UL) {
			vcpu->vm->arch_vm.hyperv.hypercall_page.enabled = 0UL;
		}
		break;
	case HV_X64_MSR_HYPERCALL:
		if (vcpu->vm->arch_vm.hyperv.guest_os_id.val64 == 0UL) {
			pr_warn("hv: %s: guest_os_id is 0", __func__);
			break;
		}
		vcpu->vm->arch_vm.hyperv.hypercall_page.val64 = wval;
		hyperv_setup_hypercall_page(vcpu, wval);
		break;
	case HV_X64_MSR_VP_INDEX:
		/* read only */
		break;
	default:
		pr_err("hv: %s: unexpected MSR[0x%x] write", __func__, msr);
		ret = -1;
		break;
	}

	dev_dbg(ACRN_DBG_HYPERV, "hv: %s: MSR=0x%x wval=0x%llx vcpuid=%d vmid=%d",
		__func__, msr, wval, vcpu->vcpu_id, vcpu->vm->vm_id);

	return ret;
}

int32_t
hyperv_rdmsr(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t *rval)
{
	int32_t ret = 0;

	switch (msr) {
	case HV_X64_MSR_GUEST_OS_ID:
		*rval = vcpu->vm->arch_vm.hyperv.guest_os_id.val64;
		break;
	case HV_X64_MSR_HYPERCALL:
		*rval = vcpu->vm->arch_vm.hyperv.hypercall_page.val64;
		break;
	case HV_X64_MSR_VP_INDEX:
		*rval = vcpu->vcpu_id;
		break;
	default:
		pr_err("hv: %s: unexpected MSR[0x%x] read", __func__, msr);
		ret = -1;
		break;
	}

	dev_dbg(ACRN_DBG_HYPERV, "hv: %s: MSR=0x%x rval=0x%llx vcpuid=%d vmid=%d",
		__func__, msr, *rval, vcpu->vcpu_id, vcpu->vm->vm_id);

	return ret;
}

void
hyperv_init_vcpuid_entry(uint32_t leaf, uint32_t subleaf, uint32_t flags,
			 struct vcpuid_entry *entry)
{
	entry->leaf = leaf;
	entry->subleaf = subleaf;
	entry->flags = flags;

	switch (leaf) {
	case 0x40000001U: /* HV interface version */
		entry->eax = 0x31237648U; /* "Hv#1" */
		entry->ebx = 0U;
		entry->ecx = 0U;
		entry->edx = 0U;
		break;
	case 0x40000002U: /* HV system identity */
		entry->eax = 0U;
		entry->ebx = 0U;
		entry->ecx = 0U;
		entry->edx = 0U;
		break;
	case 0x40000003U: /* HV supported feature */
		entry->eax = CPUID3A_HYPERCALL_MSR | CPUID3A_VP_INDEX_MSR;
		entry->ebx = 0U;
		entry->ecx = 0U;
		entry->edx = 0U;
		break;
	case 0x40000004U: /* HV Recommended hypercall usage */
		entry->eax = 0U;
		entry->ebx = 0U;
		entry->ecx = 0U;
		entry->edx = 0U;
		break;
	case 0x40000005U: /* HV Maximum Supported Virtual & logical Processors */
		entry->eax = CONFIG_MAX_VCPUS_PER_VM;
		entry->ebx = 0U;
		entry->ecx = 0U;
		entry->edx = 0U;
		break;
	case 0x40000006U: /* Implementation Hardware Features */
		entry->eax = 0U;
		entry->ebx = 0U;
		entry->ecx = 0U;
		entry->edx = 0U;
		break;
	default:
		/* do nothing */
		break;
	}

	dev_dbg(ACRN_DBG_HYPERV, "hv: %s: leaf=%x subleaf=%x flags=%x eax=%x ebx=%x ecx=%x edx=%x",
		__func__, leaf, subleaf, flags, entry->eax, entry->ebx, entry->ecx, entry->edx);
}
