/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file hypercall.h
 *
 * @brief public APIs for hypercall
 */

#ifndef HYPERCALL_H
#define HYPERCALL_H

bool is_hypercall_from_ring0(void);

/**
 * @brief Hypercall
 *
 * @addtogroup acrn_hypercall ACRN Hypercall
 * @{
 */

/**
 * @brief offline vcpu from Service VM
 *
 * The function offline specific vcpu from Service VM.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm not used
 * @param param1 lapic id of the vcpu which wants to offline
 * @param param2 not used
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_service_vm_offline_cpu(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief Get hypervisor api version
 *
 * The function only return api version information when VM is Service VM.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm not used
 * @param param1 guest physical memory address. The api version returned
 *              will be copied to this gpa
 * @param param2 not used
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_get_api_version(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);


/**
 * @brief create virtual machine
 *
 * Create a virtual machine based on parameter, currently there is no
 * limitation for calling times of this function, will add MAX_VM_NUM
 * support later.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 guest physical memory address. This gpa points to
 *              struct acrn_vm_creation
 * @param param2 not used
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_create_vm(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief destroy virtual machine
 *
 * Destroy a virtual machine, it will pause target VM then shutdown it.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vcpu not used
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 not used
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_destroy_vm(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief reset virtual machine
 *
 * Reset a virtual machine, it will make target VM rerun from
 * pre-defined entry. Comparing to start vm, this function reset
 * each vcpu state and do some initialization for guest.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vcpu not used
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 not used
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_reset_vm(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief start virtual machine
 *
 * Start a virtual machine, it will schedule target VM's vcpu to run.
 * The function will return -1 if the target VM does not exist or the
 * IOReq buffer page for the VM is not ready.
 *
 * @param vcpu not used
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 not used
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_start_vm(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief pause virtual machine
 *
 * Pause a virtual machine, if the VM is already paused, the function
 * will return 0 directly for success.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vcpu not used
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 not used
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_pause_vm(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief set vcpu regs
 *
 * Set the vcpu regs. It will set the vcpu init regs from DM. Now,
 * it's only applied to BSP. AP always uses fixed init regs.
 * The function will return -1 if the targat VM or BSP doesn't exist.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 guest physical address. This gpa points to
 *              struct acrn_vcpu_regs
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_vcpu_regs(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief set or clear IRQ line
 *
 * Set or clear a virtual IRQ line for a VM, which could be from ISA
 * or IOAPIC, normally it triggers an edge IRQ.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 info for irqline
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_irqline(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief inject MSI interrupt
 *
 * Inject a MSI interrupt for a VM.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 guest physical address. This gpa points to struct acrn_msi_entry
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_inject_msi(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief set ioreq shared buffer
 *
 * Set the ioreq share buffer for a VM.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 guest physical address. This gpa points to buffer address
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_ioreq_buffer(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief notify request done
 *
 * Notify the requestor VCPU for the completion of an ioreq.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vcpu not used
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 vcpu ID of the requestor
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_notify_ioreq_finish(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief setup ept memory mapping for multi regions
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 guest physical address. This gpa points to
 *              struct set_memmaps
 * @param param2 not used
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_vm_memory_regions(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		uint64_t param1, uint64_t param2);

/**
 * @brief change guest memory page write permission
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 guest physical address. This gpa points to
 *              struct wp_data
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_write_protect_page(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief translate guest physical address to host physical address
 *
 * Translate guest physical address to host physical address for a VM.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 guest physical address. This gpa points to struct vm_gpa2hpa
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_gpa_to_hpa(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief Assign one PCI dev to VM.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_pcidev including assign PCI device info
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_assign_pcidev(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief Deassign one PCI dev to VM.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_pcidev including deassign PCI device info
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_deassign_pcidev(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief Assign one MMIO dev to VM.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_mmiodev including assign MMIO device info
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_assign_mmiodev(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief Deassign one MMIO dev to VM.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_mmiodev including deassign MMIO device info
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_deassign_mmiodev(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief Add an emulated device in hypervisor.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_vdev including information about PCI or legacy devices
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_add_vdev(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief Remove an emulated device in hypervisor.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_vdev including information about PCI or legacy devices
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_remove_vdev(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief Set interrupt mapping info of ptdev.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 guest physical address. This gpa points to data structure of
 *              hc_ptdev_irq including intr remapping info
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_ptdev_intr_info(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief Clear interrupt mapping info of ptdev.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 guest physical address. This gpa points to data structure of
 *              hc_ptdev_irq including intr remapping info
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_reset_ptdev_intr_info(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		uint64_t param1, uint64_t param2);

/**
 * @brief Get VCPU Power state.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 cmd to show get which VCPU power state data
 * @param param2 VCPU power state data
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_get_cpu_pm_state(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief Get VCPU a VM's interrupt count data.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 not used
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_intr_monitor
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_vm_intr_monitor(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @defgroup trusty_hypercall Trusty Hypercalls
 *
 * This is a special group that includes all hypercalls
 * related to Trusty
 *
 * @{
 */

/**
 * @brief Switch vCPU state between Normal/Secure World.
 *
 * * The hypervisor uses this hypercall to do the world switch
 * * The hypervisor needs to:
 *   * save current world vCPU contexts, and load the next world
 *     vCPU contexts
 *   * update ``rdi``, ``rsi``, ``rdx``, ``rbx`` to next world
 *     vCPU contexts
 *
 * @param vcpu Pointer to VCPU data structure
 * @param target_vm not used
 * @param param1 not used
 * @param param2 not used
 *
 * @return 0 on success, non-zero on error.
 */

int32_t hcall_world_switch(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief Initialize environment for Trusty-OS on a vCPU.
 *
 * * It is used by the User VM OS bootloader (``User VM OS Loader``) to request ACRN
 *   to initialize Trusty
 * * The Trusty memory region range, entry point must be specified
 * * The hypervisor needs to save current vCPU contexts (Normal World)
 *
 * @param vcpu Pointer to vCPU data structure
 * @param target_vm not used
 * @param param1 guest physical address. This gpa points to
 *               trusty_boot_param structure
 * @param param2 not used
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_initialize_trusty(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief Save/Restore Context of Secure World.
 *
 * @param vcpu Pointer to VCPU data structure
 * @param target_vm not used
 * @param param1 not used
 * @param param2 not used
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_save_restore_sworld_ctx(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		uint64_t param1, uint64_t param2);

/**
 * @brief Handle the TEE boot done signal.
 *
 * @param vcpu Pointer to VCPU data structure
 * @param target_vm not used
 * @param param1 not used
 * @param param2 not used
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_handle_tee_vcpu_boot_done(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		uint64_t param1, uint64_t param2);

/**
 * @brief Switch the execution environment.
 *
 * @param vcpu Pointer to VCPU data structure
 * @param target_vm not used
 * @param param1 not used
 * @param param2 not used
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_switch_ee(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		uint64_t param1, uint64_t param2);

/**
 * @}
 */
/* End of trusty_hypercall */

/**
 * @brief set upcall notifier vector
 *
 * This is the API that helps to switch the notifer vecotr. If this API is
 * not called, the hypervisor will use the default notifier vector(0xF7)
 * to notify the Service VM OS kernel.
 *
 * @param vcpu not used
 * @param target_vm not used
 * @param param1 the expected notifier vector from guest
 * @param param2 not used
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_callback_vector(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief Setup a share buffer for a VM.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm not used
 * @param param1 guest physical address. This gpa points to
 *              struct sbuf_setup_param
 * @param param2 not used
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_setup_sbuf(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief Assign an asyncio to a VM.
 *
 * @param vcpu not used
 * @param target_vm which VM the asyncio belongs.
 * @param param1 guest physical address. This gpa points to
 *              struct acrn_asyncio_info
 * @param param2 not used
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_asyncio_assign(__unused struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		 __unused uint64_t param1, uint64_t param2);
/**
 * @brief Deassign an asyncio from a VM.
 *
 * @param vcpu not used
 * @param target_vm which VM the asyncio belongs.
 * @param param1 guest physical address. This gpa points to
 *              struct acrn_asyncio_info
 * @param param2 not used
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_asyncio_deassign(__unused struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		 __unused uint64_t param1, uint64_t param2);

/**
 * @brief Setup the hypervisor NPK log.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm not used
 * @param param1 guest physical address. This gpa points to
 *              struct hv_npk_log_param
 * @param param2 not used
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_setup_hv_npk_log(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief Get hardware related info
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm not used
 * @param param1 Guest physical address pointing to struct acrn_hw_info
 * @param param2 not used
 *
 * @pre vm shall point to Service VM
 * @pre param1 shall be a valid physical address
 *
 * @retval 0 on success
 * @retval -1 in case of error
 */
int32_t hcall_get_hw_info(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

/**
 * @brief Execute profiling operation
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm not used
 * @param param1 profiling command to be executed
 * @param param2 guest physical address. This gpa points to
 *             data structure required by each command
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_profiling_ops(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);

int32_t hcall_create_vcpu(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2);
/**
 * @}
 */
/* End of acrn_hypercall */

#endif /* HYPERCALL_H*/
