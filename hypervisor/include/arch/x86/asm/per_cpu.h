/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PER_CPU_H
#define PER_CPU_H

#include <types.h>
#include <sbuf.h>
#include <irq.h>
#include <timer.h>
#include <profiling.h>
#include <logmsg.h>
#include <schedule.h>
#include <asm/notify.h>
#include <asm/page.h>
#include <asm/gdt.h>
#include <asm/security.h>
#include <asm/vm_config.h>

struct per_cpu_region {
	/* vmxon_region MUST be 4KB-aligned */
	uint8_t vmxon_region[PAGE_SIZE];
	void *vmcs_run;
#ifdef HV_DEBUG
	struct shared_buf *sbuf[ACRN_SBUF_ID_MAX];
	char logbuf[LOG_MESSAGE_MAX_SIZE];
	uint32_t npk_log_ref;
#endif
	uint64_t irq_count[NR_IRQS];
	uint64_t softirq_pending;
	uint64_t spurious;
	struct acrn_vcpu *ever_run_vcpu;
#ifdef STACK_PROTECTOR
	struct stack_canary stk_canary;
#endif
	struct per_cpu_timers cpu_timers;
	struct sched_control sched_ctl;
	struct sched_noop_control sched_noop_ctl;
	struct sched_iorr_control sched_iorr_ctl;
	struct sched_bvt_control sched_bvt_ctl;
	struct sched_prio_control sched_prio_ctl;
	struct thread_object idle;
	struct host_gdt gdt;
	struct tss_64 tss;
	enum pcpu_boot_state boot_state;
	uint64_t pcpu_flag;
	uint8_t mc_stack[CONFIG_STACK_SIZE] __aligned(16);
	uint8_t df_stack[CONFIG_STACK_SIZE] __aligned(16);
	uint8_t sf_stack[CONFIG_STACK_SIZE] __aligned(16);
	uint8_t stack[CONFIG_STACK_SIZE] __aligned(16);
	uint32_t lapic_id;
	uint32_t lapic_ldr;
	uint32_t softirq_servicing;
	uint32_t mode_to_kick_pcpu;
	struct smp_call_info_data smp_call_info;
	struct list_head softirq_dev_entry_list;
#ifdef PROFILING_ON
	struct profiling_info_wrapper profiling_info;
#endif
	uint64_t shutdown_vm_bitmap;
	uint64_t tsc_suspend;
	struct acrn_vcpu *whose_iwkey;
	/*
	 * We maintain a per-pCPU array of vCPUs. vCPUs of a VM won't
	 * share same pCPU. So the maximum possible # of vCPUs that can
	 * run on a pCPU is CONFIG_MAX_VM_NUM.
	 * vcpu_array address must be aligned to 64-bit for atomic access
	 * to avoid contention between offline_vcpu and posted interrupt handler
	 */
	struct acrn_vcpu *vcpu_array[CONFIG_MAX_VM_NUM] __aligned(8);
} __aligned(PAGE_SIZE); /* per_cpu_region size aligned with PAGE_SIZE */

extern struct per_cpu_region per_cpu_data[MAX_PCPU_NUM];
/*
 * get percpu data for pcpu_id.
 */
#define per_cpu(name, pcpu_id)	\
	(per_cpu_data[(pcpu_id)].name)

/* get percpu data for current pcpu */
#define get_cpu_var(name)	per_cpu(name, get_pcpu_id())

#endif
