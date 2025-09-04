/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#ifndef RISCV_VM_CONFIG_H_
#define RISCV_VM_CONFIG_H_
#include <board_info.h>

#define MAX_VCPUS_PER_VM  MAX_PCPU_NUM
#define CONFIG_MAX_VM_NUM 16U

#define DM_OWNED_GUEST_FLAG_MASK    0UL

struct arch_vm_config {

};

#endif /* VM_CONFIG_H_ */
