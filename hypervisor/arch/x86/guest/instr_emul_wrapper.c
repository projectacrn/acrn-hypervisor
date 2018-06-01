/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#include "instr_emul_wrapper.h"
#include "instr_emul.h"

struct emul_cnx {
	struct vie vie;
	struct vm_guest_paging paging;
	struct vcpu *vcpu;
};

static DEFINE_CPU_DATA(struct emul_cnx, g_inst_ctxt);

static int
encode_vmcs_seg_desc(int seg, uint32_t *base, uint32_t *lim, uint32_t *acc);

static int32_t
get_vmcs_field(int ident);

static bool
is_segment_register(int reg);

static bool
is_descriptor_table(int reg);

int vm_get_register(struct vcpu *vcpu, int reg, uint64_t *retval)
{
	struct run_context *cur_context;

	if (!vcpu)
		return -EINVAL;
	if ((reg >= VM_REG_LAST) || (reg < VM_REG_GUEST_RAX))
		return -EINVAL;

	if ((reg >= VM_REG_GUEST_RAX) && (reg <= VM_REG_GUEST_RDI)) {
		cur_context =
			&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
		*retval = cur_context->guest_cpu_regs.longs[reg];
	} else if ((reg > VM_REG_GUEST_RDI) && (reg < VM_REG_LAST)) {
		int32_t field = get_vmcs_field(reg);

		if (field != -1)
			*retval = exec_vmread(field);
		else
			return -EINVAL;
	}

	return 0;
}

int vm_set_register(struct vcpu *vcpu, int reg, uint64_t val)
{
	struct run_context *cur_context;

	if (!vcpu)
		return -EINVAL;
	if ((reg >= VM_REG_LAST) || (reg < VM_REG_GUEST_RAX))
		return -EINVAL;

	if ((reg >= VM_REG_GUEST_RAX) && (reg <= VM_REG_GUEST_RDI)) {
		cur_context =
			&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
		cur_context->guest_cpu_regs.longs[reg] = val;
	} else if ((reg > VM_REG_GUEST_RDI) && (reg < VM_REG_LAST)) {
		int32_t field = get_vmcs_field(reg);

		if (field != -1)
			exec_vmwrite(field, val);
		else
			return -EINVAL;
	}

	return 0;
}

int vm_set_seg_desc(struct vcpu *vcpu, int seg, struct seg_desc *ret_desc)
{
	int error;
	uint32_t base, limit, access;

	if ((!vcpu) || (!ret_desc))
		return -EINVAL;

	if (!is_segment_register(seg) && !is_descriptor_table(seg))
		return -EINVAL;

	error = encode_vmcs_seg_desc(seg, &base, &limit, &access);
	if ((error != 0) || (access == 0xffffffff))
		return -EINVAL;

	exec_vmwrite(base, ret_desc->base);
	exec_vmwrite(limit, ret_desc->limit);
	exec_vmwrite(access, ret_desc->access);

	return 0;
}

int vm_get_seg_desc(struct vcpu *vcpu, int seg, struct seg_desc *desc)
{
	int error;
	uint32_t base, limit, access;

	if ((!vcpu) || (!desc))
		return -EINVAL;

	if (!is_segment_register(seg) && !is_descriptor_table(seg))
		return -EINVAL;

	error = encode_vmcs_seg_desc(seg, &base, &limit, &access);
	if ((error != 0) || (access == 0xffffffff))
		return -EINVAL;

	desc->base = exec_vmread(base);
	desc->limit = exec_vmread(limit);
	desc->access = exec_vmread(access);

	return 0;
}

int vm_restart_instruction(struct vcpu *vcpu)
{
	if (!vcpu)
		return -EINVAL;

	VCPU_RETAIN_RIP(vcpu);
	return 0;
}

static bool is_descriptor_table(int reg)
{
	switch (reg) {
	case VM_REG_GUEST_IDTR:
	case VM_REG_GUEST_GDTR:
		return true;
	default:
		return false;
	}
}

static bool is_segment_register(int reg)
{
	switch (reg) {
	case VM_REG_GUEST_ES:
	case VM_REG_GUEST_CS:
	case VM_REG_GUEST_SS:
	case VM_REG_GUEST_DS:
	case VM_REG_GUEST_FS:
	case VM_REG_GUEST_GS:
	case VM_REG_GUEST_TR:
	case VM_REG_GUEST_LDTR:
		return true;
	default:
		return false;
	}
}

static int encode_vmcs_seg_desc(int seg, uint32_t *base, uint32_t *lim,
		uint32_t *acc)
{
	switch (seg) {
	case VM_REG_GUEST_ES:
		*base = VMX_GUEST_ES_BASE;
		*lim = VMX_GUEST_ES_LIMIT;
		*acc = VMX_GUEST_ES_ATTR;
		break;
	case VM_REG_GUEST_CS:
		*base = VMX_GUEST_CS_BASE;
		*lim = VMX_GUEST_CS_LIMIT;
		*acc = VMX_GUEST_CS_ATTR;
		break;
	case VM_REG_GUEST_SS:
		*base = VMX_GUEST_SS_BASE;
		*lim = VMX_GUEST_SS_LIMIT;
		*acc = VMX_GUEST_SS_ATTR;
		break;
	case VM_REG_GUEST_DS:
		*base = VMX_GUEST_DS_BASE;
		*lim = VMX_GUEST_DS_LIMIT;
		*acc = VMX_GUEST_DS_ATTR;
		break;
	case VM_REG_GUEST_FS:
		*base = VMX_GUEST_FS_BASE;
		*lim = VMX_GUEST_FS_LIMIT;
		*acc = VMX_GUEST_FS_ATTR;
		break;
	case VM_REG_GUEST_GS:
		*base = VMX_GUEST_GS_BASE;
		*lim = VMX_GUEST_GS_LIMIT;
		*acc = VMX_GUEST_GS_ATTR;
		break;
	case VM_REG_GUEST_TR:
		*base = VMX_GUEST_TR_BASE;
		*lim = VMX_GUEST_TR_LIMIT;
		*acc = VMX_GUEST_TR_ATTR;
		break;
	case VM_REG_GUEST_LDTR:
		*base = VMX_GUEST_LDTR_BASE;
		*lim = VMX_GUEST_LDTR_LIMIT;
		*acc = VMX_GUEST_LDTR_ATTR;
		break;
	case VM_REG_GUEST_IDTR:
		*base = VMX_GUEST_IDTR_BASE;
		*lim = VMX_GUEST_IDTR_LIMIT;
		*acc = 0xffffffff;
		break;
	case VM_REG_GUEST_GDTR:
		*base = VMX_GUEST_GDTR_BASE;
		*lim = VMX_GUEST_GDTR_LIMIT;
		*acc = 0xffffffff;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int32_t get_vmcs_field(int ident)
{
	switch (ident) {
	case VM_REG_GUEST_CR0:
		return VMX_GUEST_CR0;
	case VM_REG_GUEST_CR3:
		return VMX_GUEST_CR3;
	case VM_REG_GUEST_CR4:
		return VMX_GUEST_CR4;
	case VM_REG_GUEST_DR7:
		return VMX_GUEST_DR7;
	case VM_REG_GUEST_RSP:
		return VMX_GUEST_RSP;
	case VM_REG_GUEST_RIP:
		return VMX_GUEST_RIP;
	case VM_REG_GUEST_RFLAGS:
		return VMX_GUEST_RFLAGS;
	case VM_REG_GUEST_ES:
		return VMX_GUEST_ES_SEL;
	case VM_REG_GUEST_CS:
		return VMX_GUEST_CS_SEL;
	case VM_REG_GUEST_SS:
		return VMX_GUEST_SS_SEL;
	case VM_REG_GUEST_DS:
		return VMX_GUEST_DS_SEL;
	case VM_REG_GUEST_FS:
		return VMX_GUEST_FS_SEL;
	case VM_REG_GUEST_GS:
		return VMX_GUEST_GS_SEL;
	case VM_REG_GUEST_TR:
		return VMX_GUEST_TR_SEL;
	case VM_REG_GUEST_LDTR:
		return VMX_GUEST_LDTR_SEL;
	case VM_REG_GUEST_EFER:
		return VMX_GUEST_IA32_EFER_FULL;
	case VM_REG_GUEST_PDPTE0:
		return VMX_GUEST_PDPTE0_FULL;
	case VM_REG_GUEST_PDPTE1:
		return VMX_GUEST_PDPTE1_FULL;
	case VM_REG_GUEST_PDPTE2:
		return VMX_GUEST_PDPTE2_FULL;
	case VM_REG_GUEST_PDPTE3:
		return VMX_GUEST_PDPTE3_FULL;
	default:
		return -1;
	}
}

static void get_guest_paging_info(struct vcpu *vcpu, struct emul_cnx *emul_cnx)
{
	uint32_t cpl, csar;

	ASSERT(emul_cnx != NULL && vcpu != NULL, "Error in input arguments");

	csar = exec_vmread(VMX_GUEST_CS_ATTR);
	cpl = (csar >> 5) & 3;
	emul_cnx->paging.cr3 =
		vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].cr3;
	emul_cnx->paging.cpl = cpl;
	emul_cnx->paging.cpu_mode = get_vcpu_mode(vcpu);
	emul_cnx->paging.paging_mode = get_vcpu_paging_mode(vcpu);
}

static int mmio_read(struct vcpu *vcpu, __unused uint64_t gpa, uint64_t *rval,
		__unused int size, __unused void *arg)
{
	if (!vcpu)
		return -EINVAL;

	*rval = vcpu->mmio.value;
	return 0;
}

static int mmio_write(struct vcpu *vcpu, __unused uint64_t gpa, uint64_t wval,
		__unused int size, __unused void *arg)
{
	if (!vcpu)
		return -EINVAL;

	vcpu->mmio.value = wval;
	return 0;
}

int vm_gva2gpa(struct vcpu *vcpu, uint64_t gva, uint64_t *gpa,
	uint32_t *err_code)
{

	ASSERT(gpa != NULL, "Error in input arguments");
	ASSERT(vcpu != NULL,
		"Invalid vcpu id when gva2gpa");

	return gva2gpa(vcpu, gva, gpa, err_code);
}

uint8_t decode_instruction(struct vcpu *vcpu)
{
	uint64_t guest_rip_gva, guest_rip_gpa;
	char *guest_rip_hva;
	struct emul_cnx *emul_cnx;
	uint32_t csar;
	int retval = 0;
	enum vm_cpu_mode cpu_mode;
	int error;
	uint32_t err_code;

	guest_rip_gva =
		vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].rip;

	err_code = PAGE_FAULT_ID_FLAG;
	error = gva2gpa(vcpu, guest_rip_gva, &guest_rip_gpa, &err_code);
	if (error) {
		pr_err("gva2gpa failed for guest_rip_gva  0x%016llx:",
			guest_rip_gva);
		return 0;
	}

	guest_rip_hva = GPA2HVA(vcpu->vm, guest_rip_gpa);
	emul_cnx = &per_cpu(g_inst_ctxt, vcpu->pcpu_id);
	emul_cnx->vcpu = vcpu;

	/* by now, HVA <-> HPA is 1:1 mapping, so use hpa is OK*/
	vie_init(&emul_cnx->vie, guest_rip_hva,
		vcpu->arch_vcpu.inst_len);

	get_guest_paging_info(vcpu, emul_cnx);
	csar = exec_vmread(VMX_GUEST_CS_ATTR);
	cpu_mode = get_vcpu_mode(vcpu);

	retval = __decode_instruction(vcpu, guest_rip_gva,
			cpu_mode, SEG_DESC_DEF32(csar), &emul_cnx->vie);

	if (retval != 0) {
		pr_err("decode instruction failed @ 0x%016llx:",
		exec_vmread(VMX_GUEST_RIP));
		return 0;
	}

	return  emul_cnx->vie.opsize;
}

int emulate_instruction(struct vcpu *vcpu)
{
	struct emul_cnx *emul_cnx;
	struct vm_guest_paging *paging;
	int retval = 0;
	uint64_t gpa = vcpu->mmio.paddr;
	mem_region_read_t mread = mmio_read;
	mem_region_write_t mwrite = mmio_write;

	emul_cnx = &per_cpu(g_inst_ctxt, vcpu->pcpu_id);
	paging = &emul_cnx->paging;

	retval = vmm_emulate_instruction(vcpu, gpa,
			&emul_cnx->vie, paging, mread, mwrite, &retval);

	return retval;
}
