/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <schedule.h>
#include <vm0_boot.h>
#include <security.h>
#include <virtual_cr.h>

inline uint64_t vcpu_get_gpreg(const struct acrn_vcpu *vcpu, uint32_t reg)
{
	const struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	return ctx->guest_cpu_regs.longs[reg];
}

inline void vcpu_set_gpreg(struct acrn_vcpu *vcpu, uint32_t reg, uint64_t val)
{
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	ctx->guest_cpu_regs.longs[reg] = val;
}

inline uint64_t vcpu_get_rip(struct acrn_vcpu *vcpu)
{
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	if (bitmap_test(CPU_REG_RIP, &vcpu->reg_updated) == 0 &&
		bitmap_test_and_set_lock(CPU_REG_RIP, &vcpu->reg_cached) == 0)
		ctx->rip = exec_vmread(VMX_GUEST_RIP);
	return ctx->rip;
}

inline void vcpu_set_rip(struct acrn_vcpu *vcpu, uint64_t val)
{
	vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx.rip = val;
	bitmap_set_lock(CPU_REG_RIP, &vcpu->reg_updated);
}

inline uint64_t vcpu_get_rsp(struct acrn_vcpu *vcpu)
{
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	return ctx->guest_cpu_regs.regs.rsp;
}

inline void vcpu_set_rsp(struct acrn_vcpu *vcpu, uint64_t val)
{
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	ctx->guest_cpu_regs.regs.rsp = val;
	bitmap_set_lock(CPU_REG_RSP, &vcpu->reg_updated);
}

inline uint64_t vcpu_get_efer(struct acrn_vcpu *vcpu)
{
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	if (bitmap_test(CPU_REG_EFER, &vcpu->reg_updated) == 0 &&
		bitmap_test_and_set_lock(CPU_REG_EFER, &vcpu->reg_cached) == 0)
		ctx->ia32_efer = exec_vmread64(VMX_GUEST_IA32_EFER_FULL);
	return ctx->ia32_efer;
}

inline void vcpu_set_efer(struct acrn_vcpu *vcpu, uint64_t val)
{
	vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx.ia32_efer
		= val;
	bitmap_set_lock(CPU_REG_EFER, &vcpu->reg_updated);
}

inline uint64_t vcpu_get_rflags(struct acrn_vcpu *vcpu)
{
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	if (bitmap_test(CPU_REG_RFLAGS, &vcpu->reg_updated) == 0 &&
		bitmap_test_and_set_lock(CPU_REG_RFLAGS,
			&vcpu->reg_cached) == 0 && vcpu->launched)
		ctx->rflags = exec_vmread(VMX_GUEST_RFLAGS);
	return ctx->rflags;
}

inline void vcpu_set_rflags(struct acrn_vcpu *vcpu, uint64_t val)
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
void vcpu_set_vmcs_eoi_exit(struct acrn_vcpu *vcpu)
{
	pr_dbg("%s", __func__);

	spinlock_obtain(&(vcpu->arch.lock));
	if (is_apicv_intr_delivery_supported()) {
		exec_vmwrite64(VMX_EOI_EXIT0_FULL, vcpu->arch.eoi_exit_bitmap[0]);
		exec_vmwrite64(VMX_EOI_EXIT1_FULL, vcpu->arch.eoi_exit_bitmap[1]);
		exec_vmwrite64(VMX_EOI_EXIT2_FULL, vcpu->arch.eoi_exit_bitmap[2]);
		exec_vmwrite64(VMX_EOI_EXIT3_FULL, vcpu->arch.eoi_exit_bitmap[3]);
	}
	spinlock_release(&(vcpu->arch.lock));
}

/*
 * Set the eoi_exit_bitmap bit for specific vector
 * called with vcpu->arch.lock held
 * @pre vcpu != NULL && vector <= 255U
 */
void vcpu_set_eoi_exit(struct acrn_vcpu *vcpu, uint32_t vector)
{
	pr_dbg("%s", __func__);

	if (bitmap_test_and_set_nolock((uint16_t)(vector & 0x3fU),
			&(vcpu->arch.eoi_exit_bitmap[(vector & 0xffU) >> 6U]))) {
		pr_warn("Duplicated vector %u vcpu%u", vector, vcpu->vcpu_id);
	}
}

/*
 * Reset all eoi_exit_bitmaps
 * called with vcpu->arch.lock held
 */
void vcpu_reset_eoi_exit_all(struct acrn_vcpu *vcpu)
{
	pr_dbg("%s", __func__);

	memset((void *)(vcpu->arch.eoi_exit_bitmap), 0U, sizeof(vcpu->arch.eoi_exit_bitmap));
}

struct acrn_vcpu *get_ever_run_vcpu(uint16_t pcpu_id)
{
	return per_cpu(ever_run_vcpu, pcpu_id);
}

static void set_vcpu_mode(struct acrn_vcpu *vcpu, uint32_t cs_attr, uint64_t ia32_efer,
		uint64_t cr0)
{
	if (ia32_efer & MSR_IA32_EFER_LMA_BIT) {
		if (cs_attr & 0x2000U) {
			/* CS.L = 1 */
			vcpu->arch.cpu_mode = CPU_MODE_64BIT;
		} else {
			vcpu->arch.cpu_mode = CPU_MODE_COMPATIBILITY;
		}
	} else if (cr0 & CR0_PE) {
		vcpu->arch.cpu_mode = CPU_MODE_PROTECTED;
	} else {
		vcpu->arch.cpu_mode = CPU_MODE_REAL;
	}
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
	if (vcpu_regs->cr0 & CR0_PE) {
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

	memcpy_s(&(ctx->guest_cpu_regs), sizeof(struct acrn_gp_regs),
			&(vcpu_regs->gprs), sizeof(struct acrn_gp_regs));

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

static struct acrn_vcpu_regs realmode_init_regs = {
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

void reset_vcpu_regs(struct acrn_vcpu *vcpu)
{
	set_vcpu_regs(vcpu, &realmode_init_regs);
}

void set_ap_entry(struct acrn_vcpu *vcpu, uint64_t entry)
{
	struct ext_context *ectx;

	ectx = &(vcpu->arch.contexts[vcpu->arch.cur_context].ext_ctx);
	ectx->cs.selector = (uint16_t)((entry >> 4U) & 0xFFFFU);
	ectx->cs.base = ectx->cs.selector << 4U;

	vcpu_set_rip(vcpu, 0UL);
}

/***********************************************************************
 *
 *  @pre vm != NULL && rtn_vcpu_handle != NULL
 *
 * vcpu_id/pcpu_id mapping table:
 *
 * if
 *     SOS_VM_CPUS[2] = {0, 2} , VM1_CPUS[2] = {3, 1};
 * then
 *     for physical CPU 0 : vcpu->pcpu_id = 0, vcpu->vcpu_id = 0, vmid = 0;
 *     for physical CPU 2 : vcpu->pcpu_id = 2, vcpu->vcpu_id = 1, vmid = 0;
 *     for physical CPU 3 : vcpu->pcpu_id = 3, vcpu->vcpu_id = 0, vmid = 1;
 *     for physical CPU 1 : vcpu->pcpu_id = 1, vcpu->vcpu_id = 1, vmid = 1;
 *
 ***********************************************************************/
int32_t create_vcpu(uint16_t pcpu_id, struct acrn_vm *vm, struct acrn_vcpu **rtn_vcpu_handle)
{
	struct acrn_vcpu *vcpu;
	uint16_t vcpu_id;

	pr_info("Creating VCPU working on PCPU%hu", pcpu_id);

	/*
	 * vcpu->vcpu_id = vm->hw.created_vcpus;
	 * vm->hw.created_vcpus++;
	 */
	vcpu_id = atomic_xadd16(&vm->hw.created_vcpus, 1U);
	if (vcpu_id >= CONFIG_MAX_VCPUS_PER_VM) {
		vm->hw.created_vcpus--;
		pr_err("%s, vcpu id is invalid!\n", __func__);
		return -EINVAL;
	}
	/* Allocate memory for VCPU */
	vcpu = &(vm->hw.vcpu_array[vcpu_id]);
	(void)memset((void *)vcpu, 0U, sizeof(struct acrn_vcpu));

	/* Initialize CPU ID for this VCPU */
	vcpu->vcpu_id = vcpu_id;
	vcpu->pcpu_id = pcpu_id;
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

	per_cpu(vcpu, pcpu_id) = vcpu;

	pr_info("PCPU%d is working as VM%d VCPU%d, Role: %s",
			vcpu->pcpu_id, vcpu->vm->vm_id, vcpu->vcpu_id,
			is_vcpu_bsp(vcpu) ? "PRIMARY" : "SECONDARY");

	vcpu->arch.vpid = allocate_vpid();

	/* Initialize exception field in VCPU context */
	vcpu->arch.exception_info.exception = VECTOR_INVALID;

	/* Initialize cur context */
	vcpu->arch.cur_context = NORMAL_WORLD;

	/* Create per vcpu vlapic */
	vlapic_create(vcpu);

#ifdef CONFIG_MTRR_ENABLED
	init_vmtrr(vcpu);
#endif

	spinlock_init(&(vcpu->arch.lock));

	/* Populate the return handle */
	*rtn_vcpu_handle = vcpu;

	vcpu->launched = false;
	vcpu->paused_cnt = 0U;
	vcpu->running = 0;
	vcpu->arch.nr_sipi = 0;
	vcpu->state = VCPU_INIT;

	reset_vcpu_regs(vcpu);
	(void)memset(&vcpu->req, 0U, sizeof(struct io_request));

	return 0;
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
	int64_t status = 0;
	int32_t ibrs_type = get_ibrs_type();

	if (bitmap_test_and_clear_lock(CPU_REG_RIP, &vcpu->reg_updated))
		exec_vmwrite(VMX_GUEST_RIP, ctx->rip);
	if (bitmap_test_and_clear_lock(CPU_REG_RSP, &vcpu->reg_updated))
		exec_vmwrite(VMX_GUEST_RSP, ctx->guest_cpu_regs.regs.rsp);
	if (bitmap_test_and_clear_lock(CPU_REG_EFER, &vcpu->reg_updated))
		exec_vmwrite64(VMX_GUEST_IA32_EFER_FULL, ctx->ia32_efer);
	if (bitmap_test_and_clear_lock(CPU_REG_RFLAGS, &vcpu->reg_updated))
		exec_vmwrite(VMX_GUEST_RFLAGS, ctx->rflags);

	/* If this VCPU is not already launched, launch it */
	if (!vcpu->launched) {
		pr_info("VM %d Starting VCPU %hu",
				vcpu->vm->vm_id, vcpu->vcpu_id);

		if (vcpu->arch.vpid)
			exec_vmwrite16(VMX_VPID, vcpu->arch.vpid);

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
		if (ibrs_type == IBRS_RAW)
			msr_write(MSR_IA32_PRED_CMD, PRED_SET_IBPB);

#ifdef CONFIG_L1D_FLUSH_VMENTRY_ENABLED
		cpu_l1d_flush();
#endif

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

	ctx->guest_cpu_regs.regs.rsp = exec_vmread(VMX_GUEST_RSP);

	/* Obtain VM exit reason */
	vcpu->arch.exit_reason = exec_vmread32(VMX_EXIT_REASON);

	if (status != 0) {
		/* refer to 64-ia32 spec section 24.9.1 volume#3 */
		if (vcpu->arch.exit_reason & VMX_VMENTRY_FAIL)
			pr_fatal("vmentry fail reason=%lx", vcpu->arch.exit_reason);
		else
			pr_fatal("vmexit fail err_inst=%x", exec_vmread32(VMX_INSTR_ERROR));

		ASSERT(status == 0, "vm fail");
	}

	return status;
}

int32_t shutdown_vcpu(__unused struct acrn_vcpu *vcpu)
{
	/* TODO : Implement VCPU shutdown sequence */

	return 0;
}

/*
 *  @pre vcpu != NULL
 */
void offline_vcpu(struct acrn_vcpu *vcpu)
{
	vlapic_free(vcpu);
	per_cpu(ever_run_vcpu, vcpu->pcpu_id) = NULL;
	free_pcpu(vcpu->pcpu_id);
	vcpu->state = VCPU_OFFLINE;
}

/* NOTE:
 * vcpu should be paused before call this function.
 */
void reset_vcpu(struct acrn_vcpu *vcpu)
{
	int32_t i;
	struct acrn_vlapic *vlapic;

	pr_dbg("vcpu%hu reset", vcpu->vcpu_id);
	ASSERT(vcpu->state != VCPU_RUNNING,
			"reset vcpu when it's running");

	if (vcpu->state == VCPU_INIT)
		return;

	vcpu->state = VCPU_INIT;

	vcpu->launched = false;
	vcpu->paused_cnt = 0U;
	vcpu->running = 0;
	vcpu->arch.nr_sipi = 0;

	vcpu->arch.exception_info.exception = VECTOR_INVALID;
	vcpu->arch.cur_context = NORMAL_WORLD;
	vcpu->arch.irq_window_enabled = 0;
	vcpu->arch.inject_event_pending = false;
	(void)memset(vcpu->arch.vmcs, 0U, PAGE_SIZE);

	for (i = 0; i < NR_WORLD; i++) {
		(void)memset(&vcpu->arch.contexts[i], 0U,
			sizeof(struct run_context));
	}
	vcpu->arch.cur_context = NORMAL_WORLD;

	vlapic = vcpu_vlapic(vcpu);
	vlapic_reset(vlapic);

	reset_vcpu_regs(vcpu);
}

void pause_vcpu(struct acrn_vcpu *vcpu, enum vcpu_state new_state)
{
	uint16_t pcpu_id = get_cpu_id();

	pr_dbg("vcpu%hu paused, new state: %d",
		vcpu->vcpu_id, new_state);

	get_schedule_lock(vcpu->pcpu_id);
	vcpu->prev_state = vcpu->state;
	vcpu->state = new_state;

	if (atomic_load32(&vcpu->running) == 1U) {
		remove_from_cpu_runqueue(&vcpu->sched_obj, vcpu->pcpu_id);
		make_reschedule_request(vcpu->pcpu_id);
		release_schedule_lock(vcpu->pcpu_id);

		if (vcpu->pcpu_id != pcpu_id) {
			while (atomic_load32(&vcpu->running) == 1U)
				asm_pause();
		}
	} else {
		remove_from_cpu_runqueue(&vcpu->sched_obj, vcpu->pcpu_id);
		release_schedule_lock(vcpu->pcpu_id);
	}
}

void resume_vcpu(struct acrn_vcpu *vcpu)
{
	pr_dbg("vcpu%hu resumed", vcpu->vcpu_id);

	get_schedule_lock(vcpu->pcpu_id);
	vcpu->state = vcpu->prev_state;

	if (vcpu->state == VCPU_RUNNING) {
		add_to_cpu_runqueue(&vcpu->sched_obj, vcpu->pcpu_id);
		make_reschedule_request(vcpu->pcpu_id);
	}
	release_schedule_lock(vcpu->pcpu_id);
}

static void context_switch_out(struct sched_object *prev)
{
	struct acrn_vcpu *vcpu = list_entry(prev, struct acrn_vcpu, sched_obj);

	/* cancel event(int, gp, nmi and exception) injection */
	cancel_event_injection(vcpu);

	atomic_store32(&vcpu->running, 0U);
	/* do prev vcpu context switch out */
	/* For now, we don't need to invalid ept.
	 * But if we have more than one vcpu on one pcpu,
	 * we need add ept invalid operation here.
	 */
}

static void context_switch_in(struct sched_object *next)
{
	struct acrn_vcpu *vcpu = list_entry(next, struct acrn_vcpu, sched_obj);

	atomic_store32(&vcpu->running, 1U);
	/* FIXME:
	 * Now, we don't need to load new vcpu VMCS because
	 * we only do switch between vcpu loop and idle loop.
	 * If we have more than one vcpu on on pcpu, need to
	 * add VMCS load operation here.
	 */
}

void schedule_vcpu(struct acrn_vcpu *vcpu)
{
	vcpu->state = VCPU_RUNNING;
	pr_dbg("vcpu%hu scheduled", vcpu->vcpu_id);

	get_schedule_lock(vcpu->pcpu_id);
	add_to_cpu_runqueue(&vcpu->sched_obj, vcpu->pcpu_id);
	make_reschedule_request(vcpu->pcpu_id);
	release_schedule_lock(vcpu->pcpu_id);
}

static uint64_t build_stack_frame(struct acrn_vcpu *vcpu)
{
	uint64_t rsp = (uint64_t)&vcpu->stack[CONFIG_STACK_SIZE - 1];
	uint64_t *sp;

	rsp &= ~(CPU_STACK_ALIGN - 1UL);
	sp = (uint64_t *)rsp;

	*sp-- = (uint64_t)run_sched_thread; /*return address*/
	*sp-- = 0UL; /* flag */
	*sp-- = 0UL; /* rbx */
	*sp-- = 0UL; /* rbp */
	*sp-- = 0UL; /* r12 */
	*sp-- = 0UL; /* r13 */
	*sp-- = 0UL; /* r14 */
	*sp-- = 0UL; /* r15 */
	*sp = (uint64_t)&vcpu->sched_obj; /*rdi*/

	return (uint64_t)sp;
}

/* help function for vcpu create */
int32_t prepare_vcpu(struct acrn_vm *vm, uint16_t pcpu_id)
{
	int32_t ret = 0;
	struct acrn_vcpu *vcpu = NULL;
	char thread_name[16];

	ret = create_vcpu(pcpu_id, vm, &vcpu);
	if (ret != 0) {
		return ret;
	}

	set_pcpu_used(pcpu_id);

	INIT_LIST_HEAD(&vcpu->sched_obj.run_list);
	snprintf(thread_name, 16U, "vm%hu:vcpu%hu", vm->vm_id, vcpu->vcpu_id);
	(void)strncpy_s(vcpu->sched_obj.name, 16U, thread_name, 16U);
	vcpu->sched_obj.thread = vcpu_thread;
	vcpu->sched_obj.host_sp = build_stack_frame(vcpu);
	vcpu->sched_obj.prepare_switch_out = context_switch_out;
	vcpu->sched_obj.prepare_switch_in = context_switch_in;

	return ret;
}
