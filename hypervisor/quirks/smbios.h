/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef QUIRKS_SMBIOS_H
#define QUIRKS_SMBIOS_H

void try_smbios_passthrough(struct acrn_vm *vm, struct acrn_boot_info *abi);

#endif
