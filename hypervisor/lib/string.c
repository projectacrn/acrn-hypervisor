/*
 * Copyright (C) 2018 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* FIXME: It'd be nice to configure around these, but the include files are too
 * painful.  These macros should at least be more portable than hardwired hex
 * constants.
 */

#include <hypervisor.h>

/* Categories.  */

enum {
        /* In C99 */
        _sch_isblank  = 0x0001, /* space \t */
        _sch_iscntrl  = 0x0002, /* nonprinting characters */
        _sch_isdigit  = 0x0004, /* 0-9 */
        _sch_islower  = 0x0008, /* a-z */
        _sch_isprint  = 0x0010, /* any printing character including ' ' */
        _sch_ispunct  = 0x0020, /* all punctuation */
        _sch_isspace  = 0x0040, /* space \t \n \r \f \v */
        _sch_isupper  = 0x0080, /* A-Z */
        _sch_isxdigit = 0x0100, /* 0-9A-Fa-f */

        /* Extra categories useful to cpplib.  */
        _sch_isidst     = 0x0200,       /* A-Za-z_ */
        _sch_isvsp    = 0x0400, /* \n \r */
        _sch_isnvsp   = 0x0800, /* space \t \f \v \0 */

        /* Combinations of the above.  */
        _sch_isalpha  = _sch_isupper|_sch_islower,      /* A-Za-z */
        _sch_isalnum  = _sch_isalpha|_sch_isdigit,      /* A-Za-z0-9 */
        _sch_isidnum  = _sch_isidst|_sch_isdigit,       /* A-Za-z0-9_ */
        _sch_isgraph  = _sch_isalnum|_sch_ispunct, /* isprint and not space */
        _sch_iscppsp  = _sch_isvsp|_sch_isnvsp, /* isspace + \0 */
        /* basic charset of ISO C (plus ` and @) */
        _sch_isbasic  = _sch_isprint|_sch_iscppsp
};

/* Shorthand */
#define bl _sch_isblank
#define cn _sch_iscntrl
#define di _sch_isdigit
#define is _sch_isidst
#define lo _sch_islower
#define nv _sch_isnvsp
#define pn _sch_ispunct
#define pr _sch_isprint
#define sp _sch_isspace
#define up _sch_isupper
#define vs _sch_isvsp
#define xd _sch_isxdigit

/* Masks.  */
#define L  ((const uint16_t)(lo | is | pr))     /* lower case letter */
#define XL ((const uint16_t)(lo | is | xd | pr))/* lowercase hex digit */
#define U  ((const uint16_t)(up | is | pr))     /* upper case letter */
#define XU ((const uint16_t)(up | is | xd | pr))/* uppercase hex digit */
#define D  ((const uint16_t)(di | xd | pr))     /* decimal digit */
#define P  ((const uint16_t)(pn | pr))          /* punctuation */
#define _  ((const uint16_t)(pn | is | pr))     /* underscore */

#define C  ((const uint16_t)(cn))               /* control character */
#define Z  ((const uint16_t)(nv | cn))          /* NUL */
#define M  ((const uint16_t)(nv | sp | cn))     /* cursor movement: \f \v */
#define V  ((const uint16_t)(vs | sp | cn))     /* vertical space: \r \n */
#define T  ((const uint16_t)(nv | sp | bl | cn))/* tab */
#define S  ((const uint16_t)(nv | sp | bl | pr))/* space */

/* Character classification.  */
const uint16_t _sch_istable[256] = {
        Z,  C,  C,  C,   C,  C,  C,  C, /* NUL SOH STX ETX  EOT ENQ ACK BEL */
        C,  T,  V,  M,   M,  V,  C,  C, /* BS  HT  LF  VT   FF  CR  SO  SI  */
        C,  C,  C,  C,   C,  C,  C,  C, /* DLE DC1 DC2 DC3  DC4 NAK SYN ETB */
        C,  C,  C,  C,   C,  C,  C,  C, /* CAN EM  SUB ESC  FS  GS  RS  US  */
        S,  P,  P,  P,   P,  P,  P,  P, /* SP  !   "   #    $   %   &   '   */
        P,  P,  P,  P,   P,  P,  P,  P, /* (   )   *   +    ,   -   .   /   */
        D,  D,  D,  D,   D,  D,  D,  D, /* 0   1   2   3    4   5   6   7   */
        D,  D,  P,  P,   P,  P,  P,  P, /* 8   9   :   ;    <   =   >   ?   */
        P, XU, XU, XU,  XU, XU, XU,  U, /* @   A   B   C    D   E   F   G   */
        U,  U,  U,  U,   U,  U,  U,  U, /* H   I   J   K    L   M   N   O   */
        U,  U,  U,  U,   U,  U,  U,  U, /* P   Q   R   S    T   U   V   W   */
        U,  U,  U,  P,   P,  P,  P,  _, /* X   Y   Z   [    \   ]   ^   _   */
        P, XL, XL, XL,  XL, XL, XL,  L, /* `   a   b   c    d   e   f   g   */
        L,  L,  L,  L,   L,  L,  L,  L, /* h   i   j   k    l   m   n   o   */
        L,  L,  L,  L,   L,  L,  L,  L, /* p   q   r   s    t   u   v   w   */
        L,  L,  L,  P,   P,  P,  P,  C, /* x   y   z   {    |   }   ~   DEL */

        /* high half of unsigned char is locale-specific, so all tests are
         * false in "C" locale
         */
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,

        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
};

#define _sch_test(c, bit) (_sch_istable[(c) & 0xffU] & (uint16_t)(bit))

#define ISALPHA(c)  _sch_test(c, _sch_isalpha)
#define ISALNUM(c)  _sch_test(c, _sch_isalnum)
#define ISBLANK(c)  _sch_test(c, _sch_isblank)
#define ISCNTRL(c)  _sch_test(c, _sch_iscntrl)
#define ISDIGIT(c)  _sch_test(c, _sch_isdigit)
#define ISGRAPH(c)  _sch_test(c, _sch_isgraph)
#define ISLOWER(c)  _sch_test(c, _sch_islower)
#define ISPRINT(c)  _sch_test(c, _sch_isprint)
#define ISPUNCT(c)  _sch_test(c, _sch_ispunct)
#define ISSPACE(c)  _sch_test(c, _sch_isspace)
#define ISUPPER(c)  _sch_test(c, _sch_isupper)
#define ISXDIGIT(c) _sch_test(c, _sch_isxdigit)

#define ISIDNUM(c)      _sch_test(c, _sch_isidnum)
#define ISIDST(c)       _sch_test(c, _sch_isidst)
#define IS_ISOBASIC(c)  _sch_test(c, _sch_isbasic)
#define IS_VSPACE(c)    _sch_test(c, _sch_isvsp)
#define IS_NVSPACE(c)   _sch_test(c, _sch_isnvsp)
#define IS_SPACE_OR_NUL(c)      _sch_test(c, _sch_iscppsp)

/* Character transformation.  */
const uint8_t _sch_tolower[256] = {
        0,  1,  2,  3,   4,  5,  6,  7,   8,  9, 10, 11,  12, 13, 14, 15,
        16, 17, 18, 19,  20, 21, 22, 23,  24, 25, 26, 27,  28, 29, 30, 31,
        32, 33, 34, 35,  36, 37, 38, 39,  40, 41, 42, 43,  44, 45, 46, 47,
        48, 49, 50, 51,  52, 53, 54, 55,  56, 57, 58, 59,  60, 61, 62, 63,
        64,

        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
        'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',

        91, 92, 93, 94, 95, 96,

        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
        'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',

        123, 124, 125, 126, 127,

        128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141,
        142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155,
        156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169,
        170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183,
        184, 185, 186, 187, 188, 189, 190, 191,

        192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205,
        206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219,
        220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233,
        234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247,
        248, 249, 250, 251, 252, 253, 254, 255,
};

const uint8_t _sch_toupper[256] = {
        0,  1,  2,  3,   4,  5,  6,  7,   8,  9, 10, 11,  12, 13, 14, 15,
        16, 17, 18, 19,  20, 21, 22, 23,  24, 25, 26, 27,  28, 29, 30, 31,
        32, 33, 34, 35,  36, 37, 38, 39,  40, 41, 42, 43,  44, 45, 46, 47,
        48, 49, 50, 51,  52, 53, 54, 55,  56, 57, 58, 59,  60, 61, 62, 63,
        64,

        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
        'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',

        91, 92, 93, 94, 95, 96,

        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
        'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',

        123, 124, 125, 126, 127,

        128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141,
        142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155,
        156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169,
        170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183,
        184, 185, 186, 187, 188, 189, 190, 191,

        192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205,
        206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219,
        220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233,
        234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247,
        248, 249, 250, 251, 252, 253, 254, 255,
};

#define TOUPPER(c) _sch_toupper[(c) & 0xffU]
#define TOLOWER(c) _sch_tolower[(c) & 0xffU]

#ifndef ULONG_MAX
#define ULONG_MAX       ((uint64_t)(~0UL))              /* 0xFFFFFFFF */
#endif

#ifndef LONG_MAX
#define LONG_MAX        ((long)(ULONG_MAX >> 1))        /* 0x7FFFFFFF */
#endif

#ifndef LONG_MIN
#define LONG_MIN        ((long)(~LONG_MAX))             /* 0x80000000 */
#endif

/*
 * Convert a string to a long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
long
strtol(const char *nptr, char **endptr, int base)
{
	const char *s = nptr;
	uint64_t acc;
	int c;
	uint64_t cutoff;
	int neg = 0, any, cutlim;

        /*
         * Skip white space and pick up leading +/- sign if any.
         * If base is 0, allow 0x for hex and 0 for octal, else
         * assume decimal; if base is already 16, allow 0x.
         */
        do {
                c = *s++;
        } while (ISSPACE(c) != 0U);
        if (c == '-') {
                neg = 1;
                c = *s++;
        } else if (c == '+')
                c = *s++;
        if ((base == 0 || base == 16) &&
                        c == '0' && (*s == 'x' || *s == 'X')) {
                c = s[1];
                s += 2;
                base = 16;
        }
        if (base == 0)
                base = c == '0' ? 8 : 10;

        /*
         * Compute the cutoff value between legal numbers and illegal
         * numbers.  That is the largest legal value, divided by the
         * base.  An input number that is greater than this value, if
         * followed by a legal input character, is too big.  One that
         * is equal to this value may be valid or not; the limit
         * between valid and invalid numbers is then based on the last
         * digit.  For instance, if the range for longs is
         * [-2147483648..2147483647] and the input base is 10,
         * cutoff will be set to 214748364 and cutlim to either
         * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
         * a value > 214748364, or equal but the next digit is > 7 (or 8),
         * the number is too big, and we will return a range error.
         *
         * Set any if any `digits' consumed; make it negative to indicate
         * overflow.
         */
        cutoff = (neg != 0) ? -(uint64_t)LONG_MIN : LONG_MAX;
        cutlim = cutoff % (uint64_t)base;
        cutoff /= (uint64_t)base;
        for (acc = 0, any = 0;; c = *s++) {
                if (ISDIGIT(c) != 0U)
                        c -= '0';
                else if (ISALPHA(c) != 0U)
                        c -= (ISUPPER(c) != 0U) ? 'A' - 10 : 'a' - 10;
                else
                        break;
                if (c >= base)
                        break;
                if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
                        any = -1;
                else {
                        any = 1;
                        acc *= base;
                        acc += c;
                }
        }
        if (any < 0)
                acc = (neg != 0) ? LONG_MIN : LONG_MAX;
        else if (neg != 0)
                acc = -acc;
        if (endptr != NULL)
                *endptr = (char *) ((any != 0) ? s - 1 : nptr);
        return acc;
}

/*
 * Convert a string to an uint64_t integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
uint64_t
strtoul(const char *nptr, char **endptr, int base)
{
	const char *s = nptr;
	uint64_t acc;
	int c;
	uint64_t cutoff;
	int neg = 0, any, cutlim;

        /*
         * See strtol for comments as to the logic used.
         */
        do {
                c = *s++;
        } while (ISSPACE(c) != 0U);
        if (c == '-') {
                neg = 1;
                c = *s++;
        } else if (c == '+')
                c = *s++;
        if ((base == 0 || base == 16) &&
                        c == '0' && (*s == 'x' || *s == 'X')) {
                c = s[1];
                s += 2;
                base = 16;
        }
        if (base == 0)
                base = c == '0' ? 8 : 10;
        cutoff = (uint64_t)ULONG_MAX / (uint64_t)base;
        cutlim = (uint64_t)ULONG_MAX % (uint64_t)base;
        for (acc = 0, any = 0;; c = *s++) {
                if (ISDIGIT(c) != 0U)
                        c -= '0';
                else if (ISALPHA(c) != 0U)
                        c -= (ISUPPER(c) != 0U) ? 'A' - 10 : 'a' - 10;
                else
                        break;
                if (c >= base)
                        break;
                if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
                        any = -1;
                else {
                        any = 1;
                        acc *= base;
                        acc += c;
                }
        }
        if (any < 0)
                acc = ULONG_MAX;
        else if (neg != 0)
                acc = -acc;
        if (endptr != NULL)
                *endptr = (char *) ((any != 0) ? s - 1 : nptr);
        return acc;
}

int
atoi(const char *str)
{
        return (int)strtol(str, (char **)NULL, 10);
}

char *strchr(const char *s, int ch)
{
        while ((*s != 0) && (*s != ch))
                ++s;

        return ((*s) != 0) ? ((char *)s) : 0;
}

/**
 *strcpy_s
 *
 * description:
 *    This function copies the string pointed to by s to a buffer
 *    pointed by d.
 *
 *
 * input:
 *    d        pointer to dest buffer.
 *
 *    dmax     maximum length of dest buffer
 *
 *    s        pointer to the source string
 *
 * return value:
 *    dest      pointer to dest string if string is copied
 *              successfully,or else return null.
 *
 * notes:
 *    1) both d and s shall not be null pointers.
 *    2) dmax shall not 0.
 */
char *strcpy_s(char *d, size_t dmax, const char *s)
{

        char *dest_base;
        size_t dest_avail;
        uint64_t overlap_guard;

        if (s == NULL || d == NULL || dmax == 0) {
                pr_err("%s: invalid src, dest buffer or length.", __func__);
                return NULL;
        }

        if (s == d)
                return d;

        overlap_guard = (uint64_t)((d > s) ? (d - s - 1) : (s - d - 1));

        dest_avail = dmax;
        dest_base = d;

        while (dest_avail > 0) {
                if (overlap_guard == 0) {
                        pr_err("%s: overlap happened.", __func__);
                        *(--d) = '\0';
                        return NULL;
                }

                *d = *s;
                if (*d == '\0')
                        return dest_base;

                d++;
                s++;
                dest_avail--;
                overlap_guard--;
        }

        pr_err("%s: dest buffer has no enough space.", __func__);

        /*
         * to avoid a string that is not
         * null-terminated in dest buffer
         */
        dest_base[dmax - 1] = '\0';
        return NULL;
}

/*
 * strncpy_s
 *
 * description:
 *    This function copies maximum 'slen'characters from string pointed
 *    by s to a buffer pointed by d.
 *
 * input:
 *    d         pointer to dest buffer.
 *
 *    dmax      maximum length of dest buffer.
 *
 *    s         pointer to the source string.
 *
 *    slen      the maximum number of characters to copy from source
 *              string.
 *
 * return value:
 *    dest      pointer to dest string if source string is copied
 *              successfully, or else return null.
 *
 * notes:
 *    1) both dmax and slen should not be 0.
 *    2) both d and s should not be null pointers.
 *    3) will assert() if overlap happens or dest buffer has no
 *       enough space.
 */
char *strncpy_s(char *d, size_t dmax, const char *s, size_t slen)
{
        char *dest_base;
        size_t dest_avail;
        uint64_t overlap_guard;

        if (d == NULL || s == NULL) {
                pr_err("%s: invlaid src or dest buffer", __func__);
                return NULL;
        }

        if (dmax == 0 || slen == 0) {
                pr_err("%s: invlaid length of src or dest buffer", __func__);
                return NULL;
        }

        if (d == s)
                return d;

        overlap_guard = (uint64_t)((d > s) ? (d - s - 1) : (s - d - 1));

        dest_base = d;
        dest_avail = dmax;

        while (dest_avail > 0) {
                if (overlap_guard == 0) {
                        pr_err("%s: overlap happened.", __func__);
                        *(--d) = '\0';
                        return NULL;
                }

                if (slen == 0) {
                        *d = '\0';
                        return dest_base;
                }

                *d = *s;
                if (*d == '\0')
                        return dest_base;

                d++;
                s++;
                slen--;
                dest_avail--;
                overlap_guard--;
        }

        pr_err("%s: dest buffer has no enough space.", __func__);

        /*
         * to avoid a string that is not
         * null-terminated in dest buffer
         */
        dest_base[dmax - 1] = '\0';
        return NULL;
}

/**
 *
 *    strnlen_s
 *
 * description:
 *    The function calculates the length of the string pointed
 *    to by str.
 *
 *
 * input:
 *    str      pointer to the null-terminated string to be examined.
 *
 *    dmax      maximum number of characer to examine.
 *
 *
 * return value:
 *    string length, excluding the null character.
 *    will return 0 if str is null.
 */
size_t strnlen_s(const char *str, size_t maxlen)
{
        size_t count;

        if (str == NULL)
                return 0;

        count = 0;
        while ((*str) != 0) {
                if (maxlen == 0)
                        break;

                count++;
                maxlen--;
                str++;
        }

        return count;
}

char hexdigit(int decimal_val)
{
        static const char hexdigits[] = { '0', '1', '2', '3', '4', '5', '6',
                '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

        /* Return hex character */
        return hexdigits[decimal_val & 0x0FU];
}

int strcmp(const char *s1, const char *s2)
{
        while (((*s1) != 0) && ((*s2) != 0) && ((*s1) == (*s2))) {
                s1++;
                s2++;
        }

        return *s1 - *s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
        while (((n - 1) != 0) && ((*s1) != 0) && ((*s2) != 0) && ((*s1) == (*s2))) {
                s1++;
                s2++;
                n--;
        }

        return *s1 - *s2;
}
