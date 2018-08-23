/*
 * Copyright (C)2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VMCFG_H
#define VMCFG_H

struct vmcfg_arg {
	char **argv;
	int argc;
	int (*setup)(void);
	int (*clean)(void);
};

extern struct vmcfg_arg **args_buildin;
extern int num_args_buildin;

extern struct vmcfg_arg mrb_vm1_args;

void vmcfg_list(void);
#endif
