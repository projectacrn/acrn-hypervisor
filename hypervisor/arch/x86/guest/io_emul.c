/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <atomic.h>
#include <io_req.h>
#include <vcpu.h>
#include <vm.h>
#include <instr_emul.h>
#include <vmexit.h>
#include <vmx.h>
#include <ept.h>
#include <trace.h>
#include <logmsg.h>

/**
 * @brief General complete-work for port I/O emulation
 *
 * @pre io_req->type == REQ_PORTIO
 *
 * @remark This function must be called when \p io_req is completed, after
 * either a previous call to emulate_io() returning 0 or the corresponding VHM
 * request having transferred to the COMPLETE state.
 */
static void
emulate_pio_complete(struct acrn_vcpu *vcpu, const struct io_request *io_req)
{
	const struct pio_request *pio_req = &io_req->reqs.pio;
	uint64_t mask = 0xFFFFFFFFUL >> (32UL - 8UL * pio_req->size);

	if (pio_req->direction == REQUEST_READ) {
		uint64_t value = (uint64_t)pio_req->value;
		uint64_t rax = vcpu_get_gpreg(vcpu, CPU_REG_RAX);

		rax = ((rax) & ~mask) | (value & mask);
		vcpu_set_gpreg(vcpu, CPU_REG_RAX, rax);
	}
}

/**
 * @brief General complete-work for MMIO emulation
 *
 * @param vcpu The virtual CPU that triggers the MMIO access
 * @param io_req The I/O request holding the details of the MMIO access
 *
 * @pre io_req->type == REQ_MMIO
 *
 * @remark This function must be called when \p io_req is completed, after
 * either a previous call to emulate_io() returning 0 or the corresponding VHM
 * request transferring to the COMPLETE state.
 */
static void emulate_mmio_complete(struct acrn_vcpu *vcpu, const struct io_request *io_req)
{
	const struct mmio_request *mmio_req = &io_req->reqs.mmio;

	if (mmio_req->direction == REQUEST_READ) {
		/* Emulate instruction and update vcpu register set */
		(void)emulate_instruction(vcpu);
	}
}

#ifdef CONFIG_PARTITION_MODE
static void io_instr_dest_handler(struct io_request *io_req)
{
	struct pio_request *pio_req = &io_req->reqs.pio;

	if (pio_req->direction == REQUEST_READ) {
		pio_req->value = 0xFFFFFFFFU;
	}
}

#else

static void complete_ioreq(struct acrn_vcpu *vcpu, struct io_request *io_req)
{
	union vhm_request_buffer *req_buf = NULL;
	struct vhm_request *vhm_req;

	req_buf = (union vhm_request_buffer *)(vcpu->vm->sw.io_shared_page);

	stac();
	vhm_req = &req_buf->req_queue[vcpu->vcpu_id];
	if (io_req != NULL) {
		switch (vcpu->req.type) {
		case REQ_PORTIO:
			io_req->reqs.pio.value = vhm_req->reqs.pio.value;
			break;

		case REQ_MMIO:
			io_req->reqs.mmio.value = vhm_req->reqs.mmio.value;
			break;

		default:
			/*no actions are required for other cases.*/
			break;
		}
	}
	atomic_store32(&vhm_req->processed, REQ_STATE_FREE);
	clac();
}

/**
 * @brief Complete-work of VHM requests for port I/O emulation
 *
 * @pre vcpu->req.type == REQ_PORTIO
 *
 * @remark This function must be called after the VHM request corresponding to
 * \p vcpu being transferred to the COMPLETE state.
 */
static void dm_emulate_pio_complete(struct acrn_vcpu *vcpu)
{
	struct io_request *io_req = &vcpu->req;

	complete_ioreq(vcpu, io_req);

	emulate_pio_complete(vcpu, io_req);
}

/**
 * @brief Complete-work of VHM requests for MMIO emulation
 *
 * @param vcpu The virtual CPU that triggers the MMIO access
 *
 * @pre vcpu->req.type == REQ_MMIO
 *
 * @remark This function must be called after the VHM request corresponding to
 * \p vcpu being transferred to the COMPLETE state.
 */
static void dm_emulate_mmio_complete(struct acrn_vcpu *vcpu)
{
	struct io_request *io_req = &vcpu->req;

	complete_ioreq(vcpu, io_req);

	emulate_mmio_complete(vcpu, io_req);
}

/**
 * @brief General complete-work for all kinds of VHM requests for I/O emulation
 *
 * @param vcpu The virtual CPU that triggers the MMIO access
 */
static void dm_emulate_io_complete(struct acrn_vcpu *vcpu)
{
	if (get_vhm_req_state(vcpu->vm, vcpu->vcpu_id) == REQ_STATE_COMPLETE) {
		/*
		 * If vcpu is in Zombie state and will be destroyed soon. Just
		 * mark ioreq done and don't resume vcpu.
		 */
		if (vcpu->state == VCPU_ZOMBIE) {
			complete_ioreq(vcpu, NULL);
		} else {
			switch (vcpu->req.type) {
			case REQ_MMIO:
				dm_emulate_mmio_complete(vcpu);
				break;

			case REQ_PORTIO:
			case REQ_PCICFG:
				/*
				 * REQ_PORTIO on 0xcf8 & 0xcfc may switch to REQ_PCICFG in some
				 * cases. It works to apply the post-work for REQ_PORTIO on
				 * REQ_PCICFG because the format of the first 28 bytes of
				 * REQ_PORTIO & REQ_PCICFG requests are exactly the same and
				 * post-work is mainly interested in the read value.
				 */
				dm_emulate_pio_complete(vcpu);
				break;

			default:
				/*
				 * REQ_WP can only be triggered on writes which do not need
				 * post-work. Just mark the ioreq done.
				 */
				complete_ioreq(vcpu, NULL);
				break;
			}

		}
	}
}
#endif

/**
 * Try handling the given request by any port I/O handler registered in the
 * hypervisor.
 *
 * @pre io_req->type == REQ_PORTIO
 *
 * @retval 0 Successfully emulated by registered handlers.
 * @retval -ENODEV No proper handler found.
 * @retval -EIO The request spans multiple devices and cannot be emulated.
 */
static int32_t
hv_emulate_pio(struct acrn_vcpu *vcpu, struct io_request *io_req)
{
	int32_t status = -ENODEV;
	uint16_t port, size;
	uint32_t idx;
	struct acrn_vm *vm = vcpu->vm;
	struct pio_request *pio_req = &io_req->reqs.pio;
	struct vm_io_handler_desc *handler;

	port = (uint16_t)pio_req->address;
	size = (uint16_t)pio_req->size;

	for (idx = 0U; idx < EMUL_PIO_IDX_MAX; idx++) {
		handler = &(vm->arch_vm.emul_pio[idx]);

		if ((port < handler->port_start) || (port >= handler->port_end)) {
			continue;
		}

		status = 0;

		if (pio_req->direction == REQUEST_WRITE) {
			if (handler->io_write != NULL) {
				if (!(handler->io_write(vm, port, size, pio_req->value))) {
					/*
					 * If io_write return false, it indicates that we need continue
					 * to emulate in DM.
					 */
					status = -ENODEV;
				}
			}
			pr_dbg("IO write on port %04x, data %08x", port, pio_req->value);
		} else {
			if (handler->io_read != NULL) {
				if (!(handler->io_read(vm, vcpu, port, size))) {
					/*
					 * If io_read return false, it indicates that we need continue
					 * to emulate in DM.
					 */
					status = -ENODEV;
				}
			}
			pr_dbg("IO read on port %04x, data %08x", port, pio_req->value);
		}
		break;
	}

	return status;
}

/**
 * Use registered MMIO handlers on the given request if it falls in the range of
 * any of them.
 *
 * @pre io_req->type == REQ_MMIO
 *
 * @retval 0 Successfully emulated by registered handlers.
 * @retval -ENODEV No proper handler found.
 * @retval -EIO The request spans multiple devices and cannot be emulated.
 */
static int32_t
hv_emulate_mmio(struct acrn_vcpu *vcpu, struct io_request *io_req)
{
	int32_t status = -ENODEV;
	uint16_t idx;
	uint64_t address, size;
	struct mmio_request *mmio_req = &io_req->reqs.mmio;
	struct mem_io_node *mmio_handler = NULL;

	address = mmio_req->address;
	size = mmio_req->size;

	for (idx = 0U; idx < vcpu->vm->emul_mmio_regions; idx++) {
		uint64_t base, end;
		bool emulation_done = false;

		mmio_handler = &(vcpu->vm->emul_mmio[idx]);
		base = mmio_handler->range_start;
		end = mmio_handler->range_end;

		if (((address + size) <= base) || (address >= end)) {
			continue;
		} else if (!((address >= base) && ((address + size) <= end))) {
			pr_fatal("Err MMIO, address:0x%llx, size:%x", address, size);
			status = -EIO;
			emulation_done = true;
		} else {
			/* Handle this MMIO operation */
			if (mmio_handler->read_write != NULL) {
				status = mmio_handler->read_write(io_req, mmio_handler->handler_private_data);
				emulation_done = true;
			}
		}

		if (emulation_done) {
			break;
		}
	}

	return status;
}

/**
 * @brief Emulate \p io_req for \p vcpu
 *
 * Handle an I/O request by either invoking a hypervisor-internal handler or
 * deliver to VHM.
 *
 * @param vcpu The virtual CPU that triggers the MMIO access
 * @param io_req The I/O request holding the details of the MMIO access
 *
 * @retval 0 Successfully emulated by registered handlers.
 * @retval IOREQ_PENDING The I/O request is delivered to VHM.
 * @retval -EIO The request spans multiple devices and cannot be emulated.
 * @retval -EINVAL \p io_req has an invalid type.
 * @retval <0 on other errors during emulation.
 */
static int32_t
emulate_io(struct acrn_vcpu *vcpu, struct io_request *io_req)
{
	int32_t status;

	switch (io_req->type) {
	case REQ_PORTIO:
		status = hv_emulate_pio(vcpu, io_req);
		if (status == 0) {
			emulate_pio_complete(vcpu, io_req);
		}
		break;
	case REQ_MMIO:
	case REQ_WP:
		status = hv_emulate_mmio(vcpu, io_req);
		if (status == 0) {
			emulate_mmio_complete(vcpu, io_req);
		}
		break;
	default:
		/* Unknown I/O request type */
		status = -EINVAL;
		break;
	}

	if (status == -ENODEV) {
#ifdef CONFIG_PARTITION_MODE
		/*
		 * No handler from HV side, return all FFs on read
		 * and discard writes.
		 */
		io_instr_dest_handler(io_req);
		status = 0;

#else
		/*
		 * No handler from HV side, search from VHM in Dom0
		 *
		 * ACRN insert request to VHM and inject upcall.
		 */
		status = acrn_insert_request(vcpu, io_req);
		if (status == 0) {
			dm_emulate_io_complete(vcpu);
		} else {
			/* here for both IO & MMIO, the direction, address,
			 * size definition is same
			 */
			struct pio_request *pio_req = &io_req->reqs.pio;
			pr_fatal("%s Err: access dir %d, type %d, "
				"addr = 0x%llx, size=%lu", __func__,
				pio_req->direction, io_req->type,
				pio_req->address, pio_req->size);
		}
#endif
	}

	return status;
}

/**
 * @brief The handler of VM exits on I/O instructions
 *
 * @param vcpu The virtual CPU which triggers the VM exit on I/O instruction
 */
int32_t pio_instr_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int32_t status;
	uint64_t exit_qual;
	uint32_t mask;
	int32_t cur_context_idx = vcpu->arch.cur_context;
	struct io_request *io_req = &vcpu->req;
	struct pio_request *pio_req = &io_req->reqs.pio;

	exit_qual = vcpu->arch.exit_qualification;

	io_req->type = REQ_PORTIO;
	pio_req->size = vm_exit_io_instruction_size(exit_qual) + 1UL;
	pio_req->address = vm_exit_io_instruction_port_number(exit_qual);
	if (vm_exit_io_instruction_access_direction(exit_qual) == 0UL) {
		mask = 0xFFFFFFFFU >> (32U - (8U * pio_req->size));
		pio_req->direction = REQUEST_WRITE;
		pio_req->value = (uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RAX) & mask;
	} else {
		pio_req->direction = REQUEST_READ;
	}

	TRACE_4I(TRACE_VMEXIT_IO_INSTRUCTION,
		(uint32_t)pio_req->address,
		(uint32_t)pio_req->direction,
		(uint32_t)pio_req->size,
		(uint32_t)cur_context_idx);

	status = emulate_io(vcpu, io_req);

	return status;
}

int32_t ept_violation_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int32_t status = -EINVAL, ret;
	uint64_t exit_qual;
	uint64_t gpa;
	struct io_request *io_req = &vcpu->req;
	struct mmio_request *mmio_req = &io_req->reqs.mmio;

	/* Handle page fault from guest */
	exit_qual = vcpu->arch.exit_qualification;

	io_req->type = REQ_MMIO;

	/* Specify if read or write operation */
	if ((exit_qual & 0x2UL) != 0UL) {
		/* Write operation */
		mmio_req->direction = REQUEST_WRITE;
		mmio_req->value = 0UL;

		/* XXX: write access while EPT perm RX -> WP */
		if ((exit_qual & 0x38UL) == 0x28UL) {
			io_req->type = REQ_WP;
		}
	} else {
		/* Read operation */
		mmio_req->direction = REQUEST_READ;

		/* TODO: Need to determine how sign extension is determined for
		 * reads
		 */
	}

	/* Get the guest physical address */
	gpa = exec_vmread64(VMX_GUEST_PHYSICAL_ADDR_FULL);

	TRACE_2L(TRACE_VMEXIT_EPT_VIOLATION, exit_qual, gpa);

	/* Adjust IPA appropriately and OR page offset to get full IPA of abort
	 */
	mmio_req->address = gpa;

	ret = decode_instruction(vcpu);
	if (ret > 0) {
		mmio_req->size = (uint64_t)ret;
		/*
		 * For MMIO write, ask DM to run MMIO emulation after
		 * instruction emulation. For MMIO read, ask DM to run MMIO
		 * emulation at first.
		 */

		/* Determine value being written. */
		if (mmio_req->direction == REQUEST_WRITE) {
			status = emulate_instruction(vcpu);
			if (status != 0) {
				ret = -EFAULT;
			}
		}

		if (ret > 0) {
			status = emulate_io(vcpu, io_req);
		}
	} else {
		if (ret == -EFAULT) {
			pr_info("page fault happen during decode_instruction");
			status = 0;
		}
	}

	if (ret <= 0) {
		pr_acrnlog("Guest Linear Address: 0x%016llx", exec_vmread(VMX_GUEST_LINEAR_ADDR));
		pr_acrnlog("Guest Physical Address address: 0x%016llx", gpa);
	}
	return status;
}

/**
 * @brief Allow a VM to access a port I/O range
 *
 * This API enables direct access from the given \p vm to the port I/O space
 * starting from \p port_address to \p port_address + \p nbytes - 1.
 *
 * @param vm The VM whose port I/O access permissions is to be changed
 * @param port_address The start address of the port I/O range
 * @param nbytes The size of the range, in bytes
 */
void allow_guest_pio_access(struct acrn_vm *vm, uint16_t port_address,
		uint32_t nbytes)
{
	uint16_t address = port_address;
	uint32_t *b;
	uint32_t i;

	b = (uint32_t *)vm->arch_vm.io_bitmap;
	for (i = 0U; i < nbytes; i++) {
		b[address >> 5U] &= ~(1U << (address & 0x1fU));
		address++;
	}
}

static void deny_guest_pio_access(struct acrn_vm *vm, uint16_t port_address,
		uint32_t nbytes)
{
	uint16_t address = port_address;
	uint32_t *b;
	uint32_t i;

	b = (uint32_t *)vm->arch_vm.io_bitmap;
	for (i = 0U; i < nbytes; i++) {
		b[address >> 5U] |= (1U << (address & 0x1fU));
		address++;
	}
}

/**
 * @brief Register a port I/O handler
 *
 * @param vm      The VM to which the port I/O handlers are registered
 * @param pio_idx The emulated port io index
 * @param range   The emulated port io range
 * @param io_read_fn_ptr The handler for emulating reads from the given range
 * @param io_write_fn_ptr The handler for emulating writes to the given range
 * @pre pio_idx < EMUL_PIO_IDX_MAX
 */
void register_pio_emulation_handler(struct acrn_vm *vm, uint32_t pio_idx,
		const struct vm_io_range *range, io_read_fn_t io_read_fn_ptr, io_write_fn_t io_write_fn_ptr)
{
	if (is_sos_vm(vm)) {
		deny_guest_pio_access(vm, range->base, range->len);
	}
	vm->arch_vm.emul_pio[pio_idx].port_start = range->base;
	vm->arch_vm.emul_pio[pio_idx].port_end = range->base + range->len;
	vm->arch_vm.emul_pio[pio_idx].io_read = io_read_fn_ptr;
	vm->arch_vm.emul_pio[pio_idx].io_write = io_write_fn_ptr;
}

/**
 * @brief Register a MMIO handler
 *
 * This API registers a MMIO handler to \p vm before it is launched.
 *
 * @param vm The VM to which the MMIO handler is registered
 * @param read_write The handler for emulating accesses to the given range
 * @param start The base address of the range \p read_write can emulate
 * @param end The end of the range (exclusive) \p read_write can emulate
 * @param handler_private_data Handler-specific data which will be passed to \p read_write when called
 *
 * @retval 0 Registration succeeds
 * @retval -EINVAL \p read_write is NULL, \p end is not larger than \p start or \p vm has been launched
 */
int32_t register_mmio_emulation_handler(struct acrn_vm *vm,
	hv_mem_io_handler_t read_write, uint64_t start,
	uint64_t end, void *handler_private_data)
{
	int32_t status = -EINVAL;
	struct mem_io_node *mmio_node;

	if ((vm->hw.created_vcpus > 0U) && (vm->hw.vcpu_array[0].launched)) {
		pr_err("register mmio handler after vm launched");
	} else {
		/* Ensure both a read/write handler and range check function exist */
		if ((read_write != NULL) && (end > start)) {
			if (vm->emul_mmio_regions >= CONFIG_MAX_EMULATED_MMIO_REGIONS) {
				pr_err("the emulated mmio region is out of range");
			} else {
				mmio_node = &(vm->emul_mmio[vm->emul_mmio_regions]);
				/* Fill in information for this node */
				mmio_node->read_write = read_write;
				mmio_node->handler_private_data = handler_private_data;
				mmio_node->range_start = start;
				mmio_node->range_end = end;

				(vm->emul_mmio_regions)++;

				/*
				 * SOS would map all its memory at beginning, so we
				 * should unmap it. But UOS will not, so we shouldn't
				 * need to unmap it.
				 */
				if (is_sos_vm(vm)) {
					ept_mr_del(vm, (uint64_t *)vm->arch_vm.nworld_eptp, start, end - start);
				}

				/* Return success */
				status = 0;
			}
		}
	}

	/* Return status to caller */
	return status;
}
