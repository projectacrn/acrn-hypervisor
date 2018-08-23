/*
 * Copyright (C)2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vmcfg_config.h>
#include <vmcfg.h>

static struct vmcfg_arg *vmcfg_buildin_args[] = {
};

struct vmcfg_arg **args_buildin = vmcfg_buildin_args;
int num_args_buildin = sizeof(vmcfg_buildin_args) / sizeof(struct vmcfg_arg *);
