/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IO_EMUL_H
#define IO_EMUL_H


/* Define emulated port IO index */
#define PIC_MASTER_PIO_IDX	0U
#define PIC_SLAVE_PIO_IDX	(PIC_MASTER_PIO_IDX + 1U)
#define PIC_ELC_PIO_IDX		(PIC_SLAVE_PIO_IDX + 1U)
#define PCI_CFGADDR_PIO_IDX	(PIC_ELC_PIO_IDX + 1U)
#define PCI_CFGDATA_PIO_IDX	(PCI_CFGADDR_PIO_IDX + 1U)
#define UART_PIO_IDX		(PCI_CFGDATA_PIO_IDX + 1U)
#define PM1A_EVT_PIO_IDX	(UART_PIO_IDX + 1U)
#define PM1A_CNT_PIO_IDX	(PM1A_EVT_PIO_IDX + 1U)
#define PM1B_EVT_PIO_IDX	(PM1A_CNT_PIO_IDX + 1U)
#define PM1B_CNT_PIO_IDX	(PM1B_EVT_PIO_IDX + 1U)
#define RTC_PIO_IDX		(PM1B_CNT_PIO_IDX + 1U)
#define EMUL_PIO_IDX_MAX	(RTC_PIO_IDX + 1U)

/**
 * @brief The handler of VM exits on I/O instructions
 *
 * @param vcpu The virtual CPU which triggers the VM exit on I/O instruction
 */
int32_t pio_instr_vmexit_handler(struct acrn_vcpu *vcpu);

/**
 * @brief EPT violation handling
 *
 * @param[in] vcpu the pointer that points to vcpu data structure
 *
 * @retval -EINVAL fail to handle the EPT violation
 * @retval 0 Success to handle the EPT violation
 */
int32_t ept_violation_vmexit_handler(struct acrn_vcpu *vcpu);

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
void   allow_guest_pio_access(struct acrn_vm *vm, uint16_t port_address, uint32_t nbytes);

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
 * This API registers a MMIO handler to \p vm before it is launched.
 *
 * @param vm The VM to which the MMIO handler is registered
 * @param read_write The handler for emulating accesses to the given range
 * @param start The base address of the range \p read_write can emulate
 * @param end The end of the range (exclusive) \p read_write can emulate
 * @param handler_private_data Handler-specific data which will be passed to \p read_write when called
 *
 * @retval 0 Registration succeeds
 * @retval -EINVAL \p read_write is NULL, \p end is not larger than \p start or \p vm has been launched
 */
int32_t register_mmio_emulation_handler(struct acrn_vm *vm,
	hv_mem_io_handler_t read_write, uint64_t start,
	uint64_t end, void *handler_private_data);

#endif /* IO_EMUL_H */
