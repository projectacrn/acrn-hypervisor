/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/wait.h>
#include "cmdutils.h"
#include "strutils.h"
#include "log_sys.h"

/**
 * Execute a command described in an array of pointers to null-terminated
 * strings, and redirect the output to specified file. The file will be
 * created/truncated if it exists/non-exists. This function will be blocked
 * until the child process exits.
 *
 * @param argv An array of pointers to null-terminated strings that
 *		represent the argument list available to the new program.
 *		The array of pointers must be terminated by a null pointer.
 * @param outfile File to record command's output, NULL means that this
 *		  function doesn't handle command's output.
 *
 * @return If a child process could not be created, or its status could not be
 *	   retrieved, or it was killed/stopped by signal, the return value
 *	   is -1.
 *	   If all system calls succeed, then the return value is the
 *	   termination status of the child process used to execute command.
 */
int execv_out2file(char *argv[], char *outfile)
{
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		LOGE("fork error (%s)\n", strerror(errno));
		return -1;
	}

	if (pid == 0) {
		int res;
		int fd;

		if (outfile) {
			fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0664);
			if (fd < 0) {
				LOGE("open error (%s)\n", strerror(errno));
				return -1;
			}
			res = dup2(fd, STDOUT_FILENO);
			if (res < 0) {
				close(fd);
				LOGE("dup2 error (%s)\n", strerror(errno));
				return -1;
			}
		}

		res = execvp(argv[0], argv);
		if (res == -1) {
			LOGE("execvp (%s) failed, error (%s)\n", argv[0],
			     strerror(errno));
			return -1;
		}
	} else {
		pid_t res;
		int status;
		int exit_status;

		res = waitpid(pid, &status, 0);
		if (res == -1) {
			LOGE("waitpid failed, error (%s)\n", strerror(errno));
			return res;
		}

		if (WIFEXITED(status)) {
			exit_status = WEXITSTATUS(status);
			if (!exit_status)
				LOGI("%s exited, status=0\n", argv[0]);
			else
				LOGE("%s exited, status=%d\n", argv[0],
				     exit_status);
			return exit_status;
		} else if (WIFSIGNALED(status)) {
			LOGE("%s killed by signal %d\n", argv[0],
			     WTERMSIG(status));
		} else if (WIFSTOPPED(status)) {
			LOGE("%s stopped by signal %d\n", argv[0],
			     WSTOPSIG(status));
		}

	}

	return -1;
}

int debugfs_cmd(char *loop_dev, char *cmd, char *outfile)
{
	char *argv[5] = {"debugfs", "-R", NULL, NULL, 0};

	argv[2] = cmd;
	argv[3] = loop_dev;
	return  execv_out2file(argv, outfile);
}

/**
 * Execute a command described in format string, and redirect the output
 * to specified file. The file will be created/truncated if it
 * exists/non-exists. This function will be blocked until the child process
 * exits.
 *
 * @param outfile File to record command's output, NULL means that this
 *		  function doesn't handle command's output.
 * @param fmt Format string of command.
 *
 * @return If a child process could not be created, or its status could not be
 *	   retrieved, or it was killed/stopped by signal, the return value
 *	   is -1.
 *	   If all system calls succeed, then the return value is the
 *	   termination status of the child process used to execute command.
 */
int exec_out2file(char *outfile, char *fmt, ...)
{
	va_list args;
	char *cmd;
	char *start;
	char *p;
	int ret;
	int argc;
	char **argv;
	int i = 0;

	va_start(args, fmt);
	ret = vasprintf(&cmd, fmt, args);
	va_end(args);
	if (ret < 0)
		return ret;

	strtrim(cmd);
	argc = strcnt(cmd, ' ') + 1;

	argv = (char **)calloc(argc + 1, sizeof(char *));
	if (argv == NULL) {
		free(cmd);
		LOGE("calloc failed, error (%s)\n", strerror(errno));
		return -1;
	}

	/* string to argv[] */
	start = cmd;
	argv[i++] = start;
	while (start && (p = strchr(start, ' '))) {
		argv[i++] = p + 1;
		*p = 0;
		if (*(p + 1) != '"')
			start = p + 1;
		else
			start = strchr(p + 2, '"');
	}

	ret = execv_out2file(argv, outfile);

	free(argv);
	free(cmd);

	return ret;
}

/**
 * Execute a command described in format string, and redirect the output
 * to memory. The memory is allocated by this function and needs to be freed
 * after return.
 *
 * @param fmt Format string of command.
 *
 * @return a pointer to command's output if successful, or NULL if not.
 */
char *exec_out2mem(char *fmt, ...)
{
	va_list args;
	char *cmd;
	FILE *pp;
	char *out = NULL;
	char *new;
	char tmp[1024];
	int memsize = 0;
	int newlen = 0;
	int len = 0;
	int ret;

	va_start(args, fmt);
	ret = vasprintf(&cmd, fmt, args);
	va_end(args);
	if (ret < 0)
		return NULL;

	pp = popen(cmd, "r");
	if (!pp)
		goto free_cmd;

	while (fgets(tmp, 1024, pp) != NULL) {
		newlen += strlen(tmp);
		if (newlen + 1 > memsize) {
			memsize += 1024;
			new = realloc(out, memsize);
			if (!new) {
				if (out)
					free(out);
				goto end;
			} else {
				out = new;
			}
		}
		memcpy(out + len, tmp, strlen(tmp) + 1);

		len = newlen;
	}

end:
	pclose(pp);
free_cmd:
	free(cmd);

	return out;
}
