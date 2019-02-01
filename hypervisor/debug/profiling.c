/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PROFILING_ON

#include <hypervisor.h>

#define ACRN_DBG_PROFILING		5U
#define ACRN_ERR_PROFILING		3U

#define MAJOR_VERSION			1
#define MINOR_VERSION			0

#define LBR_NUM_REGISTERS		32U
#define PERF_OVF_BIT_MASK		0xC0000070000000FULL
#define LVT_PERFCTR_BIT_UNMASK		0xFFFEFFFFU
#define LVT_PERFCTR_BIT_MASK		0x10000U
#define VALID_DEBUGCTL_BIT_MASK		0x1801U

static uint64_t sep_collection_switch;
static uint64_t socwatch_collection_switch;
static bool in_pmu_profiling;

static uint32_t profiling_pmi_irq = IRQ_INVALID;

extern struct irq_desc irq_desc_array[NR_IRQS];

static void profiling_initialize_vmsw(void)
{
	dev_dbg(ACRN_DBG_PROFILING, "%s: entering cpu%d",
		__func__, get_cpu_id());

	dev_dbg(ACRN_DBG_PROFILING, "%s: exiting cpu%d",
		__func__, get_cpu_id());
}

/*
 * Configure the PMU's for sep/socwatch profiling.
 * Initial write of PMU registers.
 * Walk through the entries and write the value of the register accordingly.
 * Note: current_group is always set to 0, only 1 group is supported.
 */
static void profiling_initialize_pmi(void)
{
	uint32_t i, group_id;
	struct profiling_msr_op *msrop = NULL;
	struct sep_state *ss = &get_cpu_var(profiling_info.sep_state);

	dev_dbg(ACRN_DBG_PROFILING, "%s: entering cpu%d",
		__func__, get_cpu_id());

	if (ss == NULL) {
		dev_dbg(ACRN_ERR_PROFILING, "%s: exiting cpu%d",
			__func__, get_cpu_id());
		return;
	}

	group_id = ss->current_pmi_group_id = 0U;
	for (i = 0U; i < MAX_MSR_LIST_NUM; i++) {
		msrop = &(ss->pmi_initial_msr_list[group_id][i]);
		if (msrop != NULL) {
			if (msrop->msr_id == (uint32_t)-1) {
				break;
			}
			if (msrop->msr_id == MSR_IA32_DEBUGCTL) {
				ss->guest_debugctl_value = msrop->value;
			}
			if (msrop->msr_op_type == (uint8_t)MSR_OP_WRITE) {
				msr_write(msrop->msr_id, msrop->value);
				dev_dbg(ACRN_DBG_PROFILING,
				"%s: MSRWRITE cpu%d, msr_id=0x%x, msr_val=0x%llx",
				__func__, get_cpu_id(), msrop->msr_id, msrop->value);
			}
		}
	}

	ss->pmu_state = PMU_SETUP;

	dev_dbg(ACRN_DBG_PROFILING, "%s: exiting cpu%d",
		__func__,  get_cpu_id());
}

/*
 * Enable all the Performance Monitoring Control registers.
 */
static void profiling_enable_pmu(void)
{
	uint32_t lvt_perf_ctr;
	uint32_t i;
	uint32_t group_id;
	struct profiling_msr_op *msrop = NULL;
	struct sep_state *ss = &get_cpu_var(profiling_info.sep_state);

	dev_dbg(ACRN_DBG_PROFILING, "%s: entering cpu%d",
		__func__, get_cpu_id());

	if (ss == NULL) {
		dev_dbg(ACRN_ERR_PROFILING, "%s: exiting cpu%d",
			__func__, get_cpu_id());
		return;
	}

	/* Unmask LAPIC LVT entry for PMC register */
	lvt_perf_ctr = (uint32_t) msr_read(MSR_IA32_EXT_APIC_LVT_PMI);
	dev_dbg(ACRN_DBG_PROFILING, "%s: 0x%x, 0x%llx",
		__func__, MSR_IA32_EXT_APIC_LVT_PMI, lvt_perf_ctr);
	lvt_perf_ctr &= LVT_PERFCTR_BIT_UNMASK;
	msr_write(MSR_IA32_EXT_APIC_LVT_PMI, lvt_perf_ctr);
	dev_dbg(ACRN_DBG_PROFILING, "%s: 0x%x, 0x%llx",
		__func__, MSR_IA32_EXT_APIC_LVT_PMI, lvt_perf_ctr);

	if (ss->guest_debugctl_value != 0U) {
		/* Merge the msr vmexit loading list with HV */
		if (ss->vmexit_msr_cnt == 0) {
			struct acrn_vcpu *vcpu = get_ever_run_vcpu(get_cpu_id());

			ss->vmexit_msr_cnt = 1 + MSR_AREA_COUNT;
			ss->vmexit_msr_list[0].msr_num
				= MSR_IA32_DEBUGCTL;
			ss->vmexit_msr_list[0].value
				= ss->guest_debugctl_value &
					VALID_DEBUGCTL_BIT_MASK;

			for (i = 0; i < MSR_AREA_COUNT; i++) {
				ss->vmexit_msr_list[i + 1].msr_num = vcpu->arch.msr_area.host[i].msr_num;
				ss->vmexit_msr_list[i + 1].value = vcpu->arch.msr_area.host[i].value;
			}

			exec_vmwrite64(VMX_EXIT_MSR_LOAD_ADDR_FULL,
				hva2hpa(ss->vmexit_msr_list));
			exec_vmwrite32(VMX_EXIT_MSR_LOAD_COUNT,
				(uint64_t)ss->vmexit_msr_cnt);
		}

		/* VMCS GUEST field */
		ss->saved_debugctl_value
			= exec_vmread64(VMX_GUEST_IA32_DEBUGCTL_FULL);
		exec_vmwrite64(VMX_GUEST_IA32_DEBUGCTL_FULL,
		  (ss->guest_debugctl_value & VALID_DEBUGCTL_BIT_MASK));
	}

	group_id = ss->current_pmi_group_id;
	for (i = 0U; i < MAX_MSR_LIST_NUM; i++) {
		msrop = &(ss->pmi_start_msr_list[group_id][i]);
		if (msrop != NULL) {
			if (msrop->msr_id == (uint32_t)-1) {
				break;
			}
			if (msrop->msr_op_type == (uint8_t)MSR_OP_WRITE) {
				msr_write(msrop->msr_id, msrop->value);
				dev_dbg(ACRN_DBG_PROFILING,
				"%s: MSRWRITE cpu%d, msr_id=0x%x, msr_val=0x%llx",
				__func__, get_cpu_id(), msrop->msr_id, msrop->value);
			}
		}
	}

	ss->pmu_state = PMU_RUNNING;

	dev_dbg(ACRN_DBG_PROFILING, "%s: exiting cpu%d",
		__func__,  get_cpu_id());
}

/*
 * Disable all Performance Monitoring Control registers
 */
static void profiling_disable_pmu(void)
{
	uint32_t lvt_perf_ctr;
	uint32_t i;
	uint32_t group_id;
	struct profiling_msr_op *msrop = NULL;
	struct sep_state *ss = &get_cpu_var(profiling_info.sep_state);

	dev_dbg(ACRN_DBG_PROFILING, "%s: entering cpu%d",
		__func__,  get_cpu_id());

	if (ss != NULL) {
		if (ss->vmexit_msr_cnt != 0) {
			/* Restore the msr exit loading list of HV */
			struct acrn_vcpu *vcpu = get_ever_run_vcpu(get_cpu_id());

			exec_vmwrite64(VMX_EXIT_MSR_LOAD_ADDR_FULL, (uint64_t)vcpu->arch.msr_area.host);
			exec_vmwrite32(VMX_EXIT_MSR_LOAD_COUNT, MSR_AREA_COUNT);

			ss->vmexit_msr_cnt = 0;
		}

		group_id = ss->current_pmi_group_id;
		for (i = 0U; i < MAX_MSR_LIST_NUM; i++) {
			msrop = &(ss->pmi_stop_msr_list[group_id][i]);
			if (msrop != NULL) {
				if (msrop->msr_id == (uint32_t)-1) {
					break;
				}
				if (msrop->msr_op_type == (uint8_t)MSR_OP_WRITE) {
					msr_write(msrop->msr_id, msrop->value);
					dev_dbg(ACRN_DBG_PROFILING,
					"%s: MSRWRITE cpu%d, msr_id=0x%x, msr_val=0x%llx",
					__func__, get_cpu_id(), msrop->msr_id, msrop->value);
				}
			}
		}

		/* Mask LAPIC LVT entry for PMC register */
		lvt_perf_ctr = (uint32_t) msr_read(MSR_IA32_EXT_APIC_LVT_PMI);

		lvt_perf_ctr |= LVT_PERFCTR_BIT_MASK;
		msr_write(MSR_IA32_EXT_APIC_LVT_PMI, lvt_perf_ctr);

		ss->pmu_state = PMU_SETUP;

		dev_dbg(ACRN_DBG_PROFILING, "%s: exiting cpu%d",
			__func__,  get_cpu_id());
	} else {
		dev_dbg(ACRN_ERR_PROFILING, "%s: exiting cpu%d",
			__func__, get_cpu_id());
	}
}

/*
 * Writes specified size of data into sbuf
 */
static int32_t profiling_sbuf_put_variable(struct shared_buf *sbuf,
					uint8_t *data, uint32_t size)
{
	uint32_t remaining_space, offset, next_tail;
	void *to;

	/*
	 * 1. check for null pointers and non-zero size
	 * 2. check if enough room available in the buffer
	 *     2a. if not, drop the sample, increment count of dropped samples,
	 *         return
	 *     2b. unless overwrite flag is enabled
	 * 3. Continue if buffer has space for the sample
	 * 4. Copy sample to buffer
	 *     4a. Split variable sample to be copied if the sample is going to
	 *         wrap around the buffer
	 *     4b. Otherwise do a simple copy
	 * 5. return number of bytes of data put in buffer
	 */

	stac();
	if ((sbuf == NULL) || (data == NULL)) {
		clac();
		return -EINVAL;
	}

	if (size == 0U) {
		clac();
		return 0;
	}

	if (sbuf->tail >= sbuf->head) {
		remaining_space = sbuf->size - (sbuf->tail - sbuf->head);
	} else {
		remaining_space = sbuf->head - sbuf->tail;
	}

	if (size >= remaining_space) {
		/* Only (remaining_space - 1) can be written to sbuf.
		 * Since if the next_tail equals head, then it is assumed
		 * that buffer is empty, not full
		 */
		clac();
		return 0;
	}

	next_tail = sbuf_next_ptr(sbuf->tail, size, sbuf->size);

	to = (void *)sbuf + SBUF_HEAD_SIZE + sbuf->tail;

	if (next_tail < sbuf->tail) { /* wrap-around */
		offset = sbuf->size - sbuf->tail;
		(void)memcpy_s(to, offset, data, offset);

		/* 2nd part */
		to = (void *)sbuf + SBUF_HEAD_SIZE;

		if ((size - offset) > 0U) {
			(void)memcpy_s(to, size - offset,
				data + offset, size - offset);
		}
	} else {
		(void)memcpy_s(to, size, data, size);
	}

	sbuf->tail = next_tail;
	clac();

	return (int32_t)size;
}

/*
 * Read profiling data and transferred to SOS
 * Drop transfer of profiling data if sbuf is full/insufficient and log it
 */
static int32_t profiling_generate_data(int32_t collector, uint32_t type)
{
	uint64_t i;
	uint32_t remaining_space = 0U;
	int32_t 	ret = 0;
	struct data_header pkt_header;
	uint64_t payload_size = 0UL;
	void *payload = NULL;
	struct shared_buf *sbuf = NULL;
	struct sep_state *ss = &(get_cpu_var(profiling_info.sep_state));
	struct sw_msr_op_info *sw_msrop
		= &(get_cpu_var(profiling_info.sw_msr_op_info));
	uint64_t rflags;
	spinlock_t *sw_lock = NULL;

	dev_dbg(ACRN_DBG_PROFILING, "%s: entering cpu%d",
		__func__,  get_cpu_id());

	if (collector == COLLECT_PROFILE_DATA) {
		sbuf = (struct shared_buf *)
				per_cpu(sbuf, get_cpu_id())[ACRN_SEP];

		if (sbuf == NULL) {
			ss->samples_dropped++;
			dev_dbg(ACRN_DBG_PROFILING, "%s: sbuf is NULL exiting cpu%d",
				__func__,  get_cpu_id());
			return 0;
		}

		if (ss->pmu_state == PMU_RUNNING) {
			stac();
			if (sbuf->tail >= sbuf->head) {
				remaining_space = sbuf->size
						- (sbuf->tail - sbuf->head);
			} else {
				remaining_space = sbuf->head - sbuf->tail;
			}
			clac();

			/* populate the data header */
			pkt_header.tsc = rdtsc();
			pkt_header.collector_id = collector;
			pkt_header.cpu_id = get_cpu_id();
			pkt_header.data_type = 1U << type;
			pkt_header.reserved = MAGIC_NUMBER;

			switch (type) {
			case CORE_PMU_SAMPLING:
				payload_size = CORE_PMU_SAMPLE_SIZE;
				payload = &get_cpu_var(profiling_info.pmu_sample);
				break;
			case LBR_PMU_SAMPLING:
				payload_size = CORE_PMU_SAMPLE_SIZE
					+ LBR_PMU_SAMPLE_SIZE;
				payload = &get_cpu_var(profiling_info.pmu_sample);
				break;
			case VM_SWITCH_TRACING:
				payload_size = VM_SWITCH_TRACE_SIZE;
				payload = &get_cpu_var(profiling_info.vm_switch_trace);
				break;
			default:
				pr_err("%s: unknown data type %u on cpu %d",
					__func__, type, get_cpu_id());
				ret = -1;
				break;
			}
			if (ret == -1) {
				return 0;
			}
			pkt_header.payload_size = payload_size;

			if ((uint64_t)remaining_space < (DATA_HEADER_SIZE + payload_size)) {
				ss->samples_dropped++;
				dev_dbg(ACRN_DBG_PROFILING,
				"%s: not enough space left in sbuf[%d: %d] exiting cpu%d",
				__func__, remaining_space,
				DATA_HEADER_SIZE + payload_size, get_cpu_id());
				return 0;
			}

			for (i = 0U; i < (((DATA_HEADER_SIZE - 1U) / SEP_BUF_ENTRY_SIZE) + 1U); i++) {
				(void)sbuf_put((struct shared_buf *)sbuf,
					(uint8_t *)&pkt_header + i * SEP_BUF_ENTRY_SIZE);
			}

			for (i = 0U; i < (((payload_size - 1U) / SEP_BUF_ENTRY_SIZE) + 1U); i++) {
				(void)sbuf_put((struct shared_buf *)sbuf,
					(uint8_t *)payload + i * SEP_BUF_ENTRY_SIZE);
			}

			ss->samples_logged++;
		}
	} else if (collector == COLLECT_POWER_DATA) {

		sbuf = (struct shared_buf *)
				per_cpu(sbuf, get_cpu_id())[ACRN_SOCWATCH];

		if (sbuf == NULL) {
			dev_dbg(ACRN_DBG_PROFILING,
			"%s: socwatch buffers not initialized?", __func__);
			return 0;
		}

		sw_lock = &(get_cpu_var(profiling_info.sw_lock));
		spinlock_irqsave_obtain(sw_lock, &rflags);
		stac();
		if (sbuf->tail >= sbuf->head) {
			remaining_space
				= sbuf->size - (sbuf->tail - sbuf->head);
		} else {
			remaining_space = sbuf->head - sbuf->tail;
		}
		clac();

		/* populate the data header */
		pkt_header.tsc = rdtsc();
		pkt_header.collector_id = collector;
		pkt_header.cpu_id = get_cpu_id();
		pkt_header.data_type = (uint16_t)type;

		switch (type) {
		case SOCWATCH_MSR_OP:
			dev_dbg(ACRN_DBG_PROFILING,
				"%s: generating cstate/pstate sample socwatch cpu %d",
				__func__, sw_msrop->cpu_id);
			pkt_header.cpu_id = (uint16_t)sw_msrop->cpu_id;
			pkt_header.data_type = sw_msrop->sample_id;
			payload_size
			= ((uint64_t)sw_msrop->valid_entries) * sizeof(uint64_t);
			payload = &(sw_msrop->core_msr[0]);
			break;

		case SOCWATCH_VM_SWITCH_TRACING:
			dev_dbg(ACRN_DBG_PROFILING,
				"%s: generating vm-switch sample", __func__);
			payload_size = VM_SWITCH_TRACE_SIZE;
			payload = &get_cpu_var(profiling_info.vm_switch_trace);
			break;
		default:
			pr_err("%s: unknown data type %u on cpu %d",
				__func__, type, get_cpu_id());
			ret = -1;
			break;
		}
		if (ret == -1){
			return 0;
		}
		pkt_header.payload_size = payload_size;

		if ((DATA_HEADER_SIZE + payload_size) >= (uint64_t)remaining_space) {
			pr_err("%s: not enough space in socwatch buffer on cpu %d",
				__func__, get_cpu_id());
			return 0;
		}
		/* copy header */
		(void)profiling_sbuf_put_variable((struct shared_buf *)sbuf,
			(uint8_t *)&pkt_header, (uint32_t)DATA_HEADER_SIZE);

		/* copy payload */
		(void)profiling_sbuf_put_variable((struct shared_buf *)sbuf,
			(uint8_t *)payload, (uint32_t)payload_size);

		spinlock_irqrestore_release(sw_lock, rflags);
	} else {
		dev_dbg(ACRN_ERR_PROFILING,
			"%s: Unknown collector type", __func__);
		return 0;
	}
	return 0;
}

/*
 * Performs MSR operations - read, write and clear
 */
static void profiling_handle_msrops(void)
{
	uint32_t i, j;
	struct profiling_msr_ops_list *my_msr_node
		= get_cpu_var(profiling_info.msr_node);
	struct sw_msr_op_info *sw_msrop
		= &(get_cpu_var(profiling_info.sw_msr_op_info));

	dev_dbg(ACRN_DBG_PROFILING, "%s: entering cpu%d",
		__func__, get_cpu_id());

	if ((my_msr_node == NULL) ||
		(my_msr_node->msr_op_state != (int32_t)MSR_OP_REQUESTED)) {
		dev_dbg(ACRN_DBG_PROFILING, "%s: invalid my_msr_node on cpu%d",
			__func__, get_cpu_id());
		return;
	}

	if ((my_msr_node->num_entries == 0U) ||
		(my_msr_node->num_entries >= MAX_MSR_LIST_NUM)) {
		dev_dbg(ACRN_DBG_PROFILING,
		"%s: invalid num_entries on cpu%d",
		__func__, get_cpu_id());
		return;
	}

	for (i = 0U; i < my_msr_node->num_entries; i++) {
		switch (my_msr_node->entries[i].msr_op_type) {
		case MSR_OP_READ:
			my_msr_node->entries[i].value
				= msr_read(my_msr_node->entries[i].msr_id);
			dev_dbg(ACRN_DBG_PROFILING,
			"%s: MSRREAD cpu%d, msr_id=0x%x, msr_val=0x%llx",
			__func__, get_cpu_id(),	my_msr_node->entries[i].msr_id,
			my_msr_node->entries[i].value);
			break;
		case MSR_OP_READ_CLEAR:
			my_msr_node->entries[i].value
				= msr_read(my_msr_node->entries[i].msr_id);
			dev_dbg(ACRN_DBG_PROFILING,
			"%s: MSRREADCLEAR cpu%d, msr_id=0x%x, msr_val=0x%llx",
			__func__, get_cpu_id(), my_msr_node->entries[i].msr_id,
			my_msr_node->entries[i].value);
			msr_write(my_msr_node->entries[i].msr_id, 0U);
			break;
		case MSR_OP_WRITE:
			msr_write(my_msr_node->entries[i].msr_id,
				my_msr_node->entries[i].value);
			dev_dbg(ACRN_DBG_PROFILING,
			"%s: MSRWRITE cpu%d, msr_id=0x%x, msr_val=0x%llx",
			__func__, get_cpu_id(), my_msr_node->entries[i].msr_id,
			my_msr_node->entries[i].value);
			break;
		default:
			pr_err("%s: unknown MSR op_type %u on cpu %d",
			__func__, my_msr_node->entries[i].msr_op_type,
			get_cpu_id());
			break;
		}
	}

	my_msr_node->msr_op_state = (int32_t)MSR_OP_HANDLED;

	/* Also generates sample */
	if ((my_msr_node->collector_id == COLLECT_POWER_DATA) &&
			(sw_msrop != NULL)) {

		sw_msrop->cpu_id = get_cpu_id();
		sw_msrop->valid_entries = my_msr_node->num_entries;

		/*
		 * if 'param' is 0, then skip generating a sample since it is
		 * an immediate MSR read operation.
		 */
		if (my_msr_node->entries[0].param != 0UL) {
			for (j = 0U; j < my_msr_node->num_entries; ++j) {
				sw_msrop->core_msr[j]
					= my_msr_node->entries[j].value;
				/*
				 * socwatch uses the 'param' field to store the
				 * sample id needed by socwatch to identify the
				 * type of sample during post-processing
				 */
				sw_msrop->sample_id
					= my_msr_node->entries[j].param;
			}

			/* generate sample */
			(void)profiling_generate_data(COLLECT_POWER_DATA,
						SOCWATCH_MSR_OP);
		}
		my_msr_node->msr_op_state = (int32_t)MSR_OP_REQUESTED;
	}

	dev_dbg(ACRN_DBG_PROFILING, "%s: exiting cpu%d",
		__func__, get_cpu_id());
}

/*
 * Interrupt handler for performance monitoring interrupts
 */
static void profiling_pmi_handler(uint32_t irq, __unused void *data)
{
	uint64_t perf_ovf_status;
	uint32_t lvt_perf_ctr;
	uint32_t i;
	uint32_t group_id;
	struct profiling_msr_op *msrop = NULL;
	struct pmu_sample *psample = &(get_cpu_var(profiling_info.pmu_sample));
	struct sep_state *ss = &(get_cpu_var(profiling_info.sep_state));

	if ((ss == NULL) || (psample == NULL)) {
		dev_dbg(ACRN_ERR_PROFILING, "%s: exiting cpu%d",
			__func__, get_cpu_id());
		return;
	}
	/* Stop all the counters first */
	msr_write(MSR_IA32_PERF_GLOBAL_CTRL, 0x0U);

	group_id = ss->current_pmi_group_id;
	for (i = 0U; i < MAX_MSR_LIST_NUM; i++) {
		msrop = &(ss->pmi_entry_msr_list[group_id][i]);
		if (msrop != NULL) {
			if (msrop->msr_id == (uint32_t)-1) {
				break;
			}
			if (msrop->msr_op_type == (uint8_t)MSR_OP_WRITE) {
				msr_write(msrop->msr_id, msrop->value);
			}
		}
	}

	ss->total_pmi_count++;
	perf_ovf_status = msr_read(MSR_IA32_PERF_GLOBAL_STATUS);
	lvt_perf_ctr = (uint32_t)msr_read(MSR_IA32_EXT_APIC_LVT_PMI);

	if (perf_ovf_status == 0U) {
		goto reconfig;
	}

	if ((perf_ovf_status & 0x80000000000000FULL) == 0U) {
		ss->nofrozen_pmi++;
	}

	(void)memset(psample, 0U, sizeof(struct pmu_sample));

	/* Attribute PMI to guest context */
	if ((get_cpu_var(profiling_info.vm_info).vmexit_reason
			== VMX_EXIT_REASON_EXTERNAL_INTERRUPT) &&
			((uint64_t)get_cpu_var(profiling_info.vm_info).external_vector
			== VECTOR_PMI)) {
		psample->csample.os_id
			= get_cpu_var(profiling_info.vm_info).guest_vm_id;
		(void)memset(psample->csample.task, 0U, 16);
		psample->csample.cpu_id = get_cpu_id();
		psample->csample.process_id = 0U;
		psample->csample.task_id = 0U;
		psample->csample.overflow_status = perf_ovf_status;
		psample->csample.rip = get_cpu_var(profiling_info.vm_info).guest_rip;
		psample->csample.rflags
			= (uint32_t)get_cpu_var(profiling_info.vm_info).guest_rflags;
		psample->csample.cs
			= (uint32_t)get_cpu_var(profiling_info.vm_info).guest_cs;
		get_cpu_var(profiling_info.vm_info).vmexit_reason = 0U;
		get_cpu_var(profiling_info.vm_info).external_vector = -1;
	/* Attribute PMI to hypervisor context */
	} else {
		psample->csample.os_id = 0xFFFFU;
		(void)memcpy_s(psample->csample.task, 16, "VMM\0", 4);
		psample->csample.cpu_id = get_cpu_id();
		psample->csample.process_id = 0U;
		psample->csample.task_id = 0U;
		psample->csample.overflow_status = perf_ovf_status;
		psample->csample.rip = irq_desc_array[irq].ctx_rip;
		psample->csample.rflags
			= (uint32_t)irq_desc_array[irq].ctx_rflags;
		psample->csample.cs = (uint32_t)irq_desc_array[irq].ctx_cs;
	}

	if ((sep_collection_switch &
				(1UL << (uint64_t)LBR_PMU_SAMPLING)) > 0UL) {
		psample->lsample.lbr_tos = msr_read(MSR_CORE_LASTBRANCH_TOS);
		for (i = 0U; i < LBR_NUM_REGISTERS; i++) {
			psample->lsample.lbr_from_ip[i]
				= msr_read(MSR_CORE_LASTBRANCH_0_FROM_IP + i);
			psample->lsample.lbr_to_ip[i]
				= msr_read(MSR_CORE_LASTBRANCH_0_TO_IP + i);
		}
		/* Generate core pmu sample and lbr data */
		(void)profiling_generate_data(COLLECT_PROFILE_DATA, LBR_PMU_SAMPLING);
	} else {
		/* Generate core pmu sample only */
		(void)profiling_generate_data(COLLECT_PROFILE_DATA, CORE_PMU_SAMPLING);
	}

	/* Clear PERF_GLOBAL_OVF_STATUS bits */
	msr_write(MSR_IA32_PERF_GLOBAL_OVF_CTRL,
			perf_ovf_status & PERF_OVF_BIT_MASK);

	ss->valid_pmi_count++;

	group_id = ss->current_pmi_group_id;
	for (i = 0U; i < MAX_MSR_LIST_NUM; i++) {
		msrop = &(ss->pmi_exit_msr_list[group_id][i]);
		if (msrop != NULL) {
			if (msrop->msr_id == (uint32_t)-1) {
				break;
			}
			if (msrop->msr_op_type == (uint8_t)MSR_OP_WRITE) {
				if (msrop->reg_type != (uint8_t)PMU_MSR_DATA) {
					if (msrop->msr_id != MSR_IA32_PERF_GLOBAL_CTRL) {
						msr_write(msrop->msr_id, msrop->value);
					}
				}
				else {
					if (((perf_ovf_status >> msrop->param) & 0x1U) > 0U) {
						msr_write(msrop->msr_id, msrop->value);
					}
				}
			}
		}
	}

reconfig:

	if (ss->pmu_state == PMU_RUNNING) {
		/* Unmask the interrupt */
		lvt_perf_ctr &= LVT_PERFCTR_BIT_UNMASK;
		msr_write(MSR_IA32_EXT_APIC_LVT_PMI, lvt_perf_ctr);
		group_id = ss->current_pmi_group_id;
		for (i = 0U; i < MAX_MSR_LIST_NUM; i++) {
			msrop = &(ss->pmi_start_msr_list[group_id][i]);
			if (msrop != NULL) {
				if (msrop->msr_id == (uint32_t)-1) {
					break;
				}
				if (msrop->msr_op_type == (uint8_t)MSR_OP_WRITE) {
					msr_write(msrop->msr_id, msrop->value);
				}
			}
		}
	} else {
		/* Mask the interrupt */
		lvt_perf_ctr |= LVT_PERFCTR_BIT_MASK;
		msr_write(MSR_IA32_EXT_APIC_LVT_PMI, lvt_perf_ctr);
	}
}

/*
 * Initialize sep state and enable PMU counters
 */
static void profiling_start_pmu(void)
{
	uint16_t i;

	dev_dbg(ACRN_DBG_PROFILING, "%s: entering", __func__);

	if (in_pmu_profiling) {
		return;
	}

	for (i = 0U; i < phys_cpu_num; i++) {
		if (per_cpu(profiling_info.sep_state, i).pmu_state != PMU_SETUP) {
			pr_err("%s: invalid pmu_state %u on cpu%d",
			__func__, get_cpu_var(profiling_info.sep_state).pmu_state, i);
			return;
		}
	}

	for (i = 0U; i < phys_cpu_num; i++) {
		per_cpu(profiling_info.ipi_cmd, i) = IPI_PMU_START;
		per_cpu(profiling_info.sep_state, i).samples_logged = 0U;
		per_cpu(profiling_info.sep_state, i).samples_dropped = 0U;
		per_cpu(profiling_info.sep_state, i).valid_pmi_count = 0U;
		per_cpu(profiling_info.sep_state, i).total_pmi_count = 0U;
		per_cpu(profiling_info.sep_state, i).total_vmexit_count = 0U;
		per_cpu(profiling_info.sep_state, i).frozen_well = 0U;
		per_cpu(profiling_info.sep_state, i).frozen_delayed = 0U;
		per_cpu(profiling_info.sep_state, i).nofrozen_pmi = 0U;
		per_cpu(profiling_info.sep_state, i).pmu_state = PMU_RUNNING;
	}

	smp_call_function(pcpu_active_bitmap, profiling_ipi_handler, NULL);

	in_pmu_profiling = true;

	dev_dbg(ACRN_DBG_PROFILING, "%s: done", __func__);
}

/*
 * Reset sep state and Disable all the PMU counters
 */
static void profiling_stop_pmu(void)
{
	uint16_t i;

	dev_dbg(ACRN_DBG_PROFILING, "%s: entering", __func__);

	if (in_pmu_profiling) {
		for (i = 0U; i < phys_cpu_num; i++) {
			per_cpu(profiling_info.ipi_cmd, i) = IPI_PMU_STOP;
			if (per_cpu(profiling_info.sep_state, i).pmu_state == PMU_RUNNING) {
				per_cpu(profiling_info.sep_state, i).pmu_state = PMU_SETUP;
			}

			dev_dbg(ACRN_DBG_PROFILING,
			"%s: pmi_cnt[%d] = total:%u valid=%u, vmexit_cnt=%u",
			__func__, i, per_cpu(profiling_info.sep_state, i).total_pmi_count,
			per_cpu(profiling_info.sep_state, i).valid_pmi_count,
			per_cpu(profiling_info.sep_state, i).total_vmexit_count);

			dev_dbg(ACRN_DBG_PROFILING,
			"%s: cpu%d frozen well:%u frozen delayed=%u, nofrozen_pmi=%u",
			__func__, i, per_cpu(profiling_info.sep_state, i).frozen_well,
			per_cpu(profiling_info.sep_state, i).frozen_delayed,
			per_cpu(profiling_info.sep_state, i).nofrozen_pmi);

			dev_dbg(ACRN_DBG_PROFILING,
			"%s: cpu%d samples captured:%u samples dropped=%u",
			__func__, i, per_cpu(profiling_info.sep_state, i).samples_logged,
			per_cpu(profiling_info.sep_state, i).samples_dropped);

		}

		smp_call_function(pcpu_active_bitmap, profiling_ipi_handler, NULL);

		in_pmu_profiling = false;

		dev_dbg(ACRN_DBG_PROFILING, "%s: done.", __func__);
	}

}


/*
 * Performs MSR operations on all the CPU's
 */
int32_t profiling_msr_ops_all_cpus(struct acrn_vm *vm, uint64_t addr)
{
	uint16_t i;
	struct profiling_msr_ops_list msr_list[phys_cpu_num];

	(void)memset((void *)&msr_list, 0U, sizeof(msr_list));

	dev_dbg(ACRN_DBG_PROFILING, "%s: entering", __func__);

	if (copy_from_gpa(vm, &msr_list, addr, sizeof(msr_list)) != 0) {
		pr_err("%s: Unable to copy addr from vm\n", __func__);
		return -EINVAL;
	}

	for (i = 0U; i < phys_cpu_num; i++) {
		per_cpu(profiling_info.ipi_cmd, i) = IPI_MSR_OP;
		per_cpu(profiling_info.msr_node, i) = &(msr_list[i]);
	}

	smp_call_function(pcpu_active_bitmap, profiling_ipi_handler, NULL);

	if (copy_to_gpa(vm, &msr_list, addr, sizeof(msr_list)) != 0) {
		pr_err("%s: Unable to copy addr from vm\n", __func__);
		return -EINVAL;
	}

	dev_dbg(ACRN_DBG_PROFILING, "%s: exiting", __func__);
	return 0;
}

/*
 * Generate VM info list
 */
int32_t profiling_vm_list_info(struct acrn_vm *vm, uint64_t addr)
{
	struct acrn_vm *tmp_vm;
	struct acrn_vcpu *vcpu;
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
		vm_info_list.vm_list[vm_idx].cpu_map[i].vcpu_id = i;
		vm_info_list.vm_list[vm_idx].cpu_map[i].pcpu_id = i;
		vm_info_list.vm_list[vm_idx].cpu_map[i].apic_id
			= per_cpu(lapic_id, i);
	}
	vm_info_list.vm_list[vm_idx].num_vcpus = i;
	vm_info_list.num_vms = 1;

	for (j = 0U; j < CONFIG_MAX_VM_NUM; j++) {
		tmp_vm = get_vm_from_vmid(j);
		if (tmp_vm == NULL) {
			break;
		}
		vm_info_list.num_vms++;
		vm_idx++;

		vm_info_list.vm_list[vm_idx].vm_id_num = tmp_vm->vm_id;
		(void)memcpy_s((void *)vm_info_list.vm_list[vm_idx].guid,
			16U, tmp_vm->GUID, 16U);
		snprintf(vm_info_list.vm_list[vm_idx].vm_name, 16U, "vm_%d",
				tmp_vm->vm_id, 16U);
		vm_info_list.vm_list[vm_idx].num_vcpus = 0;
		i = 0U;
		foreach_vcpu(i, tmp_vm, vcpu) {
			vm_info_list.vm_list[vm_idx].cpu_map[i].vcpu_id
				= vcpu->vcpu_id;
			vm_info_list.vm_list[vm_idx].cpu_map[i].pcpu_id
				= vcpu->pcpu_id;
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
int32_t profiling_get_version_info(struct acrn_vm *vm, uint64_t addr)
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
int32_t profiling_get_control(struct acrn_vm *vm, uint64_t addr)
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
int32_t profiling_set_control(struct acrn_vm *vm, uint64_t addr)
{
	uint64_t old_switch;
	uint64_t new_switch;
	uint16_t i;

	struct profiling_control prof_control;

	(void)memset((void *)&prof_control, 0U, sizeof(prof_control));

	dev_dbg(ACRN_DBG_PROFILING, "%s: entering", __func__);

	if (copy_from_gpa(vm, &prof_control, addr, sizeof(prof_control)) != 0) {
		pr_err("%s: Unable to copy addr from vm\n", __func__);
		return -EINVAL;
	}

	switch (prof_control.collector_id) {
	case COLLECT_PROFILE_DATA:
		old_switch = sep_collection_switch;
		new_switch = prof_control.switches;
		sep_collection_switch = prof_control.switches;

		dev_dbg(ACRN_DBG_PROFILING,
			" old_switch: %llu sep_collection_switch: %llu!",
			   old_switch, sep_collection_switch);

		for (i = 0U; i < (uint16_t)MAX_SEP_FEATURE_ID; i++) {
			if (((new_switch ^ old_switch) & (0x1UL << i)) != 0UL) {
				switch (i) {
				case CORE_PMU_SAMPLING:
				case CORE_PMU_COUNTING:
					if ((new_switch & (0x1UL << i)) != 0UL) {
						profiling_start_pmu();
					} else {
						profiling_stop_pmu();
					}
					break;
				case LBR_PMU_SAMPLING:
					break;
				case VM_SWITCH_TRACING:
					break;
				default:
					dev_dbg(ACRN_DBG_PROFILING,
					"%s: feature not supported %u",
					 __func__, i);
					break;
				}
			}
		}
		break;
	case COLLECT_POWER_DATA:
		dev_dbg(ACRN_DBG_PROFILING,
			"%s: configuring socwatch", __func__);

		socwatch_collection_switch = prof_control.switches;

		dev_dbg(ACRN_DBG_PROFILING,
			"socwatch_collection_switch: %llu!",
			socwatch_collection_switch);

		if (socwatch_collection_switch != 0UL) {
			dev_dbg(ACRN_DBG_PROFILING,
			"%s: socwatch start collection invoked!", __func__);
			for (i = 0U; i < (uint16_t)MAX_SOCWATCH_FEATURE_ID; i++) {
				if ((socwatch_collection_switch & (0x1UL << i)) != 0UL) {
					switch (i) {
					case SOCWATCH_COMMAND:
						break;
					case SOCWATCH_VM_SWITCH_TRACING:
						dev_dbg(ACRN_DBG_PROFILING,
						"%s: socwatch vm-switch feature requested!",
						__func__);
						break;
					default:
						dev_dbg(ACRN_DBG_PROFILING,
						"%s: socwatch feature not supported %u",
						__func__, i);
						break;
					}
				}
			}
			for (i = 0U; i < phys_cpu_num; i++) {
				per_cpu(profiling_info.soc_state, i)
					= SW_RUNNING;
			}
		} else { /* stop socwatch collection */
			dev_dbg(ACRN_DBG_PROFILING,
			"%s: socwatch stop collection invoked or collection switch not set!",
			__func__);
			for (i = 0U; i < phys_cpu_num; i++) {
				per_cpu(profiling_info.soc_state, i)
					= SW_STOPPED;
			}
		}
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
 * Configure PMI on all cpus
 */
int32_t profiling_configure_pmi(struct acrn_vm *vm, uint64_t addr)
{
	uint16_t i;
	struct profiling_pmi_config pmi_config;

	(void)memset((void *)&pmi_config, 0U, sizeof(pmi_config));

	dev_dbg(ACRN_DBG_PROFILING, "%s: entering", __func__);

	if (copy_from_gpa(vm, &pmi_config, addr, sizeof(pmi_config)) != 0) {
		pr_err("%s: Unable to copy addr from vm\n", __func__);
		return -EINVAL;
	}

	for (i = 0U; i < phys_cpu_num; i++) {
		if (!((per_cpu(profiling_info.sep_state, i).pmu_state ==
				PMU_INITIALIZED) ||
			(per_cpu(profiling_info.sep_state, i).pmu_state ==
				PMU_SETUP))) {
			pr_err("%s: invalid pmu_state %u on cpu%d",
			__func__, per_cpu(profiling_info.sep_state, i).pmu_state, i);
			return -EINVAL;
		}
	}

	if (pmi_config.num_groups == 0U ||
		pmi_config.num_groups > MAX_GROUP_NUM) {
		pr_err("%s: invalid num_groups %u",
			__func__, pmi_config.num_groups);
		return -EINVAL;
	}

	for (i = 0U; i < phys_cpu_num; i++) {
		per_cpu(profiling_info.ipi_cmd, i) = IPI_PMU_CONFIG;
		per_cpu(profiling_info.sep_state, i).num_pmi_groups
			= pmi_config.num_groups;

		(void)memcpy_s((void *)per_cpu(profiling_info.sep_state, i).pmi_initial_msr_list,
		sizeof(struct profiling_msr_op)*MAX_MSR_LIST_NUM*MAX_GROUP_NUM,
		(void *)pmi_config.initial_list,
		sizeof(struct profiling_msr_op)*MAX_MSR_LIST_NUM*MAX_GROUP_NUM);

		(void)memcpy_s((void *)per_cpu(profiling_info.sep_state, i).pmi_start_msr_list,
		sizeof(struct profiling_msr_op)*MAX_MSR_LIST_NUM*MAX_GROUP_NUM,
		(void *)pmi_config.start_list,
		sizeof(struct profiling_msr_op)*MAX_MSR_LIST_NUM*MAX_GROUP_NUM);

		(void)memcpy_s((void *)per_cpu(profiling_info.sep_state, i).pmi_stop_msr_list,
		sizeof(struct profiling_msr_op)*MAX_MSR_LIST_NUM*MAX_GROUP_NUM,
		(void *)pmi_config.stop_list,
		sizeof(struct profiling_msr_op)*MAX_MSR_LIST_NUM*MAX_GROUP_NUM);

		(void)memcpy_s((void *)per_cpu(profiling_info.sep_state, i).pmi_entry_msr_list,
		sizeof(struct profiling_msr_op)*MAX_MSR_LIST_NUM*MAX_GROUP_NUM,
		(void *)pmi_config.entry_list,
		sizeof(struct profiling_msr_op)*MAX_MSR_LIST_NUM*MAX_GROUP_NUM);

		(void)memcpy_s((void *)per_cpu(profiling_info.sep_state, i).pmi_exit_msr_list,
		sizeof(struct profiling_msr_op)*MAX_MSR_LIST_NUM*MAX_GROUP_NUM,
		(void *)pmi_config.exit_list,
		sizeof(struct profiling_msr_op)*MAX_MSR_LIST_NUM*MAX_GROUP_NUM);
	}

	smp_call_function(pcpu_active_bitmap, profiling_ipi_handler, NULL);

	if (copy_to_gpa(vm, &pmi_config, addr, sizeof(pmi_config)) != 0) {
		pr_err("%s: Unable to copy addr to vm\n", __func__);
		return -EINVAL;
	}

	dev_dbg(ACRN_DBG_PROFILING, "%s: exiting", __func__);
	return 0;
}

/*
 * Configure for VM-switch data on all cpus
 */
int32_t profiling_configure_vmsw(struct acrn_vm *vm, uint64_t addr)
{
	uint16_t i;
	int32_t ret = 0;
	struct profiling_vmsw_config vmsw_config;

	(void)memset((void *)&vmsw_config, 0U, sizeof(vmsw_config));

	dev_dbg(ACRN_DBG_PROFILING, "%s: entering", __func__);

	if (copy_from_gpa(vm, &vmsw_config, addr, sizeof(vmsw_config)) != 0) {
		pr_err("%s: Unable to copy addr from vm\n", __func__);
		return -EINVAL;
	}

	switch (vmsw_config.collector_id) {
	case COLLECT_PROFILE_DATA:
		for (i = 0U; i < phys_cpu_num; i++) {
			per_cpu(profiling_info.ipi_cmd, i) = IPI_VMSW_CONFIG;

			(void)memcpy_s(
			(void *)per_cpu(profiling_info.sep_state, i).vmsw_initial_msr_list,
			sizeof(struct profiling_msr_op)*MAX_MSR_LIST_NUM,
			(void *)vmsw_config.initial_list,
			sizeof(struct profiling_msr_op)*MAX_MSR_LIST_NUM);

			(void)memcpy_s(
			(void *)per_cpu(profiling_info.sep_state, i).vmsw_entry_msr_list,
			sizeof(struct profiling_msr_op)*MAX_MSR_LIST_NUM,
			(void *)vmsw_config.entry_list,
			sizeof(struct profiling_msr_op)*MAX_MSR_LIST_NUM);

			(void)memcpy_s(
			(void *)per_cpu(profiling_info.sep_state, i).vmsw_exit_msr_list,
			sizeof(struct profiling_msr_op)*MAX_MSR_LIST_NUM,
			(void *)vmsw_config.exit_list,
			sizeof(struct profiling_msr_op)*MAX_MSR_LIST_NUM);
		}

		smp_call_function(pcpu_active_bitmap, profiling_ipi_handler, NULL);

		break;
	case COLLECT_POWER_DATA:
		break;
	default:
		pr_err("%s: unknown collector %d",
			__func__, vmsw_config.collector_id);
		ret = -EINVAL;
		break;
	}

	if (copy_to_gpa(vm, &vmsw_config, addr, sizeof(vmsw_config)) != 0) {
		pr_err("%s: Unable to copy addr to vm\n", __func__);
		return -EINVAL;
	}

	dev_dbg(ACRN_DBG_PROFILING, "%s: exiting", __func__);

	return ret;
}

/*
 * Get the physical cpu id
 */
int32_t profiling_get_pcpu_id(struct acrn_vm *vm, uint64_t addr)
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
 * Update collection statictics
 */
int32_t profiling_get_status_info(struct acrn_vm *vm, uint64_t gpa)
{
	uint16_t i;
	struct profiling_status pstats[phys_cpu_num];

	(void)memset((void *)&pstats, 0U, phys_cpu_num*sizeof(struct profiling_status));

	dev_dbg(ACRN_DBG_PROFILING, "%s: entering", __func__);

	for (i = 0U; i < phys_cpu_num; i++) {
		pstats[i].samples_logged =
			per_cpu(profiling_info.sep_state, i).samples_logged;
		pstats[i].samples_dropped =
			per_cpu(profiling_info.sep_state, i).samples_dropped;
	}

	if (copy_to_gpa(vm, &pstats, gpa,
		phys_cpu_num*sizeof(struct profiling_status)) != 0) {
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
void profiling_vmenter_handler(__unused struct acrn_vcpu *vcpu)
{
	if (((get_cpu_var(profiling_info.sep_state).pmu_state == PMU_RUNNING) &&
			((sep_collection_switch &
				(1UL << (uint64_t)VM_SWITCH_TRACING)) > 0UL)) ||
		((get_cpu_var(profiling_info.soc_state) == SW_RUNNING) &&
			((socwatch_collection_switch &
				(1UL << (uint64_t)SOCWATCH_VM_SWITCH_TRACING)) > 0UL))) {

		get_cpu_var(profiling_info.vm_info).vmenter_tsc = rdtsc();
	}
}

/*
 * Save the VCPU info on vmexit
 */
void profiling_pre_vmexit_handler(struct acrn_vcpu *vcpu)
{
	uint64_t exit_reason = 0UL;

	exit_reason = vcpu->arch.exit_reason & 0xFFFFUL;

	if ((get_cpu_var(profiling_info.sep_state).pmu_state == PMU_RUNNING) ||
		(get_cpu_var(profiling_info.soc_state) == SW_RUNNING)) {

		get_cpu_var(profiling_info.vm_info).vmexit_tsc = rdtsc();
		get_cpu_var(profiling_info.vm_info).vmexit_reason
			= exit_reason;
		if (exit_reason == VMX_EXIT_REASON_EXTERNAL_INTERRUPT) {
			get_cpu_var(profiling_info.vm_info).external_vector
				= (int32_t)(exec_vmread(VMX_EXIT_INT_INFO) & 0xFFUL);
		} else {
			get_cpu_var(profiling_info.vm_info).external_vector = -1;
		}
		get_cpu_var(profiling_info.vm_info).guest_rip
			= vcpu_get_rip(vcpu);

		get_cpu_var(profiling_info.vm_info).guest_rflags
			= vcpu_get_rflags(vcpu);

		get_cpu_var(profiling_info.vm_info).guest_cs
			= exec_vmread64(VMX_GUEST_CS_SEL);

		get_cpu_var(profiling_info.vm_info).guest_vm_id = (int32_t)vcpu->vm->vm_id;
	}
}

/*
 * Generate vmexit data
 */
void profiling_post_vmexit_handler(struct acrn_vcpu *vcpu)
{
	per_cpu(profiling_info.sep_state, vcpu->pcpu_id).total_vmexit_count++;

	if ((get_cpu_var(profiling_info.sep_state).pmu_state == PMU_RUNNING) ||
		(get_cpu_var(profiling_info.soc_state) == SW_RUNNING)) {

		/* Generate vmswitch sample */
		if (((sep_collection_switch &
					(1UL << (uint64_t)VM_SWITCH_TRACING)) > 0UL) ||
				((socwatch_collection_switch &
					(1UL << (uint64_t)SOCWATCH_VM_SWITCH_TRACING)) > 0UL)) {
			get_cpu_var(profiling_info.vm_switch_trace).os_id
				= vcpu->vm->vm_id;
			get_cpu_var(profiling_info.vm_switch_trace).vm_enter_tsc
				= get_cpu_var(profiling_info.vm_info).vmenter_tsc;
			get_cpu_var(profiling_info.vm_switch_trace).vm_exit_tsc
				= get_cpu_var(profiling_info.vm_info).vmexit_tsc;
			get_cpu_var(profiling_info.vm_switch_trace).vm_exit_reason
				= get_cpu_var(profiling_info.vm_info).vmexit_reason;

			if ((sep_collection_switch &
					(1UL << (uint64_t)VM_SWITCH_TRACING)) > 0UL) {
				(void)profiling_generate_data(COLLECT_PROFILE_DATA,
					VM_SWITCH_TRACING);
			}
			if ((socwatch_collection_switch &
					(1UL << (uint64_t)SOCWATCH_VM_SWITCH_TRACING)) > 0UL) {
				(void)profiling_generate_data(COLLECT_POWER_DATA,
					SOCWATCH_VM_SWITCH_TRACING);
			}
		}
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

	per_cpu(profiling_info.sep_state, cpu).valid_pmi_count = 0U;
	per_cpu(profiling_info.sep_state, cpu).total_pmi_count = 0U;
	per_cpu(profiling_info.sep_state, cpu).total_vmexit_count = 0U;
	per_cpu(profiling_info.sep_state, cpu).pmu_state = PMU_INITIALIZED;
	per_cpu(profiling_info.sep_state, cpu).vmexit_msr_cnt = 0;
	per_cpu(profiling_info.sep_state, cpu).samples_logged = 0U;
	per_cpu(profiling_info.sep_state, cpu).samples_dropped = 0U;
	per_cpu(profiling_info.sep_state, cpu).frozen_well = 0U;
	per_cpu(profiling_info.sep_state, cpu).frozen_delayed = 0U;
	per_cpu(profiling_info.sep_state, cpu).nofrozen_pmi = 0U;

	msr_write(MSR_IA32_EXT_APIC_LVT_PMI,
		VECTOR_PMI | LVT_PERFCTR_BIT_MASK);

	dev_dbg(ACRN_DBG_PROFILING, "%s: exiting", __func__);
}

#endif
