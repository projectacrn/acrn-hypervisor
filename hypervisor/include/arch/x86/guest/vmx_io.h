/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IO_EMUL_H
#define IO_EMUL_H

#include <types.h>

/* Define emulated port IO index */
#define PIC_MASTER_PIO_IDX		0U
#define PIC_SLAVE_PIO_IDX		(PIC_MASTER_PIO_IDX + 1U)
#define PIC_ELC_PIO_IDX			(PIC_SLAVE_PIO_IDX + 1U)
#define PCI_CFGADDR_PIO_IDX		(PIC_ELC_PIO_IDX + 1U)
#define PCI_CFGDATA_PIO_IDX		(PCI_CFGADDR_PIO_IDX + 1U)
#define UART_PIO_IDX			(PCI_CFGDATA_PIO_IDX + 1U)
#define PM1A_EVT_PIO_IDX		(UART_PIO_IDX + 1U)
#define PM1A_CNT_PIO_IDX		(PM1A_EVT_PIO_IDX + 1U)
#define PM1B_EVT_PIO_IDX		(PM1A_CNT_PIO_IDX + 1U)
#define PM1B_CNT_PIO_IDX		(PM1B_EVT_PIO_IDX + 1U)
#define RTC_PIO_IDX			(PM1B_CNT_PIO_IDX + 1U)
#define VIRTUAL_PM1A_CNT_PIO_IDX	(RTC_PIO_IDX + 1U)
#define EMUL_PIO_IDX_MAX		(VIRTUAL_PM1A_CNT_PIO_IDX + 1U)

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
 * @brief General complete-work for port I/O emulation
 *
 * @pre io_req->type == REQ_PORTIO
 *
 * @remark This function must be called when \p io_req is completed, after
 * either a previous call to emulate_io() returning 0 or the corresponding VHM
 * request having transferred to the COMPLETE state.
 */
void emulate_pio_complete(struct acrn_vcpu *vcpu, const struct io_request *io_req);

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
 * @brief Allow a VM to access a port I/O range
 *
 * This API enables direct access from the given \p vm to the port I/O space
 * starting from \p port_address to \p port_address + \p nbytes - 1.
 *
 * @param vm The VM whose port I/O access permissions is to be changed
 * @param port_address The start address of the port I/O range
 * @param nbytes The size of the range, in bytes
 */
void deny_guest_pio_access(struct acrn_vm *vm, uint16_t port_address, uint32_t nbytes);

/**
 * @brief Fire VHM interrupt to SOS
 *
 * @return None
 */
void arch_fire_vhm_interrupt(void);

#endif /* IO_EMUL_H */
