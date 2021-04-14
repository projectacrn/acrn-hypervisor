/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <x86/vm_config.h>
#include <pci_devices.h>
#include <vpci.h>
#include <vbar_base.h>
#include <x86/mmu.h>
#include <x86/page.h>

struct acrn_vm_pci_dev_config sos_pci_devs[CONFIG_MAX_PCI_DEV_NUM];
