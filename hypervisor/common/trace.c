/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <per_cpu.h>
#include <ticks.h>
#include <trace.h>

#ifdef CONFIG_ACRNTRACE_ENABLED

/* sizeof(trace_entry) == 4 x 64bit */
struct trace_entry {
	uint64_t tsc; /* TSC */
	uint64_t id:48;
	uint8_t n_data; /* nr of data in trace_entry */
	uint8_t cpu; /* pcpu id of trace_entry */

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
} __aligned(8);

static inline bool trace_check(uint16_t cpu_id)
{
	return (per_cpu(sbuf, cpu_id)[ACRN_TRACE] != NULL);
}

static inline void trace_put(uint16_t cpu_id, uint32_t evid, uint32_t n_data, struct trace_entry *entry)
{
	struct shared_buf *sbuf = per_cpu(sbuf, cpu_id)[ACRN_TRACE];

	entry->tsc = cpu_ticks();
	entry->id = evid;
	entry->n_data = (uint8_t)n_data;
	entry->cpu = (uint8_t)cpu_id;
	(void)sbuf_put(sbuf, (uint8_t *)entry, sizeof(*entry));
}

void TRACE_2L(uint32_t evid, uint64_t e, uint64_t f)
{
	struct trace_entry entry;
	uint16_t cpu_id = get_pcpu_id();

	if (!trace_check(cpu_id)) {
		return;
	}

	entry.payload.fields_64.e = e;
	entry.payload.fields_64.f = f;
	trace_put(cpu_id, evid, 2U, &entry);
}

void TRACE_4I(uint32_t evid, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
	struct trace_entry entry;
	uint16_t cpu_id = get_pcpu_id();

	if (!trace_check(cpu_id)) {
		return;
	}

	entry.payload.fields_32.a = a;
	entry.payload.fields_32.b = b;
	entry.payload.fields_32.c = c;
	entry.payload.fields_32.d = d;
	trace_put(cpu_id, evid, 4U, &entry);
}

void TRACE_16STR(uint32_t evid, const char name[])
{
	struct trace_entry entry;
	uint16_t cpu_id = get_pcpu_id();
	size_t len, i;

	if (!trace_check(cpu_id)) {
		return;
	}

	entry.payload.fields_64.e = 0UL;
	entry.payload.fields_64.f = 0UL;

	len = strnlen_s(name, 20U);
	len = (len > 16U) ? 16U : len;
	for (i = 0U; i < len; i++) {
		entry.payload.str[i] = name[i];
	}

	entry.payload.str[15] = 0;
	trace_put(cpu_id, evid, 16U, &entry);
}

#else

void TRACE_2L(__unused uint32_t evid, __unused uint64_t e, __unused uint64_t f) {}

void TRACE_4I(__unused uint32_t evid, __unused uint32_t a, __unused uint32_t b,
		__unused uint32_t c, __unused uint32_t d)
{
}

void TRACE_16STR(__unused uint32_t evid, __unused const char name[]) {}

#endif
