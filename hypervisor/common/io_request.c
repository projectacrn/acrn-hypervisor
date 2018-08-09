/*
 * Copyright (C) 2018 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#define ACRN_DBG_IOREQUEST	6U

static void fire_vhm_interrupt(void)
{
	/*
	 * use vLAPIC to inject vector to SOS vcpu 0 if vlapic is enabled
	 * otherwise, send IPI hardcoded to BOOT_CPU_ID
	 */
	struct vm *vm0;
	struct vcpu *vcpu;

	vm0 = get_vm_from_vmid(0U);
	ASSERT(vm0 != NULL, "VM Pointer is NULL");

	vcpu = vcpu_from_vid(vm0, 0U);
	ASSERT(vcpu != NULL, "vcpu_from_vid failed");

	vlapic_intr_edge(vcpu, VECTOR_VIRT_IRQ_VHM);
}

static void acrn_print_request(uint16_t vcpu_id, struct vhm_request *req)
{
	switch (req->type) {
	case REQ_MMIO:
		dev_dbg(ACRN_DBG_IOREQUEST, "[vcpu_id=%hu type=MMIO]", vcpu_id);
		dev_dbg(ACRN_DBG_IOREQUEST,
			"gpa=0x%lx, R/W=%d, size=%ld value=0x%lx processed=%lx",
			req->reqs.mmio.address,
			req->reqs.mmio.direction,
			req->reqs.mmio.size,
			req->reqs.mmio.value,
			req->processed);
		break;
	case REQ_PORTIO:
		dev_dbg(ACRN_DBG_IOREQUEST, "[vcpu_id=%hu type=PORTIO]", vcpu_id);
		dev_dbg(ACRN_DBG_IOREQUEST,
			"IO=0x%lx, R/W=%d, size=%ld value=0x%lx processed=%lx",
			req->reqs.pio.address,
			req->reqs.pio.direction,
			req->reqs.pio.size,
			req->reqs.pio.value,
			req->processed);
		break;
	default:
		dev_dbg(ACRN_DBG_IOREQUEST, "[vcpu_id=%hu type=%d] NOT support type",
			vcpu_id, req->type);
		break;
	}
}

int32_t
acrn_insert_request_wait(struct vcpu *vcpu, struct io_request *io_req)
{
	union vhm_request_buffer *req_buf = NULL;
	struct vhm_request *vhm_req;
	uint16_t cur;

	if ((vcpu == NULL) || (io_req == NULL) ||
		(vcpu->vm->sw.io_shared_page == NULL)) {
		return -EINVAL;
	}

	req_buf = (union vhm_request_buffer *)(vcpu->vm->sw.io_shared_page);
	cur = vcpu->vcpu_id;
	vhm_req = &req_buf->req_queue[cur];

	ASSERT(atomic_load32(&vhm_req->processed) == REQ_STATE_FREE,
		"VHM request buffer is busy");

	/* ACRN insert request to VHM and inject upcall */
	vhm_req->type = io_req->type;
	(void)memcpy_s(&vhm_req->reqs, sizeof(union vhm_io_request),
		&io_req->reqs, sizeof(union vhm_io_request));

	/* pause vcpu, wait for VHM to handle the MMIO request.
	 * TODO: when pause_vcpu changed to switch vcpu out directlly, we
	 * should fix the race issue between req.valid = true and vcpu pause
	 */
	pause_vcpu(vcpu, VCPU_PAUSED);

	/* Must clear the signal before we mark req as pending
	 * Once we mark it pending, VHM may process req and signal us
	 * before we perform upcall.
	 * because VHM can work in pulling mode without wait for upcall
	 */
	vhm_req->valid = 1;
	atomic_store32(&vhm_req->processed, REQ_STATE_PENDING);

	acrn_print_request(vcpu->vcpu_id, vhm_req);

	/* signal VHM */
	fire_vhm_interrupt();

	return 0;
}

#ifdef HV_DEBUG
static void local_get_req_info_(struct vhm_request *req, int *id, char *type,
	char *state, char *dir, uint64_t *addr, uint64_t *val)
{
	(void)strcpy_s(dir, 16U, "NONE");
	*addr = 0UL;
	*val = 0UL;
	*id = req->client;

	switch (req->type) {
	case REQ_PORTIO:
		(void)strcpy_s(type, 16U, "PORTIO");
		if (req->reqs.pio.direction == REQUEST_READ) {
			(void)strcpy_s(dir, 16U, "READ");
		} else {
			(void)strcpy_s(dir, 16U, "WRITE");
		}
		*addr = req->reqs.pio.address;
		*val = req->reqs.pio.value;
		break;
	case REQ_MMIO:
	case REQ_WP:
		(void)strcpy_s(type, 16U, "MMIO/WP");
		if (req->reqs.mmio.direction == REQUEST_READ) {
			(void)strcpy_s(dir, 16U, "READ");
		} else {
			(void)strcpy_s(dir, 16U, "WRITE");
		}
		*addr = req->reqs.mmio.address;
		*val = req->reqs.mmio.value;
		break;
		break;
	default:
		(void)strcpy_s(type, 16U, "UNKNOWN");
	}

	switch (req->processed) {
	case REQ_STATE_COMPLETE:
		(void)strcpy_s(state, 16U, "COMPLETE");
		break;
	case REQ_STATE_PENDING:
		(void)strcpy_s(state, 16U, "PENDING");
		break;
	case REQ_STATE_PROCESSING:
		(void)strcpy_s(state, 16U, "PROCESS");
		break;
	case REQ_STATE_FREE:
		(void)strcpy_s(state, 16U, "FREE");
		break;
	default:
		(void)strcpy_s(state, 16U,  "UNKNOWN");
	}
}

void get_req_info(char *str_arg, int str_max)
{
	char *str = str_arg;
	uint32_t i;
	int32_t len, size = str_max, client_id;
	union vhm_request_buffer *req_buf;
	struct vhm_request *req;
	char type[16], state[16], dir[16];
	uint64_t addr, val;
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
		if (req_buf != NULL) {
			for (i = 0U; i < VHM_REQUEST_MAX; i++) {
				req = req_buf->req_queue + i;
				if (req->valid != 0) {
					local_get_req_info_(req, &client_id, type,
						state, dir, &addr, &val);
					len = snprintf(str, size,
						"\r\n%d\t%d\t%d\t%s\t%s\t%s",
						vm->vm_id, i, client_id, type,
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
}
#endif /* HV_DEBUG */
