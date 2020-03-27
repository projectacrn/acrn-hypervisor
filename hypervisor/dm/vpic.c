/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>


static void vpic_register_io_handler(struct acrn_vm *vm)
{
	struct vm_io_range master_range = {
		.base = 0x20U,
		.len = 2U
	};
	struct vm_io_range slave_range = {
		.base = 0xa0U,
		.len = 2U
	};
	struct vm_io_range elcr_range = {
		.base = 0x4d0U,
		.len = 2U
	};

	register_pio_emulation_handler(vm, PIC_MASTER_PIO_IDX, &master_range,
			pio_default_read, pio_default_write);
	register_pio_emulation_handler(vm, PIC_SLAVE_PIO_IDX, &slave_range,
			pio_default_read, pio_default_write);
	register_pio_emulation_handler(vm, PIC_ELC_PIO_IDX, &elcr_range,
			pio_default_read, pio_default_write);
}

void vpic_init(struct acrn_vm *vm)
{
	vpic_register_io_handler(vm);
}
