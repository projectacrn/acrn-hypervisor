/*
 * Copyright (C) 2018 int32_tel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PROFILING_INTERNAL_H
#define PROFILING_INTERNAL_H

#ifdef PROFILING_ON

#include <x86/guest/vcpu.h>
#include <x86/vm_config.h>

#define MAX_MSR_LIST_NUM		15U
#define MAX_PROFILING_MSR_STORE_NUM	1
#define MAX_HV_MSR_LIST_NUM		(MSR_AREA_COUNT)
#define MAX_GROUP_NUM		1U

#define COLLECT_PROFILE_DATA	0
#define COLLECT_POWER_DATA	1

#define SEP_BUF_ENTRY_SIZE 	32U
#define SOCWATCH_MSR_OP		100U

#define MAGIC_NUMBER		0x99999988U

enum MSR_CMD_STATUS {
	MSR_OP_READY = 0,
	MSR_OP_REQUESTED,
	MSR_OP_HANDLED
};
enum MSR_CMD_TYPE {
	MSR_OP_NONE = 0,
	MSR_OP_READ,
	MSR_OP_WRITE,
	MSR_OP_READ_CLEAR
};

enum PMU_MSR_TYPE {
	PMU_MSR_CCCR = 0,
	PMU_MSR_ESCR,
	PMU_MSR_DATA
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
	int32_t	collector_id;
	int32_t	reserved;
	uint64_t switches;
};

struct profiling_vcpu_pcpu_map {
	int16_t vcpu_id;
	int16_t pcpu_id;
	uint32_t apic_id;
};

struct profiling_vm_info {
	uint16_t vm_id_num;
	uint8_t uuid[16];
	char vm_name[16];
	uint16_t num_vcpus;
	struct profiling_vcpu_pcpu_map cpu_map[MAX_VCPUS_PER_VM];
};

struct profiling_vm_info_list {
	uint16_t num_vms;
	struct profiling_vm_info vm_list[CONFIG_MAX_VM_NUM+1];
};

struct sw_msr_op_info {
	uint64_t core_msr[MAX_MSR_LIST_NUM];
	uint32_t cpu_id;
	uint32_t valid_entries;
	uint16_t sample_id;
};

struct profiling_msr_op {
	/* value to write or location to write into */
	uint64_t value;
	/* MSR address to read/write; last entry will have value of -1 */
	uint32_t msr_id;
	/* parameter; usage depends on operation */
	uint16_t param;
	uint8_t	msr_op_type;
	uint8_t	reg_type;
};

struct profiling_msr_ops_list {
	int32_t	collector_id;
	uint32_t num_entries;
	int32_t	msr_op_state;
	struct profiling_msr_op entries[MAX_MSR_LIST_NUM];
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

struct guest_vm_info {
	uint64_t vmenter_tsc;
	uint64_t vmexit_tsc;
	uint64_t vmexit_reason;
	uint64_t guest_rip;
	uint64_t guest_rflags;
	uint64_t guest_cs;
	uint16_t guest_vm_id;
	int32_t external_vector;
};

struct profiling_status {
	uint32_t samples_logged;
	uint32_t samples_dropped;
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

	struct msr_store_entry vmexit_msr_list[MAX_PROFILING_MSR_STORE_NUM + MAX_HV_MSR_LIST_NUM];
	uint32_t vmexit_msr_cnt;
	uint64_t guest_debugctl_value;
	uint64_t saved_debugctl_value;
} __aligned(8);

struct data_header {
	int32_t collector_id;
	uint16_t cpu_id;
	uint16_t data_type;
	uint64_t tsc;
	uint64_t payload_size;
	uint64_t reserved;
} __aligned(SEP_BUF_ENTRY_SIZE);

#define DATA_HEADER_SIZE ((uint64_t)sizeof(struct data_header))
struct core_pmu_sample {
	/* context where PMI is triggered */
	uint16_t os_id;
	/* reserved */
	uint16_t reserved;
	/* the task id */
	uint32_t task_id;
	/* instruction pointer */
	uint64_t rip;
	/* the task name */
	char task[16];
	/* physical cpu ID */
	uint32_t cpu_id;
	/* the process id */
	uint32_t process_id;
	/* perf global status msr value (for overflow status) */
	uint64_t overflow_status;
	/* rflags */
	uint32_t rflags;
	/* code segment */
	uint32_t cs;
} __aligned(SEP_BUF_ENTRY_SIZE);

#define CORE_PMU_SAMPLE_SIZE ((uint64_t)sizeof(struct core_pmu_sample))
#define NUM_LBR_ENTRY		32

struct lbr_pmu_sample {
	/* LBR TOS */
	uint64_t lbr_tos;
	/* LBR FROM IP */
	uint64_t lbr_from_ip[NUM_LBR_ENTRY];
	/* LBR TO IP */
	uint64_t lbr_to_ip[NUM_LBR_ENTRY];
	/* LBR info */
	uint64_t lbr_info[NUM_LBR_ENTRY];
} __aligned(SEP_BUF_ENTRY_SIZE);

#define LBR_PMU_SAMPLE_SIZE ((uint64_t)sizeof(struct lbr_pmu_sample))
struct pmu_sample {
	/* core pmu sample */
	struct core_pmu_sample csample;
	/* lbr pmu sample */
	struct lbr_pmu_sample lsample;
} __aligned(SEP_BUF_ENTRY_SIZE);

struct vm_switch_trace {
	uint64_t vm_enter_tsc;
	uint64_t vm_exit_tsc;
	uint64_t vm_exit_reason;
	uint16_t os_id;
	uint16_t reserved;
}__aligned(SEP_BUF_ENTRY_SIZE);

#define VM_SWITCH_TRACE_SIZE ((uint64_t)sizeof(struct vm_switch_trace))
/*
 * Wrapper containing  SEP sampling/profiling related data structures
 */
struct profiling_info_wrapper {
	struct profiling_msr_ops_list *msr_node;
	struct sep_state s_state;
	struct guest_vm_info vm_info;
	ipi_commands ipi_cmd;
	struct pmu_sample p_sample;
	struct vm_switch_trace vm_trace;
	socwatch_state soc_state;
	struct sw_msr_op_info sw_msr_info;
	spinlock_t sw_lock;
} __aligned(8);

int32_t profiling_get_version_info(struct acrn_vm *vm, uint64_t addr);
int32_t profiling_get_pcpu_id(struct acrn_vm *vm, uint64_t addr);
int32_t profiling_msr_ops_all_cpus(struct acrn_vm *vm, uint64_t addr);
int32_t profiling_vm_list_info(struct acrn_vm *vm, uint64_t addr);
int32_t profiling_get_control(struct acrn_vm *vm, uint64_t addr);
int32_t profiling_set_control(struct acrn_vm *vm, uint64_t addr);
int32_t profiling_configure_pmi(struct acrn_vm *vm, uint64_t addr);
int32_t profiling_configure_vmsw(struct acrn_vm *vm, uint64_t addr);
void profiling_ipi_handler(void *data);
int32_t profiling_get_status_info(struct acrn_vm *vm, uint64_t addr);

#endif

#endif /* PROFILING_INTERNAL_H */
