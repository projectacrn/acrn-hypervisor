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
	uint32_t mask =
		0xFFFFFFFFUL >> (32 - 8 * vcpu->req.reqs.pio_request.size);
	uint64_t *rax;

	req_buf = (union vhm_request_buffer *)(vcpu->vm->sw.io_shared_page);

	rax = &vcpu->arch_vcpu.contexts[cur_context].guest_cpu_regs.regs.rax;
	vcpu->req.reqs.pio_request.value =
		req_buf->req_queue[cur].reqs.pio_request.value;

	/* VHM emulation data already copy to req, mark to free slot now */
	req_buf->req_queue[cur].valid = false;

	if (req_buf->req_queue[cur].processed != REQ_STATE_SUCCESS) {
		return -1;
	}

	if (vcpu->req.reqs.pio_request.direction == REQUEST_READ) {
		*rax = ((*rax) & ~mask) |
			(vcpu->req.reqs.pio_request.value & mask);
	}

	return 0;
}

static void dm_emulate_pio_pre(struct vcpu *vcpu, uint64_t exit_qual,
				uint32_t sz, uint64_t req_value)
{
	vcpu->req.type = REQ_PORTIO;
	if (VM_EXIT_IO_INSTRUCTION_ACCESS_DIRECTION(exit_qual) != 0U) {
		vcpu->req.reqs.pio_request.direction = REQUEST_READ;
	} else {
		vcpu->req.reqs.pio_request.direction = REQUEST_WRITE;
	}

	vcpu->req.reqs.pio_request.address =
		VM_EXIT_IO_INSTRUCTION_PORT_NUMBER(exit_qual);
	vcpu->req.reqs.pio_request.size = sz;
	vcpu->req.reqs.pio_request.value = req_value;
}

int io_instr_vmexit_handler(struct vcpu *vcpu)
{
	uint32_t sz;
	uint32_t mask;
	uint32_t port;
	int8_t direction;
	struct vm_io_handler *handler;
	uint64_t exit_qual;
	struct vm *vm = vcpu->vm;
	int cur_context_idx = vcpu->arch_vcpu.cur_context;
	struct run_context *cur_context;
	int status = -EINVAL;

	cur_context = &vcpu->arch_vcpu.contexts[cur_context_idx];
	exit_qual = vcpu->arch_vcpu.exit_qualification;

	sz = VM_EXIT_IO_INSTRUCTION_SIZE(exit_qual) + 1;
	port = VM_EXIT_IO_INSTRUCTION_PORT_NUMBER(exit_qual);
	direction = VM_EXIT_IO_INSTRUCTION_ACCESS_DIRECTION(exit_qual);
	mask = 0xfffffffful >> (32 - 8 * sz);

	TRACE_4I(TRACE_VMEXIT_IO_INSTRUCTION, port, (uint32_t)direction, sz,
		(uint32_t)cur_context_idx);

	for (handler = vm->arch_vm.io_handler;
			handler; handler = handler->next) {

		if ((port >= handler->desc.addr + handler->desc.len) ||
				(port + sz <= handler->desc.addr)) {
			continue;
		} else if (!((port >= handler->desc.addr) && ((port + sz)
				<= (handler->desc.addr + handler->desc.len)))) {
			pr_fatal("Err:IO, port 0x%04x, size=%u spans devices",
					port, sz);
			return -EIO;
		}

		if (direction == 0) {
			handler->desc.io_write(handler, vm, port, sz,
				cur_context->guest_cpu_regs.regs.rax);

			pr_dbg("IO write on port %04x, data %08x", port,
				cur_context->guest_cpu_regs.regs.rax & mask);

			status = 0;
			break;
		} else {
			uint32_t data = handler->desc.io_read(handler, vm,
							 port, sz);

			cur_context->guest_cpu_regs.regs.rax &= ~mask;
			cur_context->guest_cpu_regs.regs.rax |= data & mask;

			pr_dbg("IO read on port %04x, data %08x", port, data);

			status = 0;
			break;
		}
	}

	/* Go for VHM */
	if (status != 0) {
		uint64_t *rax = &cur_context->guest_cpu_regs.regs.rax;

		(void)memset(&vcpu->req, 0, sizeof(struct vhm_request));
		dm_emulate_pio_pre(vcpu, exit_qual, sz, *rax);
		status = acrn_insert_request_wait(vcpu, &vcpu->req);
	}

	if (status != 0) {
		pr_fatal("Err:IO %s access to port 0x%04x, size=%u",
			 (direction != 0) ? "read" : "write", port, sz);

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
