/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef EFI_MMAP_H
#define EFI_MMAP_H
#include <types.h>

#define MAX_EFI_MMAP_ENTRIES		256U

void init_efi_mmap_entries(struct abi_efi_info *efi_info);

uint32_t get_efi_mmap_entries_count(void);
const struct efi_memory_desc *get_efi_mmap_entry(void);

#endif
