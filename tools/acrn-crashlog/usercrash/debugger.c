/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "crash_dump.h"
#include "log_sys.h"
#include "version.h"

/**
 * Debugger can work without server when uses "debugger pid" commands to
 * debug the running process. This function will dump the process info on the
 * screen, also you can relocate the info to a file.
 */
static void print_usage(void)
{
	printf("debugger - tool to dump process info of a running process.\n");
	printf("[Usage]\n");
	printf("\t--shell cmd, debugger <pid>  (root role to run)\n");
	printf("[Option]\n");
	printf("\t-h: print this usage message\n");
	printf("\t-v: print debugger version\n");
}

int main(int argc, char *argv[])
{
	int pid;

	if (argc > 1) {
		if (strcmp(argv[1], "-v") == 0) {
			printf("version is %d.%d-%s, build by %s@%s\n",
				UC_MAJOR_VERSION, UC_MINOR_VERSION,
				UC_BUILD_VERSION, UC_BUILD_USER,
				UC_BUILD_TIME);
			return 0;
		}
		if (strcmp(argv[1], "-h") == 0) {
			print_usage();
			return 0;
		}
	} else
		print_usage();

	if (getuid() != 0) {
		printf("failed to execute debugger, root is required\n");
		exit(EXIT_FAILURE);
	}

	if (argc == 2) {
		/* it's from shell cmd */
		pid = (int)strtol(argv[1], NULL, 10);
		crash_dump(pid, 0, STDOUT_FILENO);
	} else {
		print_usage();
		return 0;
	}

	return 0;
}
