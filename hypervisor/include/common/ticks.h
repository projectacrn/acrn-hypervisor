/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef COMMON_TICKS_H
#define COMMON_TICKS_H

#include <types.h>

#define TICKS_PER_MS	us_to_ticks(1000U)

/**
 * @brief Read current CPU tick count.
 *
 * @return CPU ticks
 */
uint64_t cpu_ticks(void);

/**
 * @brief  Get CPU tick frequency in KHz.
 *
 * @return CPU frequency (KHz)
 */
uint32_t cpu_tickrate(void);

/**
 * @brief Convert micro seconds to CPU ticks.
 *
 * @param[in] us micro seconds to convert
 * @return CPU ticks
 */
uint64_t us_to_ticks(uint32_t us);

/**
 * @brief Convert CPU cycles to micro seconds.
 *
 * @param[in] ticks CPU ticks to convert
 * @return microsecond
 */
uint64_t ticks_to_us(uint64_t ticks);

/**
 * @brief Convert CPU cycles to milli seconds.
 *
 * @param[in] ticks CPU ticks to convert
 * @return millisecond
 */
uint64_t ticks_to_ms(uint64_t ticks);

#endif	/* COMMON_TICKS_H */

