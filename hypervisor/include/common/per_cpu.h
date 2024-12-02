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
#include <logmsg.h>
#include <schedule.h>
#include <asm/page.h>
#include <asm/vm_config.h>
#include <asm/per_cpu.h>
#include <asm/security.h>
#include <notify.h>
#include <cpu.h>
#include <board_info.h>


struct per_cpu_region {
	/*
	 * X86 arch percpu struct member vmxon_region
	 * need to be page size aligned, so we keep it
	 * on top to same memory,
	 */
	struct per_cpu_arch arch;
	uint64_t irq_count[NR_IRQS];
	uint64_t softirq_pending;
	uint64_t spurious;
	struct acrn_vcpu *ever_run_vcpu;
#ifdef STACK_PROTECTOR
	struct stack_canary stk_canary;
#endif
	struct per_cpu_timers cpu_timers;
	/*TODO: we only need sched_ctl as configured,
	  not neccessarily to have them all */
	struct sched_control sched_ctl;
	struct sched_noop_control sched_noop_ctl;
	struct sched_iorr_control sched_iorr_ctl;
	struct sched_bvt_control sched_bvt_ctl;
	struct sched_prio_control sched_prio_ctl;
	struct thread_object idle;
	uint64_t pcpu_flag;
	uint32_t softirq_servicing;
	uint32_t mode_to_kick_pcpu;
	uint32_t mode_to_idle;
	struct smp_call_info_data smp_call_info;
	struct list_head softirq_dev_entry_list;
	enum pcpu_boot_state boot_state;
	uint8_t stack[CONFIG_STACK_SIZE] __aligned(16);
	uint64_t shutdown_vm_bitmap;
	/*
	 * We maintain a per-pCPU array of vCPUs. vCPUs of a VM won't
	 * share same pCPU. So the maximum possible # of vCPUs that can
	 * run on a pCPU is CONFIG_MAX_VM_NUM.
	 * vcpu_array address must be aligned to 64-bit for atomic access
	 * to avoid contention between offline_vcpu and posted interrupt handler
	 */
	struct acrn_vcpu *vcpu_array[CONFIG_MAX_VM_NUM] __aligned(8);
	struct shared_buf *sbuf[ACRN_SBUF_PER_PCPU_ID_MAX];
#ifdef HV_DEBUG
	char logbuf[LOG_MESSAGE_MAX_SIZE];
	uint32_t npk_log_ref;
#endif

} __aligned(PAGE_SIZE); /* per_cpu_region size aligned with PAGE_SIZE */

extern struct per_cpu_region per_cpu_data[MAX_PCPU_NUM];
/*
 * get percpu data for pcpu_id.
 */
#define per_cpu(member_path, pcpu_id)	\
	(per_cpu_data[(pcpu_id)].member_path)

/* get percpu data for current pcpu */
#define get_cpu_var(member_path)	per_cpu(member_path, get_pcpu_id())

#endif /* PER_CPU_H */
