/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RISCV_VM_RESET_H_
#define RISCV_VM_RESET_H_

/* FIXME: riscv dummy function */
static inline void shutdown_vm_from_idle(uint16_t pcpu_id)
{
	(void)pcpu_id;
}

#endif /* RISCV_VM_RESET_H_ */
