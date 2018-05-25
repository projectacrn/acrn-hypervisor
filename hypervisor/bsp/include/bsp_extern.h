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

#define UOS_DEFAULT_START_ADDR   (0x100000000)
/**********************************/
/* EXTERNAL VARIABLES             */
/**********************************/
extern struct vm_description vm0_desc;

/* BSP Interfaces */
void init_bsp(void);

#endif /* BSP_EXTERN_H */
