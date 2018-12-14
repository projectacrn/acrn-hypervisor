/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef	HOST_PM_H
#define	HOST_PM_H

#define	BIT_SLP_TYPx	10U
#define	BIT_SLP_EN	13U
#define	BIT_WAK_STS	15U

void set_host_wake_vectors(void *vector_32, void *vector_64);
struct pm_s_state_data *get_host_sstate_data(void);

void host_enter_s3(struct pm_s_state_data *sstate_data, uint32_t pm1a_cnt_val, uint32_t pm1b_cnt_val);
extern void asm_enter_s3(struct pm_s_state_data *sstate_data, uint32_t pm1a_cnt_val, uint32_t pm1b_cnt_val);
extern void restore_s3_context(void);

#endif	/* HOST_PM_H */
