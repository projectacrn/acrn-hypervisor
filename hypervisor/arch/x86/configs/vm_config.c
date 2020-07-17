/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm_config.h>

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

bool uuid_is_equal(const uint8_t *uuid1, const uint8_t *uuid2)
{
	uint64_t uuid1_h = *(const uint64_t *)uuid1;
	uint64_t uuid1_l = *(const uint64_t *)(uuid1 + 8);
	uint64_t uuid2_h = *(const uint64_t *)uuid2;
	uint64_t uuid2_l = *(const uint64_t *)(uuid2 + 8);

	return ((uuid1_h == uuid2_h) && (uuid1_l == uuid2_l));
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
