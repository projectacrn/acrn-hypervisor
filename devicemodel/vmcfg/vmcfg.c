/*
 * Copyright (C)2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vmcfg_config.h>
#include <vmcfg.h>
#include <stdio.h>

void vmcfg_list(void)
{
	int i;
	char *name;

	for (i = 0; i < num_args_buildin; i++) {
		name = args_buildin[i]->argv[args_buildin[i]->argc - 1];
		printf("%d: %s\n", i + 1, name);
	}
}

static struct vmcfg_arg *vmcfg_buildin_args[] = {
#ifdef CONFIG_MRB_VM1
	&mrb_vm1_args,
#endif /*CONFIG_MRB_VM1*/
};

struct vmcfg_arg **args_buildin = vmcfg_buildin_args;
int num_args_buildin = sizeof(vmcfg_buildin_args) / sizeof(struct vmcfg_arg *);
