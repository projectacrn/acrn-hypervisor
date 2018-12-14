/*
 * Copyright (C) 2018 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#define ACRN_DBG_IOREQUEST	6U

uint32_t acrn_vhm_vector = VECTOR_VIRT_IRQ_VHM;

static void fire_vhm_interrupt(void)
{
	/*
	 * use vLAPIC to inject vector to SOS vcpu 0 if vlapic is enabled
	 * otherwise, send IPI hardcoded to BOOT_CPU_ID
	 */
	struct acrn_vm *vm0;
	struct acrn_vcpu *vcpu;

	vm0 = get_vm_from_vmid(0U);

	vcpu = vcpu_from_vid(vm0, 0U);

	vlapic_intr_edge(vcpu, acrn_vhm_vector);
}

#if defined(HV_DEBUG)
static void acrn_print_request(uint16_t vcpu_id, const struct vhm_request *req)
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
#endif

/**
 * @brief Reset all IO requests status of the VM
 *
 * @param vm The VM whose IO requests to be reset
 *
 * @return None
 */
void reset_vm_ioreqs(struct acrn_vm *vm)
{
	uint16_t i;

	for (i = 0U; i < VHM_REQUEST_MAX; i++) {
		set_vhm_req_state(vm, i, REQ_STATE_FREE);
	}
}

static inline bool has_complete_ioreq(struct acrn_vcpu *vcpu)
{
	return (get_vhm_req_state(vcpu->vm, vcpu->vcpu_id) == REQ_STATE_COMPLETE);
}

/**
 * @brief Handle completed ioreq if any one pending
 *
 * @param pcpu_id The physical cpu id of vcpu whose IO request to be checked
 *
 * @return None
 */
void handle_complete_ioreq(uint16_t pcpu_id)
{
	struct acrn_vcpu *vcpu = get_ever_run_vcpu(pcpu_id);
	struct acrn_vm *vm;

	if (vcpu != NULL) {
		vm = vcpu->vm;
		if (vm->sw.is_completion_polling) {
			if (has_complete_ioreq(vcpu)) {
				/* we have completed ioreq pending */
				emulate_io_post(vcpu);
			}
		}
	}
}

/**
 * @brief Deliver \p io_req to SOS and suspend \p vcpu till its completion
 *
 * @param vcpu The virtual CPU that triggers the MMIO access
 * @param io_req The I/O request holding the details of the MMIO access
 *
 * @pre vcpu != NULL && io_req != NULL
 */
int32_t acrn_insert_request_wait(struct acrn_vcpu *vcpu, const struct io_request *io_req)
{
	union vhm_request_buffer *req_buf = NULL;
	struct vhm_request *vhm_req;
	uint16_t cur;

	if (vcpu->vm->sw.io_shared_page == NULL) {
		return -EINVAL;
	}

	ASSERT(get_vhm_req_state(vcpu->vm, vcpu->vcpu_id) == REQ_STATE_FREE,
		"VHM request buffer is busy");

	req_buf = (union vhm_request_buffer *)(vcpu->vm->sw.io_shared_page);
	cur = vcpu->vcpu_id;

	stac();
	vhm_req = &req_buf->req_queue[cur];
	/* ACRN insert request to VHM and inject upcall */
	vhm_req->type = io_req->type;
	(void)memcpy_s(&vhm_req->reqs, sizeof(union vhm_io_request),
		&io_req->reqs, sizeof(union vhm_io_request));
	if (vcpu->vm->sw.is_completion_polling) {
		vhm_req->completion_polling = 1U;
	}
	clac();

	/* pause vcpu, wait for VHM to handle the MMIO request.
	 * TODO: when pause_vcpu changed to switch vcpu out directlly, we
	 * should fix the race issue between req.processed update and vcpu pause
	 */
	pause_vcpu(vcpu, VCPU_PAUSED);

	/* Must clear the signal before we mark req as pending
	 * Once we mark it pending, VHM may process req and signal us
	 * before we perform upcall.
	 * because VHM can work in pulling mode without wait for upcall
	 */
	set_vhm_req_state(vcpu->vm, vcpu->vcpu_id, REQ_STATE_PENDING);

#if defined(HV_DEBUG)
	stac();
	acrn_print_request(vcpu->vcpu_id, vhm_req);
	clac();
#endif

	/* signal VHM */
	fire_vhm_interrupt();

	return 0;
}

uint32_t get_vhm_req_state(struct acrn_vm *vm, uint16_t vhm_req_id)
{
	uint32_t state;
	union vhm_request_buffer *req_buf = NULL;
	struct vhm_request *vhm_req;

	req_buf = (union vhm_request_buffer *)vm->sw.io_shared_page;
	if (req_buf == NULL) {
		return (uint32_t)-1;
	}

	stac();
	vhm_req = &req_buf->req_queue[vhm_req_id];
	state = atomic_load32(&vhm_req->processed);
	clac();

	return state;
}

void set_vhm_req_state(struct acrn_vm *vm, uint16_t vhm_req_id, uint32_t state)
{
	union vhm_request_buffer *req_buf = NULL;
	struct vhm_request *vhm_req;

	req_buf = (union vhm_request_buffer *)vm->sw.io_shared_page;
	if (req_buf == NULL) {
		return;
	}

	stac();
	vhm_req = &req_buf->req_queue[vhm_req_id];
	atomic_store32(&vhm_req->processed, state);
	clac();
}
