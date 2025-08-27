/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <asm/guest/vm_reset.h>
#include <asm/guest/vmcs.h>
#include <asm/guest/vmexit.h>
#include <asm/guest/virq.h>
#include <schedule.h>
#include <profiling.h>
#include <sprintf.h>
#include <trace.h>
#include <logmsg.h>
#include <per_cpu.h>
