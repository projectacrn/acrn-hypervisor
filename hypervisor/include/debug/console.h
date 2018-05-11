/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#ifdef HV_DEBUG
/** Initializes the console module.
 *
 *  @param cdev A pointer to the character device to use for the console.
 *
 *  @return '0' on success. Any other value indicates an error.
 */

int console_init(void);

/** Writes a NUL terminated string to the console.
 *
 *  @param str A pointer to the NUL terminated string to write.
 *
 *  @return The number of characters written or -1 if an error occurred
 *          and no character was written.
 */

int console_puts(const char *str);

/** Writes a given number of characters to the console.
 *
 *  @param str A pointer to character array to write.
 *  @param len The number of characters to write.
 *
 *  @return The number of characters written or -1 if an error occurred
 *          and no character was written.
 */

int console_write(const char *str, size_t len);

/** Writes a single character to the console.
 *
 *  @param ch The character to write.
 *
 *  @preturn The number of characters written or -1 if an error
 *           occurred before any character was written.
 */

int console_putc(int ch);

/** Dumps an array to the console.
 *
 *  This function dumps an array of bytes to the console
 *  in a hexadecimal format.
 *
 *  @param p A pointer to the byte array to dump.
 *  @param len The number of bytes to dump.
 */

void console_dump_bytes(const void *p, unsigned int len);

void console_setup_timer(void);

uint32_t get_serial_handle(void);
#else
static inline int console_init(void)
{
	return 0;
}
static inline int console_puts(__unused const char *str)
{
	return 0;
}
static inline int console_write(__unused const char *str,
			__unused size_t len)
{
	return 0;
}
static inline int console_putc(__unused int ch)
{
	return 0;
}
static inline void console_dump_bytes(__unused const void *p,
			__unused unsigned int len)
{
}
static inline void console_setup_timer(void) {}
static inline uint32_t get_serial_handle(void) { return 0; }
#endif

#endif /* CONSOLE_H */
