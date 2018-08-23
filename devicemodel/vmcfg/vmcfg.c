/*
 * Copyright (C)2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vmcfg_config.h>
#include <vmcfg.h>

static struct vmcfg_arg *vmcfg_buildin_args[] = {
#ifdef CONFIG_MRB_VM1
	&mrb_vm1_args,
#endif /*CONFIG_MRB_VM1*/
};

struct vmcfg_arg **args_buildin = vmcfg_buildin_args;
int num_args_buildin = sizeof(vmcfg_buildin_args) / sizeof(struct vmcfg_arg *);
