/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IOREQ_H
#define IOREQ_H

#include <types.h>
#include <acrn_common.h>
#include <list.h>

/**
 * @brief I/O Emulation
 *
 * @defgroup ioemul ACRN I/O Emulation
 * @{
 */

/**
 * @brief Internal representation of a I/O request.
 */
struct io_request {
	/**
	 * @brief Type of the request (PIO, MMIO, etc).
	 *
	 * Refer to vhm_request for detailed description of I/O request types.
	 */
	uint32_t io_type;

	/**
	 * @brief Details of this request in the same format as vhm_request.
	 */
	union vhm_io_request reqs;
};

/**
 * @brief Definition of a IO port range
 */
struct vm_io_range {
	uint16_t base;		/**< IO port base */
	uint16_t len;		/**< IO port range */
};

struct vm_io_handler_desc;
struct acrn_vm;
struct acrn_vcpu;

typedef
bool (*io_read_fn_t)(struct acrn_vcpu *vcpu, uint16_t port, size_t size);

typedef
bool (*io_write_fn_t)(struct acrn_vcpu *vcpu, uint16_t port, size_t size, uint32_t val);

/**
 * @brief Describes a single IO handler description entry.
 */
struct vm_io_handler_desc {

	/**
	 * @brief The base port number of the IO range for this description.
	 */
	uint16_t port_start;

	/**
	 * @brief The last port number of the IO range for this description (non-inclusive).
	 */
	uint16_t port_end;

	/**
	 * @brief A pointer to the "read" function.
	 *
	 * The read function is called from the hypervisor whenever
	 * a read access to a range described in "ranges" occur.
	 * The arguments to the callback are:
	 *
	 *    - The address of the port to read from.
	 *    - The width of the read operation (1,2 or 4).
	 *
	 * The implementation must return the ports content as
	 * byte, word or doubleword (depending on the width).
	 *
	 * If the pointer is null, a read of 1's is assumed.
	 */
	io_read_fn_t io_read;

	/**
	 * @brief A pointer to the "write" function.
	 *
	 * The write function is called from the hypervisor code
	 * whenever a write access to a range described in "ranges"
	 * occur. The arguments to the callback are:
	 *
	 *   - The address of the port to write to.
	 *   - The width of the write operation (1,2 or 4).
	 *   - The value to write as byte, word or doubleword
	 *     (depending on the width)
	 *
	 * The implementation must write the value to the port.
	 *
	 * If the pointer is null, the write access is ignored.
	 */
	io_write_fn_t io_write;
};

/* Typedef for MMIO handler and range check routine */
struct mmio_request;
typedef int32_t (*hv_mem_io_handler_t)(struct io_request *io_req, void *handler_private_data);

/**
 * @brief Structure for MMIO handler node
 */
struct mem_io_node {

	/**
	 * @brief Whether the lock needs to hold when handle the MMIO access
	 */
	bool hold_lock;


	/**
	 * @brief A pointer to the handler
	 *
	 * The function for handling MMIO accesses to the specified range.
	 */
	hv_mem_io_handler_t read_write;

	/**
	 * @brief Private data used by the handler
	 *
	 * The pointer to any data specified at registration. This pointer is
	 * passed to the handler whenever the handler is called.
	 */
	void *handler_private_data;

	/**
	 * @brief The struct to make a bi-directional linked list
	 */
	struct list_head list;

	/**
	 * @brief The starting address
	 *
	 * This member is used in pair with \p range_end. See the documentation
	 * of \p range_end for details.
	 */
	uint64_t range_start;

	/**
	 * @brief The ending address
	 *
	 * \p range_start (inclusive) and \p range_end (exclusive) together
	 * specify the address range that this handler is expected to
	 * emulate. Note that the bytes to be accessed shall completely fall in
	 * the range before the handler is called to emulate that access, or
	 * more specifically
	 *
	 *    \p range_start <= address < address + size <= \p end
	 *
	 * where address and size are the starting address of the MMIO access
	 * and the number of bytes to be accessed, respectively. Otherwise the
	 * behavior is undefined.
	 */
	uint64_t range_end;
};

/* External Interfaces */

/**
 * @brief Deliver \p io_req to SOS and suspend \p vcpu till its completion
 *
 * @param vcpu The virtual CPU that triggers the MMIO access
 * @param io_req The I/O request holding the details of the MMIO access
 *
 * @pre vcpu != NULL && io_req != NULL
 */
int32_t acrn_insert_request(struct acrn_vcpu *vcpu, const struct io_request *io_req);

/**
 * @brief Reset all IO requests status of the VM
 *
 * @param vm The VM whose IO requests to be reset
 *
 * @return None
 */
void reset_vm_ioreqs(struct acrn_vm *vm);

/**
 * @brief Get the state of VHM request
 *
 * @param vm Target VM context
 * @param vhm_req_id VHM Request ID
 *
 * @return State of the IO Request.
 */
uint32_t get_vhm_req_state(struct acrn_vm *vm, uint16_t vhm_req_id);

/**
 * @brief Set the state of VHM request
 *
 * @param vm Target VM context
 * @param vhm_req_id VHM Request ID
 * @param state  State to be set
 * @return None
 */
void set_vhm_req_state(struct acrn_vm *vm, uint16_t vhm_req_id, uint32_t state);

/**
 * @brief Set the vector for HV callback VHM
 *
 * @param vector vector for HV callback VHM
 * @return None
 */
void set_vhm_notification_vector(uint32_t vector);

/**
 * @brief Get the vector for HV callback VHM
 *
 * @return vector for HV callbakc VH
 */
uint32_t get_vhm_notification_vector(void);

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
 * @retval -EINVAL \p io_req has an invalid type.
 * @retval <0 on other errors during emulation.
 */
int32_t emulate_io(struct acrn_vcpu *vcpu, struct io_request *io_req);

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
void   register_pio_emulation_handler(struct acrn_vm *vm, uint32_t pio_idx,
		const struct vm_io_range *range, io_read_fn_t io_read_fn_ptr, io_write_fn_t io_write_fn_ptr);

/**
 * @brief Register a MMIO handler
 *
 * This API registers a MMIO handler to \p vm.
 *
 * @param vm The VM to which the MMIO handler is registered
 * @param read_write The handler for emulating accesses to the given range
 * @param start The base address of the range \p read_write can emulate
 * @param end The end of the range (exclusive) \p read_write can emulate
 * @param handler_private_data Handler-specific data which will be passed to \p read_write when called
 * @param hold_lock Whether hold the lock to handle the MMIO access
 *
 * @return None
 */
void register_mmio_emulation_handler(struct acrn_vm *vm,
	hv_mem_io_handler_t read_write, uint64_t start,
	uint64_t end, void *handler_private_data, bool hold_lock);

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
					uint64_t start, uint64_t end);
void deinit_emul_io(struct acrn_vm *vm);
/**
 * @}
 */

#endif /* IOREQ_H */
