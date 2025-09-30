/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LD_SYM_H
#define LD_SYM_H

extern const char	_text_start;
extern const char	_text_end;

extern const char	_rodata_start;
extern const char	_rodata_end;

extern char		_data_start;
extern char		_data_end;

extern char		_bss_start;
extern char		_bss_end;

extern char		ld_ram_start;
extern char		ld_ram_end;

#endif /* LD_SYM_H */
