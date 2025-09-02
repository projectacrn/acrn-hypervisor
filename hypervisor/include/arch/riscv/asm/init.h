/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef INIT_H
#define INIT_H

void init_primary_pcpu(uint64_t hart_id, uint64_t fdt_paddr);
void init_secondary_pcpu(uint64_t hart_id);

#endif /* INIT_H*/
