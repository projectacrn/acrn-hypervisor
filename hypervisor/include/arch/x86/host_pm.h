/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef	HOST_PM_H
#define	HOST_PM_H

#define	BIT_SLP_TYPx	10U
#define	BIT_SLP_EN	13U
#define	BIT_WAK_STS	15U

extern struct pm_s_state_data host_pm_s_state;

extern uint8_t host_enter_s3_success;

void enter_s3(struct acrn_vm *vm, uint32_t pm1a_cnt_val, uint32_t pm1b_cnt_val);
extern void asm_enter_s3(struct acrn_vm *vm, uint32_t pm1a_cnt_val,
		uint32_t pm1b_cnt_val);
extern void restore_s3_context(void);

#endif	/* HOST_PM_H */
