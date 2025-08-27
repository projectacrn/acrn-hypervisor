/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vcpu.h>
#include <vm.h>
#include <errno.h>
#include <per_cpu.h>
#include <sprintf.h>
#include <logmsg.h>
#include <schedule.h>

bool is_vcpu_bsp(const struct acrn_vcpu *vcpu)
{
	return (vcpu->vcpu_id == BSP_CPU_ID);
}

static void init_vcpu_thread(struct acrn_vcpu *vcpu, uint16_t pcpu_id)
{
	struct acrn_vm *vm = vcpu->vm;
	char thread_name[16];

	snprintf(thread_name, 16U, "vm%hu:vcpu%hu", vm->vm_id, vcpu->vcpu_id);
	(void)strncpy_s(vcpu->thread_obj.name, 16U, thread_name, 16U);
	vcpu->thread_obj.sched_ctl = &per_cpu(sched_ctl, pcpu_id);
	vcpu->thread_obj.thread_entry = arch_vcpu_thread;
	vcpu->thread_obj.pcpu_id = pcpu_id;
	vcpu->thread_obj.host_sp = arch_build_stack_frame(vcpu);
	vcpu->thread_obj.switch_out = arch_context_switch_out;
	vcpu->thread_obj.switch_in = arch_context_switch_in;
	init_thread_data(&vcpu->thread_obj, &get_vm_config(vm->vm_id)->sched_params);
}

/*
 * @brief Update the state of vCPU and state of vlapic
 *
 * The vlapic state of VM shall be updated for some vCPU
 * state update cases, such as from VCPU_INIT to VCPU_RUNNING.

 * @pre (vcpu != NULL)
 */
void vcpu_set_state(struct acrn_vcpu *vcpu, enum vcpu_state new_state)
{
	vcpu->state = new_state;
}

/**
 * @brief create a vcpu for the target vm
 *
 * Creates/allocates a vCPU instance, with initialization for its vcpu_id,
 * vpid, vmcs, vlapic, etc. It sets the init vCPU state to VCPU_INIT
 *
 * The call has the following assumption:
 * - The caller is responsible to lock-protect this call
 * - We don't support having more than one vCPUs of the same VM
 *   on the same pCPU
 *
 * @param[in] pcpu_id created vcpu will run on this pcpu
 * @param[in] vm pointer to vm data structure, this vcpu will owned by this vm.
 *
 * @retval 0 vcpu created successfully, other values failed.
 */
int32_t create_vcpu(struct acrn_vm *vm, uint16_t pcpu_id)
{
	struct acrn_vcpu *vcpu;
	uint16_t vcpu_id;
	int32_t i, ret;

	pr_info("Creating VCPU on PCPU%hu", pcpu_id);

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

		cpu_compiler_barrier();

		/*
		 * We maintain a per-pCPU array of vCPUs, and use vm_id as the index to the
		 * vCPU array
		 */
		per_cpu(vcpu_array, pcpu_id)[vm->vm_id] = vcpu;

		(void)memset((void *)&vcpu->req, 0U, sizeof(struct io_request));
		vm->hw.created_vcpus++;

		/* pcpuid_from_vcpu works after this call */
		init_vcpu_thread(vcpu, pcpu_id);

		/* init event */
		for (i = 0; i < MAX_VCPU_EVENT_NUM; i++) {
			init_event(&vcpu->events[i]);
		}

		ret = arch_init_vcpu(vcpu);

		if (ret == 0) {
			vcpu->state = VCPU_INIT;
		}
	} else {
		pr_err("%s, vcpu id is invalid!\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

/*
 *  @pre vcpu != NULL
 *  @pre vcpu->state == VCPU_ZOMBIE
 */
void destroy_vcpu(struct acrn_vcpu *vcpu)
{
	arch_deinit_vcpu(vcpu);

	/* TODO: Move ever_run_vcpu to x86 specific */
	per_cpu(ever_run_vcpu, pcpuid_from_vcpu(vcpu)) = NULL;

	/* This operation must be atomic to avoid contention with posted interrupt handler */
	per_cpu(vcpu_array, pcpuid_from_vcpu(vcpu))[vcpu->vm->vm_id] = NULL;

	vcpu_set_state(vcpu, VCPU_OFFLINE);
}
