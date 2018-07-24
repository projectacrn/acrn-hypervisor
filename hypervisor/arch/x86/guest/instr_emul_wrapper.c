/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#include "instr_emul_wrapper.h"
#include "instr_emul.h"

#define VMX_INVALID_VMCS_FIELD  0xffffffffU

static int
encode_vmcs_seg_desc(enum cpu_reg_name seg,
		uint32_t *base, uint32_t *lim, uint32_t *acc);

static uint32_t
get_vmcs_field(enum cpu_reg_name ident);

static bool
is_segment_register(enum cpu_reg_name reg);

static bool
is_descriptor_table(enum cpu_reg_name reg);

int vm_get_register(struct vcpu *vcpu, enum cpu_reg_name reg, uint64_t *retval)
{
	struct run_context *cur_context;

	if (vcpu == NULL) {
		return -EINVAL;
	}

	if ((reg > CPU_REG_LAST) || (reg < CPU_REG_FIRST)) {
		return -EINVAL;
	}

	if ((reg >= CPU_REG_GENERAL_FIRST) && (reg <= CPU_REG_GENERAL_LAST)) {
		cur_context =
			&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
		*retval = cur_context->guest_cpu_regs.longs[reg];
	} else if ((reg >= CPU_REG_NONGENERAL_FIRST) && (reg <= CPU_REG_NONGENERAL_LAST)) {
		uint32_t field = get_vmcs_field(reg);

		if (field != VMX_INVALID_VMCS_FIELD) {
			if (reg <= CPU_REG_NATURAL_LAST) {
				*retval = exec_vmread(field);
			} else if (reg <= CPU_REG_64BIT_LAST) {
				*retval = exec_vmread64(field);
			} else {
				*retval = (uint64_t)exec_vmread16(field);
			}
		} else {
			return -EINVAL;
		}
	}

	return 0;
}

int vm_set_register(struct vcpu *vcpu, enum cpu_reg_name reg, uint64_t val)
{
	struct run_context *cur_context;

	if (vcpu == NULL) {
		return -EINVAL;
	}

	if ((reg > CPU_REG_LAST) || (reg < CPU_REG_FIRST)) {
		return -EINVAL;
	}

	if ((reg >= CPU_REG_GENERAL_FIRST) && (reg <= CPU_REG_GENERAL_LAST)) {
		cur_context =
			&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
		cur_context->guest_cpu_regs.longs[reg] = val;
	} else if ((reg >= CPU_REG_NONGENERAL_FIRST) && (reg <= CPU_REG_NONGENERAL_LAST)) {
		uint32_t field = get_vmcs_field(reg);

		if (field != VMX_INVALID_VMCS_FIELD) {
			if (reg <= CPU_REG_NATURAL_LAST) {
				exec_vmwrite(field, val);
			} else if (reg <= CPU_REG_64BIT_LAST) {
				exec_vmwrite64(field, val);
			} else {
				exec_vmwrite16(field, (uint16_t)val);
			}
		} else {
			return -EINVAL;
		}
	}

	return 0;
}

int vm_set_seg_desc(struct vcpu *vcpu, enum cpu_reg_name seg,
		struct seg_desc *ret_desc)
{
	int error;
	uint32_t base, limit, access;

	if ((vcpu == NULL) || (ret_desc == NULL)) {
		return -EINVAL;
	}

	if (!is_segment_register(seg) && !is_descriptor_table(seg)) {
		return -EINVAL;
	}

	error = encode_vmcs_seg_desc(seg, &base, &limit, &access);
	if ((error != 0) || (access == 0xffffffffU)) {
		return -EINVAL;
	}

	exec_vmwrite(base, ret_desc->base);
	exec_vmwrite32(limit, ret_desc->limit);
	exec_vmwrite32(access, ret_desc->access);

	return 0;
}

int vm_get_seg_desc(struct vcpu *vcpu, enum cpu_reg_name seg,
		struct seg_desc *desc)
{
	int error;
	uint32_t base, limit, access;

	if ((vcpu == NULL) || (desc == NULL)) {
		return -EINVAL;
	}

	if (!is_segment_register(seg) && !is_descriptor_table(seg)) {
		return -EINVAL;
	}

	error = encode_vmcs_seg_desc(seg, &base, &limit, &access);
	if ((error != 0) || (access == 0xffffffffU)) {
		return -EINVAL;
	}

	desc->base = exec_vmread(base);
	desc->limit = exec_vmread32(limit);
	desc->access = exec_vmread32(access);

	return 0;
}

static bool is_descriptor_table(enum cpu_reg_name reg)
{
	switch (reg) {
	case CPU_REG_IDTR:
	case CPU_REG_GDTR:
		return true;
	default:
		return false;
	}
}

static bool is_segment_register(enum cpu_reg_name reg)
{
	switch (reg) {
	case CPU_REG_ES:
	case CPU_REG_CS:
	case CPU_REG_SS:
	case CPU_REG_DS:
	case CPU_REG_FS:
	case CPU_REG_GS:
	case CPU_REG_TR:
	case CPU_REG_LDTR:
		return true;
	default:
		return false;
	}
}

static int
encode_vmcs_seg_desc(enum cpu_reg_name seg,
		uint32_t *base, uint32_t *lim, uint32_t *acc)
{
	switch (seg) {
	case CPU_REG_ES:
		*base = VMX_GUEST_ES_BASE;
		*lim = VMX_GUEST_ES_LIMIT;
		*acc = VMX_GUEST_ES_ATTR;
		break;
	case CPU_REG_CS:
		*base = VMX_GUEST_CS_BASE;
		*lim = VMX_GUEST_CS_LIMIT;
		*acc = VMX_GUEST_CS_ATTR;
		break;
	case CPU_REG_SS:
		*base = VMX_GUEST_SS_BASE;
		*lim = VMX_GUEST_SS_LIMIT;
		*acc = VMX_GUEST_SS_ATTR;
		break;
	case CPU_REG_DS:
		*base = VMX_GUEST_DS_BASE;
		*lim = VMX_GUEST_DS_LIMIT;
		*acc = VMX_GUEST_DS_ATTR;
		break;
	case CPU_REG_FS:
		*base = VMX_GUEST_FS_BASE;
		*lim = VMX_GUEST_FS_LIMIT;
		*acc = VMX_GUEST_FS_ATTR;
		break;
	case CPU_REG_GS:
		*base = VMX_GUEST_GS_BASE;
		*lim = VMX_GUEST_GS_LIMIT;
		*acc = VMX_GUEST_GS_ATTR;
		break;
	case CPU_REG_TR:
		*base = VMX_GUEST_TR_BASE;
		*lim = VMX_GUEST_TR_LIMIT;
		*acc = VMX_GUEST_TR_ATTR;
		break;
	case CPU_REG_LDTR:
		*base = VMX_GUEST_LDTR_BASE;
		*lim = VMX_GUEST_LDTR_LIMIT;
		*acc = VMX_GUEST_LDTR_ATTR;
		break;
	case CPU_REG_IDTR:
		*base = VMX_GUEST_IDTR_BASE;
		*lim = VMX_GUEST_IDTR_LIMIT;
		*acc = 0xffffffffU;
		break;
	case CPU_REG_GDTR:
		*base = VMX_GUEST_GDTR_BASE;
		*lim = VMX_GUEST_GDTR_LIMIT;
		*acc = 0xffffffffU;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
/**
 *
 *Description:
 *This local function is to covert register names into
 *the corresponding field index MACROs in VMCS.
 *
 *Post Condition:
 *In the non-general register names group (CPU_REG_CR0~CPU_REG_GDTR),
 *for register names CPU_REG_CR2, CPU_REG_IDTR and CPU_REG_GDTR,
 *this function returns VMX_INVALID_VMCS_FIELD;
 *for other register names, it returns correspoding field index MACROs
 *in VMCS.
 *
 **/
static uint32_t get_vmcs_field(enum cpu_reg_name ident)
{
	switch (ident) {
	case CPU_REG_CR0:
		return VMX_GUEST_CR0;
	case CPU_REG_CR3:
		return VMX_GUEST_CR3;
	case CPU_REG_CR4:
		return VMX_GUEST_CR4;
	case CPU_REG_DR7:
		return VMX_GUEST_DR7;
	case CPU_REG_RSP:
		return VMX_GUEST_RSP;
	case CPU_REG_RIP:
		return VMX_GUEST_RIP;
	case CPU_REG_RFLAGS:
		return VMX_GUEST_RFLAGS;
	case CPU_REG_ES:
		return VMX_GUEST_ES_SEL;
	case CPU_REG_CS:
		return VMX_GUEST_CS_SEL;
	case CPU_REG_SS:
		return VMX_GUEST_SS_SEL;
	case CPU_REG_DS:
		return VMX_GUEST_DS_SEL;
	case CPU_REG_FS:
		return VMX_GUEST_FS_SEL;
	case CPU_REG_GS:
		return VMX_GUEST_GS_SEL;
	case CPU_REG_TR:
		return VMX_GUEST_TR_SEL;
	case CPU_REG_LDTR:
		return VMX_GUEST_LDTR_SEL;
	case CPU_REG_EFER:
		return VMX_GUEST_IA32_EFER_FULL;
	case CPU_REG_PDPTE0:
		return VMX_GUEST_PDPTE0_FULL;
	case CPU_REG_PDPTE1:
		return VMX_GUEST_PDPTE1_FULL;
	case CPU_REG_PDPTE2:
		return VMX_GUEST_PDPTE2_FULL;
	case CPU_REG_PDPTE3:
		return VMX_GUEST_PDPTE3_FULL;
	default:
		return VMX_INVALID_VMCS_FIELD;
	}
}

static void get_guest_paging_info(struct vcpu *vcpu, struct emul_ctxt *emul_ctxt,
						uint32_t csar)
{
	uint8_t cpl;

	ASSERT(emul_ctxt != NULL && vcpu != NULL, "Error in input arguments");

	cpl = (uint8_t)((csar >> 5) & 3U);
	emul_ctxt->paging.cr3 =
		vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].cr3;
	emul_ctxt->paging.cpl = cpl;
	emul_ctxt->paging.cpu_mode = get_vcpu_mode(vcpu);
	emul_ctxt->paging.paging_mode = get_vcpu_paging_mode(vcpu);
}

static int mmio_read(struct vcpu *vcpu, __unused uint64_t gpa, uint64_t *rval,
		__unused uint8_t size, __unused void *arg)
{
	if (vcpu == NULL) {
		return -EINVAL;
	}

	*rval = vcpu->req.reqs.mmio.value;
	return 0;
}

static int mmio_write(struct vcpu *vcpu, __unused uint64_t gpa, uint64_t wval,
		__unused uint8_t size, __unused void *arg)
{
	if (vcpu == NULL) {
		return -EINVAL;
	}

	vcpu->req.reqs.mmio.value = wval;
	return 0;
}

int decode_instruction(struct vcpu *vcpu)
{
	struct emul_ctxt *emul_ctxt;
	uint32_t csar;
	int retval = 0;
	enum vm_cpu_mode cpu_mode;

	emul_ctxt = &per_cpu(g_inst_ctxt, vcpu->pcpu_id);
	emul_ctxt->vcpu = vcpu;

	retval = vie_init(&emul_ctxt->vie, vcpu);
	if (retval < 0) {
		if (retval != -EFAULT) {
			pr_err("decode instruction failed @ 0x%016llx:",
				vcpu->arch_vcpu.
				contexts[vcpu->arch_vcpu.cur_context].rip);
		}
		return retval;
	}

	csar = exec_vmread32(VMX_GUEST_CS_ATTR);
	get_guest_paging_info(vcpu, emul_ctxt, csar);
	cpu_mode = get_vcpu_mode(vcpu);

	retval = __decode_instruction(cpu_mode, SEG_DESC_DEF32(csar),
		&emul_ctxt->vie);

	if (retval != 0) {
		pr_err("decode instruction failed @ 0x%016llx:",
		vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].rip);
		return -EINVAL;
	}

	return  emul_ctxt->vie.opsize;
}

int emulate_instruction(struct vcpu *vcpu)
{
	struct emul_ctxt *emul_ctxt;
	struct vm_guest_paging *paging;
	int retval = 0;
	uint64_t gpa = vcpu->req.reqs.mmio.address;
	mem_region_read_t mread = mmio_read;
	mem_region_write_t mwrite = mmio_write;

	emul_ctxt = &per_cpu(g_inst_ctxt, vcpu->pcpu_id);
	paging = &emul_ctxt->paging;

	retval = vmm_emulate_instruction(vcpu, gpa,
			&emul_ctxt->vie, paging, mread, mwrite, NULL);

	return retval;
}
