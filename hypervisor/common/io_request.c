/*
 * Copyright (C) 2018 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#define ACRN_DBG_IOREQUEST	6

static void fire_vhm_interrupt(void)
{
	/*
	 * use vLAPIC to inject vector to SOS vcpu 0 if vlapic is enabled
	 * otherwise, send IPI hardcoded to CPU_BOOT_ID
	 */
	struct vm *vm0;
	struct vcpu *vcpu;

	vm0 = get_vm_from_vmid(0);
	ASSERT(vm0, "VM Pointer is NULL");

	vcpu = vcpu_from_vid(vm0, 0);
	ASSERT(vcpu, "vcpu_from_vid failed");

	vlapic_intr_edge(vcpu, VECTOR_VIRT_IRQ_VHM);
}

static void acrn_print_request(int vcpu_id, struct vhm_request *req)
{
	switch (req->type) {
	case REQ_MMIO:
		dev_dbg(ACRN_DBG_IOREQUEST, "[vcpu_id=%d type=MMIO]", vcpu_id);
		dev_dbg(ACRN_DBG_IOREQUEST,
			"gpa=0x%lx, R/W=%d, size=%ld value=0x%lx processed=%lx",
			req->reqs.mmio_request.address,
			req->reqs.mmio_request.direction,
			req->reqs.mmio_request.size,
			req->reqs.mmio_request.value,
			req->processed);
		break;
	case REQ_PORTIO:
		dev_dbg(ACRN_DBG_IOREQUEST, "[vcpu_id=%d type=PORTIO]", vcpu_id);
		dev_dbg(ACRN_DBG_IOREQUEST,
			"IO=0x%lx, R/W=%d, size=%ld value=0x%lx processed=%lx",
			req->reqs.pio_request.address,
			req->reqs.pio_request.direction,
			req->reqs.pio_request.size,
			req->reqs.pio_request.value,
			req->processed);
		break;
	default:
		dev_dbg(ACRN_DBG_IOREQUEST, "[vcpu_id=%d type=%d] NOT support type",
			vcpu_id, req->type);
		break;
	}
}

int acrn_insert_request_wait(struct vcpu *vcpu, struct vhm_request *req)
{
	union vhm_request_buffer *req_buf = NULL;
	long cur;

	ASSERT(sizeof(*req) == (4096/VHM_REQUEST_MAX),
			"vhm_request page broken!");


	if (!vcpu || !req || vcpu->vm->sw.io_shared_page == NULL)
		return -EINVAL;

	req_buf = (union vhm_request_buffer *)(vcpu->vm->sw.io_shared_page);

	/* ACRN insert request to VHM and inject upcall */
	cur = vcpu->vcpu_id;
	memcpy_s(&req_buf->req_queue[cur], sizeof(struct vhm_request),
		 req, sizeof(struct vhm_request));

	/* pause vcpu, wait for VHM to handle the MMIO request.
	 * TODO: when pause_vcpu changed to switch vcpu out directlly, we
	 * should fix the race issue between req.valid = true and vcpu pause
	 */
	atomic_store(&vcpu->ioreq_pending, 1);
	pause_vcpu(vcpu, VCPU_PAUSED);

	/* Must clear the signal before we mark req valid
	 * Once we mark to valid, VHM may process req and signal us
	 * before we perform upcall.
	 * because VHM can work in pulling mode without wait for upcall
	 */
	req_buf->req_queue[cur].valid = true;

	acrn_print_request(vcpu->vcpu_id, req_buf->req_queue + cur);

	/* signal VHM */
	fire_vhm_interrupt();

	return 0;
}

static void _get_req_info_(struct vhm_request *req, int *id, char *type,
	char *state, char *dir, long *addr, long *val)
{
	strcpy_s(dir, 16, "NONE");
	*addr = *val = 0;
	*id = req->client;

	switch (req->type) {
	case REQ_PORTIO:
		strcpy_s(type, 16, "PORTIO");
		if (req->reqs.pio_request.direction == REQUEST_READ)
			strcpy_s(dir, 16, "READ");
		else
			strcpy_s(dir, 16, "WRITE");
		*addr = req->reqs.pio_request.address;
		*val = req->reqs.pio_request.value;
		break;
	case REQ_MMIO:
	case REQ_WP:
		strcpy_s(type, 16, "MMIO/WP");
		if (req->reqs.mmio_request.direction == REQUEST_READ)
			strcpy_s(dir, 16, "READ");
		else
			strcpy_s(dir, 16, "WRITE");
		*addr = req->reqs.mmio_request.address;
		*val = req->reqs.mmio_request.value;
		break;
		break;
	default:
		strcpy_s(type, 16, "UNKNOWN");
	}

	switch (req->processed) {
	case REQ_STATE_SUCCESS:
		strcpy_s(state, 16, "SUCCESS");
		break;
	case REQ_STATE_PENDING:
		strcpy_s(state, 16, "PENDING");
		break;
	case REQ_STATE_PROCESSING:
		strcpy_s(state, 16, "PROCESS");
		break;
	case REQ_STATE_FAILED:
		strcpy_s(state, 16, "FAILED");
		break;
	default:
		strcpy_s(state, 16,  "UNKNOWN");
	}
}

int get_req_info(char *str, int str_max)
{
	int i, len, size = str_max, client_id;
	union vhm_request_buffer *req_buf;
	struct vhm_request *req;
	char type[16], state[16], dir[16];
	long addr, val;
	struct list_head *pos;
	struct vm *vm;

	len = snprintf(str, size,
		"\r\nVM\tVCPU\tCID\tTYPE\tSTATE\tDIR\tADDR\t\t\tVAL");
	size -= len;
	str += len;

	spinlock_obtain(&vm_list_lock);
	list_for_each(pos, &vm_list) {
		vm = list_entry(pos, struct vm, list);
		req_buf = (union vhm_request_buffer *)vm->sw.io_shared_page;
		if (req_buf) {
			for (i = 0; i < VHM_REQUEST_MAX; i++) {
				req = req_buf->req_queue + i;
				if (req->valid) {
					_get_req_info_(req, &client_id, type,
						state, dir, &addr, &val);
					len = snprintf(str, size,
						"\r\n%d\t%d\t%d\t%s\t%s\t%s",
						vm->attr.id, i, client_id, type,
						state, dir);
					size -= len;
					str += len;

					len = snprintf(str, size,
						"\t0x%016llx\t0x%016llx",
						addr, val);
					size -= len;
					str += len;
				}
			}
		}
	}
	spinlock_release(&vm_list_lock);
	snprintf(str, size, "\r\n");
	return 0;
}
