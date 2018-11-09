/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IOREQ_H
#define IOREQ_H

#include <types.h>
#include <acrn_common.h>

/**
 * @brief I/O Emulation
 *
 * @defgroup ioemul ACRN I/O Emulation
 * @{
 */

/* The return value of emulate_io() indicating the I/O request is delivered to
 * VHM but not finished yet. */
#define IOREQ_PENDING	1

/**
 * @brief Internal representation of a I/O request.
 */
struct io_request {
	/**
	 * @brief Type of the request (PIO, MMIO, etc).
	 *
	 * Refer to vhm_request for detailed description of I/O request types.
	 */
	uint32_t type;

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
	uint32_t flags;		/**< IO port attributes */
};

struct vm_io_handler_desc;
struct acrn_vm;
struct acrn_vcpu;

typedef
uint32_t (*io_read_fn_t)(struct acrn_vm *vm, uint16_t port, size_t size);

typedef
void (*io_write_fn_t)(struct acrn_vm *vm, uint16_t port, size_t size, uint32_t val);

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


#define IO_ATTR_R               0U
#define IO_ATTR_RW              1U
#define IO_ATTR_NO_ACCESS       2U

/* Typedef for MMIO handler and range check routine */
struct mmio_request;
typedef int (*hv_mem_io_handler_t)(struct io_request *io_req, void *handler_private_data);

/**
 * @brief Structure for MMIO handler node
 */
struct mem_io_node {
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
 * @brief The handler of VM exits on I/O instructions
 *
 * @param vcpu The virtual CPU which triggers the VM exit on I/O instruction
 */
int32_t pio_instr_vmexit_handler(struct acrn_vcpu *vcpu);

/**
 * @brief Initialize the I/O bitmap for \p vm
 *
 * @param vm The VM whose I/O bitmaps are to be initialized
 */
void   setup_io_bitmap(struct acrn_vm *vm);

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
void   allow_guest_pio_access(struct acrn_vm *vm, uint16_t port_address,
		uint32_t nbytes);

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
void   register_io_emulation_handler(struct acrn_vm *vm, uint32_t pio_idx,
		const struct vm_io_range *range, io_read_fn_t io_read_fn_ptr, io_write_fn_t io_write_fn_ptr);

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
	uint64_t end, void *handler_private_data);

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
void emulate_mmio_post(const struct acrn_vcpu *vcpu, const struct io_request *io_req);

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
void dm_emulate_mmio_post(struct acrn_vcpu *vcpu);

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
int32_t emulate_io(struct acrn_vcpu *vcpu, struct io_request *io_req);

/**
 * @brief General post-work for all kinds of VHM requests for I/O emulation
 *
 * @param vcpu The virtual CPU that triggers the MMIO access
 */
void emulate_io_post(struct acrn_vcpu *vcpu);

/**
 * @brief Deliver \p io_req to SOS and suspend \p vcpu till its completion
 *
 * @param vcpu The virtual CPU that triggers the MMIO access
 * @param io_req The I/O request holding the details of the MMIO access
 *
 * @pre vcpu != NULL && io_req != NULL
 */
int32_t acrn_insert_request_wait(struct acrn_vcpu *vcpu, const struct io_request *io_req);

/**
 * @}
 */

#endif /* IOREQ_H */
