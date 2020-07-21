/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <io.h>
#include <host_pm.h>
#include <logmsg.h>
#include <per_cpu.h>
#include <vm_reset.h>

/**
 * @pre vm != NULL
 */
void triple_fault_shutdown_vm(struct acrn_vcpu *vcpu)
{
	struct acrn_vm *vm = vcpu->vm;

	if (is_postlaunched_vm(vm)) {
		struct io_request *io_req = &vcpu->req;

		/* Device model emulates PM1A for post-launched VMs */
		io_req->io_type = REQ_PORTIO;
		io_req->reqs.pio.direction = REQUEST_WRITE;
		io_req->reqs.pio.address = VIRTUAL_PM1A_CNT_ADDR;
		io_req->reqs.pio.size = 2UL;
		io_req->reqs.pio.value = (VIRTUAL_PM1A_SLP_EN | (5U << 10U));

		/* Inject pm1a S5 request to SOS to shut down the guest */
		(void)emulate_io(vcpu, io_req);
	} else {
		if (is_sos_vm(vm)) {
			uint16_t vm_id;

			/* Shut down all non real time post-launched VMs */
			for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
				struct acrn_vm *pl_vm = get_vm_from_vmid(vm_id);

				get_vm_lock(pl_vm);
				if (!is_poweroff_vm(pl_vm) && is_postlaunched_vm(pl_vm) && !is_rt_vm(pl_vm)) {
					pause_vm(pl_vm);
					(void)shutdown_vm(pl_vm);
				}
				put_vm_lock(pl_vm);
			}
		}

		/* Either SOS or pre-launched VMs */
		get_vm_lock(vm);
		pause_vm(vm);
		put_vm_lock(vm);

		per_cpu(shutdown_vm_id, pcpuid_from_vcpu(vcpu)) = vm->vm_id;
		make_shutdown_vm_request(pcpuid_from_vcpu(vcpu));
	}
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool handle_reset_reg_read(struct acrn_vcpu *vcpu, __unused uint16_t addr,
		__unused size_t bytes)
{
	bool ret = true;

	if (is_postlaunched_vm(vcpu->vm)) {
		/* re-inject to DM */
		ret = false;
	} else {
		/*
		 * - reset control register 0xcf9: hide this from guests for now.
		 * - FADT reset register: the read behavior is not defined in spec, keep it simple to return all '1'.
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

	get_vm_lock(vm);
	if (reset) {
		if (get_highest_severity_vm(true) == vm) {
			reset_host();
		} else if (is_postlaunched_vm(vm)) {
			/* re-inject to DM */
			ret = false;

			if (is_rt_vm(vm)) {
				vm->state = VM_READY_TO_POWEROFF;
			}
		} else {
			/*
			 * If it's SOS reset while RTVM is still alive
			 *    or pre-launched VM reset,
			 * ACRN doesn't support re-launch, just shutdown the guest.
			 */
			const struct acrn_vcpu *bsp = vcpu_from_vid(vm, BSP_CPU_ID);

			pause_vm(vm);
			per_cpu(shutdown_vm_id, pcpuid_from_vcpu(bsp)) = vm->vm_id;
			make_shutdown_vm_request(pcpuid_from_vcpu(bsp));
		}
	} else {
		if (is_postlaunched_vm(vm)) {
			/* If post-launched VM write none reset value, re-inject to DM */
			ret = false;
		}
		/*
                 * ignore writes from SOS and pre-launched VM.
                 * equivalent to hide this port from guests.
                 */
	}
	put_vm_lock(vm);

	return ret;
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool handle_kb_write(struct acrn_vcpu *vcpu, __unused uint16_t addr, size_t bytes, uint32_t val)
{
	/* ignore commands other than system reset */
	return handle_common_reset_reg_write(vcpu->vm, ((bytes == 1U) && (val == 0xfeU)));
}

static bool handle_kb_read(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes)
{
	if (is_sos_vm(vcpu->vm) && (bytes == 1U)) {
		/* In case i8042 is defined as ACPI PNP device in BIOS, HV need expose physical 0x64 port. */
		vcpu->req.reqs.pio.value = pio_read8(addr);
	} else {
		/* ACRN will not expose kbd controller to the guest in this case. */
		vcpu->req.reqs.pio.value = ~0U;
	}
	return true;
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
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool handle_cf9_write(struct acrn_vcpu *vcpu, __unused uint16_t addr, size_t bytes, uint32_t val)
{
	/* We don't differentiate among hard/soft/warm/cold reset */
	return handle_common_reset_reg_write(vcpu->vm,
			((bytes == 1U) && ((val & 0x4U) == 0x4U) && ((val & 0xaU) != 0U)));
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool handle_reset_reg_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes, uint32_t val)
{
	bool ret = true;

	if (bytes == 1U) {
		struct acpi_reset_reg *reset_reg = get_host_reset_reg_data();

		if (val == reset_reg->val) {
			ret = handle_common_reset_reg_write(vcpu->vm, true);
		} else {
			/*
			 * ACPI defines the reset value but doesn't specify the meaning of other values.
			 * in the case the reset register (e.g. PIO 0xB2) has other purpose other than reset,
			 * we can't ignore the write with other values.
			 */
			pio_write8((uint8_t)val, addr);
		}
	}

	return ret;
}

/**
 * @pre vm != NULL
 */
void register_reset_port_handler(struct acrn_vm *vm)
{
	/* Don't support SOS and pre-launched VM re-launch for now. */
	if (!is_postlaunched_vm(vm) || is_rt_vm(vm)) {
		struct acpi_reset_reg *reset_reg = get_host_reset_reg_data();
		struct acpi_generic_address *gas = &(reset_reg->reg);

		struct vm_io_range io_range = {
			.len = 1U
		};

		io_range.base = 0x64U;
		register_pio_emulation_handler(vm, KB_PIO_IDX, &io_range, handle_kb_read, handle_kb_write);

		io_range.base = 0xcf9U;
		register_pio_emulation_handler(vm, CF9_PIO_IDX, &io_range, handle_reset_reg_read, handle_cf9_write);

		/*
		 * - pre-launched VMs don't support ACPI;
		 * - ACPI reset register is fixed at 0xcf9 for post-launched VMs;
		 * - here is taking care of SOS only:
		 *   Don't support MMIO or PCI based reset register for now.
		 *   ACPI Spec: Register_Bit_Width must be 8 and Register_Bit_Offset must be 0.
		 */
		if (is_sos_vm(vm) &&
			(gas->space_id == SPACE_SYSTEM_IO) &&
			(gas->bit_width == 8U) && (gas->bit_offset == 0U) &&
			(gas->address != 0xcf9U) && (gas->address != 0x64U)) {

			io_range.base = (uint16_t)reset_reg->reg.address;
			register_pio_emulation_handler(vm, PIO_RESET_REG_IDX, &io_range,
					handle_reset_reg_read, handle_reset_reg_write);
		}
	}
}

void shutdown_vm_from_idle(uint16_t pcpu_id)
{
	struct acrn_vm *vm = get_vm_from_vmid(per_cpu(shutdown_vm_id, pcpu_id));

	(void)shutdown_vm(vm);
}
