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

struct udiv_result {
	union u_qword q;
	union u_qword r;

};

/* Function prototypes */
void udelay(uint32_t us);
void *memchr(const void *void_s, int c, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy_s(char *d, size_t dmax, const char *s_arg);
char *strncpy_s(char *d, size_t dmax, const char *s, size_t slen_arg);
char *strchr(const char *s, int ch);
void mdelay(uint32_t loop_count_arg);
size_t strnlen_s(const char *str, size_t maxlen_arg);
void *memset(void *base, uint8_t v, size_t n);
void *memcpy_s(void *d, size_t dmax, const void *s, size_t slen_arg);
int udiv64(uint64_t dividend_arg, uint64_t divisor_arg, struct udiv_result *res);
int udiv32(uint32_t dividend, uint32_t divisor, struct udiv_result *res);
int atoi(const char *str);
long strtol_deci(const char *nptr);
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
	return ((uint64_t)hi << 32) | lo;
}
#endif /* RTL_H */
