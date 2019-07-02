/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SECURITY_H
#define SECURITY_H

/* type of speculation control
 * 0 - no speculation control support
 * 1 - raw IBRS + IBPB support
 * 2 - with STIBP optimization support
 */
#define IBRS_NONE	0
#define IBRS_RAW	1
#define IBRS_OPT	2

#ifndef ASSEMBLER
int32_t get_ibrs_type(void);
void cpu_l1d_flush(void);
bool check_cpu_security_cap(void);
void cpu_internal_buffers_clear(void);

#ifdef STACK_PROTECTOR
struct stack_canary {
	/* Gcc generates extra code, using [fs:40] to access canary */
	uint8_t reserved[40];
	uint64_t canary;
};
void __stack_chk_fail(void);
void set_fs_base(void);
#endif

#endif /* ASSEMBLER */

#endif /* SECURITY_H */
