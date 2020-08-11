/*
 * Copyright (C) 2019 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <irq.h>
#include <errno.h>
#include <logmsg.h>

#define DBG_LEVEL_IOREQ	6U

static uint32_t acrn_vhm_notification_vector = HYPERVISOR_CALLBACK_VHM_VECTOR;
#define MMIO_DEFAULT_VALUE_SIZE_1	(0xFFUL)
#define MMIO_DEFAULT_VALUE_SIZE_2	(0xFFFFUL)
#define MMIO_DEFAULT_VALUE_SIZE_4	(0xFFFFFFFFUL)
#define MMIO_DEFAULT_VALUE_SIZE_8	(0xFFFFFFFFFFFFFFFFUL)

#if defined(HV_DEBUG)
__unused static void acrn_print_request(uint16_t vcpu_id, const struct vhm_request *req)
{
	switch (req->type) {
	case REQ_MMIO:
		dev_dbg(DBG_LEVEL_IOREQ, "[vcpu_id=%hu type=MMIO]", vcpu_id);
		dev_dbg(DBG_LEVEL_IOREQ,
			"gpa=0x%lx, R/W=%d, size=%ld value=0x%lx processed=%lx",
			req->reqs.mmio.address,
			req->reqs.mmio.direction,
			req->reqs.mmio.size,
			req->reqs.mmio.value,
			req->processed);
		break;
	case REQ_PORTIO:
		dev_dbg(DBG_LEVEL_IOREQ, "[vcpu_id=%hu type=PORTIO]", vcpu_id);
		dev_dbg(DBG_LEVEL_IOREQ,
			"IO=0x%lx, R/W=%d, size=%ld value=0x%lx processed=%lx",
			req->reqs.pio.address,
			req->reqs.pio.direction,
			req->reqs.pio.size,
			req->reqs.pio.value,
			req->processed);
		break;
	default:
		dev_dbg(DBG_LEVEL_IOREQ, "[vcpu_id=%hu type=%d] NOT support type",
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

static inline bool has_complete_ioreq(const struct acrn_vcpu *vcpu)
{
	return (get_vhm_req_state(vcpu->vm, vcpu->vcpu_id) == REQ_STATE_COMPLETE);
}

/**
 * @brief Deliver \p io_req to SOS and suspend \p vcpu till its completion
 *
 * @param vcpu The virtual CPU that triggers the MMIO access
 * @param io_req The I/O request holding the details of the MMIO access
 *
 * @pre vcpu != NULL && io_req != NULL
 */
int32_t acrn_insert_request(struct acrn_vcpu *vcpu, const struct io_request *io_req)
{
	union vhm_request_buffer *req_buf = NULL;
	struct vhm_request *vhm_req;
	bool is_polling = false;
	int32_t ret = 0;
	uint16_t cur;

	if ((vcpu->vm->sw.io_shared_page != NULL)
		 && (get_vhm_req_state(vcpu->vm, vcpu->vcpu_id) == REQ_STATE_FREE)) {

		req_buf = (union vhm_request_buffer *)(vcpu->vm->sw.io_shared_page);
		cur = vcpu->vcpu_id;

		stac();
		vhm_req = &req_buf->req_queue[cur];
		/* ACRN insert request to VHM and inject upcall */
		vhm_req->type = io_req->io_type;
		(void)memcpy_s(&vhm_req->reqs, sizeof(union vhm_io_request),
			&io_req->reqs, sizeof(union vhm_io_request));
		if (vcpu->vm->sw.is_polling_ioreq) {
			vhm_req->completion_polling = 1U;
			is_polling = true;
		}
		clac();

		/* Before updating the vhm_req state, enforce all fill vhm_req operations done */
		cpu_write_memory_barrier();

		/* Must clear the signal before we mark req as pending
		 * Once we mark it pending, VHM may process req and signal us
		 * before we perform upcall.
		 * because VHM can work in pulling mode without wait for upcall
		 */
		set_vhm_req_state(vcpu->vm, vcpu->vcpu_id, REQ_STATE_PENDING);

		/* signal VHM */
		arch_fire_vhm_interrupt();

		/* Polling completion of the request in polling mode */
		if (is_polling) {
			while (true) {
				if (has_complete_ioreq(vcpu)) {
					/* we have completed ioreq pending */
					break;
				}
				asm_pause();
				if (need_reschedule(pcpuid_from_vcpu(vcpu))) {
					schedule();
				}
			}
		} else {
			wait_event(&vcpu->events[VCPU_EVENT_IOREQ]);
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}

uint32_t get_vhm_req_state(struct acrn_vm *vm, uint16_t vhm_req_id)
{
	uint32_t state;
	union vhm_request_buffer *req_buf = NULL;
	struct vhm_request *vhm_req;

	req_buf = (union vhm_request_buffer *)vm->sw.io_shared_page;
	if (req_buf == NULL) {
	        state =  0xffffffffU;
	} else {
		stac();
		vhm_req = &req_buf->req_queue[vhm_req_id];
		state = vhm_req->processed;
		clac();
	}

	return state;
}

void set_vhm_req_state(struct acrn_vm *vm, uint16_t vhm_req_id, uint32_t state)
{
	union vhm_request_buffer *req_buf = NULL;
	struct vhm_request *vhm_req;

	req_buf = (union vhm_request_buffer *)vm->sw.io_shared_page;
	if (req_buf != NULL) {
		stac();
		vhm_req = &req_buf->req_queue[vhm_req_id];
		/*
		 * HV will only set processed to REQ_STATE_PENDING or REQ_STATE_FREE.
		 * we don't need to sfence here is that even if the SOS/DM sees the previous state,
		 * the only side effect is that it will defer the processing of the new IOReq.
		 * It won't lead wrong processing.
		 */
		vhm_req->processed = state;
		clac();
	}
}

void set_vhm_notification_vector(uint32_t vector)
{
	acrn_vhm_notification_vector = vector;
}

uint32_t get_vhm_notification_vector(void)
{
	return acrn_vhm_notification_vector;
}

/**
 * @brief General complete-work for MMIO emulation
 *
 * @param vcpu The virtual CPU that triggers the MMIO access
 * @param io_req The I/O request holding the details of the MMIO access
 *
 * @pre io_req->io_type == REQ_MMIO
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

static void complete_ioreq(struct acrn_vcpu *vcpu, struct io_request *io_req)
{
	union vhm_request_buffer *req_buf = NULL;
	struct vhm_request *vhm_req;

	req_buf = (union vhm_request_buffer *)(vcpu->vm->sw.io_shared_page);

	stac();
	vhm_req = &req_buf->req_queue[vcpu->vcpu_id];
	if (io_req != NULL) {
		switch (vcpu->req.io_type) {
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

	/*
	 * Only HV will check whether processed is REQ_STATE_FREE on per-vCPU before inject a ioreq.
	 * Only HV will set processed to REQ_STATE_FREE when ioreq is done.
	 */
	vhm_req->processed = REQ_STATE_FREE;
	clac();
}

/**
 * @brief Complete-work of VHM requests for port I/O emulation
 *
 * @pre vcpu->req.io_type == REQ_PORTIO
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
 * @pre vcpu->req.io_type == REQ_MMIO
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
			switch (vcpu->req.io_type) {
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

/**
 * @pre width < 8U
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool pio_default_read(struct acrn_vcpu *vcpu,
	__unused uint16_t addr, size_t width)
{
	struct pio_request *pio_req = &vcpu->req.reqs.pio;

	pio_req->value = (uint32_t)((1UL << (width * 8U)) - 1UL);

	return true;
}

/**
 * @pre width < 8U
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool pio_default_write(__unused struct acrn_vcpu *vcpu, __unused uint16_t addr,
	__unused size_t width, __unused uint32_t v)
{
	return true; /* ignore write */
}

/**
 * @pre (io_req->reqs.mmio.size == 1U) || (io_req->reqs.mmio.size == 2U) ||
 *      (io_req->reqs.mmio.size == 4U) || (io_req->reqs.mmio.size == 8U)
 */
static int32_t mmio_default_access_handler(struct io_request *io_req,
	__unused void *handler_private_data)
{
	struct mmio_request *mmio = &io_req->reqs.mmio;

	if (mmio->direction == REQUEST_READ) {
		switch (mmio->size) {
		case 1U:
			mmio->value = MMIO_DEFAULT_VALUE_SIZE_1;
			break;
		case 2U:
			mmio->value = MMIO_DEFAULT_VALUE_SIZE_2;
			break;
		case 4U:
			mmio->value = MMIO_DEFAULT_VALUE_SIZE_4;
			break;
		case 8U:
			mmio->value = MMIO_DEFAULT_VALUE_SIZE_8;
			break;
		default:
			/* This case is unreachable, this is guaranteed by the design. */
			break;
		}
	}

	return 0;
}

/**
 * Try handling the given request by any port I/O handler registered in the
 * hypervisor.
 *
 * @pre io_req->io_type == REQ_PORTIO
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
	io_read_fn_t io_read = NULL;
	io_write_fn_t io_write = NULL;

	if (is_sos_vm(vcpu->vm) || is_prelaunched_vm(vcpu->vm)) {
		io_read = pio_default_read;
		io_write = pio_default_write;
	}

	port = (uint16_t)pio_req->address;
	size = (uint16_t)pio_req->size;

	for (idx = 0U; idx < EMUL_PIO_IDX_MAX; idx++) {
		handler = &(vm->emul_pio[idx]);

		if ((port < handler->port_start) || (port >= handler->port_end)) {
			continue;
		}

		if (handler->io_read != NULL) {
			io_read = handler->io_read;
		}
		if (handler->io_write != NULL) {
			io_write = handler->io_write;
		}
		break;
	}

	if ((pio_req->direction == REQUEST_WRITE) && (io_write != NULL)) {
		if (io_write(vcpu, port, size, pio_req->value)) {
			status = 0;
		}
	} else if ((pio_req->direction == REQUEST_READ) && (io_read != NULL)) {
		if (io_read(vcpu, port, size)) {
			status = 0;
		}
	} else {
		/* do nothing */
	}

	pr_dbg("IO %s on port %04x, data %08x",
		(pio_req->direction == REQUEST_READ) ? "read" : "write", port, pio_req->value);

	return status;
}

/**
 * Use registered MMIO handlers on the given request if it falls in the range of
 * any of them.
 *
 * @pre io_req->io_type == REQ_MMIO
 *
 * @retval 0 Successfully emulated by registered handlers.
 * @retval -ENODEV No proper handler found.
 * @retval -EIO The request spans multiple devices and cannot be emulated.
 */
static int32_t
hv_emulate_mmio(struct acrn_vcpu *vcpu, struct io_request *io_req)
{
	int32_t status = -ENODEV;
	bool hold_lock = true;
	uint16_t idx;
	uint64_t address, size, base, end;
	struct mmio_request *mmio_req = &io_req->reqs.mmio;
	struct mem_io_node *mmio_handler = NULL;
	hv_mem_io_handler_t read_write = NULL;
	void *handler_private_data = NULL;

	if (is_sos_vm(vcpu->vm) || is_prelaunched_vm(vcpu->vm)) {
		read_write = mmio_default_access_handler;
	}

	address = mmio_req->address;
	size = mmio_req->size;

	spinlock_obtain(&vcpu->vm->emul_mmio_lock);
	for (idx = 0U; idx <= vcpu->vm->nr_emul_mmio_regions; idx++) {
		mmio_handler = &(vcpu->vm->emul_mmio[idx]);
		if (mmio_handler->read_write != NULL) {
			base = mmio_handler->range_start;
			end = mmio_handler->range_end;

			if (((address + size) <= base) || (address >= end)) {
				continue;
			} else {
				 if ((address >= base) && ((address + size) <= end)) {
					hold_lock = mmio_handler->hold_lock;
					read_write = mmio_handler->read_write;
					handler_private_data = mmio_handler->handler_private_data;
				} else {
					pr_fatal("Err MMIO, address:0x%lx, size:%x", address, size);
					status = -EIO;
				}
				break;
			}
		}
	}

	if ((status == -ENODEV) && (read_write != NULL)) {
		/* This mmio_handler will never modify once register, so we don't
		 * need to hold the lock when handling the MMIO access.
		 */
		if (!hold_lock) {
			spinlock_release(&vcpu->vm->emul_mmio_lock);
		}
		status = read_write(io_req, handler_private_data);
		if (!hold_lock) {
			spinlock_obtain(&vcpu->vm->emul_mmio_lock);
		}
	}
	spinlock_release(&vcpu->vm->emul_mmio_lock);

	return status;
}

/**
 * @brief Emulate \p io_req for \p vcpu
 *
 * Handle an I/O request by either invoking a hypervisor-internal handler or
 * deliver to VHM.
 *
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 * @pre vcpu->vm->vm_id < CONFIG_MAX_VM_NUM
 *
 * @param vcpu The virtual CPU that triggers the MMIO access
 * @param io_req The I/O request holding the details of the MMIO access
 *
 * @retval 0 Successfully emulated by registered handlers.
 * @retval IOREQ_PENDING The I/O request is delivered to VHM.
 * @retval -EIO The request spans multiple devices and cannot be emulated.
 * @retval -EINVAL \p io_req has an invalid io_type.
 * @retval <0 on other errors during emulation.
 */
int32_t
emulate_io(struct acrn_vcpu *vcpu, struct io_request *io_req)
{
	int32_t status;
	struct acrn_vm_config *vm_config;

	vm_config = get_vm_config(vcpu->vm->vm_id);

	switch (io_req->io_type) {
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
		/* Unknown I/O request io_type */
		status = -EINVAL;
		break;
	}

	if ((status == -ENODEV) && (vm_config->load_order == POST_LAUNCHED_VM)) {
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

			pr_fatal("%s Err: access dir %d, io_type %d, addr = 0x%lx, size=%lu", __func__,
				pio_req->direction, io_req->io_type,
				pio_req->address, pio_req->size);
		}
	}

	return status;
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
	vm->emul_pio[pio_idx].port_start = range->base;
	vm->emul_pio[pio_idx].port_end = range->base + range->len;
	vm->emul_pio[pio_idx].io_read = io_read_fn_ptr;
	vm->emul_pio[pio_idx].io_write = io_write_fn_ptr;
}

/**
 * @brief Find match MMIO node
 *
 * This API find match MMIO node from \p vm.
 *
 * @param vm The VM to which the MMIO node is belong to.
 *
 * @return If there's a match mmio_node return it, otherwise return NULL;
 */
static inline struct mem_io_node *find_match_mmio_node(struct acrn_vm *vm,
				uint64_t start, uint64_t end)
{
	bool found = false;
	uint16_t idx;
	struct mem_io_node *mmio_node;

	for (idx = 0U; idx < CONFIG_MAX_EMULATED_MMIO_REGIONS; idx++) {
		mmio_node = &(vm->emul_mmio[idx]);
		if ((mmio_node->range_start == start) && (mmio_node->range_end == end)) {
			found = true;
			break;
		}
	}

	if (!found) {
		pr_info("%s, vm[%d] no match mmio region [0x%lx, 0x%lx] is found",
				__func__, vm->vm_id, start, end);
		mmio_node = NULL;
	}

	return mmio_node;
}

/**
 * @brief Find a free MMIO node
 *
 * This API find a free MMIO node from \p vm.
 *
 * @param vm The VM to which the MMIO node is belong to.
 *
 * @return If there's a free mmio_node return it, otherwise return NULL;
 */
static inline struct mem_io_node *find_free_mmio_node(struct acrn_vm *vm)
{
	uint16_t idx;
	struct mem_io_node *mmio_node = find_match_mmio_node(vm, 0UL, 0UL);

	if (mmio_node != NULL) {
		idx = (uint16_t)(uint64_t)(mmio_node - &(vm->emul_mmio[0U]));
		if (vm->nr_emul_mmio_regions < idx) {
			vm->nr_emul_mmio_regions = idx;
		}
	}

	return mmio_node;
}

/**
 * @brief Register a MMIO handler
 *
 * This API registers a MMIO handler to \p vm
 *
 * @param vm The VM to which the MMIO handler is registered
 * @param read_write The handler for emulating accesses to the given range
 * @param start The base address of the range \p read_write can emulate
 * @param end The end of the range (exclusive) \p read_write can emulate
 * @param handler_private_data Handler-specific data which will be passed to \p read_write when called
 *
 * @return None
 */
void register_mmio_emulation_handler(struct acrn_vm *vm,
	hv_mem_io_handler_t read_write, uint64_t start,
	uint64_t end, void *handler_private_data, bool hold_lock)
{
	struct mem_io_node *mmio_node;

	/* Ensure both a read/write handler and range check function exist */
	if ((read_write != NULL) && (end > start)) {
		spinlock_obtain(&vm->emul_mmio_lock);
		mmio_node = find_free_mmio_node(vm);
		if (mmio_node != NULL) {
			/* Fill in information for this node */
			mmio_node->hold_lock = hold_lock;
			mmio_node->read_write = read_write;
			mmio_node->handler_private_data = handler_private_data;
			mmio_node->range_start = start;
			mmio_node->range_end = end;
		}
		spinlock_release(&vm->emul_mmio_lock);
	}

}

/**
 * @brief Unregister a MMIO handler
 *
 * This API unregisters a MMIO handler to \p vm
 *
 * @param vm The VM to which the MMIO handler is unregistered
 * @param start The base address of the range which wants to unregister
 * @param end The end of the range (exclusive) which wants to unregister
 *
 * @return None
 */
void unregister_mmio_emulation_handler(struct acrn_vm *vm,
					uint64_t start, uint64_t end)
{
	struct mem_io_node *mmio_node;

	spinlock_obtain(&vm->emul_mmio_lock);
	mmio_node = find_match_mmio_node(vm, start, end);
	if (mmio_node != NULL) {
		(void)memset(mmio_node, 0U, sizeof(struct mem_io_node));
	}
	spinlock_release(&vm->emul_mmio_lock);
}

void deinit_emul_io(struct acrn_vm *vm)
{
	(void)memset(vm->emul_mmio, 0U, sizeof(vm->emul_mmio));
	(void)memset(vm->emul_pio, 0U, sizeof(vm->emul_pio));
}
