/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_H_
#define VM_H_

struct acrn_vm {

};

/* FIXME: riscv dummy function */
static inline bool need_shutdown_vm(uint16_t pcpu_id)
{
	(void)pcpu_id;
	return false;
}
#endif /* VM_H_ */
