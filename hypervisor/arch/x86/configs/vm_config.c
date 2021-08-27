/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/vm_config.h>
#include <util.h>

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
 * return true if the input uuid is configured in VM
 *
 * @pre vmid < CONFIG_MAX_VM_NUM
 */
bool vm_has_matched_uuid(uint16_t vmid, const uint8_t *uuid)
{
	struct acrn_vm_config *vm_config = get_vm_config(vmid);

	return (uuid_is_equal(vm_config->uuid, uuid));
}
