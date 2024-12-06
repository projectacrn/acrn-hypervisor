/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <pci.h>
#include <console.h>

size_t console_write(__unused const char *str, __unused size_t len)
{
	return 0;
}

char console_getc(void)
{
	return '\0';
}

void console_putc(__unused const char *ch) {}

void console_init(void) {}
void console_setup_timer(void) {}

void suspend_console(void) {}
void resume_console(void) {}

bool handle_dbg_cmd(__unused const char *cmd, __unused int32_t len) { return false; }
void console_vmexit_callback(__unused struct acrn_vcpu *vcpu) {}

bool is_using_init_ipi(void) { return false; }

void shell_init(void) {}
void shell_kick(void) {}

bool console_need_log(__unused uint32_t severity) { return false; }
void console_log(__unused char *buffer) {}
