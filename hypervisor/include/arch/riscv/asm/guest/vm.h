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
/* FIXME: dummy. to be implemented later */
#define EMUL_PIO_IDX_MAX 1U

struct vm_arch {
};

#endif /* RISCV_VM_H_ */
