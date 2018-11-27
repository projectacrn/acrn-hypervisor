/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

void init_logmsg(__unused uint32_t flags) {}
void do_logmsg(__unused uint32_t severity, __unused const char *fmt, ...) {}
void print_logmsg_buffer(__unused uint16_t pcpu_id) {}
void printf(__unused const char *fmt, ...) {}
void vprintf(__unused const char *fmt, __unused va_list args) {}
