/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/************************************************************************
 *
 *   FILE NAME
 *
 *       hypervisor.h
 *
 *   DESCRIPTION
 *
 *       This file includes config header file "bsp_cfg.h" and other
 *       hypervisor used header files.
 *       It should be included in all the source files.
 *
 *
 ************************************************************************/
#ifndef HYPERVISOR_H
#define HYPERVISOR_H

/* Include config header file containing config options */
#include <types.h>
#include "bsp_cfg.h"
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
#endif	/* !ASSEMBLER */

#endif /* HYPERVISOR_H */
