/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RELOCATE_H
#define RELOCATE_H

extern void _relocate(void);
extern uint64_t get_hv_image_delta(void);
extern uint64_t get_hv_image_base(void);
extern uint64_t read_trampoline_sym(void *sym);
extern void write_trampoline_sym(void *sym, uint64_t val);
extern uint64_t prepare_trampoline(void);

/* external symbols that are helpful for relocation */
extern uint8_t		_DYNAMIC[1];
extern const uint8_t	_ld_trampoline_load[1];
extern uint8_t		_ld_trampoline_start[1];
extern uint8_t		_ld_trampoline_end[1];
extern const uint64_t	_ld_trampoline_size;

extern void		cpu_primary_start_32(void);
extern void		cpu_primary_start_64(void);

extern uint16_t		trampoline_fixup_cs[1];
extern uint16_t		trampoline_fixup_ip[1];
extern void		trampoline_fixup_target(void);
extern uint64_t		CPU_Boot_Page_Tables_Start[1];
extern uint32_t		CPU_Boot_Page_Tables_ptr[1];
extern uint64_t		trampoline_pdpt_addr[4];
extern uint16_t		trampoline_gdt_ptr[5];
extern uint32_t		trampoline_start64_fixup[1];
extern uint64_t		trampoline_spinlock_ptr[1];

extern uint64_t		trampoline_start16_paddr;

#endif /* RELOCATE_H */
