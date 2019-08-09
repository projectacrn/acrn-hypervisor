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
#include <default_acpi_info.h>
#include <platform_acpi_info.h>

/* host reset register defined in ACPI */
static struct acpi_reset_reg host_reset_reg = {
	.reg = {
		.space_id = RESET_REGISTER_SPACE_ID,
		.bit_width = RESET_REGISTER_BIT_WIDTH,
		.bit_offset = RESET_REGISTER_BIT_OFFSET,
		.access_size = RESET_REGISTER_ACCESS_SIZE,
		.address = RESET_REGISTER_ADDRESS,
	},
	.val = RESET_REGISTER_VALUE
};

struct acpi_reset_reg *get_host_reset_reg_data(void)
{
	return &host_reset_reg;
}

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
		io_req->reqs.pio.size = 2ULL;
		io_req->reqs.pio.value = (VIRTUAL_PM1A_SLP_EN | (5U << 10U));

		/* Inject pm1a S5 request to SOS to shut down the guest */
		(void)emulate_io(vcpu, io_req);
	} else {
		if (is_sos_vm(vm)) {
			uint16_t vm_id;

			/* Shut down all non real time post-launched VMs */
			for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
				struct acrn_vm *pl_vm = get_vm_from_vmid(vm_id);

				if (!is_poweroff_vm(pl_vm) && is_postlaunched_vm(pl_vm) && !is_rt_vm(pl_vm)) {
					(void)shutdown_vm(pl_vm);
				}
			}
		}

		/* Either SOS or pre-launched VMs */
		pause_vm(vm);

		per_cpu(shutdown_vm_id, vcpu->pcpu_id) = vm->vm_id;
		make_shutdown_vm_request(vcpu->pcpu_id);
	}
}

static void reset_host(void)
{
	struct acpi_generic_address *gas = &(host_reset_reg.reg);


	/* TODO: gracefully shut down all guests before doing host reset. */

	/*
	 * UEFI more likely sets the reset value as 0x6 (not 0xe) for 0xcf9 port.
	 * This asserts PLTRST# to reset devices on the platform, but not the
	 * SLP_S3#/4#/5# signals, which power down the systems. This might not be
	 * enough for us.
	 */
	if ((gas->space_id == SPACE_SYSTEM_IO) &&
		(gas->bit_width == 8U) && (gas->bit_offset == 0U) &&
		(gas->address != 0U) && (gas->address != 0xcf9U)) {
		pio_write8(host_reset_reg.val, (uint16_t)host_reset_reg.reg.address);
	}

	/*
	 * Fall back
	 * making sure bit 2 (RST_CPU) is '0', when the reset command is issued.
	 */
	pio_write8(0x2U, 0xcf9U);
	pio_write8(0xeU, 0xcf9U);

	/*
	 * Fall back
	 * keyboard controller might cause the INIT# being asserted,
	 * and not power cycle the system.
	 */
	pio_write8(0xfeU, 0x64U);

	pr_fatal("%s(): can't reset host.", __func__);
	while (1) {
		asm_pause();
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
		 * - keyboard control/status register 0x64: ACRN doesn't expose kbd controller to the guest.
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

	if (is_highest_severity_vm(vm)) {
		if (reset) {
			reset_host();
		}
	} else if (is_postlaunched_vm(vm)) {
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
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool handle_kb_write(struct acrn_vcpu *vcpu, __unused uint16_t addr, size_t bytes, uint32_t val)
{
	/* ignore commands other than system reset */
	return handle_common_reset_reg_write(vcpu->vm, ((bytes == 1U) && (val == 0xfeU)));
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
	if (bytes == 1U) {
		if (val == host_reset_reg.val) {
			if (is_highest_severity_vm(vcpu->vm)) {
				reset_host();
			} else {
				/* ignore reset request */
			}
		} else {
			/*
			 * ACPI defines the reset value but doesn't specify the meaning of other values.
			 * in the case the reset register (e.g. PIO 0xB2) has other purpose other than reset,
			 * we can't ignore the write with other values.
			 */
			pio_write8((uint8_t)val, addr);
		}
	}

	return true;
}

/**
 * @pre vm != NULL
 */
void register_reset_port_handler(struct acrn_vm *vm)
{
	/* Don't support SOS and pre-launched VM re-launch for now. */
	if (!is_postlaunched_vm(vm) || is_rt_vm(vm)) {
		struct acpi_generic_address *gas = &(host_reset_reg.reg);

		struct vm_io_range io_range = {
			.flags = IO_ATTR_RW,
			.len = 1U
		};

		io_range.base = 0x64U;
		register_pio_emulation_handler(vm, KB_PIO_IDX, &io_range, handle_reset_reg_read, handle_kb_write);

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

			io_range.base = (uint16_t)host_reset_reg.reg.address;
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
