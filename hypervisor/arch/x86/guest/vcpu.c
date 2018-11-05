/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <schedule.h>
#include <vm0_boot.h>

vm_sw_loader_t vm_sw_loader;

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

inline uint64_t vcpu_get_cr0(struct acrn_vcpu *vcpu)
{
	uint64_t mask;
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	if (bitmap_test_and_set_lock(CPU_REG_CR0, &vcpu->reg_cached) == 0) {
		mask = exec_vmread(VMX_CR0_MASK);
		ctx->cr0 = (exec_vmread(VMX_CR0_READ_SHADOW) & mask) |
			(exec_vmread(VMX_GUEST_CR0) & (~mask));
	}
	return ctx->cr0;
}

inline void vcpu_set_cr0(struct acrn_vcpu *vcpu, uint64_t val)
{
	vmx_write_cr0(vcpu, val);
}

inline uint64_t vcpu_get_cr2(struct acrn_vcpu *vcpu)
{
	return vcpu->
		arch.contexts[vcpu->arch.cur_context].run_ctx.cr2;
}

inline void vcpu_set_cr2(struct acrn_vcpu *vcpu, uint64_t val)
{
	vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx.cr2 = val;
}

inline uint64_t vcpu_get_cr4(struct acrn_vcpu *vcpu)
{
	uint64_t mask;
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	if (bitmap_test_and_set_lock(CPU_REG_CR4, &vcpu->reg_cached) == 0) {
		mask = exec_vmread(VMX_CR4_MASK);
		ctx->cr4 = (exec_vmread(VMX_CR4_READ_SHADOW) & mask) |
			(exec_vmread(VMX_GUEST_CR4) & (~mask));
	}
	return ctx->cr4;
}

inline void vcpu_set_cr4(struct acrn_vcpu *vcpu, uint64_t val)
{
	vmx_write_cr4(vcpu, val);
}

inline uint64_t vcpu_get_pat_ext(const struct acrn_vcpu *vcpu)
{
	return vcpu->arch.contexts[vcpu->arch.cur_context].
		ext_ctx.ia32_pat;
}

inline void vcpu_set_pat_ext(struct acrn_vcpu *vcpu, uint64_t val)
{
	vcpu->arch.contexts[vcpu->arch.cur_context].ext_ctx.ia32_pat
		= val;
}

struct acrn_vcpu *get_ever_run_vcpu(uint16_t pcpu_id)
{
	return per_cpu(ever_run_vcpu, pcpu_id);
}

static void set_vcpu_mode(struct acrn_vcpu *vcpu, uint32_t cs_attr, uint64_t ia32_efer,
		uint64_t cr0)
{
	if (ia32_efer & MSR_IA32_EFER_LMA_BIT) {
		if (cs_attr & 0x2000)		/* CS.L = 1 */
			vcpu->arch.cpu_mode = CPU_MODE_64BIT;
		else
			vcpu->arch.cpu_mode = CPU_MODE_COMPATIBILITY;
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
 *     VM0_CPUS[2] = {0, 2} , VM1_CPUS[2] = {3, 1};
 * then
 *     for physical CPU 0 : vcpu->pcpu_id = 0, vcpu->vcpu_id = 0, vmid = 0;
 *     for physical CPU 2 : vcpu->pcpu_id = 2, vcpu->vcpu_id = 1, vmid = 0;
 *     for physical CPU 3 : vcpu->pcpu_id = 3, vcpu->vcpu_id = 0, vmid = 1;
 *     for physical CPU 1 : vcpu->pcpu_id = 1, vcpu->vcpu_id = 1, vmid = 1;
 *
 ***********************************************************************/
int create_vcpu(uint16_t pcpu_id, struct vm *vm, struct acrn_vcpu **rtn_vcpu_handle)
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
	init_mtrr(vcpu);
#endif

	/* Populate the return handle */
	*rtn_vcpu_handle = vcpu;

	vcpu->launched = false;
	vcpu->paused_cnt = 0U;
	vcpu->running = 0;
	vcpu->arch.nr_sipi = 0;
	vcpu->pending_pre_work = 0U;
	vcpu->state = VCPU_INIT;

	reset_vcpu_regs(vcpu);
	(void)memset(&vcpu->req, 0U, sizeof(struct io_request));

	return 0;
}

/*
 *  @pre vcpu != NULL
 */
int run_vcpu(struct acrn_vcpu *vcpu)
{
	uint32_t instlen, cs_attr;
	uint64_t rip, ia32_efer, cr0;
	struct run_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;
	int64_t status = 0;

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

int shutdown_vcpu(__unused struct acrn_vcpu *vcpu)
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
	int i;
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
	vcpu->pending_pre_work = 0U;

	vcpu->arch.exception_info.exception = VECTOR_INVALID;
	vcpu->arch.cur_context = NORMAL_WORLD;
	vcpu->arch.irq_window_enabled = 0;
	vcpu->arch.inject_event_pending = false;
	(void)memset(vcpu->arch.vmcs, 0U, CPU_PAGE_SIZE);

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
		remove_vcpu_from_runqueue(vcpu);
		make_reschedule_request(vcpu);
		release_schedule_lock(vcpu->pcpu_id);

		if (vcpu->pcpu_id != pcpu_id) {
			while (atomic_load32(&vcpu->running) == 1U)
				__asm__ __volatile("pause" ::: "memory");
		}
	} else {
		remove_vcpu_from_runqueue(vcpu);
		release_schedule_lock(vcpu->pcpu_id);
	}
}

void resume_vcpu(struct acrn_vcpu *vcpu)
{
	pr_dbg("vcpu%hu resumed", vcpu->vcpu_id);

	get_schedule_lock(vcpu->pcpu_id);
	vcpu->state = vcpu->prev_state;

	if (vcpu->state == VCPU_RUNNING) {
		add_vcpu_to_runqueue(vcpu);
		make_reschedule_request(vcpu);
	}
	release_schedule_lock(vcpu->pcpu_id);
}

void schedule_vcpu(struct acrn_vcpu *vcpu)
{
	vcpu->state = VCPU_RUNNING;
	pr_dbg("vcpu%hu scheduled", vcpu->vcpu_id);

	get_schedule_lock(vcpu->pcpu_id);
	add_vcpu_to_runqueue(vcpu);
	make_reschedule_request(vcpu);
	release_schedule_lock(vcpu->pcpu_id);
}

/* help function for vcpu create */
int prepare_vcpu(struct vm *vm, uint16_t pcpu_id)
{
	int ret = 0;
	struct acrn_vcpu *vcpu = NULL;

	ret = create_vcpu(pcpu_id, vm, &vcpu);
	if (ret != 0) {
		return ret;
	}

	/* init_vmcs is delayed to vcpu vmcs launch first time */
	/* initialize the vcpu tsc aux */
	vcpu->msr_tsc_aux_guest = vcpu->vcpu_id;

	set_pcpu_used(pcpu_id);

	INIT_LIST_HEAD(&vcpu->run_list);

	return ret;
}

void request_vcpu_pre_work(struct acrn_vcpu *vcpu, uint16_t pre_work_id)
{
	bitmap_set_lock(pre_work_id, &vcpu->pending_pre_work);
}

#ifdef HV_DEBUG
#define DUMPREG_SP_SIZE	32
/* the input 'data' must != NULL and indicate a vcpu structure pointer */
void vcpu_dumpreg(void *data)
{
	int status;
	uint64_t i, fault_addr, tmp[DUMPREG_SP_SIZE];
	uint32_t err_code = 0;
	struct vcpu_dump *dump = data;
	struct acrn_vcpu *vcpu = dump->vcpu;
	char *str = dump->str;
	size_t len, size = dump->str_max;

	len = snprintf(str, size,
		"=  VM ID %d ==== CPU ID %hu========================\r\n"
		"=  RIP=0x%016llx  RSP=0x%016llx RFLAGS=0x%016llx\r\n"
		"=  CR0=0x%016llx  CR2=0x%016llx\r\n"
		"=  CR3=0x%016llx  CR4=0x%016llx\r\n"
		"=  RAX=0x%016llx  RBX=0x%016llx RCX=0x%016llx\r\n"
		"=  RDX=0x%016llx  RDI=0x%016llx RSI=0x%016llx\r\n"
		"=  RBP=0x%016llx  R8=0x%016llx R9=0x%016llx\r\n"
		"=  R10=0x%016llx  R11=0x%016llx R12=0x%016llx\r\n"
		"=  R13=0x%016llx  R14=0x%016llx  R15=0x%016llx\r\n",
		vcpu->vm->vm_id, vcpu->vcpu_id,
		vcpu_get_rip(vcpu),
		vcpu_get_gpreg(vcpu, CPU_REG_RSP),
		vcpu_get_rflags(vcpu),
		vcpu_get_cr0(vcpu), vcpu_get_cr2(vcpu),
		exec_vmread(VMX_GUEST_CR3), vcpu_get_cr4(vcpu),
		vcpu_get_gpreg(vcpu, CPU_REG_RAX),
		vcpu_get_gpreg(vcpu, CPU_REG_RBX),
		vcpu_get_gpreg(vcpu, CPU_REG_RCX),
		vcpu_get_gpreg(vcpu, CPU_REG_RDX),
		vcpu_get_gpreg(vcpu, CPU_REG_RDI),
		vcpu_get_gpreg(vcpu, CPU_REG_RSI),
		vcpu_get_gpreg(vcpu, CPU_REG_RBP),
		vcpu_get_gpreg(vcpu, CPU_REG_R8),
		vcpu_get_gpreg(vcpu, CPU_REG_R9),
		vcpu_get_gpreg(vcpu, CPU_REG_R10),
		vcpu_get_gpreg(vcpu, CPU_REG_R11),
		vcpu_get_gpreg(vcpu, CPU_REG_R12),
		vcpu_get_gpreg(vcpu, CPU_REG_R13),
		vcpu_get_gpreg(vcpu, CPU_REG_R14),
		vcpu_get_gpreg(vcpu, CPU_REG_R15));
	if (len >= size) {
		goto overflow;
	}
	size -= len;
	str += len;

	/* dump sp */
	status = copy_from_gva(vcpu, tmp, vcpu_get_gpreg(vcpu, CPU_REG_RSP),
			DUMPREG_SP_SIZE*sizeof(uint64_t), &err_code,
			&fault_addr);
	if (status < 0) {
		/* copy_from_gva fail */
		len = snprintf(str, size, "Cannot handle user gva yet!\r\n");
		if (len >= size) {
			goto overflow;
		}
		size -= len;
		str += len;
	} else {
		len = snprintf(str, size, "\r\nDump RSP for vm %hu, from gva 0x%016llx\r\n",
			vcpu->vm->vm_id, vcpu_get_gpreg(vcpu, CPU_REG_RSP));
		if (len >= size) {
			goto overflow;
		}
		size -= len;
		str += len;

		for (i = 0UL; i < 8UL; i++) {
			len = snprintf(str, size, "=  0x%016llx  0x%016llx 0x%016llx  0x%016llx\r\n",
					tmp[i*4UL], tmp[(i*4UL)+1UL], tmp[(i*4UL)+2UL], tmp[(i*4UL)+3UL]);
			if (len >= size) {
				goto overflow;
			}
			size -= len;
			str += len;
		}
	}
	return;

overflow:
	printf("buffer size could not be enough! please check!\n");
}
#else
void vcpu_dumpreg(__unused void *data)
{
	return;
}
#endif /* HV_DEBUG */
