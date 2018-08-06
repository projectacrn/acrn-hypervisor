/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#ifdef HV_DEBUG
extern struct hv_timer console_timer;

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

int console_write(const char *s, size_t len);

/** Writes a single character to the console.
 *
 *  @param ch The character to write.
 *
 *  @preturn The number of characters written or -1 if an error
 *           occurred before any character was written.
 */

void console_putc(const char *ch);
char console_getc(void);
int console_gets(char *buffer, uint32_t length);

void console_setup_timer(void);

static inline void suspend_console(void)
{
	del_timer(&console_timer);
}

static inline void resume_console(void)
{
	console_setup_timer();
}
void uart16550_set_property(bool enabled, bool port_mapped, uint64_t base_addr);

#else
static inline void console_init(void)
{
}

static inline int console_write(__unused const char *str,
			__unused size_t len)
{
	return 0;
}
static inline void console_putc(__unused const char *ch) { }
static inline int console_getc(void) { return 0; }
static inline int console_gets(char *buffer, uint32_t length) { return 0; }
static inline void console_setup_timer(void) {}
static inline void suspend_console(void) {}
static inline void resume_console(void) {}
static inline void uart16550_set_property(__unused bool enabled,
		__unused bool port_mapped, __unused uint64_t base_addr) {}
#endif

#endif /* CONSOLE_H */
