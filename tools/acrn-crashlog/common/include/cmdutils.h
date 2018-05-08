/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

int execv_out2file(char *argv[], char *outfile);
int debugfs_cmd(char *loop_dev, char *cmd, char *outfile);
int exec_out2file(char *outfile, char *fmt, ...);
char *exec_out2mem(char *fmt, ...);
