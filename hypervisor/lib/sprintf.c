/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#ifndef NULL
#define NULL ((void *) 0)
#endif

#define PRINT_STRING_MAX_LEN		4096U

#define HEX_DIGITS_LEN			17U

/** Use upper case letters for hexadecimal format. */
#define PRINT_FLAG_UPPER		0x00000001U

/** Use alternate form. */
#define PRINT_FLAG_ALTERNATE_FORM 	0x00000002U

/** Use '0' instead of ' ' for padding. */
#define PRINT_FLAG_PAD_ZERO		0x00000004U

/** Use left instead of right justification. */
#define PRINT_FLAG_LEFT_JUSTIFY		0x00000008U

/** Always use the sign as prefix. */
#define PRINT_FLAG_SIGN			0x00000010U

/** Use ' ' as prefix if no sign is used. */
#define PRINT_FLAG_SPACE		0x00000020U

/** The original value was a (unsigned) char. */
#define PRINT_FLAG_CHAR			0x00000040U

/** The original value was a (unsigned) short. */
#define PRINT_FLAG_SHORT		0x00000080U

/** The original value was a (unsigned) long. 64bit on ACRN also */
#define PRINT_FLAG_LONG			0x00000200U

/** The original value was a (unsigned) long long. */
#define PRINT_FLAG_LONG_LONG		0x00000200U

/** The value is interpreted as unsigned. */
#define PRINT_FLAG_UINT32		0x00000400U

/** The characters to use for upper case hexadecimal conversion.
 *
 *  Note that this array is 17 bytes long. The first 16 characters
 *  are used to convert a 4 bit number to a printable character.
 *  The last character is used to determine the prefix for the
 *  alternate form.
 */

static const char upper_hex_digits[] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'A', 'B', 'C', 'D', 'E', 'F', 'X'
};

/** The characters to use for lower case hexadecimal conversion.
 *
 *  Note that this array is 17 bytes long. The first 16 characters
 *  are used to convert a 4 bit number to a printable character.
 *  The last character is used to determine the prefix for the
 *  alternate form.
 */

static const char lower_hex_digits[] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f', 'x'
};

static const char *get_param(const char *s_arg, uint32_t *x)
{
	const char *s = s_arg;
	*x = 0U;

	/* ignore '-' for negative numbers, it will be handled in flags*/
	if (*s == '-') {
		++s;
	}

	/* parse uint32_teger */
	while ((*s >= '0') && (*s <= '9')) {
		char delta = *s - '0';
		*x = ((*x) * 10U) + (uint32_t)delta;
		s++;
	}

	return s;
}

static const char *get_flags(const char *s_arg, uint32_t *flags)
{
	const char *s = s_arg;
	/* contains the flag characters */
	static const char flagchars[5] = "#0- +";
	/* contains the numeric flags for the characters above */
	static const uint32_t fl[sizeof(flagchars)] = {
		PRINT_FLAG_ALTERNATE_FORM,	/* # */
		PRINT_FLAG_PAD_ZERO,	/* 0 */
		PRINT_FLAG_LEFT_JUSTIFY,	/* - */
		PRINT_FLAG_SIGN,	/* + */
		PRINT_FLAG_SPACE	/* ' ' */
	};
	uint32_t i;
	bool found;

	/* parse multiple flags */
	while ((*s) != '\0') {
		/*
		 * Get index of flag.
		 * Terminate loop if no flag character was found.
		 */
		found = false;
		for (i = 0U; i < sizeof(flagchars); i++) {
			if (*s == flagchars[i]) {
				found = true;
				break;
			}
		}
		if (!found) {
			break;
		}

		/* apply matching flags and continue with the next character */
		++s;
		*flags |= fl[i];
	}

	/* Spec says that '-' has a higher priority than '0' */
	if ((*flags & PRINT_FLAG_LEFT_JUSTIFY) != 0U) {
		*flags &= ~PRINT_FLAG_PAD_ZERO;
	}

	/* Spec says that '+' has a higher priority than ' ' */
	if ((*flags & PRINT_FLAG_SIGN) != 0U) {
		*flags &= ~PRINT_FLAG_SPACE;
	}

	return s;
}

static const char *get_length_modifier(const char *s_arg,
			uint32_t *flags, uint64_t *mask)
{
	const char *s = s_arg;
	if (*s == 'h') {
		/* check for h[h] (char/short) */
		s++;
		if (*s == 'h') {
			*flags |= PRINT_FLAG_CHAR;
			*mask = 0x000000FFU;
			++s;
		} else {
			*flags |= PRINT_FLAG_SHORT;
			*mask = 0x0000FFFFU;
		}
	} else if (*s == 'l') {
		/* check for l[l] (long/long long) */
		s++;
		if (*s == 'l') {
			*flags |= PRINT_FLAG_LONG_LONG;
			++s;
		} else {
			*flags |= PRINT_FLAG_LONG;
		}
	} else {
		/* No length modifiers found. */
	}

	return s;
}

static void format_number(struct print_param *param)
{
	/* contains the character used for padding */
	char pad;
	/* effective width of the result */
	uint32_t width;
	/* number of characters to insert for width (w) and precision (p) */
	uint32_t p = 0U, w = 0U;

	/* initialize variables */
	width = param->vars.valuelen + param->vars.prefixlen;

	/* calculate additional characters for precision */
	if (param->vars.precision > width) {
		p = param->vars.precision - width;
	}

	/* calculate additional characters for width */
	if (param->vars.width > (width + p)) {
		w = param->vars.width - (width + p);
	}

	/* handle case of right justification */
	if ((param->vars.flags & PRINT_FLAG_LEFT_JUSTIFY) == 0U) {
		/* assume ' ' as padding character */
		pad = ' ';

		/*
		 * if padding with 0 is used, we have to emit the prefix (if any
		 * ) first to achieve the expected result. However, if a blank is
		 * used for padding, the prefix is emitted after the padding.
		 */

		if ((param->vars.flags & PRINT_FLAG_PAD_ZERO) != 0U) {
			/* use '0' for padding */
			pad = '0';

			/* emit prefix, return early if an error occurred */
			param->emit(PRINT_CMD_COPY, param->vars.prefix,
					param->vars.prefixlen, param->data);

			/* invalidate prefix */
			param->vars.prefix = NULL;
			param->vars.prefixlen = 0U;
		}

		/* fill the width with the padding character, return early if
		 * an error occurred
		 */
		param->emit(PRINT_CMD_FILL, &pad, w, param->data);
	}

	/* emit prefix (if any), return early in case of an error */
	param->emit(PRINT_CMD_COPY, param->vars.prefix,
			param->vars.prefixlen, param->data);

	/* insert additional 0's for precision, return early if an error
	 * occurred
	 */
	param->emit(PRINT_CMD_FILL, "0", p, param->data);

	/* emit the pre-calculated result, return early in case of an error */
	param->emit(PRINT_CMD_COPY, param->vars.value,
			param->vars.valuelen, param->data);

	/* handle left justification */
	if ((param->vars.flags & PRINT_FLAG_LEFT_JUSTIFY) != 0U) {
		/* emit trailing blanks, return early in case of an error */
		param->emit(PRINT_CMD_FILL, " ", w, param->data);
	}

}

static void print_pow2(struct print_param *param,
		uint64_t v_arg, uint32_t shift)
{
	uint64_t v = v_arg;
	/* max buffer required for octal representation of uint64_t long */
	char digitbuff[22];
	/* Insert position for the next character+1 */
	char *pos = digitbuff + sizeof(digitbuff);
	/* buffer for the 0/0x/0X prefix */
	char prefix[2];
	/* pointer to the digits translation table */
	const char (*digits)[HEX_DIGITS_LEN];
	/* mask to extract next character */
	uint64_t mask;

	/* calculate mask */
	mask = (1UL << shift) - 1UL;

	/* determine digit translation table */
	digits = ((param->vars.flags & PRINT_FLAG_UPPER) != 0U) ? &upper_hex_digits : &lower_hex_digits;

	/* apply mask for short/char */
	v &= param->vars.mask;

	/* determine prefix for alternate form */
	if ((v == 0UL) &&
		((param->vars.flags & PRINT_FLAG_ALTERNATE_FORM) != 0U)) {
		prefix[0] = '0';
		param->vars.prefix = prefix;
		param->vars.prefixlen = 1U;

		if (shift == 4U) {
			param->vars.prefixlen = 2U;
			prefix[1] = (*digits)[16];
		}
	}

	/* determine digits from right to left */
	do {
		pos--;
		*pos = (*digits)[(v & mask)];
		v >>= shift;
	} while (v != 0UL);

	/* assign parameter and apply width and precision */
	param->vars.value = pos;
	param->vars.valuelen = (digitbuff + sizeof(digitbuff)) - pos;

	format_number(param);

	param->vars.value = NULL;
	param->vars.valuelen = 0U;

}

static void print_decimal(struct print_param *param, int64_t value)
{
	/* max. required buffer for uint64_t long in decimal format */
	char digitbuff[20];
	/* pointer to the next character position (+1) */
	char *pos = digitbuff + sizeof(digitbuff);
	/* current value in 32/64 bit */
	union u_qword v;
	/* next value in 32/64 bit */
	union u_qword nv;

	/* assume an unsigned 64 bit value */
	v.qword = ((uint64_t)value) & param->vars.mask;

	/*
	 * assign sign and correct value if value is negative and
	 * value must be interpreted as signed
	 */
	if (((param->vars.flags & PRINT_FLAG_UINT32) == 0U) && (value < 0)) {
		v.qword = (uint64_t)-value;
		param->vars.prefix = "-";
		param->vars.prefixlen = 1U;
	}

	/* determine sign if explicit requested in the format string */
	if (param->vars.prefix == NULL) {
		if ((param->vars.flags & PRINT_FLAG_SIGN) != 0U) {
			param->vars.prefix = "+";
			param->vars.prefixlen = 1U;
		} else if ((param->vars.flags & PRINT_FLAG_SPACE) != 0U) {
			param->vars.prefix = " ";
			param->vars.prefixlen = 1U;
		} else {
			/* No prefix specified. */
		}
	}

	/* process 64 bit value as long as needed */
	while (v.dwords.high != 0U) {
		/* determine digits from right to left */
		pos--;
		*pos = (char)(v.qword % 10UL) + '0';
		v.qword = v.qword / 10UL;
	}

	nv.dwords.low = v.dwords.low;
	/* process 32 bit (or reduced 64 bit) value */
	do {
		/* determine digits from right to left. The compiler should be
		 * able to handle a division and multiplication by the constant
		 * 10.
		 */
		pos--;
		*pos = (char)(nv.dwords.low % 10U) + '0';
		nv.dwords.low = nv.dwords.low / 10U;
	} while (nv.dwords.low != 0U);

	/* assign parameter and apply width and precision */
	param->vars.value = pos;
	param->vars.valuelen = (digitbuff + sizeof(digitbuff)) - pos;

	format_number(param);

	param->vars.value = NULL;
	param->vars.valuelen = 0U;

}

static void print_string(const struct print_param *param, const char *s)
{
	/* the length of the string (-1) if unknown */
	uint32_t len;
	/* the number of additional characters to insert to reach the required
	 * width
	 */
	uint32_t w = 0U;

	len = strnlen_s(s, PRINT_STRING_MAX_LEN);

	/* precision gives the max. number of characters to emit. */
	if ((param->vars.precision != 0U) && (len > param->vars.precision)) {
		len = param->vars.precision;
	}

	/* calculate the number of additional characters to get the required
	 * width
	 */
	if ((param->vars.width > 0U) && (param->vars.width > len)) {
		w = param->vars.width - len;
	}

	/* emit additional characters for width, return early if an error
	 * occurred
	 */
	if ((param->vars.flags & PRINT_FLAG_LEFT_JUSTIFY) == 0U) {
		param->emit(PRINT_CMD_FILL, " ", w, param->data);
	}

	param->emit(PRINT_CMD_COPY, s, len, param->data);

	/* emit additional characters on the right, return early if an error
	 * occurred
	 */
	if ((param->vars.flags & PRINT_FLAG_LEFT_JUSTIFY) != 0U) {
		param->emit(PRINT_CMD_FILL, " ", w, param->data);
	}

}

void do_print(const char *fmt_arg, struct print_param *param,
		__builtin_va_list args)
{
	const char *fmt = fmt_arg;

	/* temp. storage for the next character */
	char ch;
	/* temp. pointer to the start of an analysed character sequence */
	const char *start;

	/* main loop: analyse until there are no more characters */
	while ((*fmt) != '\0') {
		/* mark the current position and search the next '%' */
		start = fmt;

		while (((*fmt) != '\0') && (*fmt != '%')) {
			fmt++;
		}

		/*
		 * pass all characters until the next '%' to the emit function.
		 * Return early if the function fails
		 */
			param->emit(PRINT_CMD_COPY, start, fmt - start,
				param->data);

		/* continue only if the '%' character was found */
		if (*fmt == '%') {
			/* mark current position in the format string */
			start = fmt;
			fmt++;

			/* initialize the variables for the next argument */
			(void)memset(&(param->vars), 0U, sizeof(param->vars));
			param->vars.mask = 0xFFFFFFFFFFFFFFFFUL;

			/*
			 * analyze the format specification:
			 *   - get the flags
			 *   - get the width
			 *   - get the precision
			 *   - get the length modifier
			 */
			fmt = get_flags(fmt, &(param->vars.flags));
			fmt = get_param(fmt, &(param->vars.width));

			if (*fmt == '.') {
				fmt++;
				fmt = get_param(fmt, &(param->vars.precision));
			}

			fmt = get_length_modifier(fmt, &(param->vars.flags),
						&(param->vars.mask));
			ch = *fmt;
			fmt++;

			/* a single '%'? => print out a single '%' */
			if (ch == '%') {
				param->emit(PRINT_CMD_COPY, &ch, 1U,
						param->data);
			} else if ((ch == 'd') || (ch == 'i')) {
			/* decimal number */
				if ((param->vars.flags &
					PRINT_FLAG_LONG_LONG) != 0U) {
					print_decimal(param,
						__builtin_va_arg(args, long));
				} else {
					print_decimal(param,
						__builtin_va_arg(args, int));
				}
			}
			/* unsigned decimal number */
			else if (ch == 'u') {
				param->vars.flags |= PRINT_FLAG_UINT32;
				if ((param->vars.flags &
					PRINT_FLAG_LONG_LONG) != 0U) {
					print_decimal(param,
						(int64_t)__builtin_va_arg(args,
						uint64_t));
				} else {
					print_decimal(param,
						(int64_t)__builtin_va_arg(args,
						uint32_t));
				}
			}
			/* hexadecimal number */
			else if ((ch == 'X') || (ch == 'x')) {
				if (ch == 'X') {
					param->vars.flags |= PRINT_FLAG_UPPER;
				}
				if ((param->vars.flags &
					PRINT_FLAG_LONG_LONG) != 0U) {
					print_pow2(param,
						__builtin_va_arg(args,
						uint64_t), 4U);
				} else {
					print_pow2(param,
						__builtin_va_arg(args,
						uint32_t), 4U);
				}
			}
			/* string argument */
			else if (ch == 's') {
				const char *s = __builtin_va_arg(args, char *);

				if (s == NULL) {
					s = "(null)";
				}
				print_string(param, s);
			}
			/* single character argument */
			else if (ch == 'c') {
				char c[2];

				c[0] = __builtin_va_arg(args, int);
				c[1] = 0;
				print_string(param, c);
			}
			/* default: print the format specifier as it is */
			else {
				param->emit(PRINT_CMD_COPY, start,
						fmt - start, param->data);
			}
		}
	}

}

static void
charmem(size_t cmd, const char *s_arg, uint32_t sz, struct snprint_param *param)
{
	const char *s = s_arg;
	/* pointer to the destination */
	char *p = param->dst + param->wrtn;
	/* characters actually written */
	uint32_t n = 0U;

	/* copy mode ? */
	if (cmd == PRINT_CMD_COPY) {
		if (sz > 0U) {
			while (((*s) != '\0') && (n < sz)) {
				if (n < (param->sz - param->wrtn)) {
					*p = *s;
				}
				p++;
				s++;
				n++;
			}
		}

		param->wrtn += n;
	}
	/* fill mode */
	else {
		n = (sz < (param->sz - param->wrtn)) ? sz : 0U;
		param->wrtn += sz;
		(void)memset(p, (uint8_t)*s, n);
	}

}

size_t vsnprintf(char *dst_arg, size_t sz_arg, const char *fmt, va_list args)
{
	char *dst = dst_arg;
	uint32_t sz = sz_arg;
	size_t res = 0U;


	/* struct to store all necessary parameters */
	struct print_param param;

	/* struct to store snprintf specific parameters */
	struct snprint_param snparam;

	/* initialize parameters */
	(void)memset(&snparam, 0U, sizeof(snparam));
	snparam.dst = dst;
	snparam.sz = sz;
	(void)memset(&param, 0U, sizeof(param));
	param.emit = charmem;
	param.data = &snparam;

	/* execute the printf()*/
	do_print(fmt, &param, args);

	/* ensure the written string is NULL terminated */
	if (snparam.wrtn < sz) {
		snparam.dst[snparam.wrtn] = '\0';
	}
	else {
		snparam.dst[sz - 1] = '\0';
	}

	/* return the number of chars which would be written */
	res = snparam.wrtn;

	/* done */
	return res;
}

size_t snprintf(char *dest, size_t sz, const char *fmt, ...)
{
	/* variable argument list needed for do_print() */
	va_list args;
	/* the result of this function */
	size_t res;

	va_start(args, fmt);

	/* execute the printf() */
	res = vsnprintf(dest, sz, fmt, args);

	/* destroy parameter list */
	va_end(args);

	/* done */
	return res;
}
