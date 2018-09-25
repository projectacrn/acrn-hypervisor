/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _MACROS_H_
#define _MACROS_H_

#undef __CONCAT

#define _CONCAT_(a, b) a ## b
#define __CONCAT(a, b) _CONCAT_(a, b)

#define	KB		(1024UL)
#define	MB		(1024 * 1024UL)
#define	GB		(1024 * 1024 * 1024UL)

#endif
