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

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>

#ifndef NULL
#define NULL ((void *) 0)
#endif

#define PRINT_STRING_MAX_LEN 4096

/** Use upper case letters for hexadecimal format. */
#define PRINT_FLAG_UPPER		0x00000001

/** Use alternate form. */
#define PRINT_FLAG_ALTERNATE_FORM 	0x00000002

/** Use '0' instead of ' ' for padding. */
#define PRINT_FLAG_PAD_ZERO		0x00000004

/** Use left instead of right justification. */
#define PRINT_FLAG_LEFT_JUSTIFY		0x00000008

/** Always use the sign as prefix. */
#define PRINT_FLAG_SIGN			0x00000010

/** Use ' ' as prefix if no sign is used. */
#define PRINT_FLAG_SPACE		0x00000020

/** The original value was a (unsigned) char. */
#define PRINT_FLAG_CHAR			0x00000040

/** The original value was a (unsigned) short. */
#define PRINT_FLAG_SHORT		0x00000080

/** The original value was a (unsigned) long. */
#define PRINT_FLAG_LONG			0x00000100

/** The original value was a (unsigned) long long. */
#define PRINT_FLAG_LONG_LONG		0x00000200

/** The value is interpreted as unsigned. */
#define PRINT_FLAG_UINT32		0x00000400

/** Structure used to save (v)snprintf() specific values */
struct snprint_param {
	/** The destination buffer. */
	char *dst;
	/** The size of the destination buffer. */
	int sz;
	/** Counter for written chars. */
	int wrtn;
};

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

static const char *get_int(const char *s, int *x)
{
	int negative = 0;
	*x = 0;

	/* evaluate leading '-' for negative numbers */
	if (*s == '-') {
		negative = 1;
		++s;
	}

	/* parse uint32_teger */
	while ((*s >= '0') && (*s <= '9'))
		*x = *x * 10 + (*s++ - '0');

	/* apply sign to result */
	if (negative)
		*x = -*x;

	return s;
}

static const char *get_flags(const char *s, int *flags)
{
	/* contains the flag characters */
	static const char flagchars[] = "#0- +";
	/* contains the numeric flags for the characters above */
	static const int fl[sizeof(flagchars)] = {
		PRINT_FLAG_ALTERNATE_FORM,	/* # */
		PRINT_FLAG_PAD_ZERO,	/* 0 */
		PRINT_FLAG_LEFT_JUSTIFY,	/* - */
		PRINT_FLAG_SIGN,	/* + */
		PRINT_FLAG_SPACE	/* ' ' */
	};
	const char *pos;

	/* parse multiple flags */
	while (*s) {
		/* get index of flag. Terminate loop if no flag character was
		 * found
		 */
		pos = strchr(flagchars, *s);
		if (pos == 0)
			break;

		/* apply matching flags and continue with the next character */
		++s;
		*flags |= fl[pos - flagchars];
	}

	/* Spec says that '-' has a higher priority than '0' */
	if (*flags & PRINT_FLAG_LEFT_JUSTIFY)
		*flags &= ~PRINT_FLAG_PAD_ZERO;

	/* Spec says that '+' has a higher priority than ' ' */
	if (*flags & PRINT_FLAG_SIGN)
		*flags &= ~PRINT_FLAG_SPACE;

	return s;
}

static const char *get_length_modifier(const char *s,
			int *flags, unsigned long long *mask)
{
	/* check for h[h] (char/short) */
	if (*s == 'h') {
		if (*++s == 'h') {
			*flags |= PRINT_FLAG_CHAR;
			*mask = 0x000000FF;
			++s;
		} else {
			*flags |= PRINT_FLAG_SHORT;
			*mask = 0x0000FFFF;
		}
	}
	/* check for l[l] (long/long long) */
	else if (*s == 'l') {
		if (*++s == 'l') {
			*flags |= PRINT_FLAG_LONG_LONG;
			++s;
		} else
			*flags |= PRINT_FLAG_LONG;
	}

	return s;
}

static int format_number(struct print_param *param)
{
	/* contains the character used for padding */
	char pad;
	/* effective width of the result */
	uint32_t width;
	/* number of characters to insert for width (w) and precision (p) */
	uint32_t p, w;
	/* the result */
	int res;

	/* initialize variables */
	p = w = 0;
	res = 0;
	width = param->vars.valuelen + param->vars.prefixlen;

	/* calculate additional characters for precision */
	if ((uint32_t)(param->vars.precision) > width)
		p = param->vars.precision - width;

	/* calculate additional characters for width */
	if ((uint32_t)(param->vars.width) > (width + p))
		w = param->vars.width - (width + p);

	/* handle case of right justification */
	if ((param->vars.flags & PRINT_FLAG_LEFT_JUSTIFY) == 0) {
		/* assume ' ' as padding character */
		pad = ' ';

		/*
		 * if padding with 0 is used, we have to emit the prefix (if any
		 * ) first to achieve the expected result. However, if a blank is
		 * used for padding, the prefix is emitted after the padding.
		 */

		if (param->vars.flags & PRINT_FLAG_PAD_ZERO) {
			/* use '0' for padding */
			pad = '0';

			/* emit prefix, return early if an error occurred */
			res = param->emit(PRINT_CMD_COPY, param->vars.prefix,
					param->vars.prefixlen, param->data);
			if (param->vars.prefix && (res < 0))
				return res;

			/* invalidate prefix */
			param->vars.prefix = 0;
			param->vars.prefixlen = 0;
		}

		/* fill the width with the padding character, return early if
		 * an error occurred
		 */
		res = param->emit(PRINT_CMD_FILL, &pad, w, param->data);
		if (res < 0)
			return res;
	}

	/* emit prefix (if any), return early in case of an error */
	res = param->emit(PRINT_CMD_COPY, param->vars.prefix,
			param->vars.prefixlen, param->data);
	if (param->vars.prefix && (res < 0))
		return res;

	/* insert additional 0's for precision, return early if an error
	 * occurred
	 */
	res = param->emit(PRINT_CMD_FILL, "0", p, param->data);
	if (res < 0)
		return res;

	/* emit the pre-calculated result, return early in case of an error */
	res = param->emit(PRINT_CMD_COPY, param->vars.value,
			param->vars.valuelen, param->data);
	if (res < 0)
		return res;

	/* handle left justification */
	if ((param->vars.flags & PRINT_FLAG_LEFT_JUSTIFY) != 0) {
		/* emit trailing blanks, return early in case of an error */
		res = param->emit(PRINT_CMD_FILL, " ", w, param->data);
		if (res < 0)
			return res;
	}

	/* done, return the last result */
	return res;
}

static int print_pow2(struct print_param *param,
		unsigned long long v, uint32_t shift)
{
	/* max buffer required for octal representation of unsigned long long */
	char digitbuff[22];
	/* Insert position for the next character+1 */
	char *pos = digitbuff + sizeof(digitbuff);
	/* buffer for the 0/0x/0X prefix */
	char prefix[2];
	/* pointer to the digits translation table */
	const char *digits;
	/* mask to extract next character */
	unsigned long long mask;
	int ret;

	/* calculate mask */
	mask = (1ULL << shift) - 1;

	/* determine digit translation table */
	digits = (param->vars.flags & PRINT_FLAG_UPPER) ?
			upper_hex_digits : lower_hex_digits;

	/* apply mask for short/char */
	v &= param->vars.mask;

	/* determine prefix for alternate form */
	if ((v == 0) && (param->vars.flags & PRINT_FLAG_ALTERNATE_FORM)) {
		prefix[0] = '0';
		param->vars.prefix = prefix;
		param->vars.prefixlen = 1;

		if (shift == 4) {
			param->vars.prefixlen = 2;
			prefix[1] = digits[16];
		}
	}

	/* determine digits from right to left */
	do {
		*--pos = digits[(v & mask)];
	} while (v >>= shift);

	/* assign parameter and apply width and precision */
	param->vars.value = pos;
	param->vars.valuelen = digitbuff + sizeof(digitbuff) - pos;

	ret = format_number(param);

	param->vars.value = NULL;
	param->vars.valuelen = 0;

	return ret;
}

static int print_decimal(struct print_param *param, long long value)
{
	/* max. required buffer for unsigned long long in decimal format */
	char digitbuff[20];
	/* pointer to the next character position (+1) */
	char *pos = digitbuff + sizeof(digitbuff);
	/* current value in 32/64 bit */
	union u_qword v;
	/* next value in 32/64 bit */
	union u_qword nv;
	/* helper union for division result */
	struct udiv_result d;
	int ret;

	/* assume an unsigned 64 bit value */
	v.qword = ((unsigned long long)value) & param->vars.mask;

	/*
	 * assign sign and correct value if value is negative and
	 * value must be interpreted as signed
	 */
	if (((param->vars.flags & PRINT_FLAG_UINT32) == 0) && (value < 0)) {
		v.qword = (unsigned long long)-value;
		param->vars.prefix = "-";
		param->vars.prefixlen = 1;
	}

	/* determine sign if explicit requested in the format string */
	if (!param->vars.prefix) {
		if (param->vars.flags & PRINT_FLAG_SIGN) {
			param->vars.prefix = "+";
			param->vars.prefixlen = 1;
		} else if (param->vars.flags & PRINT_FLAG_SPACE) {
			param->vars.prefix = " ";
			param->vars.prefixlen = 1;
		}
	}

	/* process 64 bit value as long as needed */
	while (v.dwords.high != 0) {
		/* determine digits from right to left */
		udiv64(v.qword, 10, &d);
		*--pos = d.r.dwords.low + '0';
		v.qword = d.q.qword;
	}

	/* process 32 bit (or reduced 64 bit) value */
	do {
		/* determine digits from right to left. The compiler should be
		 * able to handle a division and multiplication by the constant
		 * 10.
		 */
		nv.dwords.low = v.dwords.low / 10;
		*--pos = (v.dwords.low - (10 * nv.dwords.low)) + '0';
	} while ((v.dwords.low = nv.dwords.low) != 0);

	/* assign parameter and apply width and precision */
	param->vars.value = pos;
	param->vars.valuelen = digitbuff + sizeof(digitbuff) - pos;

	ret = format_number(param);

	param->vars.value = NULL;
	param->vars.valuelen = 0;

	return ret;
}

static int print_string(struct print_param *param, const char *s)
{
	/* the length of the string (-1) if unknown */
	int len;
	/* the number of additional characters to insert to reach the required
	 * width
	 */
	uint32_t w;
	/* the last result of the emit function */
	int res;

	w = 0;
	len = -1;

	/* we need the length of the string if either width or precision is
	 * given
	 */
	if (param->vars.precision || param->vars.width)
		len = strnlen_s(s, PRINT_STRING_MAX_LEN);

	/* precision gives the max. number of characters to emit. */
	if (param->vars.precision && (len > param->vars.precision))
		len = param->vars.precision;

	/* calculate the number of additional characters to get the required
	 * width
	 */
	if (param->vars.width > 0 && param->vars.width > len)
		w = param->vars.width - len;

	/* emit additional characters for width, return early if an error
	 * occurred
	 */
	if ((param->vars.flags & PRINT_FLAG_LEFT_JUSTIFY) == 0) {
		res = param->emit(PRINT_CMD_FILL, " ", w, param->data);
		if (res < 0)
			return res;
	}

	/* emit the string, return early if an error occurred */
	res = param->emit(PRINT_CMD_COPY, s, len, param->data);
	if (res < 0)
		return res;

	/* emit additional characters on the right, return early if an error
	 * occurred
	 */
	if (param->vars.flags & PRINT_FLAG_LEFT_JUSTIFY) {
		res = param->emit(PRINT_CMD_FILL, " ", w, param->data);
		if (res < 0)
			return res;
	}

	return res;
}

int do_print(const char *fmt, struct print_param *param,
		__builtin_va_list args)
{
	/* the result of this function */
	int res = 0;
	/* temp. storage for the next character */
	char ch;
	/* temp. pointer to the start of an analysed character sequence */
	const char *start;

	/* main loop: analyse until there are no more characters */
	while (*fmt) {
		/* mark the current position and search the next '%' */
		start = fmt;

		while (*fmt && (*fmt != '%'))
			fmt++;

		/*
		 * pass all characters until the next '%' to the emit function.
		 * Return early if the function fails
		 */
		res = param->emit(PRINT_CMD_COPY, start, fmt - start,
				param->data);
		if (res < 0)
			return res;

		/* continue only if the '%' character was found */
		if (*fmt == '%') {
			/* mark current position in the format string */
			start = fmt++;

			/* initialize the variables for the next argument */
			memset(&(param->vars), 0, sizeof(param->vars));
			param->vars.mask = 0xFFFFFFFFFFFFFFFFULL;

			/*
			 * analyze the format specification:
			 *   - get the flags
			 *   - get the width
			 *   - get the precision
			 *   - get the length modifier
			 */
			fmt = get_flags(fmt, &(param->vars.flags));
			fmt = get_int(fmt, &(param->vars.width));

			if (*fmt == '.') {
				fmt++;
				fmt = get_int(fmt, &(param->vars.precision));
				if (param->vars.precision < 0)
					param->vars.precision = 0;
			}

			fmt = get_length_modifier(fmt, &(param->vars.flags),
						&(param->vars.mask));
			ch = *fmt++;

			/* a single '%'? => print out a single '%' */
			if (ch == '%') {
				res = param->emit(PRINT_CMD_COPY, &ch, 1,
						param->data);
			}
			/* decimal number */
			else if ((ch == 'd') || (ch == 'i')) {
				res = print_decimal(param,
						(param->vars.flags &
						PRINT_FLAG_LONG_LONG) ?
						__builtin_va_arg(args,
							long long)
						: (long long)
						__builtin_va_arg(args,
							int));
			}
			/* unsigned decimal number */
			else if (ch == 'u') {
				param->vars.flags |= PRINT_FLAG_UINT32;
				res = print_decimal(param,
						(param->vars.flags &
						PRINT_FLAG_LONG_LONG) ?
						__builtin_va_arg(args,
							unsigned long long)
						: (unsigned long long)
						__builtin_va_arg(args,
							unsigned int));
			}
			/* octal number */
			else if (ch == 'o') {
				res = print_pow2(param,
						(param->vars.flags &
						PRINT_FLAG_LONG_LONG) ?
						__builtin_va_arg(args,
							unsigned long long)
						: (unsigned long long)
						__builtin_va_arg(args,
							uint32_t),
						3);
			}
			/* hexadecimal number */
			else if ((ch == 'X') || (ch == 'x')) {
				if (ch == 'X')
					param->vars.flags |= PRINT_FLAG_UPPER;
				res = print_pow2(param,
						(param->vars.flags &
						PRINT_FLAG_LONG_LONG) ?
						__builtin_va_arg(args,
							unsigned long long)
						: (unsigned long long)
						__builtin_va_arg(args,
							uint32_t),
						4);
			}
			/* string argument */
			else if (ch == 's') {
				const char *s = __builtin_va_arg(args, char *);

				if (s == NULL)
					s = "(null)";
				res = print_string(param, s);
			}
			/* pointer argument */
			else if (ch == 'p') {
				param->vars.flags |= PRINT_FLAG_ALTERNATE_FORM;
				/* XXXCRG res=print_pow2(param,
				 * (uint32_t) __builtin_va_arg(args,
				 * void *),4);
				 */
				res = print_pow2(param, (unsigned long long)
					__builtin_va_arg(args, void *), 4);
			}
			/* single character argument */
			else if (ch == 'c') {
				char c[2];

				c[0] = __builtin_va_arg(args, int);
				c[1] = 0;
				res = print_string(param, c);
			}
			/* default: print the format specifier as it is */
			else {
				res = param->emit(PRINT_CMD_COPY, start,
						fmt - start, param->data);
			}
		}
		/* return if an error occurred */
		if (res < 0)
			return res;
	}

	/* done. Return the result of the last emit function call */
	return res;
}

static int charmem(int cmd, const char *s, int sz, void *hnd)
{
	/* pointer to the snprint parameter list */
	struct snprint_param *param = (struct snprint_param *) hnd;
	/* pointer to the destination */
	char *p = param->dst + param->wrtn;
	/* characters actually written */
	int n = 0;

	/* copy mode ? */
	if (cmd == PRINT_CMD_COPY) {
		if (sz < 0) {
			while (*s) {
				if (n < param->sz - param->wrtn)
					*p = *s;
				p++;
				s++;
				n++;
			}

		} else if (sz > 0) {
			while (*s && n < sz) {
				if (n < param->sz - param->wrtn)
					*p = *s;
				p++;
				s++;
				n++;
			}
		}

		param->wrtn += n;
		return n;
	}
	/* fill mode */
	else {
		n = (sz < param->sz - param->wrtn) ? sz : 0;
		param->wrtn += sz;
		memset(p, *s, n);
	}

	return n;
}

int vsnprintf(char *dst, int sz, const char *fmt, va_list args)
{
	char c[1];
	/* the result of this function */
	int res = 0;

	if (sz <= 0 || !dst) {
		dst = c;
		sz = 1;
	}

	/* struct to store all necessary parameters */
	struct print_param param;

	/* struct to store snprintf specific parameters */
	struct snprint_param snparam;

	/* initialize parameters */
	memset(&snparam, 0, sizeof(snparam));
	snparam.dst = dst;
	snparam.sz = sz;
	memset(&param, 0, sizeof(param));
	param.emit = charmem;
	param.data = &snparam;

	/* execute the printf() */
	if (do_print(fmt, &param, args) < 0)
		return -1;

	/* ensure the written string is NULL terminated */
	if (snparam.wrtn < sz)
		snparam.dst[snparam.wrtn] = '\0';
	else
		snparam.dst[sz - 1] = '\0';

	/* return the number of chars which would be written */
	res = snparam.wrtn;

	/* done */
	return res;
}

int snprintf(char *dest, int sz, const char *fmt, ...)
{
	/* variable argument list needed for do_print() */
	va_list args;
	/* the result of this function */
	int res;

	va_start(args, fmt);

	/* execute the printf() */
	res = vsnprintf(dest, sz, fmt, args);

	/* destroy parameter list */
	va_end(args);

	/* done */
	return res;
}
