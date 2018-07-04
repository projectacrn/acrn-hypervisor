/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <schedule.h>

#ifdef CONFIG_EFI_STUB
#include <acrn_efi.h>
extern struct efi_ctx* efi_ctx;
#endif

vm_sw_loader_t vm_sw_loader;

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
int create_vcpu(uint16_t cpu_id, struct vm *vm, struct vcpu **rtn_vcpu_handle)
{
	struct vcpu *vcpu;

	ASSERT(vm != NULL, "");
	ASSERT(rtn_vcpu_handle != NULL, "");

	pr_info("Creating VCPU %d", cpu_id);

	/* Allocate memory for VCPU */
	vcpu = calloc(1, sizeof(struct vcpu));
	ASSERT(vcpu != NULL, "");

	/* Initialize the physical CPU ID for this VCPU */
	vcpu->pcpu_id = cpu_id;
	per_cpu(ever_run_vcpu, cpu_id) = vcpu;

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
	vcpu->vcpu_id = atomic_xadd(&vm->hw.created_vcpus, 1);
	/* vm->hw.vcpu_array[vcpu->vcpu_id] = vcpu; */
	atomic_store64(
		(long *)&vm->hw.vcpu_array[vcpu->vcpu_id],
		(long)vcpu);

	ASSERT(vcpu->vcpu_id < vm->hw.num_vcpus,
			"Allocated vcpu_id is out of range!");

	per_cpu(vcpu, cpu_id) = vcpu;

	pr_info("PCPU%d is working as VM%d VCPU%d, Role: %s",
			vcpu->pcpu_id, vcpu->vm->attr.id, vcpu->vcpu_id,
			is_vcpu_bsp(vcpu) ? "PRIMARY" : "SECONDARY");

#ifdef CONFIG_START_VM0_BSP_64BIT
	/* Is this VCPU a VM0 BSP, create page hierarchy for this VM */
	if (is_vcpu_bsp(vcpu) && is_vm0(vcpu->vm)) {
		/* Set up temporary guest page tables */
		vm->arch_vm.guest_init_pml4 = create_guest_initial_paging(vm);
		pr_info("VM %d VCPU %hu CR3: 0x%016llx ",
			vm->attr.id, vcpu->vcpu_id,
			vm->arch_vm.guest_init_pml4);
	}
#endif

	vcpu->arch_vcpu.vpid = allocate_vpid();

	/* Allocate VMCS region for this VCPU */
	vcpu->arch_vcpu.vmcs = alloc_page();
	ASSERT(vcpu->arch_vcpu.vmcs != NULL, "");

	/* Memset VMCS region for this VCPU */
	memset(vcpu->arch_vcpu.vmcs, 0, CPU_PAGE_SIZE);

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
	vcpu->ioreq_pending = 0;
	vcpu->arch_vcpu.nr_sipi = 0;
	vcpu->pending_pre_work = 0U;
	vcpu->state = VCPU_INIT;

	return 0;
}

static void set_vcpu_mode(struct vcpu *vcpu, uint32_t cs_attr)
{
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];

	if (cur_context->ia32_efer & MSR_IA32_EFER_LMA_BIT) {
		if (cs_attr & 0x2000)		/* CS.L = 1 */
			vcpu->arch_vcpu.cpu_mode = CPU_MODE_64BIT;
		else
			vcpu->arch_vcpu.cpu_mode = CPU_MODE_COMPATIBILITY;
	} else if (cur_context->cr0 & CR0_PE) {
		vcpu->arch_vcpu.cpu_mode = CPU_MODE_PROTECTED;
	} else {
		vcpu->arch_vcpu.cpu_mode = CPU_MODE_REAL;
	}
}

int start_vcpu(struct vcpu *vcpu)
{
	uint32_t instlen;
	uint64_t rip;
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
	int64_t status = 0;

	ASSERT(vcpu != NULL, "Incorrect arguments");

	/* If this VCPU is not already launched, launch it */
	if (!vcpu->launched) {
		pr_info("VM %d Starting VCPU %hu",
				vcpu->vm->attr.id, vcpu->vcpu_id);

		if (vcpu->arch_vcpu.vpid)
			exec_vmwrite(VMX_VPID, vcpu->arch_vcpu.vpid);

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
		status = vmx_vmrun(cur_context, VM_LAUNCH, ibrs_type);

		/* See if VM launched successfully */
		if (status == 0) {
			if (is_vcpu_bsp(vcpu)) {
				pr_info("VM %d VCPU %hu successfully launched",
					vcpu->vm->attr.id, vcpu->vcpu_id);
			}
		}
	} else {
		/* This VCPU was already launched, check if the last guest
		 * instruction needs to be repeated and resume VCPU accordingly
		 */
		instlen = vcpu->arch_vcpu.inst_len;
		rip = cur_context->rip;
		exec_vmwrite(VMX_GUEST_RIP, ((rip + instlen) &
				0xFFFFFFFFFFFFFFFF));

		/* Resume the VM */
		status = vmx_vmrun(cur_context, VM_RESUME, ibrs_type);
	}

	/* Save guest CR3 register */
	cur_context->cr3 = exec_vmread(VMX_GUEST_CR3);

	/* Save guest IA32_EFER register */
	cur_context->ia32_efer = exec_vmread64(VMX_GUEST_IA32_EFER_FULL);
	set_vcpu_mode(vcpu, exec_vmread(VMX_GUEST_CS_ATTR));

	/* Obtain current VCPU instruction pointer and length */
	cur_context->rip = exec_vmread(VMX_GUEST_RIP);
	vcpu->arch_vcpu.inst_len = exec_vmread(VMX_EXIT_INSTR_LEN);

	cur_context->rsp = exec_vmread(VMX_GUEST_RSP);
	cur_context->rflags = exec_vmread(VMX_GUEST_RFLAGS);

	/* Obtain VM exit reason */
	vcpu->arch_vcpu.exit_reason = exec_vmread(VMX_EXIT_REASON);

	if (status != 0) {
		/* refer to 64-ia32 spec section 24.9.1 volume#3 */
		if (vcpu->arch_vcpu.exit_reason & VMX_VMENTRY_FAIL)
			pr_fatal("vmentry fail reason=%lx", vcpu->arch_vcpu.exit_reason);
		else
			pr_fatal("vmexit fail err_inst=%lx", exec_vmread(VMX_INSTR_ERROR));

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
		(long *)&vcpu->vm->hw.vcpu_array[vcpu->vcpu_id],
		(long)NULL);

	atomic_dec(&vcpu->vm->hw.created_vcpus);

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
	struct vlapic *vlapic;

	pr_dbg("vcpu%hu reset", vcpu->vcpu_id);
	ASSERT(vcpu->state != VCPU_RUNNING,
			"reset vcpu when it's running");

	if (vcpu->state == VCPU_INIT)
		return;

	vcpu->state = VCPU_INIT;

	vcpu->launched = false;
	vcpu->paused_cnt = 0U;
	vcpu->running = 0;
	vcpu->ioreq_pending = 0;
	vcpu->arch_vcpu.nr_sipi = 0;
	vcpu->pending_pre_work = 0U;

	vcpu->arch_vcpu.exception_info.exception = VECTOR_INVALID;
	vcpu->arch_vcpu.cur_context = NORMAL_WORLD;
	vcpu->arch_vcpu.irq_window_enabled = 0;
	vcpu->arch_vcpu.inject_event_pending = false;
	memset(vcpu->arch_vcpu.vmcs, 0, CPU_PAGE_SIZE);

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

	if (atomic_load(&vcpu->running) == 1) {
		remove_vcpu_from_runqueue(vcpu);
		make_reschedule_request(vcpu);
		release_schedule_lock(vcpu->pcpu_id);

		if (vcpu->pcpu_id != pcpu_id) {
			while (atomic_load(&vcpu->running) == 1)
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
			vcpu->arch_vcpu.cpu_mode = CPU_MODE_PROTECTED;
#ifdef CONFIG_EFI_STUB
			if ((efi_ctx->efer & MSR_IA32_EFER_LMA_BIT) &&
			    (efi_ctx->cs_ar & 0x2000))
				vcpu->arch_vcpu.cpu_mode = CPU_MODE_64BIT;
#elif CONFIG_START_VM0_BSP_64BIT
			vcpu->arch_vcpu.cpu_mode = CPU_MODE_64BIT;
#endif
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
	bitmap_set(pre_work_id, &vcpu->pending_pre_work);
}
