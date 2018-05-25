/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TRACE_EVENT_H
#define TRACE_EVENT_H

#define GEN_CASE(id) case (id):{ id##_FMT; break; }

/* TIMER */
#define TRACE_TIMER_ACTION_ADDED	0x1
#define TRACE_TIMER_ACTION_PCKUP	0x2
#define TRACE_TIMER_ACTION_UPDAT	0x3
#define TRACE_TIMER_IRQ			0x4

#define TRACE_VM_EXIT	                0x10
#define TRACE_VM_ENTER                  0X11
#define TRC_VMEXIT_ENTRY                0x10000

#define TRC_VMEXIT_EXCEPTION_OR_NMI     (TRC_VMEXIT_ENTRY + 0x00000000)
#define TRC_VMEXIT_EXTERNAL_INTERRUPT   (TRC_VMEXIT_ENTRY + 0x00000001)
#define TRC_VMEXIT_INTERRUPT_WINDOW     (TRC_VMEXIT_ENTRY + 0x00000002)
#define TRC_VMEXIT_CPUID                (TRC_VMEXIT_ENTRY + 0x00000004)
#define TRC_VMEXIT_RDTSC                (TRC_VMEXIT_ENTRY + 0x00000010)
#define TRC_VMEXIT_VMCALL               (TRC_VMEXIT_ENTRY + 0x00000012)
#define TRC_VMEXIT_CR_ACCESS            (TRC_VMEXIT_ENTRY + 0x0000001C)
#define TRC_VMEXIT_IO_INSTRUCTION       (TRC_VMEXIT_ENTRY + 0x0000001E)
#define TRC_VMEXIT_RDMSR                (TRC_VMEXIT_ENTRY + 0x0000001F)
#define TRC_VMEXIT_WRMSR                (TRC_VMEXIT_ENTRY + 0x00000020)
#define TRC_VMEXIT_EPT_VIOLATION        (TRC_VMEXIT_ENTRY + 0x00000030)
#define TRC_VMEXIT_EPT_MISCONFIGURATION (TRC_VMEXIT_ENTRY + 0x00000031)
#define TRC_VMEXIT_RDTSCP               (TRC_VMEXIT_ENTRY + 0x00000033)
#define TRC_VMEXIT_APICV_WRITE          (TRC_VMEXIT_ENTRY + 0x00000038)
#define TRC_VMEXIT_APICV_ACCESS         (TRC_VMEXIT_ENTRY + 0x00000039)
#define TRC_VMEXIT_APICV_VIRT_EOI       (TRC_VMEXIT_ENTRY + 0x0000003A)

#define TRC_VMEXIT_UNHANDLED          0x20000

#define TRACE_CUSTOM			0xFC
#define TRACE_FUNC_ENTER		0xFD
#define TRACE_FUNC_EXIT			0xFE
#define TRACE_STR			0xFF
/* TRACE_EVENTID_MAX 256 */

#define PR(fmt, ...)		fprintf((fp), fmt, ##__VA_ARGS__);

#define TRACE_TIMER_ACTION_ADDED_FMT				\
{PR("TIMER_ACTION ADDED: ID %d, deadline %lx total %d\n",	\
	(e).a, ((uint64_t)((e).c)<<32)|(e).b, (e).d); }

#define TRACE_TIMER_ACTION_PCKUP_FMT				\
{PR("TIMER_ACTION PCKUP: ID %d, deadline %lx total %d\n",	\
	(e).a, ((uint64_t)((e).c)<<32)|(e).b, (e).d); }

#define TRACE_TIMER_ACTION_UPDAT_FMT				\
{PR("TIMER_ACTION UPDAT: ID %d, deadline %lx total %d\n",	\
	(e).a, ((unsigned long)((e).c)<<32)|(e).b, (e).d); }

#define TRACE_TIMER_IRQ_FMT					\
PR("TIMER_IRQ total: %lx\n", (e).e)

#define TRACE_CUSTOM_FMT					\
PR("CUSTOM: 0x%lx 0x%lx\n", (e).e, (e).f)

#define TRACE_FUNC_ENTER_FMT					\
PR("ENTER: %s\n", (e).str)

#define TRACE_FUNC_EXIT_FMT					\
PR("EXIT : %s\n", (e).str)

#define TRACE_STR_FMT						\
PR("STR: %s\n", (e).str)

#define TRACE_VM_EXIT_FMT					\
PR("VM_EXIT: exit_reason 0x%016lx, guest_rip 0x%016lx\n",	\
	(e).e, (e).f)

#define TRACE_VM_ENTER_FMT                                  	\
PR("VM_ENTER:\n")

#define TRC_VMEXIT_EXCEPTION_OR_NMI_FMT				\
PR("VMEXIT_EXCEPTION_OR_NMI:					\
	vec 0x%08x, err_code 0x%08x, type %d\n",		\
	(e).a, (e).b, (e).c)

#define TRC_VMEXIT_EXTERNAL_INTERRUPT_FMT			\
PR("VMEXIT_EXTERNAL_INTERRUPT: vec 0x%08lx\n", (e).e)

#define TRC_VMEXIT_INTERRUPT_WINDOW_FMT				\
PR("VMEXIT_INTERRUPT_WINDOW:\n")

#define TRC_VMEXIT_CPUID_FMT					\
PR("VMEXIT_CPUID: vcpuid %lu\n", (e).e)

#define TRC_VMEXIT_RDTSC_FMT					\
PR("VMEXIT_RDTSC: host_tsc 0x%016lx, tsc_offset 0x%016lx\n",	\
	(e).e, (e).f)

#define	TRC_VMEXIT_VMCALL_FMT					\
PR("VMEXIT_VMCALL: vmid %lu, hypercall_id %lu\n",		\
	(e).e, (e).f)

#define TRC_VMEXIT_CR_ACCESS_FMT				\
PR("VMEXIT_CR_ACCESS: op %s, rn_nr %lu\n",			\
	(e).e?"Read":"Write", (e).f)

#define TRC_VMEXIT_IO_INSTRUCTION_FMT				\
PR("VMEXIT_IO_INSTRUCTION:					\
	port %u, dir %u, sz %u, cur_ctx_idx %u\n",		\
	(e).a, (e).b, (e).c, (e).d)

#define TRC_VMEXIT_RDMSR_FMT					\
PR("VMEXIT_RDMSR: msr 0x%08lx, val 0x%08lx\n",			\
	(e).e, (e).f)

#define TRC_VMEXIT_WRMSR_FMT					\
PR("VMEXIT_WRMSR: msr 0x%08lx, val 0x%08lx\n",			\
	(e).e, (e).f)

#define TRC_VMEXIT_EPT_VIOLATION_FMT				\
PR("VMEXIT_EPT_VIOLATION: qual 0x%016lx, gpa 0x%016lx\n",	\
	(e).e, (e).f)

#define TRC_VMEXIT_EPT_MISCONFIGURATION_FMT			\
PR("VMEXIT_EPT_MISCONFIGURATION:\n")

#define TRC_VMEXIT_RDTSCP_FMT					\
PR("VMEXIT_RDTSCP: guest_tsc 0x%lx, tsc_aux 0x%lx\n",		\
	(e).e, (e).f)

#define TRC_VMEXIT_APICV_WRITE_FMT				\
PR("VMEXIT_APICV_WRITE: offset 0x%lx\n", (e).e)

#define TRC_VMEXIT_APICV_ACCESS_FMT				\
PR("VMEXIT_APICV_ACCESS:\n")

#define TRC_VMEXIT_APICV_VIRT_EOI_FMT				\
PR("VMEXIT_APICV_VIRT_EOI: vec 0x%08lx\n", (e).e)

#define TRC_VMEXIT_UNHANDLED_FMT				\
PR("VMEXIT_UNHANDLED: 0x%08lx\n", (e).e)

#define ALL_CASES				\
	GEN_CASE(TRACE_TIMER_ACTION_ADDED);	\
	GEN_CASE(TRACE_TIMER_ACTION_PCKUP);	\
	GEN_CASE(TRACE_TIMER_ACTION_UPDAT);	\
	GEN_CASE(TRACE_TIMER_IRQ);		\
	GEN_CASE(TRACE_CUSTOM);			\
	GEN_CASE(TRACE_STR);			\
	GEN_CASE(TRACE_FUNC_ENTER);		\
	GEN_CASE(TRACE_FUNC_EXIT);		\
	GEN_CASE(TRACE_VM_EXIT);		\
	GEN_CASE(TRACE_VM_ENTER);		\
	GEN_CASE(TRC_VMEXIT_EXCEPTION_OR_NMI);	\
	GEN_CASE(TRC_VMEXIT_EXTERNAL_INTERRUPT);\
	GEN_CASE(TRC_VMEXIT_INTERRUPT_WINDOW);	\
	GEN_CASE(TRC_VMEXIT_CPUID);		\
	GEN_CASE(TRC_VMEXIT_RDTSC);		\
	GEN_CASE(TRC_VMEXIT_VMCALL);		\
	GEN_CASE(TRC_VMEXIT_CR_ACCESS);		\
	GEN_CASE(TRC_VMEXIT_IO_INSTRUCTION);	\
	GEN_CASE(TRC_VMEXIT_RDMSR);		\
	GEN_CASE(TRC_VMEXIT_WRMSR);		\
	GEN_CASE(TRC_VMEXIT_EPT_VIOLATION);	\
	GEN_CASE(TRC_VMEXIT_EPT_MISCONFIGURATION);\
	GEN_CASE(TRC_VMEXIT_RDTSCP);		\
	GEN_CASE(TRC_VMEXIT_APICV_WRITE);	\
	GEN_CASE(TRC_VMEXIT_APICV_ACCESS);	\
	GEN_CASE(TRC_VMEXIT_APICV_VIRT_EOI);	\
	GEN_CASE(TRC_VMEXIT_UNHANDLED);	\

#endif /* TRACE_EVENT_H */
