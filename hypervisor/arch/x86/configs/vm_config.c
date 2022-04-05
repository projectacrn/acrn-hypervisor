/*
 * Copyright (C) 2018 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/vm_config.h>
#include <util.h>
#include <rtl.h>

/*
 * @pre vm_id < CONFIG_MAX_VM_NUM
 * @post return != NULL
 */
struct acrn_vm_config *get_vm_config(uint16_t vm_id)
{
	return &vm_configs[vm_id];
}

/*
 * @pre vm_id < CONFIG_MAX_VM_NUM
 */
uint8_t get_vm_severity(uint16_t vm_id)
{
	return vm_configs[vm_id].severity;
}

/**
 * return true if the input vm-name is configured in VM
 *
 * @pre vmid < CONFIG_MAX_VM_NUM
 */
bool vm_has_matched_name(uint16_t vmid, const char *name)
{
	struct acrn_vm_config *vm_config = get_vm_config(vmid);

	return (strncmp(vm_config->name, name, MAX_VM_NAME_LEN) == 0);
}
