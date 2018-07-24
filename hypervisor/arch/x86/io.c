/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

int dm_emulate_pio_post(struct vcpu *vcpu)
{
	uint16_t cur = vcpu->vcpu_id;
	int cur_context = vcpu->arch_vcpu.cur_context;
	union vhm_request_buffer *req_buf = NULL;
	struct io_request *io_req = &vcpu->req;
	struct pio_request *pio_req = &io_req->reqs.pio;
	uint64_t mask = 0xFFFFFFFFUL >> (32UL - 8UL * pio_req->size);
	uint64_t *rax;
	struct vhm_request *vhm_req;

	req_buf = (union vhm_request_buffer *)(vcpu->vm->sw.io_shared_page);
	vhm_req = &req_buf->req_queue[cur];

	rax = &vcpu->arch_vcpu.contexts[cur_context].guest_cpu_regs.regs.rax;
	io_req->processed = vhm_req->processed;
	pio_req->value = vhm_req->reqs.pio.value;

	/* VHM emulation data already copy to req, mark to free slot now */
	vhm_req->valid = 0;

	if (io_req->processed != REQ_STATE_SUCCESS) {
		return -1;
	}

	if (pio_req->direction == REQUEST_READ) {
		uint64_t value = (uint64_t)pio_req->value;
		*rax = ((*rax) & ~mask) | (value & mask);
	}

	return 0;
}

static void
dm_emulate_pio_pre(struct vcpu *vcpu, uint64_t exit_qual, uint64_t req_value)
{
	struct pio_request *pio_req = &vcpu->req.reqs.pio;

	pio_req->value = req_value;
}

int io_instr_vmexit_handler(struct vcpu *vcpu)
{
	uint64_t exit_qual;
	uint64_t mask;
	uint16_t port, size;
	struct vm_io_handler *handler;
	struct vm *vm = vcpu->vm;
	struct io_request *io_req = &vcpu->req;
	struct pio_request *pio_req = &io_req->reqs.pio;
	int cur_context_idx = vcpu->arch_vcpu.cur_context;
	struct run_context *cur_context;
	int status = -EINVAL;

	io_req->type = REQ_PORTIO;
	io_req->processed = REQ_STATE_PENDING;

	cur_context = &vcpu->arch_vcpu.contexts[cur_context_idx];
	exit_qual = vcpu->arch_vcpu.exit_qualification;

	pio_req->size = VM_EXIT_IO_INSTRUCTION_SIZE(exit_qual) + 1UL;
	pio_req->address = VM_EXIT_IO_INSTRUCTION_PORT_NUMBER(exit_qual);
	if (VM_EXIT_IO_INSTRUCTION_ACCESS_DIRECTION(exit_qual) == 0UL) {
		pio_req->direction = REQUEST_WRITE;
	} else {
		pio_req->direction = REQUEST_READ;
	}

	size = (uint16_t)pio_req->size;
	port = (uint16_t)pio_req->address;
	mask = 0xffffffffUL >> (32U - 8U * size);

	TRACE_4I(TRACE_VMEXIT_IO_INSTRUCTION,
		(uint32_t)port,
		(uint32_t)pio_req->direction,
		(uint32_t)size,
		(uint32_t)cur_context_idx);

	/*
	 * Post-conditions of the loop:
	 *
	 *     status == 0       : The access has been handled properly.
	 *     status == -EIO    : The access spans multiple devices and cannot
	 *                         be handled.
	 *     status == -EINVAL : No valid handler found for this access.
	 */
	for (handler = vm->arch_vm.io_handler;
			handler; handler = handler->next) {

		if ((port >= (handler->desc.addr + handler->desc.len)) ||
				(port + size <= handler->desc.addr)) {
			continue;
		} else if (!((port >= handler->desc.addr) && ((port + size)
				<= (handler->desc.addr + handler->desc.len)))) {
			pr_fatal("Err:IO, port 0x%04x, size=%hu spans devices",
					port, size);
			status = -EIO;
			break;
		} else {
			struct cpu_gp_regs *regs =
					&cur_context->guest_cpu_regs.regs;

			if (pio_req->direction == REQUEST_WRITE) {
				handler->desc.io_write(handler, vm, port, size,
					regs->rax);

				pr_dbg("IO write on port %04x, data %08x", port,
					regs->rax & mask);
			} else {
				uint32_t data = handler->desc.io_read(handler,
						vm, port, size);

				regs->rax &= ~mask;
				regs->rax |= data & mask;

				pr_dbg("IO read on port %04x, data %08x",
					port, data);
			}
			status = 0;
			break;
		}
	}

	/* Go for VHM */
	if (status == -EINVAL) {
		uint64_t rax = cur_context->guest_cpu_regs.regs.rax;

		dm_emulate_pio_pre(vcpu, exit_qual, rax);
		status = acrn_insert_request_wait(vcpu, io_req);

		if (status != 0) {
			pr_fatal("Err:IO %s access to port 0x%04x, size=%u",
				(pio_req->direction != REQUEST_READ) ? "read" : "write",
				port, size);
		}
	}

	return status;
}

static void register_io_handler(struct vm *vm, struct vm_io_handler *hdlr)
{
	if (vm->arch_vm.io_handler != NULL) {
		hdlr->next = vm->arch_vm.io_handler;
	}

	vm->arch_vm.io_handler = hdlr;
}

static void empty_io_handler_list(struct vm *vm)
{
	struct vm_io_handler *handler = vm->arch_vm.io_handler;
	struct vm_io_handler *tmp;

	while (handler != NULL) {
		tmp = handler;
		handler = tmp->next;
		free(tmp);
	}
	vm->arch_vm.io_handler = NULL;
}

void free_io_emulation_resource(struct vm *vm)
{
	empty_io_handler_list(vm);

	/* Free I/O emulation bitmaps */
	free(vm->arch_vm.iobitmap[0]);
	free(vm->arch_vm.iobitmap[1]);
}

void allow_guest_io_access(struct vm *vm, uint32_t address, uint32_t nbytes)
{
	uint32_t *b;
	uint32_t i;
	uint32_t a;

	b = vm->arch_vm.iobitmap[0];
	for (i = 0U; i < nbytes; i++) {
		if ((address & 0x8000U) != 0U) {
			b = vm->arch_vm.iobitmap[1];
		}
		a = address & 0x7fffU;
		b[a >> 5] &= ~(1 << (a & 0x1fU));
		address++;
	}
}

static void deny_guest_io_access(struct vm *vm, uint32_t address, uint32_t nbytes)
{
	uint32_t *b;
	uint32_t i;
	uint32_t a;

	b = vm->arch_vm.iobitmap[0];
	for (i = 0U; i < nbytes; i++) {
		if ((address & 0x8000U) != 0U) {
			b = vm->arch_vm.iobitmap[1];
		}
		a = address & 0x7fffU;
		b[a >> 5U] |= (1U << (a & 0x1fU));
		address++;
	}
}

static struct vm_io_handler *create_io_handler(uint32_t port, uint32_t len,
				io_read_fn_t io_read_fn_ptr,
				io_write_fn_t io_write_fn_ptr)
{

	struct vm_io_handler *handler;

	handler = calloc(1, sizeof(struct vm_io_handler));

	if (handler != NULL) {
		handler->desc.addr = port;
		handler->desc.len = len;
		handler->desc.io_read = io_read_fn_ptr;
		handler->desc.io_write = io_write_fn_ptr;
	} else {
		pr_err("Error: out of memory");
	}

	return handler;
}

void setup_io_bitmap(struct vm *vm)
{
	/* Allocate VM architecture state and IO bitmaps A and B */
	vm->arch_vm.iobitmap[0] = alloc_page();
	vm->arch_vm.iobitmap[1] = alloc_page();

	ASSERT((vm->arch_vm.iobitmap[0] != NULL) &&
	       (vm->arch_vm.iobitmap[1] != NULL), "");

	if (is_vm0(vm)) {
		(void)memset(vm->arch_vm.iobitmap[0], 0x00, CPU_PAGE_SIZE);
		(void)memset(vm->arch_vm.iobitmap[1], 0x00, CPU_PAGE_SIZE);
	} else {
		/* block all IO port access from Guest */
		(void)memset(vm->arch_vm.iobitmap[0], 0xFF, CPU_PAGE_SIZE);
		(void)memset(vm->arch_vm.iobitmap[1], 0xFF, CPU_PAGE_SIZE);
	}
}

void register_io_emulation_handler(struct vm *vm, struct vm_io_range *range,
		io_read_fn_t io_read_fn_ptr,
		io_write_fn_t io_write_fn_ptr)
{
	struct vm_io_handler *handler = NULL;

	if (io_read_fn_ptr == NULL || io_write_fn_ptr == NULL) {
		pr_err("Invalid IO handler.");
		return;
	}

	if (is_vm0(vm)) {
		deny_guest_io_access(vm, range->base, range->len);
	}

	handler = create_io_handler(range->base,
			range->len, io_read_fn_ptr, io_write_fn_ptr);

	register_io_handler(vm, handler);
}
