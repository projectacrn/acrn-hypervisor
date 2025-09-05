/*
 * Copyright (C) 2019-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/guest/vm.h>
#include <asm/io.h>
#include <asm/host_pm.h>
#include <logmsg.h>
#include <per_cpu.h>
#include <asm/guest/vm_reset.h>
#include <vm_event.h>

/**
 * @pre vm != NULL
 */
void triple_fault_shutdown_vm(struct acrn_vcpu *vcpu)
{
	struct acrn_vm *vm = vcpu->vm;
	struct vm_event trp_event;

	if (is_postlaunched_vm(vm)) {
		struct io_request *io_req = &vcpu->req;

		/* Device model emulates PM1A for post-launched VMs */
		io_req->io_type = ACRN_IOREQ_TYPE_PORTIO;
		io_req->reqs.pio_request.direction = ACRN_IOREQ_DIR_WRITE;
		io_req->reqs.pio_request.address = VIRTUAL_PM1A_CNT_ADDR;
		io_req->reqs.pio_request.size = 2UL;
		io_req->reqs.pio_request.value = (VIRTUAL_PM1A_SLP_EN | (5U << 10U));

		/* Send the tripple fault event to DM. */
		trp_event.type = VM_EVENT_TRIPLE_FAULT;
		(void)send_vm_event(vcpu->vm, &trp_event);

		/* Inject pm1a S5 request to Service VM to shut down the guest */
		(void)emulate_io(vcpu, io_req);
	} else {
		if (is_service_vm(vm)) {
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

		/* Either Service VM or pre-launched VMs */
		get_vm_lock(vm);
		poweroff_if_rt_vm(vm);
		pause_vm(vm);
		put_vm_lock(vm);

		bitmap_set_non_atomic(vm->vm_id,
				&per_cpu(shutdown_vm_bitmap, pcpuid_from_vcpu(vcpu)));
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
	struct acrn_vm *vm = vcpu->vm;

	if (is_postlaunched_vm(vm)) {
		/* re-inject to DM */
		ret = false;
	} else {
		vcpu->req.reqs.pio_request.value = vm->reset_control;
	}

	return ret;
}

/**
 * @pre vm != NULL
 */
static bool handle_common_reset_reg_write(struct acrn_vcpu *vcpu, bool reset, bool warm)
{
	struct acrn_vm *vm = vcpu->vm;
	bool ret = true;

	get_vm_lock(vm);
	if (reset) {
		poweroff_if_rt_vm(vm);

		if (get_highest_severity_vm(true) == vm) {
			reset_host(warm);
		} else if (is_postlaunched_vm(vm)) {
			/* re-inject to DM */
			ret = false;
		} else {
			/*
			 * If it's Service VM reset while RTVM is still alive
			 *    or pre-launched VM reset,
			 * ACRN doesn't support re-launch, just shutdown the guest.
			 */
			pause_vm(vm);
			bitmap_set_non_atomic(vm->vm_id,
					&per_cpu(shutdown_vm_bitmap, pcpuid_from_vcpu(vcpu)));
			make_shutdown_vm_request(pcpuid_from_vcpu(vcpu));
		}
	} else {
		if (is_postlaunched_vm(vm)) {
			/* If post-launched VM write none reset value, re-inject to DM */
			ret = false;
		}
		/*
		 * Ignore writes from Service VM and pre-launched VM.
		 * Equivalent to hiding this port from the guest.
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
	return handle_common_reset_reg_write(vcpu, ((bytes == 1U) && (val == 0xfeU)), false);
}

static bool handle_kb_read(struct acrn_vcpu *vcpu, uint16_t addr, size_t bytes)
{
	if (is_service_vm(vcpu->vm) && (bytes == 1U)) {
		/* In case i8042 is defined as ACPI PNP device in BIOS, HV need expose physical 0x64 port. */
		vcpu->req.reqs.pio_request.value = pio_read8(addr);
	} else {
		/* ACRN will not expose kbd controller to the guest in this case. */
		vcpu->req.reqs.pio_request.value = ~0U;
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
	struct acrn_vm *vm = vcpu->vm;

	vm->reset_control = val & 0xeU;
	return handle_common_reset_reg_write(vcpu,
			((bytes == 1U) && ((val & 0x4U) == 0x4U) && ((val & 0xaU) != 0U)),
			((val & 0x8U) == 0U));
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
			ret = handle_common_reset_reg_write(vcpu, true, false);
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
	/* Don't support Service VM and pre-launched VM re-launch for now. */
	if (!is_postlaunched_vm(vm) || is_rt_vm(vm)) {
		struct acpi_reset_reg *reset_reg = get_host_reset_reg_data();
		struct acrn_acpi_generic_address *gas = &(reset_reg->reg);

		struct vm_io_range io_range = {
			.len = 1U
		};

		io_range.base = 0x64U;
		register_pio_emulation_handler(vm, KB_PIO_IDX, &io_range, handle_kb_read, handle_kb_write);

		/* ACPI reset register is fixed at 0xcf9 for post-launched and pre-launched VMs */
		io_range.base = 0xcf9U;
		register_pio_emulation_handler(vm, CF9_PIO_IDX, &io_range, handle_reset_reg_read, handle_cf9_write);

		/*
		 * - here is taking care of Service VM only:
		 *   Don't support MMIO or PCI based reset register for now.
		 *   ACPI Spec: Register_Bit_Width must be 8 and Register_Bit_Offset must be 0.
		 */
		if (is_service_vm(vm) &&
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
	uint16_t vm_id;
	uint64_t *vms = &per_cpu(shutdown_vm_bitmap, pcpu_id);
	struct acrn_vm *vm;

	for (vm_id = fls64(*vms); vm_id < CONFIG_MAX_VM_NUM; vm_id = fls64(*vms)) {
		vm = get_vm_from_vmid(vm_id);
		get_vm_lock(vm);
		if (is_paused_vm(vm)) {
			(void)shutdown_vm(vm);
		}
		put_vm_lock(vm);
		bitmap_clear_non_atomic(vm_id, vms);
	}
}
