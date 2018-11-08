/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RELOCATE_H
#define RELOCATE_H

extern void relocate(void);
extern uint64_t get_hv_image_delta(void);
extern uint64_t get_hv_image_base(void);
extern uint64_t read_trampoline_sym(const void *sym);
extern void write_trampoline_sym(const void *sym, uint64_t val);
extern uint64_t prepare_trampoline(void);

/* external symbols that are helpful for relocation */
extern uint8_t		_DYNAMIC[1];
extern const uint8_t	ld_trampoline_load;
extern uint8_t		ld_trampoline_start;
extern uint8_t		ld_trampoline_end;
extern const uint64_t	ld_trampoline_size;

extern uint8_t		cpu_primary_start_32;
extern uint8_t		cpu_primary_start_64;

extern uint8_t		trampoline_fixup_cs;
extern uint8_t		trampoline_fixup_ip;
extern uint8_t		trampoline_fixup_target;
extern uint8_t		cpu_boot_page_tables_start;
extern uint8_t		cpu_boot_page_tables_ptr;
extern uint8_t		trampoline_pdpt_addr;
extern uint8_t		trampoline_gdt_ptr;
extern uint8_t		trampoline_start64_fixup;
extern uint8_t		trampoline_spinlock_ptr;

extern uint64_t		trampoline_start16_paddr;

#endif /* RELOCATE_H */
