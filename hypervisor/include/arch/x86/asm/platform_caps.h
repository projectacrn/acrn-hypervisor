/*
 * Copyright (C) 2020-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLATFORM_CAPS_H
#define PLATFORM_CAPS_H

struct platform_caps_x86 {
	/* true if posted interrupt is supported by all IOMMUs */
	bool pi;
};

extern struct platform_caps_x86 platform_caps;

#endif /* PLATFORM_CAPS_H */
