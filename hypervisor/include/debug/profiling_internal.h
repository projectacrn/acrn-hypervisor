/*
 * Copyright (C) 2018 int32_tel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PROFILING_INTERNAL_H
#define PROFILING_INTERNAL_H

#ifdef PROFILING_ON

 typedef enum IPI_COMMANDS {
	IPI_MSR_OP = 0,
	IPI_PMU_CONFIG,
	IPI_PMU_START,
	IPI_PMU_STOP,
	IPI_VMSW_CONFIG,
	IPI_UNKNOWN,
} ipi_commands;
 /*
 * Wrapper containing  SEP sampling/profiling related data structures
 */
struct profiling_info_wrapper {
	ipi_commands			ipi_cmd;
};

int32_t profiling_get_version_info(struct vm *vm, uint64_t addr);
int32_t profiling_get_pcpu_id(struct vm *vm, uint64_t addr);
int32_t profiling_msr_ops_all_cpus(struct vm *vm, uint64_t addr);
int32_t profiling_vm_list_info(struct vm *vm, uint64_t addr);
int32_t profiling_get_control(struct vm *vm, uint64_t addr);
int32_t profiling_set_control(struct vm *vm, uint64_t addr);
int32_t profiling_configure_pmi(struct vm *vm, uint64_t addr);
int32_t profiling_configure_vmsw(struct vm *vm, uint64_t addr);
void profiling_ipi_handler(void *data);

#endif

#endif /* PROFILING_INTERNAL_H */
