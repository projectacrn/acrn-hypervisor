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
 *@pre Pointer vm shall point to VM0
 */
int32_t hcall_profiling_ops(struct acrn_vm *vm, uint64_t cmd, uint64_t param)
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
#endif
