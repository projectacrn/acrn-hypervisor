/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SHELL_H
#define SHELL_H

/* Switching key combinations for shell and uart console */
#define GUEST_CONSOLE_TO_HV_SWITCH_KEY      0       /* CTRL + SPACE */

#ifdef HV_DEBUG
void shell_init(void);
void shell_kick(void);
#else
static inline void shell_init(void) {}
static inline void shell_kick(void) {}
#endif

#endif	/* SHELL_H */
