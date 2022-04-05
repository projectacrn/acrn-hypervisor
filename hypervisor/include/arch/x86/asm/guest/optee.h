/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEE_H_
#define TEE_H_

#include <asm/guest/vm.h>
#include <asm/vm_config.h>
#include <ptdev.h>

#define TEE_FIXED_NONSECURE_VECTOR 0x29U

/* If the RDI equals to this value, then this is a RETURN after FIQ DONE */
#define OPTEE_RETURN_FIQ_DONE	0xBE000006UL

/* This value tells OPTEE that this switch to TEE is due to secure interrupt */
#define OPTEE_FIQ_ENTRY	0xB20000FFUL

int is_tee_vm(struct acrn_vm *vm);
int is_ree_vm(struct acrn_vm *vm);
void prepare_tee_vm_memmap(struct acrn_vm *vm, const struct acrn_vm_config *vm_config);
void handle_x86_tee_int(struct ptirq_remapping_info *entry, uint16_t pcpu_id);

#endif /* TEE_H_ */
