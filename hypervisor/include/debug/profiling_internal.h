/*
 * Copyright (C) 2018 int32_tel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PROFILING_INTERNAL_H
#define PROFILING_INTERNAL_H

#ifdef PROFILING_ON

#define MAX_NR_VCPUS			8
#define MAX_NR_VMS				6

#define COLLECT_PROFILE_DATA	0
#define COLLECT_POWER_DATA		1

typedef enum IPI_COMMANDS {
	IPI_MSR_OP = 0,
	IPI_PMU_CONFIG,
	IPI_PMU_START,
	IPI_PMU_STOP,
	IPI_VMSW_CONFIG,
	IPI_UNKNOWN,
} ipi_commands;

typedef enum PROFILING_SEP_FEATURE {
	CORE_PMU_SAMPLING = 0,
	CORE_PMU_COUNTING,
	PEBS_PMU_SAMPLING,
	LBR_PMU_SAMPLING,
	UNCORE_PMU_SAMPLING,
	VM_SWITCH_TRACING,
	MAX_SEP_FEATURE_ID
} profiling_sep_feature;

struct profiling_version_info {
	int32_t major;
	int32_t minor;
	int64_t supported_features;
	int64_t reserved;
};

struct profiling_pcpuid {
	uint32_t leaf;
	uint32_t subleaf;
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
};

struct profiling_control {
	int32_t		collector_id;
	int32_t		reserved;
	uint64_t	switches;
};

struct profiling_vcpu_pcpu_map {
	int32_t vcpu_id;
	int32_t pcpu_id;
	int32_t apic_id;
};

struct profiling_vm_info {
	int32_t		vm_id_num;
	unsigned char	guid[16];
	char		vm_name[16];
	int32_t		num_vcpus;
	struct profiling_vcpu_pcpu_map	cpu_map[MAX_NR_VCPUS];
};

struct profiling_vm_info_list {
	int32_t num_vms;
	struct profiling_vm_info vm_list[MAX_NR_VMS];
};

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
