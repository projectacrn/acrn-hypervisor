/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/************************************************************************
 *
 *   FILE NAME
 *
 *       bsp_extern.h
 *
 *   DESCRIPTION
 *
 *       This file defines the generic BSP interface
 *
 ************************************************************************/
#ifndef BSP_EXTERN_H
#define BSP_EXTERN_H

#include "default_acpi_info.h"
#include "platform_acpi_info.h"

#define UOS_DEFAULT_START_ADDR   (0x100000000UL)

/* BSP Interfaces */
void init_bsp(void);
uint64_t bsp_get_ap_trampoline(void);
void *bsp_get_rsdp(void);
void bsp_init_irq(void);

#ifndef CONFIG_CONSTANT_ACPI
void acpi_fixup(void);
#endif

#endif /* BSP_EXTERN_H */
