/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file hypercall.h
 *
 * @brief public APIs for hypercall
 */

#ifndef HYPERCALL_H
#define HYPERCALL_H

struct vhm_request;

bool is_hypercall_from_ring0(void);
int acrn_insert_request_wait(struct vcpu *vcpu, struct vhm_request *req);
int acrn_insert_request_nowait(struct vcpu *vcpu, struct vhm_request *req);
int get_req_info(char *str, int str_max);

int acrn_vpic_inject_irq(struct vm *vm, int irq, enum irq_mode mode);

/**
 * @brief Hypercall
 *
 * @addtogroup acrn_hypercall ACRN Hypercall
 * @{
 */

/**
 * @brief Get hypervisor api version
 *
 * The function only return api version information when VM is VM0.
 *
 * @param vm Pointer to VM data structure
 * @param param guest physical memory address. The api version returned
 *              will be copied to this gpa
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_get_api_version(struct vm *vm, uint64_t param);

/**
 * @brief create virtual machine
 *
 * Create a virtual machine based on parameter, currently there is no
 * limitation for calling times of this function, will add MAX_VM_NUM
 * support later.
 *
 * @param vm Pointer to VM data structure
 * @param param guest physical memory address. This gpa points to
 *              struct acrn_create_vm
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_create_vm(struct vm *vm, uint64_t param);

/**
 * @brief destroy virtual machine
 *
 * Destroy a virtual machine, it will pause target VM then shutdown it.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vmid ID of the VM
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_destroy_vm(uint64_t vmid);

/**
 * @brief resume virtual machine
 *
 * Resume a virtual machine, it will schedule target VM's vcpu to run.
 * The function will return -1 if the target VM does not exist or the
 * IOReq buffer page for the VM is not ready.
 *
 * @param vmid ID of the VM
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_resume_vm(uint64_t vmid);

/**
 * @brief pause virtual machine
 *
 * Pause a virtual machine, if the VM is already paused, the function
 * will return 0 directly for success.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vmid ID of the VM
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_pause_vm(uint64_t vmid);

/**
 * @brief create vcpu
 *
 * Create a vcpu based on parameter for a VM, it will allocate vcpu from
 * freed physical cpus, if there is no available pcpu, the function will
 * return -1.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical addressx. This gpa points to
 *              struct acrn_create_vcpu
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_create_vcpu(struct vm *vm, uint64_t vmid, uint64_t param);

/**
 * @brief assert IRQ line
 *
 * Assert a virtual IRQ line for a VM, which could be from ISA or IOAPIC,
 * normally it will active a level IRQ.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to struct acrn_irqline
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_assert_irqline(struct vm *vm, uint64_t vmid, uint64_t param);

/**
 * @brief deassert IRQ line
 *
 * Deassert a virtual IRQ line for a VM, which could be from ISA or IOAPIC,
 * normally it will deactive a level IRQ.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to struct acrn_irqline
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_deassert_irqline(struct vm *vm, uint64_t vmid, uint64_t param);

/**
 * @brief trigger a pulse on IRQ line
 *
 * Trigger a pulse on a virtual IRQ line for a VM, which could be from ISA
 * or IOAPIC, normally it triggers an edge IRQ.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to struct acrn_irqline
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_pulse_irqline(struct vm *vm, uint64_t vmid, uint64_t param);

/**
 * @brief inject MSI interrupt
 *
 * Inject a MSI interrupt for a VM.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to struct acrn_msi_entry
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_inject_msi(struct vm *vm, uint64_t vmid, uint64_t param);

/**
 * @brief set ioreq shared buffer
 *
 * Set the ioreq share buffer for a VM.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to
 *              struct acrn_set_ioreq_buffer
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_set_ioreq_buffer(struct vm *vm, uint64_t vmid, uint64_t param);

/**
 * @brief notify request done
 *
 * Notify the requestor VCPU for the completion of an ioreq.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vmid ID of the VM
 * @param param vcpu ID of the requestor
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_notify_req_finish(uint64_t vmid, uint64_t param);

/**
 * @brief setup ept memory mapping
 *
 * Set the ept memory mapping for a VM.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to
 *              struct vm_set_memmap
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_set_vm_memmap(struct vm *vm, uint64_t vmid, uint64_t param);

/**
 * @brief setup ept memmory mapping for multi regions
 *
 * @param vm Pointer to VM data structure
 * @param param guest physical address. This gpa points to
 *              struct set_memmaps
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_set_vm_memmaps(struct vm *vm, uint64_t param);

/**
 * @brief remap PCI MSI interrupt
 *
 * Remap a PCI MSI interrupt from a VM's virtual vector to native vector.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to
 *              struct acrn_vm_pci_msix_remap
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_remap_pci_msix(struct vm *vm, uint64_t vmid, uint64_t param);

/**
 * @brief translate guest physical address ot host physical address
 *
 * Translate guest physical address to host physical address for a VM.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to struct vm_gpa2hpa
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_gpa_to_hpa(struct vm *vm, uint64_t vmid, uint64_t param);

/**
 * @brief Assign one passthrough dev to VM.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to
 *              physical BDF of the assigning ptdev
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_assign_ptdev(struct vm *vm, uint64_t vmid, uint64_t param);

/**
 * @brief Deassign one passthrough dev from VM.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to
 *              physical BDF of the deassigning ptdev
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_deassign_ptdev(struct vm *vm, uint64_t vmid, uint64_t param);

/**
 * @brief Set interrupt mapping info of ptdev.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to data structure of
 *              hc_ptdev_irq including intr remapping info
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_set_ptdev_intr_info(struct vm *vm, uint64_t vmid, uint64_t param);

/**
 * @brief Clear interrupt mapping info of ptdev.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to data structure of
 *              hc_ptdev_irq including intr remapping info
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_reset_ptdev_intr_info(struct vm *vm, uint64_t vmid,
	uint64_t param);

/**
 * @brief Setup a share buffer for a VM.
 *
 * @param vm Pointer to VM data structure
 * @param param guest physical address. This gpa points to
 *              struct sbuf_setup_param
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_setup_sbuf(struct vm *vm, uint64_t param);

/**
 * @brief Switch VCPU state between Normal/Secure World.
 *
 * @param vcpu Pointer to VCPU data structure
 *
 * @return 0 on success, non-zero on error.
 */

int64_t hcall_get_cpu_pm_state(struct vm *vm, uint64_t cmd, uint64_t param);

/**
 * @brief Get VCPU Power state.
 *
 * @param VCPU power state data
 *
 * @return 0 on success, non-zero on error.
 */

int64_t hcall_world_switch(struct vcpu *vcpu);

/**
 * @brief Initialize environment for Trusty-OS on a VCPU.
 *
 * @param vcpu Pointer to VCPU data structure
 * @param param guest physical address. This gpa points to
 *              struct trusty_boot_param
 *
 * @return 0 on success, non-zero on error.
 */
int64_t hcall_initialize_trusty(struct vcpu *vcpu, uint64_t param);

/**
 * @}
 */

static inline int check_result(int found)
{
	return found ? 0 : -1;
}

#define copy_from_vm(vm, ptr, gpa, size) ({		\
	int found = 0;					\
	typeof(*(ptr)) *h_ptr = (ptr);			\
	typeof(*(ptr)) *g_ptr =				\
		HPA2HVA(gpa2hpa_check(vm, gpa,	\
		size, &found, true));			\
	if (found) {					\
		memcpy_s(h_ptr, size, g_ptr, size);	\
	}						\
	check_result(found);				\
})

#define copy_to_vm(vm, ptr, gpa, size) ({		\
	int found = 0;					\
	typeof(*(ptr)) *h_ptr = (ptr);			\
	typeof(*(ptr)) *g_ptr =				\
		HPA2HVA(gpa2hpa_check(vm, gpa,	\
		size, &found, true));			\
	if (found) {					\
		memcpy_s(g_ptr, size, h_ptr, size);	\
	}						\
	check_result(found);				\
})

#endif /* HYPERCALL_H*/
