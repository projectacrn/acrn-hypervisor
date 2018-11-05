/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SPRINTF_H
#define SPRINTF_H

/* Command for the emit function: copy string to output. */
#define PRINT_CMD_COPY			0x00000000U

/* Command for the emit function: fill output with first character. */
#define PRINT_CMD_FILL			0x00000001U

/** Structure used to call back emit lived in print_param */
struct snprint_param {
	/** The destination buffer. */
	char *dst;
	/** The size of the destination buffer. */
	uint32_t sz;
	/** Counter for written chars. */
	uint32_t wrtn;
};

/* Structure used to parse parameters and variables to subroutines. */
struct print_param {
	/* A pointer to the function that is used to emit characters. */
	void (*emit)(size_t, const char *, uint32_t, struct snprint_param *);
	/* An opaque pointer that is passed as forth argument to the emit
	 * function.
	 */
        struct snprint_param *data;
	/* Contains variables which are recalculated for each argument. */
	struct {
		/* A bitfield with the parsed format flags. */
		uint32_t flags;
		/* The parsed format width. */
		uint32_t width;
		/* The parsed format precision. */
		uint32_t precision;
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

void do_print(const char *fmt_arg, struct print_param *param,
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
size_t vsnprintf(char *dst_arg, size_t sz_arg, const char *fmt, va_list args);

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

size_t snprintf(char *dest, size_t sz, const char *fmt, ...);

#endif /* SPRINTF_H */
