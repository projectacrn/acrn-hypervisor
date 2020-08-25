/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SHELL_H
#define SHELL_H

void shell_init(void);
void shell_kick(void);

void set_vmexit_sample_flag(bool to_enable);
bool is_vmexit_sample_enabled(void);

#endif /* SHELL_H */
