/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include "log_sys.h"
#include "crash_dump.h"
#include "cmdutils.h"
#include "fsutils.h"
#include "strutils.h"

#define GET_TID "/proc/%d/status"
#define GET_COMM "/proc/%d/exe"
#define GET_K_STACK "/proc/%d/stack"
#define GET_MAPS "/proc/%d/maps"
#define GET_OPEN_FILES "/proc/%d/fd"
#define DEBUGGER_SIGNAL (__SIGRTMIN + 3)
#define DUMP_FILE "/tmp/core"
#define BUFFER_SIZE 8196
#define LINK_LEN 512

static void loginfo(int fd, const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	size_t len;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	len = strlen(buf);
	if (len <= 0)
		return;

	write(fd, buf, len);
}

static const char *get_signame(int sig)
{
	switch (sig) {
	case SIGABRT:
		return "SIGABRT";
	case SIGBUS:
		return "SIGBUS";
	case SIGFPE:
		return "SIGFPE";
	case SIGILL:
		return "SIGILL";
	case SIGSEGV:
		return "SIGSEGV";
#if defined(SIGSTKFLT)
	case SIGSTKFLT:
		return "SIGSTKFLT";
#endif
	case SIGSTOP:
		return "SIGSTOP";
	case SIGSYS:
		return "SIGSYS";
	case SIGTRAP:
		return "SIGTRAP";
	case DEBUGGER_SIGNAL:
		return "<debuggerd signal>";
	default:
		return "?";
	}
}

/**
 * Save core dump to file.
 * @filename: core dump file name
 * return 0 on success, or -1 on error
 */
static int save_coredump(const char *filename)
{
	size_t read;
	char input_buffer[BUFFER_SIZE];
	FILE *dump_file;

	/* open dump file in write and binary mode */
	dump_file = fopen(filename, "wb");
	if (dump_file != NULL) {
		/* Read from STDIN and write to dump file */
		while (1) {
			read = fread(input_buffer, 1, BUFFER_SIZE, stdin);
			if (read > 0) {
				if (fwrite(input_buffer, 1, read, dump_file) <=
							0) {
					LOGE("fwrite error\n");
					goto fail;
				}
				continue;
			} else if (read == 0) {
				break;
			}

			LOGE("fread error\n");
			goto fail;
		}
		fclose(dump_file);
		return 0;
	}

	LOGE("fopen core file failed\n");
	return -1;

fail:
	fclose(dump_file);
	return -1;
}

static int get_backtrace(int pid, int fd, int sig, char *comm)
{
	char *membkt;
	char format[512];

	loginfo(fd, "\nBackTrace:\n\n");
	memset(format, 0, sizeof(format));
	if (sig == DEBUGGER_SIGNAL) {
		snprintf(format, sizeof(format), "-p %d", pid);
	} else {
		snprintf(format, sizeof(format), "%s %s", comm, DUMP_FILE);
		if (save_coredump(DUMP_FILE) == -1) {
			LOGE("save core file failed\n");
			return -1;
		}
	}
	membkt = exec_out2mem(GET_GDB_INFO, format);
	if (!membkt) {
		LOGE("get gdb info failed\n");
		return -1;
	}
	write(fd, membkt, strlen(membkt));
	free(membkt);
	return 0;

}

/**
 * Save process proc info to file.
 * @pid: process pid
 * @fd: usercrash file fd
 * @path: process proc path
 * @name: a string save to file
 * return 0 on success, or -1 on error
 */
static int save_proc_info(int pid, int fd, char *path, char *name)
{
	int ret;
	char *data;
	char format[128];
	unsigned long size;

	loginfo(fd, "\n%s:\n\n", name);
	memset(format, 0, sizeof(format));
	snprintf(format, sizeof(format), path, pid);
	ret = read_file(format, &size, (void *)&data);
	if (ret) {
		LOGE("read file failed\n");
		return -1;
	}
	write(fd, data, size);
	free(data);
	return 0;

}

static int get_openfiles(int pid, int fd, char *path, char *name)
{
	int ret;
	int loop;
	int fdcount;
	char *fdname;
	char format[128];
	char *files[256];
	char linkname[LINK_LEN];

	loginfo(fd, "\n%s:\n\n", name);
	memset(format, 0, sizeof(format));
	snprintf(format, sizeof(format), path, pid);
	fdcount = lsdir(format, files, ARRAY_SIZE(files));
	if (fdcount < 0) {
		LOGE("get fd list failed\n");
		return -1;
	}
	for (loop = 2; loop < fdcount; loop++) {
		memset(linkname, 0, LINK_LEN);
		ret = readlink(files[loop], linkname, LINK_LEN);
		if (ret < 0 || ret >= LINK_LEN) {
			LOGE("get fd link fd failed\n");
			continue;
		}
		fdname = strrchr(files[loop], '/') + 1;
		loginfo(fd, "%s -> %s\n", fdname, linkname);
	}
	for (loop = 0; loop < fdcount; loop++)
		free(files[loop]);

	return 0;
}

static int save_usercrash_file(int pid, int tgid, char *comm,
		int sig, int out_fd)
{
	int ret;

	loginfo(out_fd, "*** *** *** *** *** *** *** *** *** *** *** *** *** ");
	loginfo(out_fd, "*** *** ***\n");
	loginfo(out_fd, "pid: %d, tgid: %d, comm: %s\n\n\n", pid, tgid, comm);
	loginfo(out_fd, "signal: %d (%s)\n", sig, get_signame(sig));

	ret = save_proc_info(pid, out_fd, GET_K_STACK, "Stack");
	if (ret) {
		LOGE("save stack failed\n");
		return -1;
	}

	ret = save_proc_info(pid, out_fd, GET_MAPS, "Maps");
	if (ret) {
		LOGE("save maps failed\n");
		return -1;
	}

	ret = get_openfiles(pid, out_fd, GET_OPEN_FILES, "Open files");
	if (ret) {
		LOGE("save openfiles failed\n");
		return -1;
	}

	ret = get_backtrace(pid, out_fd, sig, comm);
	if (ret) {
		LOGE("save backtrace failed\n");
		return -1;
	}

	return 0;
}

static int get_key_value(int pid, char *path, char *key, char *value)
{
	int len;
	int ret;
	unsigned long size;
	char *data;
	char *msg;
	char *start;
	char *end;
	char format[128];

	memset(format, 0, sizeof(format));
	snprintf(format, sizeof(format), path, pid);
	ret = read_file(format, &size, (void *)&data);
	if (ret) {
		LOGE("read file failed\n");
		return -1;
	}
	msg = strstr(data, key);
	if (!msg) {
		LOGE("find key:%s failed\n", key);
		free(data);
		return -1;
	}
	end = strchr(msg, '\n');
	if (end == NULL)
		end = data + size;
	start = msg + strlen(key);
	len = end - start;
	memcpy(value, start, len);
	*(value + len) = 0;
	free(data);

	return 0;
}

/**
 * Get and save process info to out_fd.
 * @pid: process pid
 * @sig: signal from core dump
 * @out_fd: file fd to save info
 */
void crash_dump(int pid, int sig, int out_fd)
{
	int tgid;
	int ret;
	char comm[LINK_LEN];
	char result[16];
	char format[128];

	memset(format, 0, sizeof(format));
	snprintf(format, sizeof(format), GET_COMM, pid);
	ret = readlink(format, comm, LINK_LEN);
	if (ret < 0 || ret >= LINK_LEN) {
		LOGE("get process exe link failed\n");
		return;
	}
	comm[ret] = '\0';

	ret = get_key_value(pid, GET_TID, "Tgid:", result);
	if (ret) {
		LOGE("get Tgid error\n");
		return;
	}
	tgid = atoi(result);
	if (!sig)
		sig = DEBUGGER_SIGNAL;
	if (save_usercrash_file(pid, tgid, comm, sig, out_fd))
		LOGE("dump log file failed\n");
}
