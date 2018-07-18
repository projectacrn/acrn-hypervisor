/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
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
		uint32_t flags;
		/* The parsed format width. */
		int width;
		/* The parsed format precision. */
		int precision;
		/* The bitmask for unsigned values. */
		uint64_t mask;
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
