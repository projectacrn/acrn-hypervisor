/*
 * Copyright (C) 2023 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <sys/queue.h>

#define list_foreach_safe(var, head, field, tvar)	\
for ((var) = LIST_FIRST((head));			\
	(var) && ((tvar) = LIST_NEXT((var), field), 1); \
	(var) = (tvar))
