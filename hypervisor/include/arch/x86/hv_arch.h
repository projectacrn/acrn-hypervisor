/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef HV_ARCH_H
#define HV_ARCH_H

#include <cpu.h>
#include <gdt.h>
#include <idt.h>
#include <apicreg.h>
#include <ioapic.h>
#include <lapic.h>
#include <msr.h>
#include <io.h>
#include <ioreq.h>
#include <mtrr.h>
#include <vcpu.h>
#include <trusty.h>
#include <guest_pm.h>
#include <host_pm.h>
#include <vm.h>
#include <cpuid.h>
#include <mmu.h>
#include <pgtable_types.h>
#include <pgtable.h>
#include <irq.h>
#include <timer.h>
#include <softirq.h>
#include <vmx.h>
#include <assign.h>
#include <vtd.h>

#include <vpic.h>
#include <vlapic.h>
#include <vioapic.h>
#include <guest.h>
#include <vmexit.h>
#include <cpufeatures.h>

#endif /* HV_ARCH_H */
