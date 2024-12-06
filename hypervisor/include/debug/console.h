/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#include <vuart.h>
#include <asm/guest/vcpu.h>

/** Initializes the console module.
 *
 */
void console_init(void);

/** Writes a given number of characters to the console.
 *
 *  @param s A pointer to character array to write.
 *  @param len The number of characters to write.
 *
 *  @return The number of characters written or -1 if an error occurred
 *          and no character was written.
 */
size_t console_write(const char *s, size_t len);

/** Writes a single character to the console.
 *
 *  @param ch The character to write.
 *
 *  @preturn The number of characters written or -1 if an error
 *           occurred before any character was written.
 */
void console_putc(const char *ch);
char console_getc(void);

void console_setup_timer(void);
void console_vmexit_callback(struct acrn_vcpu *vcpu);

void suspend_console(void);
void resume_console(void);
struct acrn_vuart *vm_console_vuart(struct acrn_vm *vm);
bool is_using_init_ipi(void);

bool console_need_log(uint32_t severity);
void console_log(char *buffer);

#endif /* CONSOLE_H */
