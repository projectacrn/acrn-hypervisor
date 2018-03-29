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

#ifndef SPRINTF_H
#define SPRINTF_H

/* Command for the emit function: copy string to output. */
#define PRINT_CMD_COPY			0x00000000

/* Command for the emit function: fill output with first character. */
#define PRINT_CMD_FILL			0x00000001

/* Structure used to parse parameters and variables to subroutines. */
struct print_param {
	/* A pointer to the function that is used to emit characters. */
	int (*emit)(int, const char *, int, void *);
	/* An opaque pointer that is passed as third argument to the emit
	 * function.
	 */
	void *data;
	/* Contains variables which are recalculated for each argument. */
	struct {
		/* A bitfield with the parsed format flags. */
		int flags;
		/* The parsed format width. */
		int width;
		/* The parsed format precision. */
		int precision;
		/* The bitmask for unsigned values. */
		unsigned long long mask;
		/* A pointer to the preformated value. */
		const char *value;
		/* The number of characters in the preformated value buffer. */
		uint32_t valuelen;
		/* A pointer to the values prefix. */
		const char *prefix;
		/* The number of characters in the prefix buffer. */
		uint32_t prefixlen;
	} vars;
};

int do_print(const char *fmt, struct print_param *param,
		__builtin_va_list args);

/**  The well known vsnprintf() function.
 *
 *  Formats and writes a string with a max. size to memory.
 *
 * @param dst A pointer to the destination memory.
 * @param sz The size of the destination memory.
 * @param fmt A pointer to the NUL terminated format string.
 * @param args The variable long argument list as va_list.
 * @return The number of bytes which would be written, even if the destination
 *         is smaller. On error a negative number is returned.
 */

int vsnprintf(char *dst, int sz, const char *fmt, va_list args);

/** The well known snprintf() function.
 *
 *  Formats a string and writes it to the console output.
 *
 *  @param dest Pointer to the destination memory.
 *  @param sz   Max. size of dest.
 *  @param fmt  A pointer to the NUL terminated format string.
 *
 *  @return The number of characters would by written or a negative
 *          number if an error occurred.
 *
 *  @bug    sz == 0 doesn't work
 */

int snprintf(char *dest, int sz, const char *fmt, ...);

#endif /* SPRINTF_H */
