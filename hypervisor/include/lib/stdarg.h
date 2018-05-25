/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STDARG_H
#define STDARG_H

#include <types.h>

#define va_start(x, y) __builtin_va_start((x), (y))
#define va_end(x)     __builtin_va_end(x)

#endif /* STDARG_H */
