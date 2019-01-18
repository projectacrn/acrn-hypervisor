/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PER_CPU_H
#define PER_CPU_H

#include <hypervisor.h>
#include <bsp_extern.h>
#include <schedule.h>
#include <common/irq.h>
#include <arch/x86/irq.h>
#include <sbuf.h>
#include <gdt.h>
#include <timer.h>
#include <logmsg.h>
#include <instr_emul.h>
#include <profiling.h>
#include <security.h>

struct per_cpu_region {
	/* vmxon_region MUST be 4KB-aligned */
	uint8_t vmxon_region[PAGE_SIZE];
	void *vmcs_run;
#ifdef HV_DEBUG
	uint64_t *sbuf[ACRN_SBUF_ID_MAX];
	char logbuf[LOG_MESSAGE_MAX_SIZE];
	uint32_t npk_log_ref;
#endif
	uint64_t irq_count[NR_IRQS];
	uint64_t softirq_pending;
	uint64_t spurious;
	void *vcpu;
	void *ever_run_vcpu;
#ifdef STACK_PROTECTOR
	struct stack_canary stk_canary;
#endif
	struct per_cpu_timers cpu_timers;
	struct sched_context sched_ctx;
	struct sched_object idle;
	struct instr_emul_ctxt g_inst_ctxt;
	struct host_gdt gdt;
	struct tss_64 tss;
	enum pcpu_boot_state boot_state;
	uint8_t mc_stack[CONFIG_STACK_SIZE] __aligned(16);
	uint8_t df_stack[CONFIG_STACK_SIZE] __aligned(16);
	uint8_t sf_stack[CONFIG_STACK_SIZE] __aligned(16);
	uint8_t stack[CONFIG_STACK_SIZE] __aligned(16);
	uint32_t lapic_id;
	uint32_t lapic_ldr;
	struct smp_call_info_data smp_call_info;
#ifdef PROFILING_ON
	struct profiling_info_wrapper profiling_info;
#endif
} __aligned(PAGE_SIZE); /* per_cpu_region size aligned with PAGE_SIZE */

extern struct per_cpu_region per_cpu_data[CONFIG_MAX_PCPU_NUM];
/*
 * get percpu data for pcpu_id.
 */
#define per_cpu(name, pcpu_id)	\
	(per_cpu_data[(pcpu_id)].name)

/* get percpu data for current pcpu */
#define get_cpu_var(name)	per_cpu(name, get_cpu_id())

#endif
