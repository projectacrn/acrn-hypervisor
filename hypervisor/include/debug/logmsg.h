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

#ifndef LOGMSG_H
#define LOGMSG_H

/* Logging severity levels */
#define LOG_FATAL		1
/* For msg should be write to console and sbuf meanwhile but not fatal error */
#define LOG_ACRN		2
#define LOG_ERROR		3
#define LOG_WARNING		4
#define LOG_INFO		5
#define LOG_DEBUG		6

/* Logging flags */
#define LOG_FLAG_STDOUT		0x00000001
#define LOG_FLAG_MEMORY		0x00000002

#if defined(HV_DEBUG)

extern uint32_t console_loglevel;
extern uint32_t mem_loglevel;
void init_logmsg(uint32_t mem_size, uint32_t flags);
void print_logmsg_buffer(uint32_t cpu_id);
void do_logmsg(uint32_t severity, const char *fmt, ...);

#else /* HV_DEBUG */

static inline void init_logmsg(__unused uint32_t mem_size,
			__unused uint32_t flags)
{
}

static inline void do_logmsg(__unused uint32_t severity,
			__unused const char *fmt, ...)
{
}

static inline void print_logmsg_buffer(__unused uint32_t cpu_id)
{
}

#endif /* HV_DEBUG */

#ifndef pr_prefix
#define pr_prefix
#endif

#define pr_fatal(...)						\
	do {							\
		do_logmsg(LOG_FATAL, pr_prefix __VA_ARGS__);	\
	} while (0)

#define pr_acrnlog(...)						\
	do {							\
		do_logmsg(LOG_ACRN, pr_prefix __VA_ARGS__);	\
	} while (0)

#define pr_err(...)						\
	do {							\
		do_logmsg(LOG_ERROR, pr_prefix __VA_ARGS__);	\
	} while (0)

#define pr_warn(...)						\
	do {							\
		do_logmsg(LOG_WARNING, pr_prefix __VA_ARGS__);	\
	} while (0)

#define pr_info(...)						\
	do {							\
		do_logmsg(LOG_INFO, pr_prefix __VA_ARGS__);	\
	} while (0)

#define pr_dbg(...)						\
	do {							\
		do_logmsg(LOG_DEBUG, pr_prefix __VA_ARGS__);	\
	} while (0)

#define dev_dbg(lvl, ...)					\
	do {							\
		do_logmsg(lvl, pr_prefix __VA_ARGS__);	\
	} while (0)

#define panic(...) 							\
	do { pr_fatal("PANIC: %s line: %d\n", __func__, __LINE__);	\
		pr_fatal(__VA_ARGS__); 					\
		while (1) { asm volatile ("pause" ::: "memory"); }; } while (0)

#endif /* LOGMSG_H */
