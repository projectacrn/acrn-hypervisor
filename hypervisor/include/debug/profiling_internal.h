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

#define MAX_MSR_LIST_NUM		15U
#define MAX_GROUP_NUM			1U

#define COLLECT_PROFILE_DATA	0
#define COLLECT_POWER_DATA		1

enum MSR_CMD_TYPE {
	MSR_OP_NONE = 0,
	MSR_OP_READ,
	MSR_OP_WRITE,
	MSR_OP_READ_CLEAR
};

typedef enum IPI_COMMANDS {
	IPI_MSR_OP = 0,
	IPI_PMU_CONFIG,
	IPI_PMU_START,
	IPI_PMU_STOP,
	IPI_VMSW_CONFIG,
	IPI_UNKNOWN,
} ipi_commands;

typedef enum SEP_PMU_STATE {
	PMU_INITIALIZED = 0,
	PMU_SETUP,
	PMU_RUNNING,
	PMU_UNINITIALIZED,
	PMU_UNKNOWN
} sep_pmu_state;

typedef enum PROFILING_SEP_FEATURE {
	CORE_PMU_SAMPLING = 0,
	CORE_PMU_COUNTING,
	PEBS_PMU_SAMPLING,
	LBR_PMU_SAMPLING,
	UNCORE_PMU_SAMPLING,
	VM_SWITCH_TRACING,
	MAX_SEP_FEATURE_ID
} profiling_sep_feature;

typedef enum SOCWATCH_STATE {
	SW_SETUP = 0,
	SW_RUNNING,
	SW_STOPPED
} socwatch_state;

typedef enum PROFILING_SOCWATCH_FEATURE {
	SOCWATCH_COMMAND = 0,
	SOCWATCH_VM_SWITCH_TRACING,
	MAX_SOCWATCH_FEATURE_ID
} profiling_socwatch_feature;

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

struct profiling_msr_op {
	/* value to write or location to write into */
	uint64_t	value;
	/* MSR address to read/write; last entry will have value of -1 */
	uint32_t	msr_id;
	/* parameter; usage depends on operation */
	uint16_t	param;
	uint8_t		msr_op_type;
	uint8_t		reg_type;
};

struct profiling_pmi_config {
	uint32_t num_groups;
	uint32_t trigger_count;
	struct profiling_msr_op initial_list[MAX_GROUP_NUM][MAX_MSR_LIST_NUM];
	struct profiling_msr_op start_list[MAX_GROUP_NUM][MAX_MSR_LIST_NUM];
	struct profiling_msr_op stop_list[MAX_GROUP_NUM][MAX_MSR_LIST_NUM];
	struct profiling_msr_op entry_list[MAX_GROUP_NUM][MAX_MSR_LIST_NUM];
	struct profiling_msr_op exit_list[MAX_GROUP_NUM][MAX_MSR_LIST_NUM];
};

struct profiling_vmsw_config {
	int32_t collector_id;
	struct profiling_msr_op initial_list[MAX_MSR_LIST_NUM];
	struct profiling_msr_op entry_list[MAX_MSR_LIST_NUM];
	struct profiling_msr_op exit_list[MAX_MSR_LIST_NUM];
};

struct vmexit_msr {
	uint32_t msr_idx;
	uint32_t reserved;
	uint64_t msr_data;
};

struct sep_state {
	sep_pmu_state pmu_state;

	uint32_t current_pmi_group_id;
	uint32_t num_pmi_groups;

	struct profiling_msr_op
		pmi_initial_msr_list[MAX_GROUP_NUM][MAX_MSR_LIST_NUM];
	struct profiling_msr_op
		pmi_start_msr_list[MAX_GROUP_NUM][MAX_MSR_LIST_NUM];
	struct profiling_msr_op
		pmi_stop_msr_list[MAX_GROUP_NUM][MAX_MSR_LIST_NUM];
	struct profiling_msr_op
		pmi_entry_msr_list[MAX_GROUP_NUM][MAX_MSR_LIST_NUM];
	struct profiling_msr_op
		pmi_exit_msr_list[MAX_GROUP_NUM][MAX_MSR_LIST_NUM];

	uint32_t current_vmsw_group_id;
	uint32_t num_msw_groups;
	struct profiling_msr_op
		vmsw_initial_msr_list[MAX_GROUP_NUM][MAX_MSR_LIST_NUM];
	struct profiling_msr_op
		vmsw_entry_msr_list[MAX_GROUP_NUM][MAX_MSR_LIST_NUM];
	struct profiling_msr_op
		vmsw_exit_msr_list[MAX_GROUP_NUM][MAX_MSR_LIST_NUM];

	/* sep handling statistics */
	uint32_t samples_logged;
	uint32_t samples_dropped;
	uint32_t valid_pmi_count;
	uint32_t total_pmi_count;
	uint32_t total_vmexit_count;
	uint32_t frozen_well;
	uint32_t frozen_delayed;
	uint32_t nofrozen_pmi;

	struct vmexit_msr vmexit_msr_list[MAX_MSR_LIST_NUM];
	int32_t vmexit_msr_cnt;
	uint64_t guest_debugctl_value;
	uint64_t saved_debugctl_value;
} __aligned(8);

/*
 * Wrapper containing  SEP sampling/profiling related data structures
 */
struct profiling_info_wrapper {
	struct sep_state		sep_state;
	ipi_commands			ipi_cmd;
	socwatch_state			soc_state;
} __aligned(8);

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
