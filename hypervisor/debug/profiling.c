/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PROFILING_ON

#include <hypervisor.h>

#define ACRN_DBG_PROFILING		5U

#define MAJOR_VERSION			1
#define MINOR_VERSION			0

#define LVT_PERFCTR_BIT_MASK		0x10000U

static uint64_t	sep_collection_switch;

static uint32_t profiling_pmi_irq = IRQ_INVALID;

static void profiling_initialize_vmsw(void)
{
	/* to be implemented */
}

/*
 * Configure the PMU's for sep/socwatch profiling.
 */
static void profiling_initialize_pmi(void)
{
	/* to be implemented */
}

/*
 * Enable all the Performance Monitoring Control registers.
 */
static void profiling_enable_pmu(void)
{
	/* to be implemented */
}

/*
 * Disable all Performance Monitoring Control registers
 */
static void profiling_disable_pmu(void)
{
	/* to be implemented */
}

/*
 * Performs MSR operations - read, write and clear
 */
static void profiling_handle_msrops(void)
{
	/* to be implemented */
}

/*
 * Interrupt handler for performance monitoring interrupts
 */
static void profiling_pmi_handler(__unused unsigned int irq, __unused void *data)
{
	/* to be implemented */
}

/*
 * Performs MSR operations on all the CPU's
 */

int32_t profiling_msr_ops_all_cpus(__unused struct vm *vm, __unused uint64_t addr)
{
	/* to be implemented
	 * call to smp_call_function profiling_ipi_handler
	 */
	return 0;
}

/*
 * Generate VM info list
 */
int32_t profiling_vm_list_info(struct vm *vm, uint64_t addr)
{
	struct vm *tmp_vm;
	struct vcpu *vcpu;
	int32_t vm_idx;
	uint16_t i, j;
	struct profiling_vm_info_list vm_info_list;

	(void)memset((void *)&vm_info_list, 0U, sizeof(vm_info_list));

	dev_dbg(ACRN_DBG_PROFILING, "%s: entering", __func__);

	if (copy_from_gpa(vm, &vm_info_list, addr, sizeof(vm_info_list)) != 0) {
		pr_err("%s: Unable to copy addr from vm\n", __func__);
		return -EINVAL;
	}

	vm_idx = 0;
	vm_info_list.vm_list[vm_idx].vm_id_num = -1;
	(void)memcpy_s((void *)vm_info_list.vm_list[vm_idx].vm_name, 4U, "VMM\0", 4U);
	for (i = 0U; i < phys_cpu_num; i++) {
		vm_info_list.vm_list[vm_idx].cpu_map[i].vcpu_id = (int32_t)i;
		vm_info_list.vm_list[vm_idx].cpu_map[i].pcpu_id = (int32_t)i;
		vm_info_list.vm_list[vm_idx].cpu_map[i].apic_id
			= (int32_t)per_cpu(lapic_id, i);
	}
	vm_info_list.vm_list[vm_idx].num_vcpus = (int32_t)i;
	vm_info_list.num_vms = 1;

	for (j = 0U; j < CONFIG_MAX_VM_NUM; j++) {
		tmp_vm = get_vm_from_vmid(j);
		if (tmp_vm == NULL) {
			break;
		}
		vm_info_list.num_vms++;
		vm_idx++;

		vm_info_list.vm_list[vm_idx].vm_id_num = (int32_t)tmp_vm->vm_id;
		(void)memcpy_s((void *)vm_info_list.vm_list[vm_idx].guid,
			16U, tmp_vm->GUID, 16U);
		snprintf(vm_info_list.vm_list[vm_idx].vm_name, 16U, "vm_%d",
				tmp_vm->vm_id, 16U);
		vm_info_list.vm_list[vm_idx].num_vcpus = 0;
		i = 0U;
		foreach_vcpu(i, tmp_vm, vcpu) {
			vm_info_list.vm_list[vm_idx].cpu_map[i].vcpu_id
					= (int32_t)vcpu->vcpu_id;
			vm_info_list.vm_list[vm_idx].cpu_map[i].pcpu_id
					= (int32_t)vcpu->pcpu_id;
			vm_info_list.vm_list[vm_idx].cpu_map[i].apic_id = 0;
			vm_info_list.vm_list[vm_idx].num_vcpus++;
		}
	}

	if (copy_to_gpa(vm, &vm_info_list, addr, sizeof(vm_info_list)) != 0) {
		pr_err("%s: Unable to copy addr to vm\n", __func__);
		return -EINVAL;
	}

	dev_dbg(ACRN_DBG_PROFILING, "%s: exiting", __func__);
	return 0;
}

/*
 * Sep/socwatch profiling version
 */
int32_t profiling_get_version_info(struct vm *vm, uint64_t addr)
{
	struct profiling_version_info ver_info;

	(void)memset((void *)&ver_info, 0U, sizeof(ver_info));

	dev_dbg(ACRN_DBG_PROFILING, "%s: entering", __func__);

	if (copy_from_gpa(vm, &ver_info, addr, sizeof(ver_info)) != 0) {
		pr_err("%s: Unable to copy addr from vm\n", __func__);
		return -EINVAL;
	}

	ver_info.major = MAJOR_VERSION;
	ver_info.minor = MINOR_VERSION;
	ver_info.supported_features = (int64_t)
					((1U << (uint64_t)CORE_PMU_SAMPLING) |
					(1U << (uint64_t)CORE_PMU_COUNTING) |
					(1U << (uint64_t)LBR_PMU_SAMPLING) |
					(1U << (uint64_t)VM_SWITCH_TRACING));

	if (copy_to_gpa(vm, &ver_info, addr, sizeof(ver_info)) != 0) {
		pr_err("%s: Unable to copy addr to vm\n", __func__);
		return -EINVAL;
	}

	dev_dbg(ACRN_DBG_PROFILING, "%s: exiting", __func__);

	return 0;
}

/*
 * Gets type of profiling - sep/socwatch
 */
int32_t profiling_get_control(struct vm *vm, uint64_t addr)
{
	struct profiling_control prof_control;

	(void)memset((void *)&prof_control, 0U, sizeof(prof_control));

	dev_dbg(ACRN_DBG_PROFILING, "%s: entering", __func__);

	if (copy_from_gpa(vm, &prof_control, addr, sizeof(prof_control)) != 0) {
		pr_err("%s: Unable to copy addr from vm\n", __func__);
		return -EINVAL;
	}

	switch (prof_control.collector_id) {
	case COLLECT_PROFILE_DATA:
		prof_control.switches = sep_collection_switch;
		break;
	case COLLECT_POWER_DATA:
		break;
	default:
		pr_err("%s: unknown collector %d",
			__func__, prof_control.collector_id);
		break;
	}

	if (copy_to_gpa(vm, &prof_control, addr, sizeof(prof_control)) != 0) {
		pr_err("%s: Unable to copy addr to vm\n", __func__);
		return -EINVAL;
	}

	dev_dbg(ACRN_DBG_PROFILING, "%s: exiting", __func__);

	return 0;
}

/*
 * Update the profiling type based on control switch
 */
int32_t profiling_set_control(__unused struct vm *vm, __unused uint64_t addr)
{
	/* to be implemented */
	return 0;
}

/*
 * Configure PMI on all cpus
 */
int32_t profiling_configure_pmi(__unused struct vm *vm, __unused uint64_t addr)
{
	/* to be implemented
	 * call to smp_call_function profiling_ipi_handler
	 */
	return 0;
}

/*
 * Configure for VM-switch data on all cpus
 */
int32_t profiling_configure_vmsw(__unused struct vm *vm, __unused uint64_t addr)
{
	/* to be implemented
	 * call to smp_call_function profiling_ipi_handler
	 */
	return 0;
}

/*
 * Get the physical cpu id
 */
int32_t profiling_get_pcpu_id(struct vm *vm, uint64_t addr)
{
	struct profiling_pcpuid pcpuid;

	(void)memset((void *)&pcpuid, 0U, sizeof(pcpuid));

	dev_dbg(ACRN_DBG_PROFILING, "%s: entering", __func__);

	if (copy_from_gpa(vm, &pcpuid, addr, sizeof(pcpuid)) != 0) {
		pr_err("%s: Unable to copy addr from vm\n", __func__);
		return -EINVAL;
	}

	cpuid_subleaf(pcpuid.leaf, pcpuid.subleaf, &pcpuid.eax,
			&pcpuid.ebx, &pcpuid.ecx, &pcpuid.edx);

	if (copy_to_gpa(vm, &pcpuid, addr, sizeof(pcpuid)) != 0) {
		pr_err("%s: Unable to copy param to vm\n", __func__);
		return -EINVAL;
	}

	dev_dbg(ACRN_DBG_PROFILING, "%s: exiting", __func__);

	return 0;
}

/*
 * IPI interrupt handler function
 */
void profiling_ipi_handler(__unused void *data)
{
	switch (get_cpu_var(profiling_info.ipi_cmd)) {
	case IPI_PMU_START:
		profiling_enable_pmu();
		break;
	case IPI_PMU_STOP:
		profiling_disable_pmu();
		break;
	case IPI_MSR_OP:
		profiling_handle_msrops();
		break;
	case IPI_PMU_CONFIG:
		profiling_initialize_pmi();
		break;
	case IPI_VMSW_CONFIG:
		profiling_initialize_vmsw();
		break;
	default:
		pr_err("%s: unknown IPI command %d on cpu %d",
		__func__, get_cpu_var(profiling_info.ipi_cmd), get_cpu_id());
		break;
	}
	get_cpu_var(profiling_info.ipi_cmd) = IPI_UNKNOWN;
}

/*
 * Save the VCPU info on vmenter
 */
void profiling_vmenter_handler(__unused struct vcpu *vcpu)
{
	/* to be implemented */
}

/*
 * Save the VCPU info on vmexit
 */
void profiling_vmexit_handler(__unused struct vcpu *vcpu, __unused uint64_t exit_reason)
{
	if (exit_reason == VMX_EXIT_REASON_EXTERNAL_INTERRUPT) {
		/* to be implemented */
	} else {
		/* to be implemented */
	}
}

/*
 * Setup PMI irq vector
 */
void profiling_setup(void)
{
	uint16_t cpu;
	int32_t retval;
	dev_dbg(ACRN_DBG_PROFILING, "%s: entering", __func__);
	cpu = get_cpu_id();
	/* support PMI notification, VM0 will register all CPU */
	if ((cpu == BOOT_CPU_ID) && (profiling_pmi_irq == IRQ_INVALID)) {
		pr_info("%s: calling request_irq", __func__);
		retval = request_irq(PMI_IRQ,
			profiling_pmi_handler, NULL, IRQF_NONE);
		if (retval < 0) {
			pr_err("Failed to add PMI isr");
			return;
		}
		profiling_pmi_irq = (uint32_t)retval;
	}

	msr_write(MSR_IA32_EXT_APIC_LVT_PMI,
		VECTOR_PMI | LVT_PERFCTR_BIT_MASK);

	dev_dbg(ACRN_DBG_PROFILING, "%s: exiting", __func__);
}

#endif
