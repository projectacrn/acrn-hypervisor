/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RTL_H
#define RTL_H

#include <types.h>

union u_qword {
	struct {
		uint32_t low;
		uint32_t high;
	} dwords;

	uint64_t qword;

};

/* MACRO related to string */
#define ULONG_MAX	((uint64_t)(~0UL))	/* 0xFFFFFFFF */
#define LONG_MAX	(ULONG_MAX >> 1U)	/* 0x7FFFFFFF */
#define LONG_MIN	(~LONG_MAX)		/* 0x80000000 */

static inline bool is_space(char c)
{
	return ((c == ' ') || (c == '\t'));
}

/* Function prototypes */
void udelay(uint32_t us);
int32_t strcmp(const char *s1_arg, const char *s2_arg);
int32_t strncmp(const char *s1_arg, const char *s2_arg, size_t n_arg);
char *strncpy_s(char *d_arg, size_t dmax, const char *s_arg, size_t slen_arg);
char *strchr(char *s_arg, char ch);
size_t strnlen_s(const char *str_arg, size_t maxlen_arg);
void *memset(void *base, uint8_t v, size_t n);
void *memcpy_s(void *d, size_t dmax, const void *s, size_t slen);
int64_t strtol_deci(const char *nptr);
uint64_t strtoul_hex(const char *nptr);
char *strstr_s(const char *str1, size_t maxlen1,
			const char *str2, size_t maxlen2);

/**
 * Frequency of TSC in KHz (where 1KHz = 1000Hz). Only valid after
 * calibrate_tsc() returns.
 */
extern uint32_t tsc_khz;

static inline uint64_t us_to_ticks(uint32_t us)
{
	return (((uint64_t)us * (uint64_t)tsc_khz) / 1000UL);
}

#define CYCLES_PER_MS	us_to_ticks(1000U)

static inline uint64_t ticks_to_us(uint64_t ticks)
{
	return (ticks * 1000UL) / (uint64_t)tsc_khz;
}

static inline uint64_t ticks_to_ms(uint64_t ticks)
{
	return ticks / (uint64_t)tsc_khz;
}

static inline uint64_t rdtsc(void)
{
	uint32_t lo, hi;

	asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
	return ((uint64_t)hi << 32U) | lo;
}
#endif /* RTL_H */
