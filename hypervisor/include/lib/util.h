/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef UTIL_H
#define UTIL_H

/** Add an offset (in bytes) to an (base)address.
 *
 *  @param  addr Baseaddress
 *  @param  off  Offset
 *  @return Returns baseaddress + offset in bytes.
 */
#define ADD_OFFSET(addr, off) (void *)(((uint8_t *)(addr))+(off))

#define offsetof(st, m) __builtin_offsetof(st, m)

/** Roundup (x/y) to ( x/y + (x%y) ? 1 : 0) **/
#define INT_DIV_ROUNDUP(x, y)	(((x)+(y)-1)/(y))

#define min(x, y)	((x) < (y)) ? (x) : (y)

#define max(x, y)	((x) < (y)) ? (y) : (x)

#endif /* UTIL_H */
