/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEE_H_
#define TEE_H_

#include <asm/guest/vm.h>
#include <asm/vm_config.h>

void prepare_tee_vm_memmap(struct acrn_vm *vm, const struct acrn_vm_config *vm_config);

#endif /* TEE_H_ */
