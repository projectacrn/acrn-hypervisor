/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RISCV_VM_H_
#define RISCV_VM_H_


#include <vm_configurations.h>
#include <vuart.h>

#define INVALID_PIO_IDX	-1U
#define UART_PIO_IDX0	INVALID_PIO_IDX

struct vm_arch {

};

/* FIXME: riscv dummy function */
static inline bool need_shutdown_vm(uint16_t pcpu_id)
{
	(void)pcpu_id;
	return false;
}

#endif /* RISCV_VM_H_ */
