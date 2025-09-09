/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef RISCV_CPU_H
#define RISCV_CPU_H

#include <types.h>
#include <lib/util.h>
#include <debug/logmsg.h>
#include <board_info.h>

#define barrier()	__asm__ __volatile__("fence": : :"memory")
#define cpu_relax()	barrier() /* TODO: replace with yield instruction */
#define NR_CPUS		MAX_PCPU_NUM

static inline uint16_t get_pcpu_id(void)
{
	/**
	 * Dummy implementation.
	 * Official implementations are to be provided in the platform initialization patchset (by Hang).
	 */
	return 0U;
}

/* Write CSR */
#define cpu_csr_write(reg, csr_val)					\
({									\
	uint64_t val = (uint64_t)csr_val;				\
	asm volatile (" csrw " STRINGIFY(reg) ", %0 \n\t"		\
			:: "r"(val): "memory");	 			\
})

void wait_sync_change(volatile const uint64_t *sync, uint64_t wake_sync);

#endif /* RISCV_CPU_H */
