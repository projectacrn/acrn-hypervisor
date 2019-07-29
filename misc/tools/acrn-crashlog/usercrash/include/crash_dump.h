/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CRASH_DUMP_H
#define CRASH_DUMP_H

/**
 * So far, crash_dump use gdb command to dump process info. But in the future,
 * we will replace gdb command with a better implementation.
 */
#define GET_GDB_INFO "/usr/bin/gdb %s -batch "\
			"-ex 'bt' "\
			"-ex 'printf \"\nRegisters\n\"' "\
			"-ex 'info registers' "\
			"-ex 'printf \"\n\nMemory near rax\n\"' "\
			"-ex 'x/16x $rax-0x20' "\
			"-ex 'x/48x $rax' "\
			"-ex 'printf \"\n\nMemory near rbx\n\"' "\
			"-ex 'x/16x $rbx-0x20' "\
			"-ex 'x/48x $rbx' "\
			"-ex 'printf \"\n\nMemory near rcx\n\"' "\
			"-ex 'x/16x $rcx-0x20' "\
			"-ex 'x/48x $rcx' "\
			"-ex 'printf \"\n\nMemory near rdx\n\"' "\
			"-ex 'x/16x $rdx-0x20' "\
			"-ex 'x/48x $rdx' "\
			"-ex 'printf \"\n\nMemory near rsi\n\"' "\
			"-ex 'x/16x $rsi-0x20' "\
			"-ex 'x/48x $rsi' "\
			"-ex 'printf \"\n\nMemory near rdi\n\"' "\
			"-ex 'x/16x $rdi-0x20' "\
			"-ex 'x/48x $rdi' "\
			"-ex 'printf \"\n\nMemory near rbp\n\"' "\
			"-ex 'x/16x $rbp-0x20' "\
			"-ex 'x/48x $rbp' "\
			"-ex 'printf \"\n\nMemory near rsp\n\"' "\
			"-ex 'x/16x $rsp-0x20' "\
			"-ex 'x/48x $rsp' "\
			"-ex 'printf \"\n\ncode around rip\n\"' "\
			"-ex 'x/8i $rip-0x20' "\
			"-ex 'x/48i $rip' "\
			"-ex 'printf \"\nThreads\n\n\"' "\
			"-ex 'info threads' "\
			"-ex 'printf \"\nThreads backtrace\n\n\"' "\
			"-ex 'thread apply all bt' "\
			"-ex 'quit'"

void crash_dump(int pid, int sig, int out_fd);

#endif
