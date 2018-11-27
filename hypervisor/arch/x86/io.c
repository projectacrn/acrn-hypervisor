/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#include "guest/instr_emul.h"

static void complete_ioreq(struct vhm_request *vhm_req)
{
	vhm_req->valid = 0;
	atomic_store32(&vhm_req->processed, REQ_STATE_FREE);
}

/**
 * @brief Post-work for port I/O emulation
 *
 * @pre io_req->type == REQ_PORTIO
 *
 * @remark This function must be called when \p io_req is completed, after
 * either a previous call to emulate_io() returning 0 or the corresponding VHM
 * request having transferred to the COMPLETE state.
 */
static void
emulate_pio_post(struct acrn_vcpu *vcpu, const struct io_request *io_req)
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
 * @brief Post-work of VHM requests for port I/O emulation
 *
 * @pre vcpu->req.type == REQ_PORTIO
 *
 * @remark This function must be called after the VHM request corresponding to
 * \p vcpu being transferred to the COMPLETE state.
 */
void dm_emulate_pio_post(struct acrn_vcpu *vcpu)
{
	uint16_t cur = vcpu->vcpu_id;
	union vhm_request_buffer *req_buf = NULL;
	struct io_request *io_req = &vcpu->req;
	struct pio_request *pio_req = &io_req->reqs.pio;
	struct vhm_request *vhm_req;

	req_buf = (union vhm_request_buffer *)(vcpu->vm->sw.io_shared_page);
	vhm_req = &req_buf->req_queue[cur];

	pio_req->value = vhm_req->reqs.pio.value;

	/* VHM emulation data already copy to req, mark to free slot now */
	complete_ioreq(vhm_req);

	emulate_pio_post(vcpu, io_req);
}

/**
 * @brief General post-work for MMIO emulation
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
void emulate_mmio_post(const struct acrn_vcpu *vcpu, const struct io_request *io_req)
{
	const struct mmio_request *mmio_req = &io_req->reqs.mmio;

	if (mmio_req->direction == REQUEST_READ) {
		/* Emulate instruction and update vcpu register set */
		(void)emulate_instruction(vcpu);
	}
}

/**
 * @brief Post-work of VHM requests for MMIO emulation
 *
 * @param vcpu The virtual CPU that triggers the MMIO access
 *
 * @pre vcpu->req.type == REQ_MMIO
 *
 * @remark This function must be called after the VHM request corresponding to
 * \p vcpu being transferred to the COMPLETE state.
 */
void dm_emulate_mmio_post(struct acrn_vcpu *vcpu)
{
	uint16_t cur = vcpu->vcpu_id;
	struct io_request *io_req = &vcpu->req;
	struct mmio_request *mmio_req = &io_req->reqs.mmio;
	union vhm_request_buffer *req_buf;
	struct vhm_request *vhm_req;

	req_buf = (union vhm_request_buffer *)(vcpu->vm->sw.io_shared_page);
	vhm_req = &req_buf->req_queue[cur];

	mmio_req->value = vhm_req->reqs.mmio.value;

	/* VHM emulation data already copy to req, mark to free slot now */
	complete_ioreq(vhm_req);

	emulate_mmio_post(vcpu, io_req);
}

#ifdef CONFIG_PARTITION_MODE
static void io_instr_dest_handler(struct io_request *io_req)
{
	struct pio_request *pio_req = &io_req->reqs.pio;

	if (pio_req->direction == REQUEST_READ) {
		pio_req->value = 0xFFFFFFFFU;
	}
}
#endif

/**
 * @brief General post-work for all kinds of VHM requests for I/O emulation
 *
 * @param vcpu The virtual CPU that triggers the MMIO access
 */
void emulate_io_post(struct acrn_vcpu *vcpu)
{
	union vhm_request_buffer *req_buf;
	struct vhm_request *vhm_req;

	req_buf = (union vhm_request_buffer *)vcpu->vm->sw.io_shared_page;
	vhm_req = &req_buf->req_queue[vcpu->vcpu_id];

	if ((vhm_req->valid == 0) ||
		(atomic_load32(&vhm_req->processed) != REQ_STATE_COMPLETE)) {
		return;
	}

	/*
	 * If vcpu is in Zombie state and will be destroyed soon. Just
	 * mark ioreq done and don't resume vcpu.
	 */
	if (vcpu->state == VCPU_ZOMBIE) {
		complete_ioreq(vhm_req);
		return;
	}

	switch (vcpu->req.type) {
	case REQ_MMIO:
		request_vcpu_pre_work(vcpu, ACRN_VCPU_MMIO_COMPLETE);
		break;

	case REQ_PORTIO:
	case REQ_PCICFG:
		/* REQ_PORTIO on 0xcf8 & 0xcfc may switch to REQ_PCICFG in some
		 * cases. It works to apply the post-work for REQ_PORTIO on
		 * REQ_PCICFG because the format of the first 28 bytes of
		 * REQ_PORTIO & REQ_PCICFG requests are exactly the same and
		 * post-work is mainly interested in the read value.
		 */
		dm_emulate_pio_post(vcpu);
		break;

	default:
		/* REQ_WP can only be triggered on writes which do not need
		 * post-work. Just mark the ioreq done. */
		complete_ioreq(vhm_req);
		break;
	}

	resume_vcpu(vcpu);
}

/**
 * Try handling the given request by any port I/O handler registered in the
 * hypervisor.
 *
 * @pre io_req->type == REQ_PORTIO
 *
 * @return 0       - Successfully emulated by registered handlers.
 * @return -ENODEV - No proper handler found.
 * @return -EIO    - The request spans multiple devices and cannot be emulated.
 */
int32_t
hv_emulate_pio(const struct acrn_vcpu *vcpu, struct io_request *io_req)
{
	int32_t status = -ENODEV;
	uint16_t port, size;
	uint32_t mask;
	uint32_t idx;
	struct acrn_vm *vm = vcpu->vm;
	struct pio_request *pio_req = &io_req->reqs.pio;
	struct vm_io_handler_desc *handler;

	port = (uint16_t)pio_req->address;
	size = (uint16_t)pio_req->size;
	mask = 0xFFFFFFFFU >> (32U - 8U * size);

	for (idx = 0U; idx < EMUL_PIO_IDX_MAX; idx++) {
		handler = &(vm->arch_vm.emul_pio[idx]);

		if ((port < handler->port_start) || (port >= handler->port_end)) {
			continue;
		}

		if (pio_req->direction == REQUEST_WRITE) {
			if (handler->io_write) {
				handler->io_write(vm, port, size, pio_req->value & mask);
			}
			pr_dbg("IO write on port %04x, data %08x", port, pio_req->value & mask);
		} else {
			if (handler->io_read) {
				pio_req->value = handler->io_read(vm, port, size);
			}
			pr_dbg("IO read on port %04x, data %08x", port, pio_req->value);
		}
		status = 0;
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
 * @return 0       - Successfully emulated by registered handlers.
 * @return -ENODEV - No proper handler found.
 * @return -EIO    - The request spans multiple devices and cannot be emulated.
 */
static int32_t
hv_emulate_mmio(struct acrn_vcpu *vcpu, struct io_request *io_req)
{
	int status = -ENODEV;
	uint16_t idx;
	uint64_t address, size;
	struct mmio_request *mmio_req = &io_req->reqs.mmio;
	struct mem_io_node *mmio_handler = NULL;

	address = mmio_req->address;
	size = mmio_req->size;

	for (idx = 0U; idx < vcpu->vm->emul_mmio_regions; idx++) {
		uint64_t base, end;

		mmio_handler = &(vcpu->vm->emul_mmio[idx]);
		base = mmio_handler->range_start;
		end = mmio_handler->range_end;

		if (((address + size) <= base) || (address >= end)) {
			continue;
		} else if (!((address >= base) && ((address + size) <= end))) {
			pr_fatal("Err MMIO, address:0x%llx, size:%x", address, size);
			return -EIO;
		} else {
			/* Handle this MMIO operation */
			if (mmio_handler->read_write) {
				status = mmio_handler->read_write(io_req, mmio_handler->handler_private_data);
				break;
			}
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
 * @return 0       - Successfully emulated by registered handlers.
 * @return IOREQ_PENDING - The I/O request is delivered to VHM.
 * @return -EIO    - The request spans multiple devices and cannot be emulated.
 * @return -EINVAL - \p io_req has an invalid type.
 * @return Negative on other errors during emulation.
 */
int32_t
emulate_io(struct acrn_vcpu *vcpu, struct io_request *io_req)
{
	int32_t status;

	switch (io_req->type) {
	case REQ_PORTIO:
		status = hv_emulate_pio(vcpu, io_req);
		break;
	case REQ_MMIO:
	case REQ_WP:
		status = hv_emulate_mmio(vcpu, io_req);
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
		status = acrn_insert_request_wait(vcpu, io_req);

		if (status != 0) {
			/* here for both IO & MMIO, the direction, address,
			 * size definition is same
			 */
			struct pio_request *pio_req = &io_req->reqs.pio;
			pr_fatal("%s Err: access dir %d, type %d, "
				"addr = 0x%llx, size=%lu", __func__,
				pio_req->direction, io_req->type,
				pio_req->address, pio_req->size);
		} else {
			status = IOREQ_PENDING;
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
	int32_t cur_context_idx = vcpu->arch.cur_context;
	struct io_request *io_req = &vcpu->req;
	struct pio_request *pio_req = &io_req->reqs.pio;

	exit_qual = vcpu->arch.exit_qualification;

	io_req->type = REQ_PORTIO;
	pio_req->size = vm_exit_io_instruction_size(exit_qual) + 1UL;
	pio_req->address = vm_exit_io_instruction_port_number(exit_qual);
	if (vm_exit_io_instruction_access_direction(exit_qual) == 0UL) {
		pio_req->direction = REQUEST_WRITE;
		pio_req->value = (uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RAX);
	} else {
		pio_req->direction = REQUEST_READ;
	}

	TRACE_4I(TRACE_VMEXIT_IO_INSTRUCTION,
		(uint32_t)pio_req->address,
		(uint32_t)pio_req->direction,
		(uint32_t)pio_req->size,
		(uint32_t)cur_context_idx);

	status = emulate_io(vcpu, io_req);

	if (status == 0) {
		emulate_pio_post(vcpu, io_req);
	} else if (status == IOREQ_PENDING) {
		status = 0;
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
 * @brief Initialize the I/O bitmap for \p vm
 *
 * @param vm The VM whose I/O bitmap is to be initialized
 */
void setup_io_bitmap(struct acrn_vm *vm)
{
	if (is_vm0(vm)) {
		(void)memset(vm->arch_vm.io_bitmap, 0x00U, CPU_PAGE_SIZE * 2U);
	} else {
		/* block all IO port access from Guest */
		(void)memset(vm->arch_vm.io_bitmap, 0xFFU, CPU_PAGE_SIZE * 2U);
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
void register_io_emulation_handler(struct acrn_vm *vm, uint32_t pio_idx,
		const struct vm_io_range *range, io_read_fn_t io_read_fn_ptr, io_write_fn_t io_write_fn_ptr)
{
	if (is_vm0(vm)) {
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
 * @return 0 - Registration succeeds
 * @return -EINVAL - \p read_write is NULL, \p end is not larger than \p start or \p vm has been launched
 */
int register_mmio_emulation_handler(struct acrn_vm *vm,
	hv_mem_io_handler_t read_write, uint64_t start,
	uint64_t end, void *handler_private_data)
{
	int status = -EINVAL;
	struct mem_io_node *mmio_node;

	if ((vm->hw.created_vcpus > 0U) && vm->hw.vcpu_array[0].launched) {
		ASSERT(false, "register mmio handler after vm launched");
		return status;
	}

	/* Ensure both a read/write handler and range check function exist */
	if ((read_write != NULL) && (end > start)) {

		if (vm->emul_mmio_regions >= CONFIG_MAX_EMULATED_MMIO_REGIONS) {
			pr_err("the emulated mmio region is out of range");
			return status;
		}
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
		if (is_vm0(vm)) {
			ept_mr_del(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
				start, end - start);
		}

		/* Return success */
		status = 0;

	}

	/* Return status to caller */
	return status;
}
