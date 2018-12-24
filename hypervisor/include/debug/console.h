/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CONSOLE_H
#define CONSOLE_H

/* Switching key combinations for shell and uart console */
#define GUEST_CONSOLE_TO_HV_SWITCH_KEY      0       /* CTRL + SPACE */

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
void uart16550_set_property(bool enabled, bool port_mapped, uint64_t base_addr);
bool is_pci_dbg_uart(union pci_bdf bdf_value);
bool is_dbg_uart_enabled(void);

void shell_init(void);
void shell_kick(void);

void suspend_console(void);
void resume_console(void);

#endif /* CONSOLE_H */
