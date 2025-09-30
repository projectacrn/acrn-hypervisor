/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RELOCATE_H
#define RELOCATE_H

#include <elf.h>
#include <asm/boot/ld_sym.h>

uint64_t arch_get_hv_image_delta(void);

static inline uint64_t get_hv_image_delta(void)
{
	return arch_get_hv_image_delta();
}

/* get the actual Hypervisor load address (HVA) */
static inline uint64_t get_hv_image_base(void)
{
	return (get_hv_image_delta() + CONFIG_HV_RAM_START);
}

static inline uint64_t get_hv_image_size(void)
{
	return (uint64_t)(&ld_ram_end - &ld_ram_start);
}

#ifdef CONFIG_RELOC
extern void relocate(struct Elf64_Dyn *dynamic);
#endif

#endif /* RELOCATE_H */
