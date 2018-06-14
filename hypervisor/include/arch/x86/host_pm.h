/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef	HOST_PM_H

#define	BIT_WAK_STS	15U

int enter_s3(struct vm *vm, uint32_t pm1a_cnt_val,
		uint32_t pm1b_cnt_val);
extern void __enter_s3(struct vm *vm, uint32_t pm1a_cnt_val,
		uint32_t pm1b_cnt_val);
extern void restore_s3_context(void);

#endif	/* ARCH_X86_PM_H */
