/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
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

uint16_t get_unused_vmid(void)
{
	uint16_t vm_id;
	struct acrn_vm_config *vm_config;

	for (vm_id = 0; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);
		if (vm_config->name[0] == '\0') {
			break;
		}
	}
	return (vm_id < CONFIG_MAX_VM_NUM) ? (vm_id) : (ACRN_INVALID_VMID);
}
