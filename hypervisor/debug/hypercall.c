/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <schedule.h>
#include <hypercall.h>
#include <version.h>
#include <reloc.h>

#ifdef PROFILING_ON
/**
 * @brief Execute profiling operation
 *
 * @param vm Pointer to VM data structure
 * @param cmd profiling command to be executed
 * @param cmd profiling command to be executed
 * @param param guest physical address. This gpa points to
 *             data structure required by each command
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
static int32_t hcall_profiling_ops(struct acrn_vm *vm, uint64_t cmd, uint64_t param)
{
	int32_t ret;
	switch (cmd) {
	case PROFILING_MSR_OPS:
		ret = profiling_msr_ops_all_cpus(vm, param);
		break;
	case PROFILING_GET_VMINFO:
		ret = profiling_vm_list_info(vm, param);
		break;
	case PROFILING_GET_VERSION:
		ret = profiling_get_version_info(vm, param);
		break;
	case PROFILING_GET_CONTROL_SWITCH:
		ret = profiling_get_control(vm, param);
		break;
	case PROFILING_SET_CONTROL_SWITCH:
		ret = profiling_set_control(vm, param);
		break;
	case PROFILING_CONFIG_PMI:
		ret = profiling_configure_pmi(vm, param);
		break;
	case PROFILING_CONFIG_VMSWITCH:
		ret = profiling_configure_vmsw(vm, param);
		break;
	case PROFILING_GET_PCPUID:
		ret = profiling_get_pcpu_id(vm, param);
		break;
	default:
		pr_err("%s: invalid profiling command %llu\n", __func__, cmd);
		ret = -1;
		break;
	}
 	return ret;
}
#endif /* PROFILING_ON */

/**
 * @brief Setup a share buffer for a VM.
 *
 * @param vm Pointer to VM data structure
 * @param param guest physical address. This gpa points to
 *              struct sbuf_setup_param
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
static int32_t hcall_setup_sbuf(struct acrn_vm *vm, uint64_t param)
{
	struct sbuf_setup_param ssp;
	uint64_t *hva;

	(void)memset((void *)&ssp, 0U, sizeof(ssp));

	if (copy_from_gpa(vm, &ssp, param, sizeof(ssp)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	if (ssp.gpa != 0U) {
		hva = (uint64_t *)gpa2hva(vm, ssp.gpa);
	} else {
		hva = (uint64_t *)NULL;
	}

	return sbuf_share_setup(ssp.pcpu_id, ssp.sbuf_id, hva);
}

/**
  * @brief Setup the hypervisor NPK log.
  *
  * @param vm Pointer to VM data structure
  * @param param guest physical address. This gpa points to
  *              struct hv_npk_log_param
  *
  * @pre Pointer vm shall point to VM0
  * @return 0 on success, non-zero on error.
  */
static int32_t hcall_setup_hv_npk_log(struct acrn_vm *vm, uint64_t param)
{
	struct hv_npk_log_param npk_param;

	(void)memset((void *)&npk_param, 0U, sizeof(npk_param));

	if (copy_from_gpa(vm, &npk_param, param, sizeof(npk_param)) != 0) {
		pr_err("%s: Unable copy param from vm\n", __func__);
		return -1;
	}

	npk_log_setup(&npk_param);

	if (copy_to_gpa(vm, &npk_param, param, sizeof(npk_param)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		return -1;
	}

	return 0;
}

/**
  * @brief Setup hypervisor debug infrastructure, such as share buffer, NPK log and profiling.
  *
  * @param vm Pointer to VM data structure
  * @param param1 hypercall param1 from guest
  * @param param2 hypercall param2 from guest
  * @param hypcall_id hypercall ID from guest
  *
  * @pre Pointer vm shall point to VM0
  * @return 0 on success, non-zero on error.
  */
int32_t hcall_debug(struct acrn_vm *vm, uint64_t param1, uint64_t param2, uint64_t hypcall_id)
{
	int32_t ret;

	/* Dispatch the debug hypercall handler */
	switch (hypcall_id) {
	case HC_SETUP_SBUF:
		ret = hcall_setup_sbuf(vm, param1);
		break;

	case HC_SETUP_HV_NPK_LOG:
		ret = hcall_setup_hv_npk_log(vm, param1);
		break;

	case HC_PROFILING_OPS:
		ret = hcall_profiling_ops(vm, param1, param2);
		break;

	default:
		pr_err("op %d: Invalid hypercall\n", hypcall_id);
		ret = -EPERM;
		break;
	}

	return ret;
}
