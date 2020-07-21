/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <spinlock.h>
#include <vcpu.h>
#include <vm.h>
#include <acrn_hv_defs.h>
#include <hypercall.h>
#include <trace.h>
#include <logmsg.h>

static int32_t dispatch_sos_hypercall(const struct acrn_vcpu *vcpu)
{
	struct acrn_vm *sos_vm = vcpu->vm;
	/* hypercall ID from guest*/
	uint64_t hypcall_id = vcpu_get_gpreg(vcpu, CPU_REG_R8);
	/* hypercall param1 from guest*/
	uint64_t param1 = vcpu_get_gpreg(vcpu, CPU_REG_RDI);
	/* hypercall param2 from guest*/
	uint64_t param2 = vcpu_get_gpreg(vcpu, CPU_REG_RSI);
	/* hypercall param1 is a relative vm id from SOS view */
	uint16_t relative_vm_id = (uint16_t)param1;
	uint16_t vm_id = rel_vmid_2_vmid(sos_vm->vm_id, relative_vm_id);
	int32_t ret = -1;

	switch (hypcall_id) {
	case HC_SOS_OFFLINE_CPU:
		ret = hcall_sos_offline_cpu(sos_vm, param1);
		break;
	case HC_GET_API_VERSION:
		ret = hcall_get_api_version(sos_vm, param1);
		break;

	case HC_GET_PLATFORM_INFO:
		ret = hcall_get_platform_info(sos_vm, param1);
		break;

	case HC_SET_CALLBACK_VECTOR:
		ret = hcall_set_callback_vector(sos_vm, param1);

		break;

	case HC_CREATE_VM:
		ret = hcall_create_vm(sos_vm, param1);
		break;

	case HC_DESTROY_VM:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_destroy_vm(vm_id);
		}
		break;

	case HC_START_VM:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_start_vm(vm_id);
		}
		break;

	case HC_RESET_VM:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_reset_vm(vm_id);
		}
		break;

	case HC_PAUSE_VM:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_pause_vm(vm_id);
		}
		break;

	case HC_CREATE_VCPU:
		ret = 0;
		break;

	case HC_SET_VCPU_REGS:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_set_vcpu_regs(sos_vm, vm_id, param2);
		}
		break;

	case HC_SET_IRQLINE:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_set_irqline(sos_vm, vm_id,
					(struct acrn_irqline_ops *)&param2);
		}
		break;

	case HC_INJECT_MSI:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_inject_msi(sos_vm, vm_id, param2);
		}
		break;

	case HC_SET_IOREQ_BUFFER:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_set_ioreq_buffer(sos_vm, vm_id, param2);
		}
		break;

	case HC_NOTIFY_REQUEST_FINISH:
		/* param1: relative vmid to sos, vm_id: absolute vmid
		 * param2: vcpu_id */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_notify_ioreq_finish(vm_id,
				(uint16_t)param2);
		}
		break;

	case HC_VM_SET_MEMORY_REGIONS:
		ret = hcall_set_vm_memory_regions(sos_vm, param1);
		break;

	case HC_VM_WRITE_PROTECT_PAGE:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_write_protect_page(sos_vm, vm_id, param2);
		}
		break;

	/*
	 * Don't do MSI remapping and make the pmsi_data equal to vmsi_data
	 * This is a temporary solution before this hypercall is removed from SOS
	 */
	case HC_VM_PCI_MSIX_REMAP:
		ret = 0;
		break;

	case HC_VM_GPA2HPA:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if ((vm_id < CONFIG_MAX_VM_NUM) && !is_prelaunched_vm(get_vm_from_vmid(vm_id))) {
			ret = hcall_gpa_to_hpa(sos_vm, vm_id, param2);
		}
		break;

	case HC_ASSIGN_PCIDEV:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_assign_pcidev(sos_vm, vm_id, param2);
		}
		break;

	case HC_DEASSIGN_PCIDEV:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_deassign_pcidev(sos_vm, vm_id, param2);
		}
		break;

	case HC_ASSIGN_MMIODEV:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_assign_mmiodev(sos_vm, vm_id, param2);
		}
		break;

	case HC_DEASSIGN_MMIODEV:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_deassign_mmiodev(sos_vm, vm_id, param2);
		}
		break;

	case HC_SET_PTDEV_INTR_INFO:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_set_ptdev_intr_info(sos_vm, vm_id, param2);
		}
		break;

	case HC_RESET_PTDEV_INTR_INFO:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_reset_ptdev_intr_info(sos_vm, vm_id, param2);
		}
		break;

	case HC_PM_GET_CPU_STATE:
		ret = hcall_get_cpu_pm_state(sos_vm, param1, param2);
		break;

	case HC_VM_INTR_MONITOR:
		/* param1: relative vmid to sos, vm_id: absolute vmid */
		if (is_valid_postlaunched_vmid(vm_id)) {
			ret = hcall_vm_intr_monitor(sos_vm, vm_id, param2);
		}
		break;

	default:
		ret = hcall_debug(sos_vm, param1, param2, hypcall_id);
		break;
	}

	return ret;
}

/*
 * Pass return value to SOS by register rax.
 * This function should always return 0 since we shouldn't
 * deal with hypercall error in hypervisor.
 */
int32_t vmcall_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int32_t ret;
	struct acrn_vm *vm = vcpu->vm;
	/* hypercall ID from guest*/
	uint64_t hypcall_id = vcpu_get_gpreg(vcpu, CPU_REG_R8);

	if (!is_hypercall_from_ring0()) {
		pr_err("hypercall 0x%lx is only allowed from RING-0!\n", hypcall_id);
		vcpu_inject_gp(vcpu, 0U);
		ret = -EACCES;
	} else if (hypcall_id == HC_WORLD_SWITCH) {
		ret = hcall_world_switch(vcpu);
	} else if (hypcall_id == HC_INITIALIZE_TRUSTY) {
		/* hypercall param1 from guest*/
		uint64_t param1 = vcpu_get_gpreg(vcpu, CPU_REG_RDI);

		ret = hcall_initialize_trusty(vcpu, param1);
	} else if (hypcall_id == HC_SAVE_RESTORE_SWORLD_CTX) {
		ret = hcall_save_restore_sworld_ctx(vcpu);
	} else if (is_sos_vm(vm)) {
		/* Dispatch the hypercall handler */
		ret = dispatch_sos_hypercall(vcpu);
	} else  {
		pr_err("hypercall 0x%lx is only allowed from SOS_VM!\n", hypcall_id);
		vcpu_inject_ud(vcpu);
		ret = -ENODEV;
	}

	if ((ret != -EACCES) && (ret != -ENODEV)) {
		vcpu_set_gpreg(vcpu, CPU_REG_RAX, (uint64_t)ret);
	}
	TRACE_2L(TRACE_VMEXIT_VMCALL, vm->vm_id, hypcall_id);

	return 0;
}
