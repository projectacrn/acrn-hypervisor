/*
 * Copyright (C) 2019-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <pci.h>

void uart16550_init(__unused bool early_boot) {}

bool is_pci_dbg_uart(__unused union pci_bdf bdf_value) { return false; }

bool get_pio_dbg_uart_cfg(__unused uint16_t *pio_address, __unused uint32_t *nbytes) {
	return false;
}
