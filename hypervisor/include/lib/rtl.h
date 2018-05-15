/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
void udelay(int us);
void *memchr(const void *void_s, int c, size_t n);
void *memmove(void *s1, const void *s2, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy_s(char *d, size_t dmax, const char *s);
char *strncpy_s(char *d, size_t dmax, const char *s, size_t slen);
char *strchr(const char *s, int ch);
void mdelay(unsigned int ms);
size_t strnlen_s(const char *str, size_t maxlen);
void *memset(void *base, uint8_t v, size_t n);
void *memcpy_s(void *d, size_t dmax, const void *s, size_t slen);
int udiv64(uint64_t dividend, uint64_t divisor, struct udiv_result *res);
int udiv32(uint32_t dividend, uint32_t divisor, struct udiv_result *res);
int atoi(const char *str);
long strtol(const char *nptr, char **endptr, register int base);
uint64_t strtoul(const char *nptr, char **endptr, register int base);

extern uint64_t tsc_hz;
#define US_TO_TICKS(x)	((x) * tsc_hz / 1000000UL)
#define CYCLES_PER_MS	US_TO_TICKS(1000UL)

#define TICKS_TO_US(x)	((((x) * (1000000UL >> 8)) / tsc_hz) << 8)
#define TICKS_TO_MS(x)	(((x) * 1000UL) / tsc_hz)

static inline uint64_t rdtsc(void)
{
	uint32_t lo, hi;

	asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
	return ((uint64_t)hi << 32) | lo;
}
#endif /* RTL_H */
