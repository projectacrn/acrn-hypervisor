/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <io.h>
#include <logmsg.h>
#include <per_cpu.h>
#include <vm_reset.h>

/**
 * @pre vcpu != NULL && vm != NULL
 */
static bool handle_reset_reg_read(struct acrn_vm *vm, struct acrn_vcpu *vcpu, __unused uint16_t addr,
		__unused size_t bytes)
{
	bool ret = true;

	if (is_postlaunched_vm(vm)) {
		/* re-inject to DM */
		ret = false;
	} else {
		/*
		 * - keyboard control/status register 0x64: ACRN doesn't expose kbd controller to the guest.
		 * - reset control register 0xcf9: hide this from guests for now.
		 */
		vcpu->req.reqs.pio.value = ~0U;
	}

	return ret;
}

/**
 * @pre vm != NULL
 */
static bool handle_common_reset_reg_write(struct acrn_vm *vm, bool reset)
{
	bool ret = true;

	if (is_postlaunched_vm(vm)) {
		/* re-inject to DM */
		ret = false;

		if (reset && is_rt_vm(vm)) {
			vm->state = VM_POWERING_OFF;
		}
	} else {
		/*
		 * ignore writes from SOS or pre-launched VMs.
		 * equivalent to hide this port from guests.
		 */
	}

	return ret;
}

/**
 * @pre vm != NULL
 */
static bool handle_kb_write(struct acrn_vm *vm, __unused uint16_t addr, size_t bytes, uint32_t val)
{
	/* ignore commands other than system reset */
	return handle_common_reset_reg_write(vm, ((bytes == 1U) && (val == 0xfeU)));
}

/*
 * Reset Control register at I/O port 0xcf9.
 *     Bit 1 - 0: "soft" reset. Force processor begin execution at power-on reset vector.
 *             1: "hard" reset. e.g. assert PLTRST# (if implemented) to do a host reset.
 *     Bit 2 - initiates a system reset when it transitions from 0 to 1.
 *     Bit 3 - 1: full reset (aka code reset), SLP_S3#/4#/5# or similar pins are asserted for full power cycle.
 *             0: will be dropped if system in S3/S4/S5.
 */
/**
 * @pre vm != NULL
 */
static bool handle_cf9_write(struct acrn_vm *vm, __unused uint16_t addr, size_t bytes, uint32_t val)
{
	/* We don't differentiate among hard/soft/warm/cold reset */
	return handle_common_reset_reg_write(vm, ((bytes == 1U) && ((val & 0x4U) == 0x4U) && ((val & 0xaU) != 0U)));
}

/**
 * @pre vm != NULL
 */
void register_reset_port_handler(struct acrn_vm *vm)
{
	/* Don't support SOS and pre-launched VM re-launch for now. */
	if (!is_postlaunched_vm(vm) || is_rt_vm(vm)) {
		struct vm_io_range io_range = {
			.flags = IO_ATTR_RW,
			.len = 1U
		};

		io_range.base = 0x64U;
		register_pio_emulation_handler(vm, KB_PIO_IDX, &io_range, handle_reset_reg_read, handle_kb_write);

		io_range.base = 0xcf9U;
		register_pio_emulation_handler(vm, CF9_PIO_IDX, &io_range, handle_reset_reg_read, handle_cf9_write);
	}
}

void shutdown_vm_from_idle(uint16_t pcpu_id)
{
	struct acrn_vm *vm = get_vm_from_vmid(per_cpu(shutdown_vm_id, pcpu_id));
	const struct acrn_vcpu *vcpu = vcpu_from_vid(vm, BOOT_CPU_ID);

	if (vcpu->pcpu_id == pcpu_id) {
		(void)shutdown_vm(vm);
	}
}
