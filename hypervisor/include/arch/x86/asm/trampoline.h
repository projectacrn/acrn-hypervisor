/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TRAMPOLINE_H
#define TRAMPOLINE_H

extern uint64_t read_trampoline_sym(const void *sym);
extern void write_trampoline_sym(const void *sym, uint64_t val);
extern void write_trampoline_stack_sym(uint16_t pcpu_id);
extern uint64_t prepare_trampoline(void);
extern uint64_t get_trampoline_start16_paddr(void);

/* external symbols that are helpful for relocation */
extern uint8_t		trampoline_fixup_cs;
extern uint8_t		trampoline_fixup_ip;
extern uint8_t		trampoline_fixup_target;
extern uint8_t		cpu_boot_page_tables_start;
extern uint8_t		cpu_boot_page_tables_ptr;
extern uint8_t		trampoline_pdpt_addr;
extern uint8_t		trampoline_gdt_ptr;
extern uint8_t		trampoline_start64_fixup;
#endif /* TRAMPOLINE_H */
