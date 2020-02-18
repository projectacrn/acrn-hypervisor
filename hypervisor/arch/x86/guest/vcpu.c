/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <vcpu.h>
#include <bits.h>
#include <vmx.h>
#include <logmsg.h>
#include <cpu_caps.h>
#include <per_cpu.h>
#include <init.h>
#include <vm.h>
#include <vmcs.h>
#include <mmu.h>
#include <sprintf.h>

/* stack_frame is linked with the sequence of stack operation in arch_switch_to() */
struct stack_frame {
	uint64_t rdi;
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t rbp;
	uint64_t rbx;
	uint64_t rflag;
	uint64_t rip;
	uint64_t magic;
};

uint64_t vcpu_get_gpreg(const struct acrn_vcpu *vcpu, uint32_t reg)
{
	const struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	return ctx->cpu_regs.longs[reg];
}

void vcpu_set_gpreg(struct acrn_vcpu *vcpu, uint32_t reg, uint64_t val)
{
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	ctx->cpu_regs.longs[reg] = val;
}

uint64_t vcpu_get_rip(struct acrn_vcpu *vcpu)
{
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	if (!bitmap_test(CPU_REG_RIP, &vcpu->reg_updated) &&
		!bitmap_test_and_set_lock(CPU_REG_RIP, &vcpu->reg_cached)) {
		ctx->rip = exec_vmread(VMX_GUEST_RIP);
	}
	return ctx->rip;
}

void vcpu_set_rip(struct acrn_vcpu *vcpu, uint64_t val)
{
	vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx.rip = val;
	bitmap_set_lock(CPU_REG_RIP, &vcpu->reg_updated);
}

uint64_t vcpu_get_rsp(const struct acrn_vcpu *vcpu)
{
	const struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	return ctx->cpu_regs.regs.rsp;
}

void vcpu_set_rsp(struct acrn_vcpu *vcpu, uint64_t val)
{
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	ctx->cpu_regs.regs.rsp = val;
	bitmap_set_lock(CPU_REG_RSP, &vcpu->reg_updated);
}

uint64_t vcpu_get_efer(struct acrn_vcpu *vcpu)
{
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	if (!bitmap_test(CPU_REG_EFER, &vcpu->reg_updated) &&
		!bitmap_test_and_set_lock(CPU_REG_EFER, &vcpu->reg_cached)) {
		ctx->ia32_efer = exec_vmread64(VMX_GUEST_IA32_EFER_FULL);
	}
	return ctx->ia32_efer;
}

void vcpu_set_efer(struct acrn_vcpu *vcpu, uint64_t val)
{
	vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx.ia32_efer
		= val;
	bitmap_set_lock(CPU_REG_EFER, &vcpu->reg_updated);
}

uint64_t vcpu_get_rflags(struct acrn_vcpu *vcpu)
{
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	if (!bitmap_test(CPU_REG_RFLAGS, &vcpu->reg_updated) &&
		!bitmap_test_and_set_lock(CPU_REG_RFLAGS,
			&vcpu->reg_cached) && vcpu->launched) {
		ctx->rflags = exec_vmread(VMX_GUEST_RFLAGS);
	}
	return ctx->rflags;
}

void vcpu_set_rflags(struct acrn_vcpu *vcpu, uint64_t val)
{
	vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx.rflags =
		val;
	bitmap_set_lock(CPU_REG_RFLAGS, &vcpu->reg_updated);
}

uint64_t vcpu_get_guest_msr(const struct acrn_vcpu *vcpu, uint32_t msr)
{
	uint32_t index = vmsr_get_guest_msr_index(msr);
	uint64_t val = 0UL;

	if (index < NUM_GUEST_MSRS) {
		val = vcpu->arch.guest_msrs[index];
	}

	return val;
}

void vcpu_set_guest_msr(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t val)
{
	uint32_t index = vmsr_get_guest_msr_index(msr);

	if (index < NUM_GUEST_MSRS) {
		vcpu->arch.guest_msrs[index] = val;
	}
}

/*
 * Write the eoi_exit_bitmaps to VMCS fields
 */
void vcpu_set_vmcs_eoi_exit(const struct acrn_vcpu *vcpu)
{
	pr_dbg("%s", __func__);

	if (is_apicv_advanced_feature_supported()) {
		exec_vmwrite64(VMX_EOI_EXIT0_FULL, vcpu->arch.eoi_exit_bitmap[0]);
		exec_vmwrite64(VMX_EOI_EXIT1_FULL, vcpu->arch.eoi_exit_bitmap[1]);
		exec_vmwrite64(VMX_EOI_EXIT2_FULL, vcpu->arch.eoi_exit_bitmap[2]);
		exec_vmwrite64(VMX_EOI_EXIT3_FULL, vcpu->arch.eoi_exit_bitmap[3]);
	}
}

/*
 * Set the eoi_exit_bitmap bit for specific vector
 * @pre vcpu != NULL && vector <= 255U
 */
void vcpu_set_eoi_exit_bitmap(struct acrn_vcpu *vcpu, uint32_t vector)
{
	pr_dbg("%s", __func__);

	if (!bitmap_test_and_set_lock((uint16_t)(vector & 0x3fU),
			&(vcpu->arch.eoi_exit_bitmap[(vector & 0xffU) >> 6U]))) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EOI_EXIT_BITMAP_UPDATE);
	}
}

void vcpu_clear_eoi_exit_bitmap(struct acrn_vcpu *vcpu, uint32_t vector)
{
	pr_dbg("%s", __func__);

	if (bitmap_test_and_clear_lock((uint16_t)(vector & 0x3fU),
			&(vcpu->arch.eoi_exit_bitmap[(vector & 0xffU) >> 6U]))) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EOI_EXIT_BITMAP_UPDATE);
	}
}

/*
 * Reset all eoi_exit_bitmaps
 */
void vcpu_reset_eoi_exit_bitmaps(struct acrn_vcpu *vcpu)
{
	pr_dbg("%s", __func__);

	(void)memset((void *)(vcpu->arch.eoi_exit_bitmap), 0U, sizeof(vcpu->arch.eoi_exit_bitmap));
	vcpu_make_request(vcpu, ACRN_REQUEST_EOI_EXIT_BITMAP_UPDATE);
}

/* As a vcpu reset internal API, DO NOT touch any vcpu state transition in this function. */
static void vcpu_reset_internal(struct acrn_vcpu *vcpu, enum reset_mode mode)
{
	int32_t i;
	struct acrn_vlapic *vlapic;

	vcpu->launched = false;
	vcpu->running = false;
	vcpu->arch.nr_sipi = 0U;

	vcpu->arch.exception_info.exception = VECTOR_INVALID;
	vcpu->arch.cur_context = NORMAL_WORLD;
	vcpu->arch.irq_window_enabled = false;
	(void)memset((void *)vcpu->arch.vmcs, 0U, PAGE_SIZE);

	for (i = 0; i < NR_WORLD; i++) {
		(void)memset((void *)(&vcpu->arch.contexts[i]), 0U,
			sizeof(struct run_context));
	}

	/* TODO: we may need to add one scheduler->reset_data to reset the thread_obj */
	vcpu->thread_obj.notify_mode = SCHED_NOTIFY_IPI;

	vlapic = vcpu_vlapic(vcpu);
	vlapic_reset(vlapic, apicv_ops, mode);

	reset_vcpu_regs(vcpu);

	for (i = 0; i < VCPU_EVENT_NUM; i++) {
		reset_event(&vcpu->events[i]);
	}
}

struct acrn_vcpu *get_running_vcpu(uint16_t pcpu_id)
{
	struct thread_object *curr = sched_get_current(pcpu_id);
	struct acrn_vcpu *vcpu = NULL;

	if ((curr != NULL) && (!is_idle_thread(curr))) {
		vcpu = list_entry(curr, struct acrn_vcpu, thread_obj);
	}

	return vcpu;
}

struct acrn_vcpu *get_ever_run_vcpu(uint16_t pcpu_id)
{
	return per_cpu(ever_run_vcpu, pcpu_id);
}

static void set_vcpu_mode(struct acrn_vcpu *vcpu, uint32_t cs_attr, uint64_t ia32_efer,
		uint64_t cr0)
{
	if ((ia32_efer & MSR_IA32_EFER_LMA_BIT) != 0UL) {
		if ((cs_attr & 0x2000U) != 0U) {
			/* CS.L = 1 */
			vcpu->arch.cpu_mode = CPU_MODE_64BIT;
		} else {
			vcpu->arch.cpu_mode = CPU_MODE_COMPATIBILITY;
		}
	} else if ((cr0 & CR0_PE) != 0UL) {
		vcpu->arch.cpu_mode = CPU_MODE_PROTECTED;
	} else {
		vcpu->arch.cpu_mode = CPU_MODE_REAL;
	}
}

static void init_xsave(struct acrn_vcpu *vcpu)
{
	struct cpuinfo_x86 *cpu_info = get_pcpu_info();
	struct ext_context *ectx = &(vcpu->arch.contexts[vcpu->arch.cur_context].ext_ctx);

	/* Get user state components */
	ectx->xcr0 = ((uint64_t)cpu_info->cpuid_leaves[FEAT_D_0_EDX] << 32U)
		+ cpu_info->cpuid_leaves[FEAT_D_0_EAX];

	/* Get supervisor state components */
	ectx->xss = ((uint64_t)cpu_info->cpuid_leaves[FEAT_D_1_EDX] << 32U)
		+ cpu_info->cpuid_leaves[FEAT_D_1_ECX];

	/* xsaves only support compacted format, so set it in xcomp_bv[63],
	 * keep the reset area in header area as zero.
	 * With this config, the first time a vcpu is scheduled in, it will
	 * initiate all the xsave componets */
	ectx->xs_area.xsave_hdr.hdr.xcomp_bv |= XSAVE_COMPACTED_FORMAT;

}
void set_vcpu_regs(struct acrn_vcpu *vcpu, struct acrn_vcpu_regs *vcpu_regs)
{
	struct ext_context *ectx;
	struct run_context *ctx;
	uint16_t *sel = &(vcpu_regs->cs_sel);
	struct segment_sel *seg;
	uint32_t limit, attr;

	ectx = &(vcpu->arch.contexts[vcpu->arch.cur_context].ext_ctx);
	ctx = &(vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx);

	/* NOTE:
	 * This is to set the attr and limit to default value.
	 * If the set_vcpu_regs is used not only for vcpu state
	 * initialization, this part of code needs be revised.
	 */
	if ((vcpu_regs->cr0 & CR0_PE) != 0UL) {
		attr = PROTECTED_MODE_DATA_SEG_AR;
		limit = PROTECTED_MODE_SEG_LIMIT;
	} else {
		attr = REAL_MODE_DATA_SEG_AR;
		limit = REAL_MODE_SEG_LIMIT;
	}

	for (seg = &(ectx->cs); seg <= &(ectx->gs); seg++) {
		seg->base     = 0UL;
		seg->limit    = limit;
		seg->attr     = attr;
		seg->selector = *sel;
		sel++;
	}

	/* override cs attr/base/limit */
	ectx->cs.attr = vcpu_regs->cs_ar;
	ectx->cs.base = vcpu_regs->cs_base;
	ectx->cs.limit = vcpu_regs->cs_limit;

	ectx->gdtr.base = vcpu_regs->gdt.base;
	ectx->gdtr.limit = vcpu_regs->gdt.limit;

	ectx->idtr.base = vcpu_regs->idt.base;
	ectx->idtr.limit = vcpu_regs->idt.limit;

	ectx->ldtr.selector = vcpu_regs->ldt_sel;
	ectx->tr.selector = vcpu_regs->tr_sel;

	/* NOTE:
	 * This is to set the ldtr and tr to default value.
	 * If the set_vcpu_regs is used not only for vcpu state
	 * initialization, this part of code needs be revised.
	 */
	ectx->ldtr.base = 0UL;
	ectx->tr.base = 0UL;
	ectx->ldtr.limit = 0xFFFFU;
	ectx->tr.limit = 0xFFFFU;
	ectx->ldtr.attr = LDTR_AR;
	ectx->tr.attr = TR_AR;

	(void)memcpy_s((void *)&(ctx->cpu_regs), sizeof(struct acrn_gp_regs),
			(void *)&(vcpu_regs->gprs), sizeof(struct acrn_gp_regs));

	vcpu_set_rip(vcpu, vcpu_regs->rip);
	vcpu_set_efer(vcpu, vcpu_regs->ia32_efer);
	vcpu_set_rsp(vcpu, vcpu_regs->gprs.rsp);

	if (vcpu_regs->rflags == 0UL) {
		vcpu_set_rflags(vcpu, 0x02UL);
	} else {
		vcpu_set_rflags(vcpu, vcpu_regs->rflags & ~(0x8d5UL));
	}

	/* cr0, cr3 and cr4 needs be set without using API.
	 * The real cr0/cr3/cr4 writing will be delayed to
	 * init_vmcs
	 */
	ctx->cr0 = vcpu_regs->cr0;
	ectx->cr3 = vcpu_regs->cr3;
	ctx->cr4 = vcpu_regs->cr4;

	set_vcpu_mode(vcpu, vcpu_regs->cs_ar, vcpu_regs->ia32_efer,
			vcpu_regs->cr0);
}

static struct acrn_vcpu_regs realmode_init_vregs = {
	.gdt = {
		.limit = 0xFFFFU,
		.base = 0UL,
	},
	.idt = {
		.limit = 0xFFFFU,
		.base = 0UL,
	},
	.cs_ar = REAL_MODE_CODE_SEG_AR,
	.cs_sel = REAL_MODE_BSP_INIT_CODE_SEL,
	.cs_base = 0xFFFF0000UL,
	.cs_limit = 0xFFFFU,
	.rip = 0xFFF0UL,
	.cr0 = CR0_ET | CR0_NE,
	.cr3 = 0UL,
	.cr4 = 0UL,
};

static uint64_t init_vgdt[] = {
	0x0UL,
	0x0UL,
	0x00CF9B000000FFFFUL,   /* Linear Code */
	0x00CF93000000FFFFUL,   /* Linear Data */
};

static struct acrn_vcpu_regs protect_mode_init_vregs = {
	.cs_ar = PROTECTED_MODE_CODE_SEG_AR,
	.cs_limit = PROTECTED_MODE_SEG_LIMIT,
	.cs_sel = 0x10U,
	.cr0 = CR0_ET | CR0_NE | CR0_PE,
	.ds_sel = 0x18U,
	.ss_sel = 0x18U,
	.es_sel = 0x18U,
};

void reset_vcpu_regs(struct acrn_vcpu *vcpu)
{
	set_vcpu_regs(vcpu, &realmode_init_vregs);
}

void init_vcpu_protect_mode_regs(struct acrn_vcpu *vcpu, uint64_t vgdt_base_gpa)
{
	struct acrn_vcpu_regs vcpu_regs;

	(void)memcpy_s((void*)&vcpu_regs, sizeof(struct acrn_vcpu_regs),
		(void *)&protect_mode_init_vregs, sizeof(struct acrn_vcpu_regs));

	vcpu_regs.gdt.base = vgdt_base_gpa;
	vcpu_regs.gdt.limit = sizeof(init_vgdt) - 1U;
	(void)copy_to_gpa(vcpu->vm, &init_vgdt, vgdt_base_gpa, sizeof(init_vgdt));

	set_vcpu_regs(vcpu, &vcpu_regs);
}

void set_vcpu_startup_entry(struct acrn_vcpu *vcpu, uint64_t entry)
{
	struct ext_context *ectx;

	ectx = &(vcpu->arch.contexts[vcpu->arch.cur_context].ext_ctx);
	ectx->cs.selector = (uint16_t)((entry >> 4U) & 0xFFFFU);
	ectx->cs.base = ectx->cs.selector << 4U;

	vcpu_set_rip(vcpu, 0UL);
}

/*
 *  @pre vm != NULL && rtn_vcpu_handle != NULL
 */
int32_t create_vcpu(uint16_t pcpu_id, struct acrn_vm *vm, struct acrn_vcpu **rtn_vcpu_handle)
{
	struct acrn_vcpu *vcpu;
	uint16_t vcpu_id;
	int32_t ret;

	pr_info("Creating VCPU working on PCPU%hu", pcpu_id);

	/*
	 * vcpu->vcpu_id = vm->hw.created_vcpus;
	 * vm->hw.created_vcpus++;
	 */
	vcpu_id = vm->hw.created_vcpus;
	if (vcpu_id < MAX_VCPUS_PER_VM) {
		/* Allocate memory for VCPU */
		vcpu = &(vm->hw.vcpu_array[vcpu_id]);
		(void)memset((void *)vcpu, 0U, sizeof(struct acrn_vcpu));

		/* Initialize CPU ID for this VCPU */
		vcpu->vcpu_id = vcpu_id;
		per_cpu(ever_run_vcpu, pcpu_id) = vcpu;

		/* Initialize the parent VM reference */
		vcpu->vm = vm;

		/* Initialize the virtual ID for this VCPU */
		/* FIXME:
		 * We have assumption that we always destroys vcpus in one
		 * shot (like when vm is destroyed). If we need to support
		 * specific vcpu destroy on fly, this vcpu_id assignment
		 * needs revise.
		 */

		pr_info("Create VM%d-VCPU%d, Role: %s",
				vcpu->vm->vm_id, vcpu->vcpu_id,
				is_vcpu_bsp(vcpu) ? "PRIMARY" : "SECONDARY");

		/*
		 * If the logical processor is in VMX non-root operation and
		 * the "enable VPID" VM-execution control is 1, the current VPID
		 * is the value of the VPID VM-execution control field in the VMCS.
		 *
		 * This assignment guarantees a unique non-zero per vcpu vpid in runtime.
		 */
		vcpu->arch.vpid = 1U + (vm->vm_id * MAX_VCPUS_PER_VM) + vcpu->vcpu_id;

		/* Create per vcpu vlapic */
		vlapic_create(vcpu);

		if (!vm_hide_mtrr(vm)) {
			init_vmtrr(vcpu);
		}

		/* Populate the return handle */
		*rtn_vcpu_handle = vcpu;
		vcpu->state = VCPU_INIT;

		init_xsave(vcpu);
		vcpu_reset_internal(vcpu, POWER_ON_RESET);
		(void)memset((void *)&vcpu->req, 0U, sizeof(struct io_request));
		vm->hw.created_vcpus++;
		ret = 0;
	} else {
		pr_err("%s, vcpu id is invalid!\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

/*
 *  @pre vcpu != NULL
 */
int32_t run_vcpu(struct acrn_vcpu *vcpu)
{
	uint32_t instlen, cs_attr;
	uint64_t rip, ia32_efer, cr0;
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;
	int32_t status = 0;
	int32_t ibrs_type = get_ibrs_type();

	if (bitmap_test_and_clear_lock(CPU_REG_RIP, &vcpu->reg_updated)) {
		exec_vmwrite(VMX_GUEST_RIP, ctx->rip);
	}
	if (bitmap_test_and_clear_lock(CPU_REG_RSP, &vcpu->reg_updated)) {
		exec_vmwrite(VMX_GUEST_RSP, ctx->cpu_regs.regs.rsp);
	}
	if (bitmap_test_and_clear_lock(CPU_REG_EFER, &vcpu->reg_updated)) {
		exec_vmwrite64(VMX_GUEST_IA32_EFER_FULL, ctx->ia32_efer);
	}
	if (bitmap_test_and_clear_lock(CPU_REG_RFLAGS, &vcpu->reg_updated)) {
		exec_vmwrite(VMX_GUEST_RFLAGS, ctx->rflags);
	}

	/*
	 * Currently, updating CR0/CR4 here is only designed for world
	 * switching. There should no other module request updating
	 * CR0/CR4 here.
	 */
	if (bitmap_test_and_clear_lock(CPU_REG_CR0, &vcpu->reg_updated)) {
		vcpu_set_cr0(vcpu, ctx->cr0);
	}

	if (bitmap_test_and_clear_lock(CPU_REG_CR4, &vcpu->reg_updated)) {
		vcpu_set_cr4(vcpu, ctx->cr4);
	}

	/* If this VCPU is not already launched, launch it */
	if (!vcpu->launched) {
		pr_info("VM %d Starting VCPU %hu",
				vcpu->vm->vm_id, vcpu->vcpu_id);

		if (vcpu->arch.vpid != 0U) {
			exec_vmwrite16(VMX_VPID, vcpu->arch.vpid);
		}

		/*
		 * A power-up or a reset invalidates all linear mappings,
		 * guest-physical mappings, and combined mappings
		 */
		flush_vpid_global();

		/* Set vcpu launched */
		vcpu->launched = true;

		/* avoid VMCS recycling RSB usage, set IBPB.
		 * NOTE: this should be done for any time vmcs got switch
		 * currently, there is no other place to do vmcs switch
		 * Please add IBPB set for future vmcs switch case(like trusty)
		 */
		if (ibrs_type == IBRS_RAW) {
			msr_write(MSR_IA32_PRED_CMD, PRED_SET_IBPB);
		}

#ifdef CONFIG_L1D_FLUSH_VMENTRY_ENABLED
		cpu_l1d_flush();
#endif

		/*Mitigation for MDS vulnerability, overwrite CPU internal buffers */
		cpu_internal_buffers_clear();

		/* Launch the VM */
		status = vmx_vmrun(ctx, VM_LAUNCH, ibrs_type);

		/* See if VM launched successfully */
		if (status == 0) {
			if (is_vcpu_bsp(vcpu)) {
				pr_info("VM %d VCPU %hu successfully launched",
					vcpu->vm->vm_id, vcpu->vcpu_id);
			}
		}
	} else {
		/* This VCPU was already launched, check if the last guest
		 * instruction needs to be repeated and resume VCPU accordingly
		 */
		instlen = vcpu->arch.inst_len;
		rip = vcpu_get_rip(vcpu);
		exec_vmwrite(VMX_GUEST_RIP, ((rip+(uint64_t)instlen) &
				0xFFFFFFFFFFFFFFFFUL));
#ifdef CONFIG_L1D_FLUSH_VMENTRY_ENABLED
		cpu_l1d_flush();
#endif

		/* Mitigation for MDS vulnerability, overwrite CPU internal buffers */
		cpu_internal_buffers_clear();

		/* Resume the VM */
		status = vmx_vmrun(ctx, VM_RESUME, ibrs_type);
	}

	vcpu->reg_cached = 0UL;

	cs_attr = exec_vmread32(VMX_GUEST_CS_ATTR);
	ia32_efer = vcpu_get_efer(vcpu);
	cr0 = vcpu_get_cr0(vcpu);
	set_vcpu_mode(vcpu, cs_attr, ia32_efer, cr0);

	/* Obtain current VCPU instruction length */
	vcpu->arch.inst_len = exec_vmread32(VMX_EXIT_INSTR_LEN);

	ctx->cpu_regs.regs.rsp = exec_vmread(VMX_GUEST_RSP);

	/* Obtain VM exit reason */
	vcpu->arch.exit_reason = exec_vmread32(VMX_EXIT_REASON);

	if (status != 0) {
		/* refer to 64-ia32 spec section 24.9.1 volume#3 */
		if ((vcpu->arch.exit_reason & VMX_VMENTRY_FAIL) != 0U) {
			pr_fatal("vmentry fail reason=%lx", vcpu->arch.exit_reason);
		} else {
			pr_fatal("vmexit fail err_inst=%x", exec_vmread32(VMX_INSTR_ERROR));
		}

		ASSERT(status == 0, "vm fail");
	}

	return status;
}

/*
 *  @pre vcpu != NULL
 */
void offline_vcpu(struct acrn_vcpu *vcpu)
{
	if (vcpu->state == VCPU_ZOMBIE) {
		vlapic_free(vcpu);
		per_cpu(ever_run_vcpu, pcpuid_from_vcpu(vcpu)) = NULL;
		vcpu->state = VCPU_OFFLINE;
	}
}

void kick_vcpu(const struct acrn_vcpu *vcpu)
{
	kick_thread(&vcpu->thread_obj);
}

/*
* @pre (&vcpu->stack[CONFIG_STACK_SIZE] & (CPU_STACK_ALIGN - 1UL)) == 0
*/
static uint64_t build_stack_frame(struct acrn_vcpu *vcpu)
{
	uint64_t stacktop = (uint64_t)&vcpu->stack[CONFIG_STACK_SIZE];
	struct stack_frame *frame;
	uint64_t *ret;

	frame = (struct stack_frame *)stacktop;
	frame -= 1;

	frame->magic = SP_BOTTOM_MAGIC;
	frame->rip = (uint64_t)vcpu->thread_obj.thread_entry; /*return address*/
	frame->rflag = 0UL;
	frame->rbx = 0UL;
	frame->rbp = 0UL;
	frame->r12 = 0UL;
	frame->r13 = 0UL;
	frame->r14 = 0UL;
	frame->r15 = 0UL;
	frame->rdi = (uint64_t)&vcpu->thread_obj;

	ret = &frame->rdi;

	return (uint64_t) ret;
}

/* NOTE:
 * vcpu should be paused before call this function.
 */
void reset_vcpu(struct acrn_vcpu *vcpu, enum reset_mode mode)
{
	pr_dbg("vcpu%hu reset", vcpu->vcpu_id);
	ASSERT(vcpu->state == VCPU_ZOMBIE,
			"reset vcpu only when it's in zombie");

	if (vcpu->state == VCPU_ZOMBIE) {
		vcpu_reset_internal(vcpu, mode);
		vcpu->state = VCPU_INIT;
	}
}

void pause_vcpu(struct acrn_vcpu *vcpu, enum vcpu_state new_state)
{
	uint16_t pcpu_id = pcpuid_from_vcpu(vcpu);

	pr_dbg("vcpu%hu paused, new state: %d",
		vcpu->vcpu_id, new_state);

	if (((vcpu->state == VCPU_RUNNING) || (vcpu->state == VCPU_PAUSED) || (vcpu->state == VCPU_INIT))
			&& ((new_state == VCPU_PAUSED) || (new_state == VCPU_ZOMBIE))) {
		vcpu->prev_state = vcpu->state;
		vcpu->state = new_state;

		if (vcpu->prev_state == VCPU_RUNNING) {
			sleep_thread(&vcpu->thread_obj);
		}
		if (pcpu_id != get_pcpu_id()) {
			while (vcpu->running) {
				asm_pause();
			}
		}
	}
}

int32_t resume_vcpu(struct acrn_vcpu *vcpu)
{
	int32_t ret = -1;

	pr_dbg("vcpu%hu resumed", vcpu->vcpu_id);

	if (vcpu->state == VCPU_PAUSED) {
		vcpu->state = vcpu->prev_state;
		if (vcpu->state == VCPU_RUNNING) {
			wake_thread(&vcpu->thread_obj);
		}
		ret = 0;
	}
	return ret;
}

void save_xsave_area(struct ext_context *ectx)
{
	ectx->xcr0 = read_xcr(0);
	ectx->xss = msr_read(MSR_IA32_XSS);
	asm volatile("xsaves %0"
			: : "m" (ectx->xs_area),
			"d" (UINT32_MAX),
			"a" (UINT32_MAX):
			"memory");
}

void rstore_xsave_area(const struct ext_context *ectx)
{
	write_xcr(0, ectx->xcr0);
	msr_write(MSR_IA32_XSS, ectx->xss);
	asm volatile("xrstors %0"
			: : "m" (ectx->xs_area),
			"d" (UINT32_MAX),
			"a" (UINT32_MAX):
			"memory");
}

/* TODO:
 * Now we have switch_out and switch_in callbacks for each thread_object, and schedule
 * will call them every thread switch. We can implement lazy context swtich , which
 * only do context swtich when really need.
 */
static void context_switch_out(struct thread_object *prev)
{
	struct acrn_vcpu *vcpu = list_entry(prev, struct acrn_vcpu, thread_obj);
	struct ext_context *ectx = &(vcpu->arch.contexts[vcpu->arch.cur_context].ext_ctx);

	/* We don't flush TLB as we assume each vcpu has different vpid */
	ectx->ia32_star = msr_read(MSR_IA32_STAR);
	ectx->ia32_lstar = msr_read(MSR_IA32_LSTAR);
	ectx->ia32_fmask = msr_read(MSR_IA32_FMASK);
	ectx->ia32_kernel_gs_base = msr_read(MSR_IA32_KERNEL_GS_BASE);

	save_xsave_area(ectx);

	vcpu->running = false;
}

static void context_switch_in(struct thread_object *next)
{
	struct acrn_vcpu *vcpu = list_entry(next, struct acrn_vcpu, thread_obj);
	struct ext_context *ectx = &(vcpu->arch.contexts[vcpu->arch.cur_context].ext_ctx);

	load_vmcs(vcpu);

	msr_write(MSR_IA32_STAR, ectx->ia32_star);
	msr_write(MSR_IA32_LSTAR, ectx->ia32_lstar);
	msr_write(MSR_IA32_FMASK, ectx->ia32_fmask);
	msr_write(MSR_IA32_KERNEL_GS_BASE, ectx->ia32_kernel_gs_base);

	rstore_xsave_area(ectx);

	vcpu->running = true;
}

void launch_vcpu(struct acrn_vcpu *vcpu)
{
	uint16_t pcpu_id = pcpuid_from_vcpu(vcpu);

	pr_dbg("vcpu%hu scheduled on pcpu%hu", vcpu->vcpu_id, pcpu_id);

	if (vcpu->state == VCPU_INIT) {
		vcpu->state = VCPU_RUNNING;
		wake_thread(&vcpu->thread_obj);
	}
}

/* help function for vcpu create */
int32_t prepare_vcpu(struct acrn_vm *vm, uint16_t pcpu_id)
{
	int32_t ret, i;
	struct acrn_vcpu *vcpu = NULL;
	char thread_name[16];

	ret = create_vcpu(pcpu_id, vm, &vcpu);
	if (ret == 0) {
		snprintf(thread_name, 16U, "vm%hu:vcpu%hu", vm->vm_id, vcpu->vcpu_id);
		(void)strncpy_s(vcpu->thread_obj.name, 16U, thread_name, 16U);
		vcpu->thread_obj.sched_ctl = &per_cpu(sched_ctl, pcpu_id);
		vcpu->thread_obj.thread_entry = vcpu_thread;
		vcpu->thread_obj.pcpu_id = pcpu_id;
		/* vcpu->thread_obj.notify_mode is initialized in vcpu_reset_internal() when create vcpu */
		vcpu->thread_obj.host_sp = build_stack_frame(vcpu);
		vcpu->thread_obj.switch_out = context_switch_out;
		vcpu->thread_obj.switch_in = context_switch_in;
		init_thread_data(&vcpu->thread_obj);
		for (i = 0; i < VCPU_EVENT_NUM; i++) {
			init_event(&vcpu->events[i]);
		}
	}

	return ret;
}

/**
 * @pre vcpu != NULL
 */
uint16_t pcpuid_from_vcpu(const struct acrn_vcpu *vcpu)
{
	return sched_get_pcpuid(&vcpu->thread_obj);
}

uint64_t vcpumask2pcpumask(struct acrn_vm *vm, uint64_t vdmask)
{
	uint16_t vcpu_id;
	uint64_t dmask = 0UL;
	struct acrn_vcpu *vcpu;

	for (vcpu_id = 0U; vcpu_id < vm->hw.created_vcpus; vcpu_id++) {
		if ((vdmask & (1UL << vcpu_id)) != 0UL) {
			vcpu = vcpu_from_vid(vm, vcpu_id);
			bitmap_set_nolock(pcpuid_from_vcpu(vcpu), &dmask);
		}
	}

	return dmask;
}

/*
 * @brief Check if vCPU uses LAPIC in x2APIC mode and the VM, vCPU belongs to, is configured for
 * LAPIC Pass-through
 *
 * @pre vcpu != NULL
 *
 *  @return true, if vCPU LAPIC is in x2APIC mode and VM, vCPU belongs to, is configured for
 *  				LAPIC Pass-through
 */
bool is_lapic_pt_enabled(struct acrn_vcpu *vcpu)
{
	return ((is_x2apic_enabled(vcpu_vlapic(vcpu))) && (is_lapic_pt_configured(vcpu->vm)));
}
