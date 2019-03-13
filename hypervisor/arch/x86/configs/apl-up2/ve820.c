/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>

/**
 * @pre vm != NULL
 */
void create_prelaunched_vm_e820(struct acrn_vm *vm)
{
	vm->e820_entry_num = 0U;
	vm->e820_entries = NULL;
}
