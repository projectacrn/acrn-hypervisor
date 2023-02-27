/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ASSIGN_H
#define ASSIGN_H

#include <types.h>
#include <ptdev.h>

/**
 * @file assign.h
 *
 * @brief public APIs for Passthrough Interrupt Remapping
 */

/**
 * @brief VT-d
 *
 * @defgroup acrn_passthrough ACRN Passthrough
 * @{
 */

/**
 * @brief Acknowledge a virtual interrupt for passthrough device.
 *
 * Acknowledge a virtual legacy interrupt for a passthrough device.
 *
 * @param[in] vm pointer to acrn_vm
 * @param[in] virt_gsi virtual GSI number associated with the passthrough device
 * @param[in] vgsi_ctlr INTX_CTLR_IOAPIC or INTX_CTLR_PIC
 *
 * @return None
 *
 * @pre vm != NULL
 *
 */
void ptirq_intx_ack(struct acrn_vm *vm, uint32_t virt_gsi, enum intx_ctlr vgsi_ctlr);

/**
 * @brief MSI/MSI-x remapping for passthrough device.
 *
 * Main entry for PCI device assignment with MSI and MSI-X.
 * MSI can up to 8 vectors and MSI-X can up to 1024 Vectors.
 *
 * @param[in] vm pointer to acrn_vm
 * @param[in] virt_bdf virtual bdf associated with the passthrough device
 * @param[in] phys_bdf virtual bdf associated with the passthrough device
 * @param[in] entry_nr indicate coming vectors, entry_nr = 0 means first vector
 * @param[in] info structure used for MSI/MSI-x remapping
 * @param[in] irte_idx caller can pass a valid IRTE index, otherwise, use INVALID_IRTE_ID
 *
 * @return
 *    - 0: on success
 *    - \p -ENODEV:
 *      - for Service VM, the entry already be held by others
 *      - for User VM, no pre-hold mapping found.
 *
 * @pre vm != NULL
 * @pre info != NULL
 *
 */
int32_t ptirq_prepare_msix_remap(struct acrn_vm *vm, uint16_t virt_bdf,  uint16_t phys_bdf,
				uint16_t entry_nr, struct msi_info *info, uint16_t irte_idx);


/**
 * @brief INTx remapping for passthrough device.
 *
 * Set up the remapping of the given virtual pin for the given vm.
 * This is the main entry for PCI/Legacy device assignment with INTx, calling from vIOAPIC or vPIC.
 *
 * @param[in] vm pointer to acrn_vm
 * @param[in] virt_gsi virtual GSI number associated with the passthrough device
 * @param[in] vgsi_ctlr INTX_CTLR_IOAPIC or INTX_CTLR_PIC
 *
 * @return
 *    - 0: on success
 *    - \p -ENODEV:
 *      - for Service VM, the entry already be held by others
 *      - for User VM, no pre-hold mapping found.
 *
 * @pre vm != NULL
 *
 */
int32_t ptirq_intx_pin_remap(struct acrn_vm *vm, uint32_t virt_gsi, enum intx_ctlr vgsi_ctlr);

/**
 * @brief Add an interrupt remapping entry for INTx as pre-hold mapping.
 *
 * Except Service VM, Device Model should call this function to pre-hold ptdev intx
 * The entry is identified by phys_pin, one entry vs. one phys_pin.
 * Currently, one phys_pin can only be held by one pin source (vPIC or vIOAPIC).
 *
 * @param[in] vm pointer to acrn_vm
 * @param[in] virt_gsi virtual pin number associated with the passthrough device
 * @param[in] phys_gsi physical pin number associated with the passthrough device
 * @param[in] pic_pin true for pic, false for ioapic
 *
 * @return
 *    - 0: on success
 *    - \p -EINVAL: invalid virt_pin value
 *    - \p -ENODEV: failed to add the remapping entry
 *
 * @pre vm != NULL
 *
 */
int32_t ptirq_add_intx_remapping(struct acrn_vm *vm, uint32_t virt_gsi, uint32_t phys_gsi, bool pic_pin);

/**
 * @brief Remove an interrupt remapping entry for INTx.
 *
 * Deactivate & remove mapping entry of the given virt gsi for given vm or
 * phys gsi assigned to this vm.
 *
 * @param[in] vm pointer to acrn_vm
 * @param[in] gsi virtual gsi number or physical gsi number associated with the passthrough device
 * @param[in] pic_pin true for pic, false for ioapic
 * @param[in] is_phy_gsi true if gsi is physical, false if gsi is virtual
 *
 * @return None
 *
 * @pre vm != NULL
 *
 */
void ptirq_remove_intx_remapping(const struct acrn_vm *vm, uint32_t gsi, bool pic_pin, bool is_phy_gsi);

/**
 * @brief Remove interrupt remapping entry/entries for MSI/MSI-x.
 *
 * Remove the mapping of given number of vectors of the given virtual BDF for the given vm.
 *
 * @param[in] vm pointer to acrn_vm
 * @param[in] phys_bdf physical bdf associated with the passthrough device
 * @param[in] vector_count number of vectors
 *
 * @return None
 *
 * @pre vm != NULL
 *
 */
void ptirq_remove_msix_remapping(const struct acrn_vm *vm, uint16_t phys_bdf, uint32_t vector_count);

/**
 * @brief Remove all interrupt remappings for INTx which are defined in VM config.
 *
 * Deactivate & remove all mapping entries of the virt_gsis defined in VM config for given vm.
 *
 * @param[in] vm pointer to acrn_vm
 *
 * @return None
 *
 * @pre vm != NULL
 *
 */
void ptirq_remove_configured_intx_remappings(const struct acrn_vm *vm);

/**
  * @}
  */

#endif /* ASSIGN_H */
