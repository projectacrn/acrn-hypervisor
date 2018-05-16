/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <schedule.h>
#include <hv_debug.h>

vm_sw_loader_t vm_sw_loader;

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
int create_vcpu(int cpu_id, struct vm *vm, struct vcpu **rtn_vcpu_handle)
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
	vcpu->vcpu_id = atomic_xadd_int(&vm->hw.created_vcpus, 1);
	/* vm->hw.vcpu_array[vcpu->vcpu_id] = vcpu; */
	atomic_store_rel_64(
		(unsigned long *)&vm->hw.vcpu_array[vcpu->vcpu_id],
		(unsigned long)vcpu);

	ASSERT(vcpu->vcpu_id < vm->hw.num_vcpus,
			"Allocated vcpu_id is out of range!");

	per_cpu(vcpu, cpu_id) = vcpu;

	pr_info("PCPU%d is working as VM%d VCPU%d, Role: %s",
			vcpu->pcpu_id, vcpu->vm->attr.id, vcpu->vcpu_id,
			is_vcpu_bsp(vcpu) ? "PRIMARY" : "SECONDARY");

	/* Is this VCPU a VM BSP, create page hierarchy for this VM */
	if (is_vcpu_bsp(vcpu)) {
		/* Set up temporary guest page tables */
		vm->arch_vm.guest_init_pml4 = create_guest_initial_paging(vm);
		pr_info("VM *d VCPU %d CR3: 0x%016llx ",
			vm->attr.id, vcpu->vcpu_id,
			vm->arch_vm.guest_init_pml4);
	}

	/* Allocate VMCS region for this VCPU */
	vcpu->arch_vcpu.vmcs = alloc_page();
	ASSERT(vcpu->arch_vcpu.vmcs != NULL, "");

	/* Memset VMCS region for this VCPU */
	memset(vcpu->arch_vcpu.vmcs, 0, CPU_PAGE_SIZE);

	/* Initialize exception field in VCPU context */
	vcpu->arch_vcpu.exception_info.exception = -1;

	/* Initialize cur context */
	vcpu->arch_vcpu.cur_context = NORMAL_WORLD;

	/* Create per vcpu vlapic */
	vlapic_create(vcpu);

	/* Populate the return handle */
	*rtn_vcpu_handle = vcpu;

	vcpu->launched = false;
	vcpu->paused_cnt = 0;
	vcpu->running = 0;
	vcpu->ioreq_pending = 0;
	vcpu->arch_vcpu.nr_sipi = 0;
	vcpu->pending_pre_work = 0;
	vcpu->state = VCPU_INIT;

	return 0;
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
		pr_info("VM %d Starting VCPU %d",
				vcpu->vm->attr.id, vcpu->vcpu_id);

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
				pr_info("VM %d VCPU %d successfully launched",
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

int destroy_vcpu(struct vcpu *vcpu)
{
	ASSERT(vcpu != NULL, "Incorrect arguments");

	/* vcpu->vm->hw.vcpu_array[vcpu->vcpu_id] = NULL; */
	atomic_store_rel_64(
		(unsigned long *)&vcpu->vm->hw.vcpu_array[vcpu->vcpu_id],
		(unsigned long)NULL);

	atomic_subtract_int(&vcpu->vm->hw.created_vcpus, 1);

	vlapic_free(vcpu);
	free(vcpu->arch_vcpu.vmcs);
	free(vcpu->guest_msrs);
	free_pcpu(vcpu->pcpu_id);
	free(vcpu);

	return 0;
}

/* NOTE:
 * vcpu should be paused before call this function.
 */
void reset_vcpu(struct vcpu *vcpu)
{
	struct vlapic *vlapic;

	pr_dbg("vcpu%d reset", vcpu->vcpu_id);
	ASSERT(vcpu->state != VCPU_RUNNING,
			"reset vcpu when it's running");

	if (vcpu->state == VCPU_INIT)
		return;

	vcpu->state = VCPU_INIT;

	vcpu->launched = false;
	vcpu->paused_cnt = 0;
	vcpu->running = 0;
	vcpu->ioreq_pending = 0;
	vcpu->arch_vcpu.nr_sipi = 0;
	vcpu->pending_pre_work = 0;
	vlapic = vcpu->arch_vcpu.vlapic;
	vlapic_init(vlapic);
}

void init_vcpu(struct vcpu *vcpu)
{
	if (is_vcpu_bsp(vcpu))
		vcpu->arch_vcpu.cpu_mode = PAGE_PROTECTED_MODE;
	else
		vcpu->arch_vcpu.cpu_mode = REAL_MODE;
	/* init_vmcs is delayed to vcpu vmcs launch first time */
}

void pause_vcpu(struct vcpu *vcpu, enum vcpu_state new_state)
{
	int pcpu_id = get_cpu_id();

	pr_dbg("vcpu%d paused, new state: %d",
		vcpu->vcpu_id, new_state);

	vcpu->prev_state = vcpu->state;
	vcpu->state = new_state;

	get_schedule_lock(vcpu->pcpu_id);
	if (atomic_load_acq_32(&vcpu->running) == 1) {
		remove_vcpu_from_runqueue(vcpu);
		make_reschedule_request(vcpu);
		release_schedule_lock(vcpu->pcpu_id);

		if (vcpu->pcpu_id != pcpu_id) {
			while (atomic_load_acq_32(&vcpu->running) == 1)
				__asm__ __volatile("pause" ::: "memory");
		}
	} else {
		remove_vcpu_from_runqueue(vcpu);
		release_schedule_lock(vcpu->pcpu_id);
	}
}

void resume_vcpu(struct vcpu *vcpu)
{
	pr_dbg("vcpu%d resumed", vcpu->vcpu_id);

	vcpu->state = vcpu->prev_state;

	get_schedule_lock(vcpu->pcpu_id);
	if (vcpu->state == VCPU_RUNNING) {
		add_vcpu_to_runqueue(vcpu);
		make_reschedule_request(vcpu);
	}
	release_schedule_lock(vcpu->pcpu_id);
}

void schedule_vcpu(struct vcpu *vcpu)
{
	vcpu->state = VCPU_RUNNING;
	pr_dbg("vcpu%d scheduled", vcpu->vcpu_id);

	get_schedule_lock(vcpu->pcpu_id);
	add_vcpu_to_runqueue(vcpu);
	make_reschedule_request(vcpu);
	release_schedule_lock(vcpu->pcpu_id);
}

/* help function for vcpu create */
int prepare_vcpu(struct vm *vm, int pcpu_id)
{
	int ret = 0;
	struct vcpu *vcpu = NULL;

	ret = create_vcpu(pcpu_id, vm, &vcpu);
	ASSERT(ret == 0, "vcpu create failed");

	if (is_vcpu_bsp(vcpu)) {
		/* Load VM SW */
		if (!vm_sw_loader)
			vm_sw_loader = general_sw_loader;
		vm_sw_loader(vm, vcpu);
		vcpu->arch_vcpu.cpu_mode = PAGE_PROTECTED_MODE;
	} else {
		vcpu->arch_vcpu.cpu_mode = REAL_MODE;
	}

	/* init_vmcs is delayed to vcpu vmcs launch first time */

	/* initialize the vcpu tsc aux */
	vcpu->msr_tsc_aux_guest = vcpu->vcpu_id;

	set_pcpu_used(pcpu_id);

	INIT_LIST_HEAD(&vcpu->run_list);

	return ret;
}

void request_vcpu_pre_work(struct vcpu *vcpu, int pre_work_id)
{
	bitmap_set(pre_work_id, &vcpu->pending_pre_work);
}
