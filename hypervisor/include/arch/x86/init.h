/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef	INIT_H

/* hypervisor stack bottom magic('intl') */
#define SP_BOTTOM_MAGIC    0x696e746cUL

void bsp_boot_init(void);
void init_secondary_cpu(void);

#endif /* INIT_H*/
