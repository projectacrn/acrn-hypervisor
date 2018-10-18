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

struct acpi_info {
	uint8_t			x86_family;
	uint8_t			x86_model;
	struct pm_s_state_data	pm_s_state;
	/* TODO: we can add more acpi info field here if needed. */
};

/**********************************/
/* EXTERNAL VARIABLES             */
/**********************************/
/* BSP Interfaces */
void init_bsp(void);

#ifndef CONFIG_CONSTANT_ACPI
void acpi_fixup(void);
#endif

#endif /* BSP_EXTERN_H */
