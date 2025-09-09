/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/cpu.h>
#include <asm/lib/atomic.h>
#include <asm/lib/bits.h>
#include <per_cpu.h>
#include <asm/notify.h>
#include <common/notify.h>
#include <debug/logmsg.h>
#include <asm/cpu.h>
#include <cpu.h>

static volatile uint64_t smp_call_mask = 0UL;

/**
 * Run in interrupt context.
 *
 * Two use cases are covered:
 *  - SMP call: when the corresponding bit in smp_call_mask is set and
 *              smp_call_info in the per-CPU region is specified.
 *              The registered callback will be invoked.
 *
 *  - Kick pCPU out of non-root mode: when the corresponding bit in smp_call_mask is clear and
 *                                    smp_call_info in the per-CPU region is not specified.
 *                                    No callback is invoked.
 */
void kick_notification(__unused uint32_t irq, __unused void *data)
{
	uint16_t pcpu_id = get_pcpu_id();

	if (bitmap_test(pcpu_id, &smp_call_mask)) {
		struct smp_call_info_data *smp_call = &per_cpu(smp_call_info, pcpu_id);

		if (smp_call->func != NULL) {
			smp_call->func(smp_call->data);
		}
		bitmap_clear_lock(pcpu_id, &smp_call_mask);
	}
}

/**
 * Execute the SMP call notification handler
 */
void handle_smp_call(void)
{
	kick_notification(0U, NULL);
}

/**
 * Trigger the SMP call request to target pCPUs
 */
void smp_call_function(uint64_t mask, smp_call_func_t func, void *data)
{
	uint16_t pcpu_id;
	struct smp_call_info_data *smp_call;

	/* wait for previous smp call complete, which may run on other cpus */
	while (atomic_cmpxchg64(&smp_call_mask, 0UL, mask) != 0UL);

	pcpu_id = ffs64(mask);
	while (pcpu_id < MAX_PCPU_NUM) {
		bitmap_clear_nolock(pcpu_id, &mask);
		if (pcpu_id == get_pcpu_id()) {
			func(data);
			bitmap_clear_nolock(pcpu_id, &smp_call_mask);
		} else if (is_pcpu_active(pcpu_id)) {
			smp_call = &per_cpu(smp_call_info, pcpu_id);

			smp_call->func = func;
			smp_call->data = data;

			/**
			 * arch_smp_call_kick_pcpu() is abstracted because:
			 *  - On x86, special handling is required when LAPIC is passthrough.
			 *  - On RISC-V, a plain IPI is sufficient to kick the target pCPU.
			 */
			arch_smp_call_kick_pcpu(pcpu_id);
		} else {
			/* pcpu is not in active, print error */
			pr_err("pcpu_id %d not in active!", pcpu_id);
			bitmap_clear_nolock(pcpu_id, &smp_call_mask);
		}
		pcpu_id = ffs64(mask);
	}
	/* wait for current smp call complete */
	wait_sync_change(&smp_call_mask, 0UL);
}

/**
 * Initialize the SMP call support during pCPU initialization
 */
void init_smp_call(void)
{
	/**
	 * arch_init_smp_call() is abstracted because:
	 *  - On x86, during CPU initialization, software reserves dedicated vectors and registers callback handlers
	 *    for purposes such as notifications or posted interrupts.
	 *  - On RISC-V, no special handling is required at present; this can be extended in the future if needed.
	 */
	arch_init_smp_call();
}
