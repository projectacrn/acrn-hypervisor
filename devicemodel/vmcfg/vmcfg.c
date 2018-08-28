/*
 * Copyright (C)2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vmcfg_config.h>
#include <vmcfg.h>
#include <stdio.h>
#include <getopt.h>

void vmcfg_list(void)
{
	int i;
	char *name;

	for (i = 0; i < num_args_buildin; i++) {
		name = args_buildin[i]->argv[args_buildin[i]->argc - 1];
		printf("%d: %s\n", i + 1, name);
	}
}

void vmcfg_dump(int index, struct option *long_options, char *optstr)
{
	char **argv;
	int argc;
	int c, option_idx = 0;
	int i = 1;

	if ((index <= 0) || (index > num_args_buildin)) {
		fprintf(stderr, "%s(%d) index should be 1~%d\n", __FUNCTION__,
				index, num_args_buildin);
		return;
	}

	if (args_buildin[index - 1]->setup)
                args_buildin[index - 1]->setup();

	argv = args_buildin[index - 1]->argv;
	argc = args_buildin[index - 1]->argc;

	printf("%s build-in args:\n", argv[argc - 1]);

	optind = 0;

	while ((c = getopt_long(argc, argv, optstr, long_options,
		&option_idx)) != -1) {
		if (optarg)
			printf("%s %s\n", argv[i], optarg);
		else
			printf("%s\n", argv[i]);
		i = optind;
	}
}

static struct vmcfg_arg *vmcfg_buildin_args[] = {
#ifdef CONFIG_MRB_VM1
	&mrb_vm1_args,
#endif /*CONFIG_MRB_VM1*/
};

struct vmcfg_arg **args_buildin = vmcfg_buildin_args;
int num_args_buildin = sizeof(vmcfg_buildin_args) / sizeof(struct vmcfg_arg *);
