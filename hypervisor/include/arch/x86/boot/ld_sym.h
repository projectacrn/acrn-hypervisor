/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LD_SYM_H
#define LD_SYM_H

extern uint8_t		ld_text_end;
extern uint8_t		ld_bss_start;
extern uint8_t		ld_bss_end;
extern const uint8_t	ld_trampoline_load;
extern uint8_t		ld_trampoline_start;
extern uint8_t		ld_trampoline_end;

#endif /* LD_SYM_H */
