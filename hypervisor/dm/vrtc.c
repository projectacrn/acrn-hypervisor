/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <x86/guest/vm.h>
#include <x86/io.h>

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
	int32_t tries = 2000;

	spinlock_obtain(&cmos_lock);

	/* Make sure an update isn't in progress */
	while (cmos_update_in_progress() && (tries != 0)) {
		tries -= 1;
	}

	reg = cmos_read(addr);

	spinlock_release(&cmos_lock);
	return reg;
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool vrtc_read(struct acrn_vcpu *vcpu, uint16_t addr, __unused size_t width)
{
	uint8_t offset;
	struct pio_request *pio_req = &vcpu->req.reqs.pio;
	struct acrn_vm *vm = vcpu->vm;

	offset = vm->vrtc_offset;

	if (addr == CMOS_ADDR_PORT) {
		pio_req->value = vm->vrtc_offset;
	} else {
		pio_req->value = cmos_get_reg_val(offset);
	}

	return true;
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool vrtc_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t width,
			uint32_t value)
{
	if ((width == 1U) && (addr == CMOS_ADDR_PORT)) {
		vcpu->vm->vrtc_offset = (uint8_t)value & 0x7FU;
	}

	return true;
}

void vrtc_init(struct acrn_vm *vm)
{
	struct vm_io_range range = {
	.base = CMOS_ADDR_PORT, .len = 2U};

	/* Initializing the CMOS RAM offset to 0U */
	vm->vrtc_offset = 0U;

	register_pio_emulation_handler(vm, RTC_PIO_IDX, &range, vrtc_read, vrtc_write);
}
