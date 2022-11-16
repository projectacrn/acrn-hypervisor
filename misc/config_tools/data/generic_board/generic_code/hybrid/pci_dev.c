/*
 * Copyright (C) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/vm_config.h>
#include <vpci.h>
#include <asm/mmu.h>
#include <asm/page.h>
#include <vmcs9900.h>
#include <ivshmem_cfg.h>
#define INVALID_PCI_BASE 0U
struct acrn_vm_pci_dev_config sos_pci_devs[CONFIG_MAX_PCI_DEV_NUM] = {};
