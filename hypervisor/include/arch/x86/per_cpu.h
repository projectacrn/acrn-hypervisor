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
#include "arch/x86/guest/instr_emul.h"

struct per_cpu_region {
#ifdef HV_DEBUG
	uint64_t *sbuf[ACRN_SBUF_ID_MAX];
	uint64_t vmexit_cnt[64];
	uint64_t vmexit_time[64];
	uint32_t npk_log_ref;
#endif
	uint64_t irq_count[NR_IRQS];
	uint64_t softirq_pending;
	uint64_t spurious;
	uint64_t vmxon_region_pa;
	struct shared_buf *earlylog_sbuf;
	void *vcpu;
	void *ever_run_vcpu;
#ifdef STACK_PROTECTOR
	struct stack_canary stk_canary;
#endif
	struct per_cpu_timers cpu_timers;
	struct sched_context sched_ctx;
	struct instr_emul_ctxt g_inst_ctxt;
	struct host_gdt gdt;
	struct tss_64 tss;
	enum cpu_state cpu_state;
	uint8_t mc_stack[CONFIG_STACK_SIZE] __aligned(16);
	uint8_t df_stack[CONFIG_STACK_SIZE] __aligned(16);
	uint8_t sf_stack[CONFIG_STACK_SIZE] __aligned(16);
	uint8_t stack[CONFIG_STACK_SIZE] __aligned(16);
	char logbuf[LOG_MESSAGE_MAX_SIZE];
	uint8_t lapic_id;
	struct smp_call_info_data smp_call_info;
} __aligned(CPU_PAGE_SIZE); //per_cpu_region size aligned with CPU_PAGE_SIZE

extern struct per_cpu_region *per_cpu_data_base_ptr;
extern uint16_t phys_cpu_num;
extern uint64_t pcpu_active_bitmap;
/*
 * get percpu data for pcpu_id.
 */
#define per_cpu(name, pcpu_id)	\
	(per_cpu_data_base_ptr[(pcpu_id)].name)

/* get percpu data for current pcpu */
#define get_cpu_var(name)	per_cpu(name, get_cpu_id())

#endif
