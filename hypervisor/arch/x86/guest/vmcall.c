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

#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>
#include <acrn_hv_defs.h>
#include <hypercall.h>

int vmcall_vmexit_handler(struct vcpu *vcpu)
{
	int64_t ret = 0;
	struct vm *vm = vcpu->vm;
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
	/* hypercall ID from guest*/
	uint64_t hypcall_id = cur_context->guest_cpu_regs.regs.r8;
	/* hypercall param1 from guest*/
	uint64_t param1 = cur_context->guest_cpu_regs.regs.rdi;
	/* hypercall param2 from guest*/
	uint64_t param2 = cur_context->guest_cpu_regs.regs.rsi;
	/* hypercall param3 from guest, reserved*/
	/* uint64_t param3 = cur_context->guest_cpu_regs.regs.rdx; */
	/* hypercall param4 from guest, reserved*/
	/* uint64_t param4 = cur_context->guest_cpu_regs.regs.rcx; */

	/* Dispatch the hypercall handler */
	switch (hypcall_id) {
	case HC_GET_API_VERSION:
		/* vm0 will call HC_GET_API_VERSION as first hypercall, fixup
		 * vm0 vcpu here.
		 */
		vm_fixup(vm);
		ret = hcall_get_api_version(vm, param1);
		break;

	case HC_CREATE_VM:
		ret = hcall_create_vm(vm, param1);
		break;

	case HC_DESTROY_VM:
		ret = hcall_destroy_vm(param1);
		break;

	case HC_START_VM:
		ret = hcall_resume_vm(param1);
		break;

	case HC_PAUSE_VM:
		ret = hcall_pause_vm(param1);
		break;

	case HC_CREATE_VCPU:
		ret = hcall_create_vcpu(vm, param1, param2);
		break;

	case HC_ASSERT_IRQLINE:
		ret = hcall_assert_irqline(vm, param1, param2);
		break;

	case HC_DEASSERT_IRQLINE:
		ret = hcall_deassert_irqline(vm, param1, param2);
		break;

	case HC_PULSE_IRQLINE:
		ret = hcall_pulse_irqline(vm, param1, param2);
		break;

	case HC_INJECT_MSI:
		ret = hcall_inject_msi(vm, param1, param2);
		break;

	case HC_SET_IOREQ_BUFFER:
		ret = hcall_set_ioreq_buffer(vm, param1, param2);
		break;

	case HC_NOTIFY_REQUEST_FINISH:
		ret = hcall_notify_req_finish(param1, param2);
		break;

	case HC_VM_SET_MEMMAP:
		ret = hcall_set_vm_memmap(vm, param1, param2);
		break;

	case HC_VM_SET_MEMMAPS:
		ret = hcall_set_vm_memmaps(vm, param1);
		break;

	case HC_VM_PCI_MSIX_REMAP:
		ret = hcall_remap_pci_msix(vm, param1, param2);
		break;

	case HC_VM_GPA2HPA:
		ret = hcall_gpa_to_hpa(vm, param1, param2);
		break;

	case HC_ASSIGN_PTDEV:
		ret = hcall_assign_ptdev(vm, param1, param2);
		break;

	case HC_DEASSIGN_PTDEV:
		ret = hcall_deassign_ptdev(vm, param1, param2);
		break;

	case HC_SET_PTDEV_INTR_INFO:
		ret = hcall_set_ptdev_intr_info(vm, param1, param2);
		break;

	case HC_RESET_PTDEV_INTR_INFO:
		ret = hcall_reset_ptdev_intr_info(vm, param1, param2);
		break;

	case HC_SETUP_SBUF:
		ret = hcall_setup_sbuf(vm, param1);
		break;

	case HC_WORLD_SWITCH:
		ret = hcall_world_switch(vcpu);
		break;

	case HC_INITIALIZE_TRUSTY:
		ret = hcall_initialize_trusty(vcpu, param1);
		break;

	case HC_PM_GET_CPU_STATE:
		ret = hcall_get_cpu_pm_state(vm, param1, param2);
		break;

	default:
		pr_err("op %d: Invalid hypercall\n", hypcall_id);
		ret = -1;
		break;
	}

	cur_context->guest_cpu_regs.regs.rax = ret;

	TRACE_2L(TRC_VMEXIT_VMCALL, vm->attr.id, hypcall_id);

	return 0;
}
