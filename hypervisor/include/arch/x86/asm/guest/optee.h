/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEE_H_
#define TEE_H_

#include <asm/guest/vm.h>
#include <asm/vm_config.h>

/* If the RDI equals to this value, then this is a RETURN after FIQ DONE */
#define OPTEE_RETURN_FIQ_DONE	0xBE000006UL

void prepare_tee_vm_memmap(struct acrn_vm *vm, const struct acrn_vm_config *vm_config);

#endif /* TEE_H_ */
