/*
 * Copyright (C) 2018 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef NPK_LOG_H
#define NPK_LOG_H

struct hv_npk_log_param;

void npk_log_setup(struct hv_npk_log_param *param);
void npk_log_write(const char *buf, size_t len);

#endif /* NPK_LOG_H */
