/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <per_cpu.h>

#define TRACE_CUSTOM			0xFCU
#define TRACE_FUNC_ENTER		0xFDU
#define TRACE_FUNC_EXIT			0xFEU
#define TRACE_STR			0xFFU

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
	if (per_cpu(sbuf, cpu_id)[ACRN_TRACE] == NULL) {
		return false;
	}

	return true;
}

static inline void trace_put(uint16_t cpu_id, uint32_t evid, uint32_t n_data, struct trace_entry *entry)
{
	struct shared_buf *sbuf = (struct shared_buf *)
				per_cpu(sbuf, cpu_id)[ACRN_TRACE];

	entry->tsc = rdtsc();
	entry->id = evid;
	entry->n_data = (uint8_t)n_data;
	entry->cpu = (uint8_t)cpu_id;
	(void)sbuf_put(sbuf, (uint8_t *)entry);
}

void TRACE_2L(uint32_t evid, uint64_t e, uint64_t f)
{
	struct trace_entry entry;
	uint16_t cpu_id = get_cpu_id();

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
	uint16_t cpu_id = get_cpu_id();

	if (!trace_check(cpu_id)) {
		return;
	}

	entry.payload.fields_32.a = a;
	entry.payload.fields_32.b = b;
	entry.payload.fields_32.c = c;
	entry.payload.fields_32.d = d;
	trace_put(cpu_id, evid, 4U, &entry);
}

void TRACE_6C(uint32_t evid, uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4, uint8_t b1, uint8_t b2)
{
	struct trace_entry entry;
	uint16_t cpu_id = get_cpu_id();

	if (!trace_check(cpu_id)) {
		return;
	}

	entry.payload.fields_8.a1 = a1;
	entry.payload.fields_8.a2 = a2;
	entry.payload.fields_8.a3 = a3;
	entry.payload.fields_8.a4 = a4;
	entry.payload.fields_8.b1 = b1;
	entry.payload.fields_8.b2 = b2;
	/* payload.fields_8.b3/b4 not used, but is put in trace buf */
	trace_put(cpu_id, evid, 8U, &entry);
}

#define TRACE_ENTER TRACE_16STR(TRACE_FUNC_ENTER, __func__)
#define TRACE_EXIT TRACE_16STR(TRACE_FUNC_EXIT, __func__)

static inline void TRACE_16STR(uint32_t evid, const char name[])
{
	struct trace_entry entry;
	uint16_t cpu_id = get_cpu_id();
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
