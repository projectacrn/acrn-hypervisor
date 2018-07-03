/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

int execv_out2file(char * const argv[], const char *outfile);
int debugfs_cmd(const char *loop_dev, const char *cmd, const char *outfile);
int exec_out2file(const char *outfile, const char *fmt, ...);
char *exec_out2mem(const char *fmt, ...);
