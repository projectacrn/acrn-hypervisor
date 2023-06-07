/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef COMMON_DELAY_H
#define COMMON_DELAY_H

#include <types.h>

/**
 * @brief Busy wait a few micro seconds.
 *
 * @param[in] us micro seconds to delay.
 */
void udelay(uint32_t us);

#endif	/* COMMON_DELAY_H */
