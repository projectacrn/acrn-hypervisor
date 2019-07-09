/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <atomic.h>
#include <sprintf.h>
#include <spinlock.h>
#include <per_cpu.h>
#include <npk_log.h>
#include <logmsg.h>

/* buf size should be identical to the size in hvlog option, which is
 * transfered to SOS:
 * bsp/uefi/clearlinux/acrn.conf: hvlog=2M@0x1FE00000
 */

struct acrn_logmsg_ctl {
	uint32_t flags;
	int32_t seq;
	spinlock_t lock;
};

static struct acrn_logmsg_ctl logmsg_ctl;

void init_logmsg(uint32_t flags)
{
	logmsg_ctl.flags = flags;
	logmsg_ctl.seq = 0;
}

void do_logmsg(uint32_t severity, const char *fmt, ...)
{
	va_list args;
	uint64_t timestamp, rflags;
	uint16_t pcpu_id;
	bool do_console_log;
	bool do_mem_log;
	bool do_npk_log;
	char *buffer;

	do_console_log = (((logmsg_ctl.flags & LOG_FLAG_STDOUT) != 0U) && (severity <= console_loglevel));
	do_mem_log = (((logmsg_ctl.flags & LOG_FLAG_MEMORY) != 0U) && (severity <= mem_loglevel));
	do_npk_log = ((logmsg_ctl.flags & LOG_FLAG_NPK) != 0U && (severity <= npk_loglevel));

	if (!do_console_log && !do_mem_log && !do_npk_log) {
		return;
	}

	/* Get time-stamp value */
	timestamp = rdtsc();

	/* Scale time-stamp appropriately */
	timestamp = ticks_to_us(timestamp);

	/* Get CPU ID */
	pcpu_id = get_pcpu_id();
	buffer = per_cpu(logbuf, pcpu_id);

	(void)memset(buffer, 0U, LOG_MESSAGE_MAX_SIZE);
	/* Put time-stamp, CPU ID and severity into buffer */
	snprintf(buffer, LOG_MESSAGE_MAX_SIZE, "[%lluus][cpu=%hu][sev=%u][seq=%u]:",
			timestamp, pcpu_id, severity, atomic_inc_return(&logmsg_ctl.seq));

	/* Put message into remaining portion of local buffer */
	va_start(args, fmt);
	vsnprintf(buffer + strnlen_s(buffer, LOG_MESSAGE_MAX_SIZE),
		LOG_MESSAGE_MAX_SIZE
		- strnlen_s(buffer, LOG_MESSAGE_MAX_SIZE), fmt, args);
	va_end(args);

	/* Check if flags specify to output to NPK */
	if (do_npk_log) {
		npk_log_write(buffer, strnlen_s(buffer, LOG_MESSAGE_MAX_SIZE));
	}

	/* Check if flags specify to output to stdout */
	if (do_console_log) {
		spinlock_irqsave_obtain(&(logmsg_ctl.lock), &rflags);

		/* Send buffer to stdout */
		printf("%s\n\r", buffer);

		spinlock_irqrestore_release(&(logmsg_ctl.lock), rflags);
	}

	/* Check if flags specify to output to memory */
	if (do_mem_log) {
		uint32_t i, msg_len;
		struct shared_buf *sbuf = per_cpu(sbuf, pcpu_id)[ACRN_HVLOG];

		/* If sbuf is not ready, we just drop the massage */
		if (sbuf != NULL) {
			msg_len = strnlen_s(buffer, LOG_MESSAGE_MAX_SIZE);

			for (i = 0U; i < (((msg_len - 1U) / LOG_ENTRY_SIZE) + 1U);
					i++) {
				(void)sbuf_put(sbuf, (uint8_t *)buffer +
							(i * LOG_ENTRY_SIZE));
			}
		}
	}
}
