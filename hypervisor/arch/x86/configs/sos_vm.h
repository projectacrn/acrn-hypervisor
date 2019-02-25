/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This is a template of sos_vm.h and is linked to ./$(CONFIG_BOARD)/sos_vm.h,
 * If you need a board specific sos_vm.h, make a copy in ./$(CONFIG_BOARD)/
 * and replace the original symbol link.
 */

#ifndef SOS_VM_CONFIG_H
#define SOS_VM_CONFIG_H

#define SOS_VM_CONFIG_NAME		"ACRN SOS VM"
#define SOS_VM_CONFIG_GUEST_FLAGS	IO_COMPLETION_POLLING

#define SOS_VM_CONFIG_OS_NAME		"ACRN Service OS"

#endif /* SOS_VM_CONFIG_H */
