/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SECURITY_VM_FIXUP_H_
#define _SECURITY_VM_FIXUP_H_

void passthrough_smbios(struct acrn_vm *vm, struct acrn_boot_info *abi);
void security_vm_fixup(uint16_t vm_id);

#endif /* _SECURITY_VM_FIXUP_H_ */
