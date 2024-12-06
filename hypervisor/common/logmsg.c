/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <per_cpu.h>
#include <atomic.h>
#include <sprintf.h>
#include <logmsg.h>
#include <ticks.h>
#include <console.h>
#include <npk_log.h>

/* buf size should be identical to the size in hvlog option, which is
 * transfered to Service VM:
 * bsp/uefi/clearlinux/acrn.conf: hvlog=2M@0x1FE00000
 */

static int32_t log_seq = 0;

uint16_t mem_loglevel = CONFIG_MEM_LOGLEVEL_DEFAULT;

static inline bool mem_need_log(uint32_t severity)
{
	return (severity <= mem_loglevel);
}

static void mem_log(uint16_t pcpu_id, char *buffer)
{
	uint32_t msg_len;
	struct shared_buf *sbuf = per_cpu(sbuf, pcpu_id)[ACRN_HVLOG];

	/* If sbuf is not ready, we just drop the massage */
	if (sbuf != NULL) {
		msg_len = strnlen_s(buffer, LOG_MESSAGE_MAX_SIZE);
		(void)sbuf_put_many(sbuf, LOG_ENTRY_SIZE, (uint8_t *)buffer,
			LOG_ENTRY_SIZE * (((msg_len - 1U) / LOG_ENTRY_SIZE) + 1));
	}
}

void do_logmsg(uint32_t severity, const char *fmt, ...)
{
	va_list args;
	uint64_t timestamp;
	uint16_t pcpu_id;
	char *buffer;
	struct thread_object *current;

	if (!mem_need_log(severity) && !console_need_log(severity) && !npk_need_log(severity)) {
		return;
	}

	/* Get time-stamp value */
	timestamp = cpu_ticks();

	/* Scale time-stamp appropriately */
	timestamp = ticks_to_us(timestamp);

	/* Get CPU ID */
	pcpu_id = get_pcpu_id();
	buffer = per_cpu(logbuf, pcpu_id);
	current = sched_get_current(pcpu_id);

	(void)memset(buffer, 0U, LOG_MESSAGE_MAX_SIZE);
	/* Put time-stamp, CPU ID and severity into buffer */
	snprintf(buffer, LOG_MESSAGE_MAX_SIZE, "[%luus][cpu=%hu][%s][sev=%u][seq=%u]:",
			timestamp, pcpu_id, current->name, severity, atomic_inc_return(&log_seq));

	/* Put message into remaining portion of local buffer */
	va_start(args, fmt);
	vsnprintf(buffer + strnlen_s(buffer, LOG_MESSAGE_MAX_SIZE),
		LOG_MESSAGE_MAX_SIZE
		- strnlen_s(buffer, LOG_MESSAGE_MAX_SIZE), fmt, args);
	va_end(args);

	/* Check whether output to memory */
	if (mem_need_log(severity)) {
		mem_log(pcpu_id, buffer);
	}

	/* Check whether output to stdout */
	if (console_need_log(severity)) {
		console_log(buffer);
	}

	/* Check whether output to NPK */
	if (npk_need_log(severity)) {
		npk_log(buffer);
	}
}
