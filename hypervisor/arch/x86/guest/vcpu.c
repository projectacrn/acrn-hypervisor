/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <schedule.h>
#include <vm0_boot.h>

vm_sw_loader_t vm_sw_loader;

inline uint64_t vcpu_get_gpreg(struct vcpu *vcpu, uint32_t reg)
{
	struct run_context *ctx =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].run_ctx;

	return ctx->guest_cpu_regs.longs[reg];
}

inline void vcpu_set_gpreg(struct vcpu *vcpu, uint32_t reg, uint64_t val)
{
	struct run_context *ctx =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].run_ctx;

	ctx->guest_cpu_regs.longs[reg] = val;
}

inline uint64_t vcpu_get_rip(struct vcpu *vcpu)
{
	struct run_context *ctx =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].run_ctx;

	if (bitmap_test(CPU_REG_RIP, &vcpu->reg_updated) == 0 &&
		bitmap_test_and_set_lock(CPU_REG_RIP, &vcpu->reg_cached) == 0)
		ctx->rip = exec_vmread(VMX_GUEST_RIP);
	return ctx->rip;
}

inline void vcpu_set_rip(struct vcpu *vcpu, uint64_t val)
{
	vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].run_ctx.rip = val;
	bitmap_set_lock(CPU_REG_RIP, &vcpu->reg_updated);
}

inline uint64_t vcpu_get_rsp(struct vcpu *vcpu)
{
	struct run_context *ctx =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].run_ctx;

	return ctx->guest_cpu_regs.regs.rsp;
}

inline void vcpu_set_rsp(struct vcpu *vcpu, uint64_t val)
{
	struct run_context *ctx =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].run_ctx;

	ctx->guest_cpu_regs.regs.rsp = val;
	bitmap_set_lock(CPU_REG_RSP, &vcpu->reg_updated);
}

inline uint64_t vcpu_get_efer(struct vcpu *vcpu)
{
	struct run_context *ctx =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].run_ctx;

	if (bitmap_test(CPU_REG_EFER, &vcpu->reg_updated) == 0 &&
		bitmap_test_and_set_lock(CPU_REG_EFER, &vcpu->reg_cached) == 0)
		ctx->ia32_efer = exec_vmread64(VMX_GUEST_IA32_EFER_FULL);
	return ctx->ia32_efer;
}

inline void vcpu_set_efer(struct vcpu *vcpu, uint64_t val)
{
	vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].run_ctx.ia32_efer
		= val;
	bitmap_set_lock(CPU_REG_EFER, &vcpu->reg_updated);
}

inline uint64_t vcpu_get_rflags(struct vcpu *vcpu)
{
	struct run_context *ctx =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].run_ctx;

	if (bitmap_test(CPU_REG_RFLAGS, &vcpu->reg_updated) == 0 &&
		bitmap_test_and_set_lock(CPU_REG_RFLAGS,
			&vcpu->reg_cached) == 0 && vcpu->launched)
		ctx->rflags = exec_vmread(VMX_GUEST_RFLAGS);
	return ctx->rflags;
}

inline void vcpu_set_rflags(struct vcpu *vcpu, uint64_t val)
{
	vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].run_ctx.rflags =
		val;
	bitmap_set_lock(CPU_REG_RFLAGS, &vcpu->reg_updated);
}

inline uint64_t vcpu_get_cr0(struct vcpu *vcpu)
{
	uint64_t mask;
	struct run_context *ctx =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].run_ctx;

	if (bitmap_test_and_set_lock(CPU_REG_CR0, &vcpu->reg_cached) == 0) {
		mask = exec_vmread(VMX_CR0_MASK);
		ctx->cr0 = (exec_vmread(VMX_CR0_READ_SHADOW) & mask) |
			(exec_vmread(VMX_GUEST_CR0) & (~mask));
	}
	return ctx->cr0;
}

inline int vcpu_set_cr0(struct vcpu *vcpu, uint64_t val)
{
	return vmx_write_cr0(vcpu, val);
}

inline uint64_t vcpu_get_cr2(struct vcpu *vcpu)
{
	return vcpu->
		arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].run_ctx.cr2;
}

inline void vcpu_set_cr2(struct vcpu *vcpu, uint64_t val)
{
	vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].run_ctx.cr2 = val;
}

inline uint64_t vcpu_get_cr4(struct vcpu *vcpu)
{
	uint64_t mask;
	struct run_context *ctx =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].run_ctx;

	if (bitmap_test_and_set_lock(CPU_REG_CR4, &vcpu->reg_cached) == 0) {
		mask = exec_vmread(VMX_CR4_MASK);
		ctx->cr4 = (exec_vmread(VMX_CR4_READ_SHADOW) & mask) |
			(exec_vmread(VMX_GUEST_CR4) & (~mask));
	}
	return ctx->cr4;
}

inline int vcpu_set_cr4(struct vcpu *vcpu, uint64_t val)
{
	return vmx_write_cr4(vcpu, val);
}

inline uint64_t vcpu_get_pat_ext(struct vcpu *vcpu)
{
	return vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].
		ext_ctx.ia32_pat;
}

inline void vcpu_set_pat_ext(struct vcpu *vcpu, uint64_t val)
{
	vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].ext_ctx.ia32_pat
		= val;
}

struct vcpu *get_ever_run_vcpu(uint16_t pcpu_id)
{
	return per_cpu(ever_run_vcpu, pcpu_id);
}

/***********************************************************************
 *  vcpu_id/pcpu_id mapping table:
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
int create_vcpu(uint16_t pcpu_id, struct vm *vm, struct vcpu **rtn_vcpu_handle)
{
	struct vcpu *vcpu;

	ASSERT(vm != NULL, "");
	ASSERT(rtn_vcpu_handle != NULL, "");

	pr_info("Creating VCPU %hu", pcpu_id);

	/* Allocate memory for VCPU */
	vcpu = calloc(1U, sizeof(struct vcpu));
	ASSERT(vcpu != NULL, "");

	/* Initialize the physical CPU ID for this VCPU */
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

	/*
	 * vcpu->vcpu_id = vm->hw.created_vcpus;
	 * vm->hw.created_vcpus++;
	 */
	vcpu->vcpu_id = atomic_xadd16(&vm->hw.created_vcpus, 1U);
	/* vm->hw.vcpu_array[vcpu->vcpu_id] = vcpu; */
	atomic_store64(
		(uint64_t *)&vm->hw.vcpu_array[vcpu->vcpu_id],
		(uint64_t)vcpu);

	ASSERT(vcpu->vcpu_id < vm->hw.num_vcpus,
			"Allocated vcpu_id is out of range!");

	per_cpu(vcpu, pcpu_id) = vcpu;

	pr_info("PCPU%d is working as VM%d VCPU%d, Role: %s",
			vcpu->pcpu_id, vcpu->vm->vm_id, vcpu->vcpu_id,
			is_vcpu_bsp(vcpu) ? "PRIMARY" : "SECONDARY");

#ifdef CONFIG_START_VM0_BSP_64BIT
	/* Is this VCPU a VM0 BSP, create page hierarchy for this VM */
	if (is_vcpu_bsp(vcpu) && is_vm0(vcpu->vm)) {
		/* Set up temporary guest page tables */
		vm->arch_vm.guest_init_pml4 = create_guest_initial_paging(vm);
		pr_info("VM %d VCPU %hu CR3: 0x%016llx ",
			vm->vm_id, vcpu->vcpu_id,
			vm->arch_vm.guest_init_pml4);
	}
#endif

	vcpu->arch_vcpu.vpid = allocate_vpid();

	/* Allocate VMCS region for this VCPU */
	vcpu->arch_vcpu.vmcs = alloc_page();
	ASSERT(vcpu->arch_vcpu.vmcs != NULL, "");

	/* Memset VMCS region for this VCPU */
	(void)memset(vcpu->arch_vcpu.vmcs, 0U, CPU_PAGE_SIZE);

	/* Initialize exception field in VCPU context */
	vcpu->arch_vcpu.exception_info.exception = VECTOR_INVALID;

	/* Initialize cur context */
	vcpu->arch_vcpu.cur_context = NORMAL_WORLD;

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
	vcpu->arch_vcpu.nr_sipi = 0;
	vcpu->pending_pre_work = 0U;
	vcpu->state = VCPU_INIT;

	(void)memset(&vcpu->req, 0U, sizeof(struct io_request));

	return 0;
}

static void set_vcpu_mode(struct vcpu *vcpu, uint32_t cs_attr)
{
	if (vcpu_get_efer(vcpu) & MSR_IA32_EFER_LMA_BIT) {
		if (cs_attr & 0x2000)		/* CS.L = 1 */
			vcpu->arch_vcpu.cpu_mode = CPU_MODE_64BIT;
		else
			vcpu->arch_vcpu.cpu_mode = CPU_MODE_COMPATIBILITY;
	} else if (vcpu_get_cr0(vcpu) & CR0_PE) {
		vcpu->arch_vcpu.cpu_mode = CPU_MODE_PROTECTED;
	} else {
		vcpu->arch_vcpu.cpu_mode = CPU_MODE_REAL;
	}
}

int start_vcpu(struct vcpu *vcpu)
{
	uint32_t instlen;
	uint64_t rip;
	struct run_context *ctx =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].run_ctx;
	int64_t status = 0;

	ASSERT(vcpu != NULL, "Incorrect arguments");

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

		if (vcpu->arch_vcpu.vpid)
			exec_vmwrite16(VMX_VPID, vcpu->arch_vcpu.vpid);

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
		instlen = vcpu->arch_vcpu.inst_len;
		rip = vcpu_get_rip(vcpu);
		exec_vmwrite(VMX_GUEST_RIP, ((rip+(uint64_t)instlen) &
				0xFFFFFFFFFFFFFFFFUL));

		/* Resume the VM */
		status = vmx_vmrun(ctx, VM_RESUME, ibrs_type);
	}

	vcpu->reg_cached = 0UL;

	set_vcpu_mode(vcpu, exec_vmread32(VMX_GUEST_CS_ATTR));

	/* Obtain current VCPU instruction length */
	vcpu->arch_vcpu.inst_len = exec_vmread32(VMX_EXIT_INSTR_LEN);

	ctx->guest_cpu_regs.regs.rsp = exec_vmread(VMX_GUEST_RSP);

	/* Obtain VM exit reason */
	vcpu->arch_vcpu.exit_reason = exec_vmread32(VMX_EXIT_REASON);

	if (status != 0) {
		/* refer to 64-ia32 spec section 24.9.1 volume#3 */
		if (vcpu->arch_vcpu.exit_reason & VMX_VMENTRY_FAIL)
			pr_fatal("vmentry fail reason=%lx", vcpu->arch_vcpu.exit_reason);
		else
			pr_fatal("vmexit fail err_inst=%x", exec_vmread32(VMX_INSTR_ERROR));

		ASSERT(status == 0, "vm fail");
	}

	return status;
}

int shutdown_vcpu(__unused struct vcpu *vcpu)
{
	/* TODO : Implement VCPU shutdown sequence */

	return 0;
}

void destroy_vcpu(struct vcpu *vcpu)
{
	ASSERT(vcpu != NULL, "Incorrect arguments");

	/* vcpu->vm->hw.vcpu_array[vcpu->vcpu_id] = NULL; */
	atomic_store64(
		(uint64_t *)&vcpu->vm->hw.vcpu_array[vcpu->vcpu_id],
		(uint64_t)NULL);

	atomic_dec16(&vcpu->vm->hw.created_vcpus);

	vlapic_free(vcpu);
	free(vcpu->arch_vcpu.vmcs);
	free(vcpu->guest_msrs);
	per_cpu(ever_run_vcpu, vcpu->pcpu_id) = NULL;
	free_pcpu(vcpu->pcpu_id);
	free(vcpu);
}

/* NOTE:
 * vcpu should be paused before call this function.
 */
void reset_vcpu(struct vcpu *vcpu)
{
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
	vcpu->arch_vcpu.nr_sipi = 0;
	vcpu->pending_pre_work = 0U;

	vcpu->arch_vcpu.exception_info.exception = VECTOR_INVALID;
	vcpu->arch_vcpu.cur_context = NORMAL_WORLD;
	vcpu->arch_vcpu.irq_window_enabled = 0;
	vcpu->arch_vcpu.inject_event_pending = false;
	(void)memset(vcpu->arch_vcpu.vmcs, 0U, CPU_PAGE_SIZE);

	vlapic = vcpu->arch_vcpu.vlapic;
	vlapic_reset(vlapic);
}

void pause_vcpu(struct vcpu *vcpu, enum vcpu_state new_state)
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

void resume_vcpu(struct vcpu *vcpu)
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

void schedule_vcpu(struct vcpu *vcpu)
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
	struct vcpu *vcpu = NULL;

	ret = create_vcpu(pcpu_id, vm, &vcpu);
	ASSERT(ret == 0, "vcpu create failed");

	if (is_vcpu_bsp(vcpu)) {
		/* Load VM SW */
		if (!vm_sw_loader)
			vm_sw_loader = general_sw_loader;
		if (is_vm0(vcpu->vm)) {
			struct boot_ctx *vm0_init_ctx =
				(struct boot_ctx *)vm0_boot_context;
			/* VM0 bsp start mode is decided by the boot context
			 * setup by bootloader / bios */
			if ((vm0_init_ctx->ia32_efer & MSR_IA32_EFER_LMA_BIT) &&
			    (vm0_init_ctx->cs_ar & 0x2000)) {
				vcpu->arch_vcpu.cpu_mode = CPU_MODE_64BIT;
			} else if (vm0_init_ctx->cr0 & CR0_PE) {
				vcpu->arch_vcpu.cpu_mode = CPU_MODE_PROTECTED;
			} else {
				return -EINVAL;
			}
		} else {
#ifdef CONFIG_EFI_STUB
			/* currently non-vm0 will boot kernel directly */
			vcpu->arch_vcpu.cpu_mode = CPU_MODE_PROTECTED;
#else
			vcpu->arch_vcpu.cpu_mode = CPU_MODE_REAL;
#endif
		}
		vm_sw_loader(vm, vcpu);
	} else {
		vcpu->arch_vcpu.cpu_mode = CPU_MODE_REAL;
	}

	/* init_vmcs is delayed to vcpu vmcs launch first time */

	/* initialize the vcpu tsc aux */
	vcpu->msr_tsc_aux_guest = vcpu->vcpu_id;

	set_pcpu_used(pcpu_id);

	INIT_LIST_HEAD(&vcpu->run_list);

	return ret;
}

void request_vcpu_pre_work(struct vcpu *vcpu, uint16_t pre_work_id)
{
	bitmap_set_lock(pre_work_id, &vcpu->pending_pre_work);
}
