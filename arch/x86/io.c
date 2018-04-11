/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>
#include <hypercall.h>

int dm_emulate_pio_post(struct vcpu *vcpu)
{
	int cur = vcpu->vcpu_id;
	int cur_context = vcpu->arch_vcpu.cur_context;
	struct vhm_request_buffer *req_buf = NULL;
	uint32_t mask =
		0xFFFFFFFFul >> (32 - 8 * vcpu->req.reqs.pio_request.size);
	uint64_t *rax;

	req_buf = (struct vhm_request_buffer *)(vcpu->vm->sw.req_buf);

	rax = &vcpu->arch_vcpu.contexts[cur_context].guest_cpu_regs.regs.rax;
	vcpu->req.reqs.pio_request.value =
		req_buf->req_queue[cur].reqs.pio_request.value;

	/* VHM emulation data already copy to req, mark to free slot now */
	req_buf->req_queue[cur].valid = false;

	if (req_buf->req_queue[cur].processed != REQ_STATE_SUCCESS)
		return -1;

	if (vcpu->req.reqs.pio_request.direction == REQUEST_READ)
		*rax = ((*rax) & ~mask) |
			(vcpu->req.reqs.pio_request.value & mask);

	return 0;
}

static void dm_emulate_pio_pre(struct vcpu *vcpu, uint64_t exit_qual,
				uint32_t sz, uint64_t req_value)
{
	vcpu->req.type = REQ_PORTIO;
	if (VM_EXIT_IO_INSTRUCTION_ACCESS_DIRECTION(exit_qual))
		vcpu->req.reqs.pio_request.direction = REQUEST_READ;
	else
		vcpu->req.reqs.pio_request.direction = REQUEST_WRITE;

	vcpu->req.reqs.pio_request.address =
		VM_EXIT_IO_INSTRUCTION_PORT_NUMBER(exit_qual);
	vcpu->req.reqs.pio_request.size = sz;
	vcpu->req.reqs.pio_request.value = req_value;
}

int io_instr_handler(struct vcpu *vcpu)
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

	TRACE_4I(TRC_VMEXIT_IO_INSTRUCTION, port, direction, sz,
		cur_context_idx);

	for (handler = vm->arch_vm.io_handler;
			handler; handler = handler->next) {

		if ((port >= handler->desc.addr + handler->desc.len) ||
				(port + sz <= handler->desc.addr))
			continue;

		/* Dom0 do not require IO emulation */
		if (is_vm0(vm))
			status = 0;

		if (direction == 0) {
			if (handler->desc.io_write == NULL)
				continue;

			handler->desc.io_write(handler, vm, port, sz,
				cur_context->guest_cpu_regs.regs.rax);

			pr_dbg("IO write on port %04x, data %08x", port,
				cur_context->guest_cpu_regs.regs.rax & mask);

			status = 0;
			break;
		} else if (handler->desc.io_read) {
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

		memset(&vcpu->req, 0, sizeof(struct vhm_request));
		dm_emulate_pio_pre(vcpu, exit_qual, sz, *rax);
		status = acrn_insert_request_wait(vcpu, &vcpu->req);
	}

	if (status != 0) {
		pr_fatal("IO %s access to port 0x%04x, size=%u",
				direction ? "read" : "write", port, sz);

	}

	/* Catch any problems */
	ASSERT(status == 0, "Invalid IO access");

	return status;
}

static void register_io_handler(struct vm *vm, struct vm_io_handler *hdlr)
{
	if (vm->arch_vm.io_handler)
		hdlr->next = vm->arch_vm.io_handler;

	vm->arch_vm.io_handler = hdlr;
}

static void empty_io_handler_list(struct vm *vm)
{
	struct vm_io_handler *handler = vm->arch_vm.io_handler;
	struct vm_io_handler *tmp;

	while (handler) {
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

static void deny_guest_io_access(struct vm *vm, uint32_t address, uint32_t nbytes)
{
	uint32_t *b;
	uint32_t i;
	uint32_t a;

	for (i = 0; i < nbytes; i++) {
		b = vm->arch_vm.iobitmap[0];
		if (address & 0x8000)
			b = vm->arch_vm.iobitmap[1];
		a = address & 0x7fff;
		b[a >> 5] |= (1 << (a & 0x1f));
		address++;
	}
}

static uint32_t
default_io_read(__unused struct vm_io_handler *hdlr, __unused struct vm *vm,
			ioport_t address, size_t width)
{
	uint32_t v = io_read(address, width);
	return v;
}

static void default_io_write(__unused struct vm_io_handler *hdlr,
			__unused struct vm *vm, ioport_t addr,
			size_t width, uint32_t v)
{
	io_write(v, addr, width);
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

	ASSERT(vm->arch_vm.iobitmap[0] && vm->arch_vm.iobitmap[1], "");

	if (is_vm0(vm)) {
		memset(vm->arch_vm.iobitmap[0], 0x00, CPU_PAGE_SIZE);
		memset(vm->arch_vm.iobitmap[1], 0x00, CPU_PAGE_SIZE);
	} else {
		/* block all IO port access from Guest */
		memset(vm->arch_vm.iobitmap[0], 0xFF, CPU_PAGE_SIZE);
		memset(vm->arch_vm.iobitmap[1], 0xFF, CPU_PAGE_SIZE);
	}
}

void register_io_emulation_handler(struct vm *vm, struct vm_io_range *range,
		io_read_fn_t io_read_fn_ptr,
		io_write_fn_t io_write_fn_ptr)
{
	struct vm_io_handler *handler = NULL;
	io_read_fn_t io_read_fn = &default_io_read;
	io_write_fn_t io_write_fn = &default_io_write;

	if (range->flags == IO_ATTR_RW && io_read_fn_ptr && io_write_fn_ptr) {
		io_read_fn = io_read_fn_ptr;
		io_write_fn = io_write_fn_ptr;
	} else if (range->flags == IO_ATTR_R) {
		if (io_read_fn_ptr)
			io_read_fn = io_read_fn_ptr;
		io_write_fn = NULL;
	}

	if (is_vm0(vm))
		deny_guest_io_access(vm, range->base, range->len);

	handler = create_io_handler(range->base,
			range->len, io_read_fn, io_write_fn);

	register_io_handler(vm, handler);
}
