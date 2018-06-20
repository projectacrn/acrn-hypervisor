/*
 * ACRN TRACE
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
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
#define TRACE_TIMER_ACTION_ADDED	0x1
#define TRACE_TIMER_ACTION_PCKUP	0x2
#define TRACE_TIMER_ACTION_UPDAT	0x3
#define TRACE_TIMER_IRQ			0x4

#define TRACE_VM_EXIT			0x10
#define TRACE_VM_ENTER			0X11
#define TRC_VMEXIT_ENTRY		0x10000

#define TRC_VMEXIT_EXCEPTION_OR_NMI	(TRC_VMEXIT_ENTRY + 0x00000000)
#define TRC_VMEXIT_EXTERNAL_INTERRUPT	(TRC_VMEXIT_ENTRY + 0x00000001)
#define TRC_VMEXIT_INTERRUPT_WINDOW	(TRC_VMEXIT_ENTRY + 0x00000002)
#define TRC_VMEXIT_CPUID		(TRC_VMEXIT_ENTRY + 0x00000004)
#define TRC_VMEXIT_RDTSC		(TRC_VMEXIT_ENTRY + 0x00000010)
#define TRC_VMEXIT_VMCALL		(TRC_VMEXIT_ENTRY + 0x00000012)
#define TRC_VMEXIT_CR_ACCESS		(TRC_VMEXIT_ENTRY + 0x0000001C)
#define TRC_VMEXIT_IO_INSTRUCTION	(TRC_VMEXIT_ENTRY + 0x0000001E)
#define TRC_VMEXIT_RDMSR		(TRC_VMEXIT_ENTRY + 0x0000001F)
#define TRC_VMEXIT_WRMSR		(TRC_VMEXIT_ENTRY + 0x00000020)
#define TRC_VMEXIT_EPT_VIOLATION	(TRC_VMEXIT_ENTRY + 0x00000030)
#define TRC_VMEXIT_EPT_MISCONFIGURATION	(TRC_VMEXIT_ENTRY + 0x00000031)
#define TRC_VMEXIT_RDTSCP		(TRC_VMEXIT_ENTRY + 0x00000033)
#define TRC_VMEXIT_APICV_WRITE		(TRC_VMEXIT_ENTRY + 0x00000038)
#define TRC_VMEXIT_APICV_ACCESS		(TRC_VMEXIT_ENTRY + 0x00000039)
#define TRC_VMEXIT_APICV_VIRT_EOI	(TRC_VMEXIT_ENTRY + 0x0000003A)

#define TRC_VMEXIT_UNHANDLED		0x20000

#ifdef HV_DEBUG

#include <sbuf.h>

#define TRACE_CUSTOM			0xFC
#define TRACE_FUNC_ENTER		0xFD
#define TRACE_FUNC_EXIT			0xFE
#define TRACE_STR			0xFF

/* sizeof(trace_entry) == 3 x 64bit */
struct trace_entry {
	uint64_t tsc; /* TSC */
	uint64_t id;
	union {
		struct {
			uint32_t a, b, c, d;
		} fields_32;
		struct {
			uint8_t a1, a2, a3, a4;
			uint8_t b1, b2, b3, b4;
			uint8_t c1, c2, c3, c4;
			uint8_t d1, d2, d3, d4;
		} fields_8;
		struct {
			uint64_t e;
			uint64_t f;
		} fields_64;
		char str[16];
	} payload;
} __attribute__((aligned(8)));

static inline bool
trace_check(uint16_t cpu_id, __unused int evid)
{
	if (cpu_id >= phy_cpu_num)
		return false;

	if (per_cpu(sbuf, cpu_id)[ACRN_TRACE] == NULL)
		return false;

	return true;
}

static inline void
_trace_put(uint16_t cpu_id, int evid, struct trace_entry *entry)
{
	struct shared_buf *sbuf = (struct shared_buf *)
				per_cpu(sbuf, cpu_id)[ACRN_TRACE];

	entry->tsc = rdtsc();
	entry->id = evid;
	sbuf_put(sbuf, (uint8_t *)entry);
}

static inline void
TRACE_2L(int evid, uint64_t e, uint64_t f)
{
	struct trace_entry entry;
	uint16_t cpu_id = get_cpu_id();

	if (!trace_check(cpu_id, evid))
		return;

	entry.payload.fields_64.e = e;
	entry.payload.fields_64.f = f;
	_trace_put(cpu_id, evid, &entry);
}

static inline void
TRACE_4I(int evid, uint32_t a, uint32_t b, uint32_t c,
		uint32_t d)
{
	struct trace_entry entry;
	uint16_t cpu_id = get_cpu_id();

	if (!trace_check(cpu_id, evid))
		return;

	entry.payload.fields_32.a = a;
	entry.payload.fields_32.b = b;
	entry.payload.fields_32.c = c;
	entry.payload.fields_32.d = d;
	_trace_put(cpu_id, evid, &entry);
}

static inline void
TRACE_6C(int evid, uint8_t a1, uint8_t a2, uint8_t a3,
		uint8_t a4, uint8_t b1, uint8_t b2)
{
	struct trace_entry entry;
	uint16_t cpu_id = get_cpu_id();

	if (!trace_check(cpu_id, evid))
		return;

	entry.payload.fields_8.a1 = a1;
	entry.payload.fields_8.a2 = a2;
	entry.payload.fields_8.a3 = a3;
	entry.payload.fields_8.a4 = a4;
	entry.payload.fields_8.b1 = b1;
	entry.payload.fields_8.b2 = b2;
	_trace_put(cpu_id, evid, &entry);
}

#define TRACE_ENTER TRACE_16STR(TRACE_FUNC_ENTER, __func__)
#define TRACE_EXIT TRACE_16STR(TRACE_FUNC_EXIT, __func__)

static inline void
TRACE_16STR(int evid, const char name[])
{
	struct trace_entry entry;
	uint16_t cpu_id = get_cpu_id();
	int len;
	int i;

	if (!trace_check(cpu_id, evid))
		return;

	entry.payload.fields_64.e = 0;
	entry.payload.fields_64.f = 0;

	len = strnlen_s(name, 20);
	len = (len > 16) ? 16 : len;
	for (i = 0; i < len; i++)
		entry.payload.str[i] = name[i];

	entry.payload.str[15] = 0;
	_trace_put(cpu_id, evid, &entry);
}

#else /* HV_DEBUG */

#define TRACE_ENTER
#define TRACE_EXIT

static inline void
TRACE_2L(__unused int evid,
		__unused uint64_t e,
		__unused uint64_t f)
{
}

static inline void
TRACE_4I(__unused int evid,
		__unused uint32_t a,
		__unused uint32_t b,
		__unused uint32_t c,
		__unused uint32_t d)
{
}

static inline void
TRACE_6C(__unused int evid,
		__unused uint8_t a1,
		__unused uint8_t a2,
		__unused uint8_t a3,
		__unused uint8_t a4,
		__unused uint8_t b1,
		__unused uint8_t b2)
{
}

#endif /* HV_DEBUG */

#endif /* TRACE_H */
