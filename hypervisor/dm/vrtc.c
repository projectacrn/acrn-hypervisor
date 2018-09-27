/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#define CMOS_ADDR_PORT		0x70U
#define CMOS_DATA_PORT		0x71U

#define RTC_STATUSA		0x0AU   /* status register A */
#define RTCSA_TUP		0x80U   /* time update, don't look now */

static spinlock_t cmos_lock = { .head = 0U, .tail = 0U };

static uint8_t cmos_read(uint8_t addr)
{
	pio_write8(addr, CMOS_ADDR_PORT);
	return pio_read8(CMOS_DATA_PORT);
}

static bool cmos_update_in_progress(void)
{
	return (cmos_read(RTC_STATUSA) & RTCSA_TUP)?1:0;
}

static uint8_t cmos_get_reg_val(uint8_t addr)
{
	uint8_t reg;
	int tries = 2000U;

	spinlock_obtain(&cmos_lock);

	/* Make sure an update isn't in progress */
	while (cmos_update_in_progress() && tries--)
	;

	reg = cmos_read(addr);

	spinlock_release(&cmos_lock);
	return reg;
}

static uint32_t vrtc_read(struct vm *vm, uint16_t addr, __unused size_t width)
{
	uint8_t reg;
	uint8_t offset;

	offset = vm->vrtc_offset;

	if (addr == CMOS_ADDR_PORT) {
		return vm->vrtc_offset;
	}

	reg = cmos_get_reg_val(offset);
	return reg;
}

static void vrtc_write(struct vm *vm, uint16_t addr, size_t width,
			uint32_t value)
{

	if (width != 1U)
		return;

	if (addr == CMOS_ADDR_PORT) {
		vm->vrtc_offset = value & 0x7FU;
	}
}

void vrtc_init(struct vm *vm)
{
	struct vm_io_range range = {
	.flags = IO_ATTR_RW, .base = CMOS_ADDR_PORT, .len = 2U};

	/* Initializing the CMOS RAM offset to 0U */
	vm->vrtc_offset = 0U;

	register_io_emulation_handler(vm, &range, vrtc_read, vrtc_write);
}
