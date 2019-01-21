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
#include <errno.h>
#include <rtl.h>
#include <spinlock.h>
#include <mem_mgt.h>
#include <util.h>
#include <list.h>
#include <atomic.h>
#include <bits.h>
#include <sprintf.h>
#include "acrn_common.h"
#include <acrn_hv_defs.h>
#include <hv_arch.h>
#include <hv_debug.h>

#ifndef ASSEMBLER
/* gpa --> hpa -->hva */
static inline void *gpa2hva(struct acrn_vm *vm, uint64_t x)
{
	return hpa2hva(gpa2hpa(vm, x));
}

static inline uint64_t hva2gpa(struct acrn_vm *vm, void *x)
{
	return (is_sos_vm(vm)) ? sos_vm_hpa2gpa(hva2hpa(x)) : INVALID_GPA;
}

#endif	/* !ASSEMBLER */

#endif /* HYPERVISOR_H */
