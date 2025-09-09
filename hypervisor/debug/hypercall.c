/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <profiling.h>
#include <sbuf.h>
#include <hypercall.h>
#include <npk_log.h>
#include <asm/guest/vm.h>
#include <logmsg.h>
#include <cpu.h>

#ifdef PROFILING_ON
/**
 * @brief Execute profiling operation
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param param1 profiling command to be executed
 * @param param2 guest physical address. This gpa points to
 *             data structure required by each command
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_profiling_ops(struct acrn_vcpu *vcpu, __unused struct acrn_vm *target_vm,
		uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
	int32_t ret;
	uint64_t cmd = param1;

	switch (cmd) {
	case PROFILING_MSR_OPS:
		ret = profiling_msr_ops_all_cpus(vm, param2);
		break;
	case PROFILING_GET_VMINFO:
		ret = profiling_vm_list_info(vm, param2);
		break;
	case PROFILING_GET_VERSION:
		ret = profiling_get_version_info(vm, param2);
		break;
	case PROFILING_GET_CONTROL_SWITCH:
		ret = profiling_get_control(vm, param2);
		break;
	case PROFILING_SET_CONTROL_SWITCH:
		ret = profiling_set_control(vm, param2);
		break;
	case PROFILING_CONFIG_PMI:
		ret = profiling_configure_pmi(vm, param2);
		break;
	case PROFILING_CONFIG_VMSWITCH:
		ret = profiling_configure_vmsw(vm, param2);
		break;
	case PROFILING_GET_PCPUID:
		ret = profiling_get_pcpu_id(vm, param2);
		break;
	case PROFILING_GET_STATUS:
		ret = profiling_get_status_info(vm, param2);
		break;
	default:
		pr_err("%s: invalid profiling command %lu\n", __func__, cmd);
		ret = -1;
		break;
	}
 	return ret;
}
#endif /* PROFILING_ON */

/**
 * @brief Setup the hypervisor NPK log.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param param1 guest physical address. This gpa points to
 *              struct hv_npk_log_param
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_setup_hv_npk_log(struct acrn_vcpu *vcpu, __unused struct acrn_vm *target_vm,
		uint64_t param1, __unused uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
	struct hv_npk_log_param npk_param;

	if (copy_from_gpa(vm, &npk_param, param1, sizeof(npk_param)) != 0) {
		return -1;
	}

	npk_log_setup(&npk_param);

	return copy_to_gpa(vm, &npk_param, param1, sizeof(npk_param));
}

/**
 * @brief Get hardware related info
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param param1 Guest physical address pointing to struct acrn_hw_info
 *
 * @pre is_service_vm(vcpu->vm)
 * @pre param1 shall be a valid physical address
 *
 * @retval 0 on success
 * @retval -1 in case of error
 */
int32_t hcall_get_hw_info(struct acrn_vcpu *vcpu, __unused struct acrn_vm *target_vm,
		uint64_t param1, __unused uint64_t param2)
{
	struct acrn_hw_info hw_info;

	(void)memset((void *)&hw_info, 0U, sizeof(hw_info));

	hw_info.cpu_num = get_pcpu_nums();
	return copy_to_gpa(vcpu->vm, &hw_info, param1, sizeof(hw_info));
}
