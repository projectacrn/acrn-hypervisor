/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <per_cpu.h>
#include <atomic.h>
#include <sprintf.h>
#include <spinlock.h>
#include <npk_log.h>
#include <logmsg.h>
#include <ticks.h>

/* buf size should be identical to the size in hvlog option, which is
 * transfered to Service VM:
 * bsp/uefi/clearlinux/acrn.conf: hvlog=2M@0x1FE00000
 */

struct acrn_logmsg_ctl {
	int32_t seq;
	spinlock_t lock;
};

static struct acrn_logmsg_ctl logmsg_ctl;

void init_logmsg()
{
	logmsg_ctl.seq = 0;

	spinlock_init(&(logmsg_ctl.lock));
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
	struct thread_object *current;

	do_console_log = (severity <= console_loglevel);
	do_mem_log = (severity <= mem_loglevel);
	do_npk_log = (severity <= npk_loglevel);

	if (!do_console_log && !do_mem_log && !do_npk_log) {
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
			timestamp, pcpu_id, current->name, severity, atomic_inc_return(&logmsg_ctl.seq));

	/* Put message into remaining portion of local buffer */
	va_start(args, fmt);
	vsnprintf(buffer + strnlen_s(buffer, LOG_MESSAGE_MAX_SIZE),
		LOG_MESSAGE_MAX_SIZE
		- strnlen_s(buffer, LOG_MESSAGE_MAX_SIZE), fmt, args);
	va_end(args);

	/* Check whether output to NPK */
	if (do_npk_log) {
		npk_log_write(buffer, strnlen_s(buffer, LOG_MESSAGE_MAX_SIZE));
	}

	/* Check whether output to stdout */
	if (do_console_log) {
		spinlock_irqsave_obtain(&(logmsg_ctl.lock), &rflags);

		/* Send buffer to stdout */
		printf("%s\n\r", buffer);

		spinlock_irqrestore_release(&(logmsg_ctl.lock), rflags);
	}

	/* Check whether output to memory */
	if (do_mem_log) {
		uint32_t msg_len;
		struct shared_buf *sbuf = per_cpu(sbuf, pcpu_id)[ACRN_HVLOG];

		/* If sbuf is not ready, we just drop the massage */
		if (sbuf != NULL) {
			msg_len = strnlen_s(buffer, LOG_MESSAGE_MAX_SIZE);
			(void)sbuf_put_many(sbuf, LOG_ENTRY_SIZE, (uint8_t *)buffer,
				LOG_ENTRY_SIZE * (((msg_len - 1U) / LOG_ENTRY_SIZE) + 1));
		}
	}
}
