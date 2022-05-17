/*
 * ACRN TRACE
 *
 * Copyright (C) 2017-2018 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Li Fei <fei1.li@intel.com>
 *
 */

#ifndef TRACE_H
#define TRACE_H
#include <per_cpu.h>
 /* TIMER EVENT */
#define TRACE_TIMER_ACTION_ADDED	0x1U
#define TRACE_TIMER_ACTION_PCKUP	0x2U
#define TRACE_TIMER_ACTION_UPDAT	0x3U
#define TRACE_TIMER_IRQ			0x4U

#define TRACE_VM_EXIT			0x10U
#define TRACE_VM_ENTER			0X11U
#define TRACE_VMEXIT_ENTRY		0x10000U

#define TRACE_VMEXIT_EXCEPTION_OR_NMI	    (TRACE_VMEXIT_ENTRY + 0x00000000U)
#define TRACE_VMEXIT_EXTERNAL_INTERRUPT     (TRACE_VMEXIT_ENTRY + 0x00000001U)
#define TRACE_VMEXIT_INTERRUPT_WINDOW	    (TRACE_VMEXIT_ENTRY + 0x00000002U)
#define TRACE_VMEXIT_CPUID		    (TRACE_VMEXIT_ENTRY + 0x00000004U)
#define TRACE_VMEXIT_RDTSC		    (TRACE_VMEXIT_ENTRY + 0x00000010U)
#define TRACE_VMEXIT_VMCALL		    (TRACE_VMEXIT_ENTRY + 0x00000012U)
#define TRACE_VMEXIT_CR_ACCESS		    (TRACE_VMEXIT_ENTRY + 0x0000001CU)
#define TRACE_VMEXIT_IO_INSTRUCTION	    (TRACE_VMEXIT_ENTRY + 0x0000001EU)
#define TRACE_VMEXIT_RDMSR		    (TRACE_VMEXIT_ENTRY + 0x0000001FU)
#define TRACE_VMEXIT_WRMSR		    (TRACE_VMEXIT_ENTRY + 0x00000020U)
#define TRACE_VMEXIT_EPT_VIOLATION	    (TRACE_VMEXIT_ENTRY + 0x00000030U)
#define TRACE_VMEXIT_EPT_MISCONFIGURATION   (TRACE_VMEXIT_ENTRY + 0x00000031U)
#define TRACE_VMEXIT_RDTSCP		    (TRACE_VMEXIT_ENTRY + 0x00000033U)
#define TRACE_VMEXIT_APICV_WRITE	    (TRACE_VMEXIT_ENTRY + 0x00000038U)
#define TRACE_VMEXIT_APICV_ACCESS	    (TRACE_VMEXIT_ENTRY + 0x00000039U)
#define TRACE_VMEXIT_APICV_VIRT_EOI	    (TRACE_VMEXIT_ENTRY + 0x0000003AU)

#define TRACE_VMEXIT_UNHANDLED		0x20000U

void TRACE_2L(uint32_t evid, uint64_t e, uint64_t f);
void TRACE_4I(uint32_t evid, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
void TRACE_6C(uint32_t evid, uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4, uint8_t b1, uint8_t b2);

#endif /* TRACE_H */
