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
#include <vmx.h>
#include <hyperv.h>

#define ACRN_DBG_HYPERV		6U

/* Partition Reference Counter (HV_X64_MSR_TIME_REF_COUNT) */
#define CPUID3A_TIME_REF_COUNT_MSR	(1U << 1U)
/* Hypercall MSRs (HV_X64_MSR_GUEST_OS_ID and HV_X64_MSR_HYPERCALL) */
#define CPUID3A_HYPERCALL_MSR		(1U << 5U)
/* Access virtual processor index MSR (HV_X64_MSR_VP_INDEX) */
#define CPUID3A_VP_INDEX_MSR		(1U << 6U)
/* Partition reference TSC MSR (HV_X64_MSR_REFERENCE_TSC) */
#define CPUID3A_REFERENCE_TSC_MSR	(1U << 9U)

struct HV_REFERENCE_TSC_PAGE {
	uint32_t tsc_sequence;
	uint32_t reserved1;
	uint64_t tsc_scale;
	uint64_t tsc_offset;
	uint64_t reserved2[509];
};

static inline uint64_t
u64_shl64_div_u64(uint64_t a, uint64_t divisor)
{
	uint64_t ret, tmp;

	asm volatile ("divq %2" :
		"=a" (ret), "=d" (tmp) :
		"rm" (divisor), "0" (0U), "1" (a));

	return ret;
}

static inline uint64_t
u64_mul_u64_shr64(uint64_t a, uint64_t b)
{
	uint64_t ret, disc;

	asm volatile ("mulq %3" :
		"=d" (ret), "=a" (disc) :
		"a" (a), "r" (b));

	return ret;
}

static inline void
hyperv_get_tsc_scale_offset(struct acrn_vm *vm, uint64_t *scale, uint64_t *offset)
{
	if (vm->arch_vm.hyperv.tsc_scale == 0UL) {
		/*
		 * The partition reference time is computed by the following formula:
		 * ReferenceTime = ((VirtualTsc * TscScale) >> 64) + TscOffset
		 * ReferenceTime is in 100ns units
		 *
		 * ReferenceTime =
		 *     VirtualTsc / (get_tsc_khz() * 1000) * 1000000000 / 100
		 *     + TscOffset
		 */

		uint64_t ret, khz = get_tsc_khz();

		/* ret = (10000U << 64U) / get_tsc_khz() */
		ret = u64_shl64_div_u64(10000U, khz);

		dev_dbg(ACRN_DBG_HYPERV, "%s, ret = 0x%lx", __func__, ret);

		vm->arch_vm.hyperv.tsc_scale = ret;
		vm->arch_vm.hyperv.tsc_offset = 0UL;
	}

	*scale = vm->arch_vm.hyperv.tsc_scale;
	*offset = vm->arch_vm.hyperv.tsc_offset;
}

static void
hyperv_setup_tsc_page(const struct acrn_vcpu *vcpu, uint64_t val)
{
	union hyperv_ref_tsc_page_msr *ref_tsc_page = &vcpu->vm->arch_vm.hyperv.ref_tsc_page;
	struct HV_REFERENCE_TSC_PAGE *p;
	uint64_t tsc_scale, tsc_offset;
	uint32_t tsc_seq;

	ref_tsc_page->val64 = val;

	if (ref_tsc_page->enabled == 1U) {
		p = (struct HV_REFERENCE_TSC_PAGE *)gpa2hva(vcpu->vm, ref_tsc_page->gpfn << PAGE_SHIFT);
		if (p != NULL) {
			hyperv_get_tsc_scale_offset(vcpu->vm, &tsc_scale, &tsc_offset);
			stac();
			p->tsc_scale = tsc_scale;
			p->tsc_offset = tsc_offset;
			cpu_write_memory_barrier();
			tsc_seq = p->tsc_sequence + 1U;
			if ((tsc_seq == 0xFFFFFFFFU) || (tsc_seq == 0U)) {
				tsc_seq = 1U;
			}
			p->tsc_sequence = tsc_seq;
			clac();
		}
	}
}

static inline uint64_t
hyperv_get_ref_count(struct acrn_vm *vm)
{
	uint64_t tsc, tsc_scale, tsc_offset, ret;

	/* currently only "use tsc offsetting" is set to 1 */
	tsc = rdtsc() + exec_vmread64(VMX_TSC_OFFSET_FULL);

	hyperv_get_tsc_scale_offset(vm, &tsc_scale, &tsc_offset);

	/* ret = ((tsc * tsc_scale) >> 64) + tsc_offset */
	ret = u64_mul_u64_shr64(tsc, tsc_scale) + tsc_offset;

	return ret;
}

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
	case HV_X64_MSR_TIME_REF_COUNT:
		/* read only */
		break;
	case HV_X64_MSR_REFERENCE_TSC:
		hyperv_setup_tsc_page(vcpu, wval);
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
	case HV_X64_MSR_TIME_REF_COUNT:
		*rval = hyperv_get_ref_count(vcpu->vm);
		break;
	case HV_X64_MSR_REFERENCE_TSC:
		*rval = vcpu->vm->arch_vm.hyperv.ref_tsc_page.val64;
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
		entry->eax = CPUID3A_HYPERCALL_MSR | CPUID3A_VP_INDEX_MSR |
			CPUID3A_TIME_REF_COUNT_MSR | CPUID3A_REFERENCE_TSC_MSR;
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
