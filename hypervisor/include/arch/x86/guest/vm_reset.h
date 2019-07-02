/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_RESET_H_
#define VM_RESET_H_

#include <acrn_common.h>

struct acpi_reset_reg {
	struct acpi_generic_address reg;
	uint8_t val;
};

void register_reset_port_handler(struct acrn_vm *vm);
void shutdown_vm_from_idle(uint16_t pcpu_id);
void triple_fault_shutdown_vm(struct acrn_vcpu *vcpu);
struct acpi_reset_reg *get_host_reset_reg_data(void);

#endif /* VM_RESET_H_ */
