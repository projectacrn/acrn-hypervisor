/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <asm/lib/spinlock.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/virq.h>
#include <acrn_hv_defs.h>
#include <hypercall.h>
#include <trace.h>
#include <logmsg.h>

struct hc_dispatch {
	int32_t (*handler)(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

	/* The permission_flags is a bitmap of guest flags indicating whether a VM can invoke this hypercall:
	 *
	 * - If permission_flags == 0UL (which is the default value), this hypercall can only be invoked by the SOS.
	 * - Otherwise, this hypercall can only be invoked by a VM whose guest flags have ALL set bits in
	 *   permission_flags.
	 */
	uint64_t permission_flags;
};

/* VM Dispatch table for Exit condition handling */
static const struct hc_dispatch hc_dispatch_table[] = {
	[HC_IDX(HC_GET_API_VERSION)] = {
		.handler = hcall_get_api_version},
	[HC_IDX(HC_SOS_OFFLINE_CPU)] = {
		.handler = hcall_sos_offline_cpu},
	[HC_IDX(HC_SET_CALLBACK_VECTOR)] = {
		.handler = hcall_set_callback_vector},
	[HC_IDX(HC_GET_PLATFORM_INFO)] = {
		.handler = hcall_get_platform_info},
	[HC_IDX(HC_CREATE_VM)] = {
		.handler = hcall_create_vm},
	[HC_IDX(HC_DESTROY_VM)] = {
		.handler = hcall_destroy_vm},
	[HC_IDX(HC_START_VM)] = {
		.handler = hcall_start_vm},
	[HC_IDX(HC_RESET_VM)] = {
		.handler = hcall_reset_vm},
	[HC_IDX(HC_PAUSE_VM)] = {
		.handler = hcall_pause_vm},
	[HC_IDX(HC_SET_VCPU_REGS)] = {
		.handler = hcall_set_vcpu_regs},
	[HC_IDX(HC_CREATE_VCPU)] = {
		.handler = hcall_create_vcpu},
	[HC_IDX(HC_SET_IRQLINE)] = {
		.handler = hcall_set_irqline},
	[HC_IDX(HC_INJECT_MSI)] = {
		.handler = hcall_inject_msi},
	[HC_IDX(HC_SET_IOREQ_BUFFER)] = {
		.handler = hcall_set_ioreq_buffer},
	[HC_IDX(HC_NOTIFY_REQUEST_FINISH)] = {
		.handler = hcall_notify_ioreq_finish},
	[HC_IDX(HC_VM_SET_MEMORY_REGIONS)] = {
		.handler = hcall_set_vm_memory_regions},
	[HC_IDX(HC_VM_WRITE_PROTECT_PAGE)] = {
		.handler = hcall_write_protect_page},
	[HC_IDX(HC_VM_GPA2HPA)] = {
		.handler = hcall_gpa_to_hpa},
	[HC_IDX(HC_ASSIGN_PCIDEV)] = {
		.handler = hcall_assign_pcidev},
	[HC_IDX(HC_DEASSIGN_PCIDEV)] = {
		.handler = hcall_deassign_pcidev},
	[HC_IDX(HC_ASSIGN_MMIODEV)] = {
		.handler = hcall_assign_mmiodev},
	[HC_IDX(HC_DEASSIGN_MMIODEV)] = {
		.handler = hcall_deassign_mmiodev},
	[HC_IDX(HC_ADD_VDEV)] = {
		.handler = hcall_add_vdev},
	[HC_IDX(HC_REMOVE_VDEV)] = {
		.handler = hcall_remove_vdev},
	[HC_IDX(HC_SET_PTDEV_INTR_INFO)] = {
		.handler = hcall_set_ptdev_intr_info},
	[HC_IDX(HC_RESET_PTDEV_INTR_INFO)] = {
		.handler = hcall_reset_ptdev_intr_info},
	[HC_IDX(HC_PM_GET_CPU_STATE)] = {
		.handler = hcall_get_cpu_pm_state},
	[HC_IDX(HC_VM_INTR_MONITOR)] = {
		.handler = hcall_vm_intr_monitor},
	[HC_IDX(HC_SETUP_SBUF)] = {
		.handler = hcall_setup_sbuf},
	[HC_IDX(HC_SETUP_HV_NPK_LOG)] = {
		.handler = hcall_setup_hv_npk_log},
	[HC_IDX(HC_PROFILING_OPS)] = {
		.handler = hcall_profiling_ops},
	[HC_IDX(HC_GET_HW_INFO)] = {
		.handler = hcall_get_hw_info},
	[HC_IDX(HC_INITIALIZE_TRUSTY)] = {
		.handler = hcall_initialize_trusty,
		.permission_flags = GUEST_FLAG_SECURE_WORLD_ENABLED},
	[HC_IDX(HC_WORLD_SWITCH)] = {
		.handler = hcall_world_switch,
		.permission_flags = GUEST_FLAG_SECURE_WORLD_ENABLED},
	[HC_IDX(HC_SAVE_RESTORE_SWORLD_CTX)] = {
		.handler = hcall_save_restore_sworld_ctx,
		.permission_flags = GUEST_FLAG_SECURE_WORLD_ENABLED},
};

#define GUEST_FLAGS_ALLOWING_HYPERCALLS GUEST_FLAG_SECURE_WORLD_ENABLED

struct acrn_vm *parse_target_vm(struct acrn_vm *sos_vm, uint64_t hcall_id, uint64_t param1, __unused uint64_t param2)
{
	struct acrn_vm *target_vm = NULL;
	uint16_t vm_id = ACRN_INVALID_VMID;
	struct acrn_vm_creation cv;
	struct set_regions regions;
	uint16_t relative_vm_id;

	switch (hcall_id) {
	case HC_CREATE_VM:
		if (copy_from_gpa(sos_vm, &cv, param1, sizeof(cv)) == 0) {
			vm_id = get_vmid_by_uuid(&cv.uuid[0]);
		}
		break;

	case HC_PM_GET_CPU_STATE:
		vm_id = rel_vmid_2_vmid(sos_vm->vm_id, (uint16_t)((param1 & PMCMD_VMID_MASK) >> PMCMD_VMID_SHIFT));
		break;

	case HC_VM_SET_MEMORY_REGIONS:
		if (copy_from_gpa(sos_vm, &regions, param1, sizeof(regions)) == 0) {
			/* the vmid in regions is a relative vm id, need to convert to absolute vm id */
			vm_id = rel_vmid_2_vmid(sos_vm->vm_id, regions.vmid);
		}
		break;
	case HC_GET_API_VERSION:
	case HC_SOS_OFFLINE_CPU:
	case HC_SET_CALLBACK_VECTOR:
	case HC_GET_PLATFORM_INFO:
	case HC_SETUP_SBUF:
	case HC_SETUP_HV_NPK_LOG:
	case HC_PROFILING_OPS:
	case HC_GET_HW_INFO:
		target_vm = sos_vm;
		break;
	default:
		relative_vm_id = (uint16_t)param1;
		vm_id = rel_vmid_2_vmid(sos_vm->vm_id, relative_vm_id);
		break;
	}

	if ((target_vm == NULL) && (vm_id  < CONFIG_MAX_VM_NUM)) {
		target_vm = get_vm_from_vmid(vm_id);
		if (hcall_id == HC_CREATE_VM) {
			target_vm->vm_id = vm_id;
		}
	}

	return target_vm;
}

static int32_t dispatch_hypercall(struct acrn_vcpu *vcpu)
{
	int32_t ret = -EINVAL;
	struct acrn_vm *vm = vcpu->vm;
	uint64_t guest_flags = get_vm_config(vm->vm_id)->guest_flags;  /* hypercall ID from guest */
	uint64_t hcall_id = vcpu_get_gpreg(vcpu, CPU_REG_R8);  /* hypercall ID from guest */

	if (HC_IDX(hcall_id) < ARRAY_SIZE(hc_dispatch_table)) {
		const struct hc_dispatch *dispatch = &(hc_dispatch_table[HC_IDX(hcall_id)]);
		uint64_t permission_flags = dispatch->permission_flags;

		if (dispatch->handler != NULL) {
			uint64_t param1 = vcpu_get_gpreg(vcpu, CPU_REG_RDI);  /* hypercall param1 from guest */
			uint64_t param2 = vcpu_get_gpreg(vcpu, CPU_REG_RSI);  /* hypercall param2 from guest */

			if ((permission_flags == 0UL) && is_sos_vm(vm)) {
				/* A permission_flags of 0 indicates that this hypercall is for SOS to manage
				 * post-launched VMs.
				 */
				struct acrn_vm *target_vm = parse_target_vm(vm, hcall_id, param1, param2);

				if ((target_vm != NULL) && !is_prelaunched_vm(target_vm)) {
					get_vm_lock(target_vm);
					ret = dispatch->handler(vcpu, target_vm, param1, param2);
					put_vm_lock(target_vm);
				}
			} else if ((permission_flags != 0UL) &&
					((guest_flags & permission_flags) == permission_flags)) {
				ret = dispatch->handler(vcpu, vcpu->vm, param1, param2);
			} else {
				/* The vCPU is not allowed to invoke the given hypercall. Keep `ret` as -EINVAL and no
				 * further actions required.
				 */
			}
		}
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
	uint64_t guest_flags = get_vm_config(vm->vm_id)->guest_flags;

	/*
	 * The following permission checks are applied to hypercalls.
	 *
	 * 1. Only SOS and VMs with specific guest flags (referred to as 'allowed VMs' hereinafter) can invoke
	 *    hypercalls by executing the `vmcall` instruction. Attempts to execute the `vmcall` instruction in the
	 *    other VMs will trigger #UD.
	 * 2. Attempts to execute the `vmcall` instruction from ring 1, 2 or 3 in an allowed VM will trigger #GP(0).
	 * 3. An allowed VM is permitted to only invoke some of the supported hypercalls depending on its load order and
	 *    guest flags. Attempts to invoke an unpermitted hypercall will make a vCPU see -EINVAL as the return
	 *    value. No exception is triggered in this case.
	 */
	if (!is_sos_vm(vm) && ((guest_flags & GUEST_FLAGS_ALLOWING_HYPERCALLS) == 0UL)) {
		vcpu_inject_ud(vcpu);
		ret = -ENODEV;
	} else if (!is_hypercall_from_ring0()) {
		vcpu_inject_gp(vcpu, 0U);
		ret = -EACCES;
	} else {
		ret = dispatch_hypercall(vcpu);
	}

	if ((ret != -EACCES) && (ret != -ENODEV)) {
		vcpu_set_gpreg(vcpu, CPU_REG_RAX, (uint64_t)ret);
	}
	if (ret < 0) {
		pr_err("ret=%d hypercall=0x%lx failed in %s\n", ret, hypcall_id, __func__);
	}
	TRACE_2L(TRACE_VMEXIT_VMCALL, vm->vm_id, hypcall_id);

	return 0;
}
