/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/************************************************************************
 *
 *   FILE NAME
 *
 *       hypervisor.h
 *
 *   DESCRIPTION
 *
 *       This file includes hypervisor used header files.
 *       It should be included in all the source files.
 *
 *
 ************************************************************************/
#ifndef HYPERVISOR_H
#define HYPERVISOR_H

/* Include config header file containing config options */
#include <types.h>
#include "acrn_common.h"
#include <acrn_hv_defs.h>
#include <hv_lib.h>
#include <hv_arch.h>
#include <hv_debug.h>

#ifndef ASSEMBLER
/* hpa <--> hva, now it is 1:1 mapping */
#define HPA2HVA(x) ((void *)(x))
#define HVA2HPA(x) ((uint64_t)(x))
/* gpa --> hpa -->hva */
#define GPA2HVA(vm, x) HPA2HVA(gpa2hpa(vm, x))
#define HVA2GPA(vm, x) hpa2gpa(vm, HVA2HPA(x))
#endif	/* !ASSEMBLER */

#endif /* HYPERVISOR_H */
