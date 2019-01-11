/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <schedule.h>
#include <hypercall.h>
#include <version.h>
#include <reloc.h>
#include <vtd.h>

#define ACRN_DBG_HYCALL	6U

bool is_hypercall_from_ring0(void)
{
	uint16_t cs_sel;
	bool ret;

	cs_sel = exec_vmread16(VMX_GUEST_CS_SEL);
	/* cs_selector[1:0] is CPL */
	if ((cs_sel & 0x3U) == 0U) {
	        ret = true;
	} else {
		ret = false;
	}

	return ret;
}

/**
 * @brief offline vcpu from SOS
 *
 * The function offline specific vcpu from SOS.
 *
 * @param vm Pointer to VM data structure
 * @param lapicid lapic id of the vcpu which wants to offline
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_sos_offline_cpu(struct acrn_vm *vm, uint64_t lapicid)
{
	struct acrn_vcpu *vcpu;
	uint16_t i;
	int32_t ret = 0;

	pr_info("sos offline cpu with lapicid %lld", lapicid);

	foreach_vcpu(i, vm, vcpu) {
		if (vlapic_get_apicid(vcpu_vlapic(vcpu)) == lapicid) {
			/* should not offline BSP */
			if (vcpu->vcpu_id == BOOT_CPU_ID) {
				ret = -1;
				break;
			}
			pause_vcpu(vcpu, VCPU_ZOMBIE);
			reset_vcpu(vcpu);
			offline_vcpu(vcpu);
		}
	}

	return ret;
}

/**
 * @brief Get hypervisor api version
 *
 * The function only return api version information when VM is VM0.
 *
 * @param vm Pointer to VM data structure
 * @param param guest physical memory address. The api version returned
 *              will be copied to this gpa
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_get_api_version(struct acrn_vm *vm, uint64_t param)
{
	struct hc_api_version version;

	version.major_version = HV_API_MAJOR_VERSION;
	version.minor_version = HV_API_MINOR_VERSION;
	int32_t ret;

	if (copy_to_gpa(vm, &version, param, sizeof(version)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
		ret = -1;
	} else {
		ret = 0;
	}

	return ret;
}

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
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_create_vm(struct acrn_vm *vm, uint64_t param)
{
	int32_t ret;
	struct acrn_vm *target_vm = NULL;
	struct acrn_create_vm cv;
	struct vm_description vm_desc;

	(void)memset((void *)&cv, 0U, sizeof(cv));
	if (copy_from_gpa(vm, &cv, param, sizeof(cv)) == 0) {
		(void)memset(&vm_desc, 0U, sizeof(vm_desc));
		vm_desc.sworld_supported = ((cv.vm_flag & (SECURE_WORLD_ENABLED)) != 0U);
		(void)memcpy_s(&vm_desc.GUID[0], 16U, &cv.GUID[0], 16U);

		ret = create_vm(&vm_desc, &target_vm);
		if (ret != 0) {
			dev_dbg(ACRN_DBG_HYCALL, "HCALL: Create VM failed");
			cv.vmid = ACRN_INVALID_VMID;
			ret = -1;
		} else {
			cv.vmid = target_vm->vm_id;
			ret = 0;
		}

		if (copy_to_gpa(vm, &cv.vmid, param, sizeof(cv.vmid)) != 0) {
			pr_err("%s: Unable copy param to vm\n", __func__);
			ret = -1;
		}
	} else {
		pr_err("%s: Unable copy param to vm\n", __func__);
	        ret = -1;
	}

	return ret;
}

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
int32_t hcall_destroy_vm(uint16_t vmid)
{
	int32_t ret;
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL) {
		ret = -1;
	} else {
		ret = shutdown_vm(target_vm);
	}

	return ret;
}

/**
 * @brief start virtual machine
 *
 * Start a virtual machine, it will schedule target VM's vcpu to run.
 * The function will return -1 if the target VM does not exist or the
 * IOReq buffer page for the VM is not ready.
 *
 * @param vmid ID of the VM
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_start_vm(uint16_t vmid)
{
	int32_t ret = 0;
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL) {
		ret = -1;
	} else if (target_vm->sw.io_shared_page == NULL) {
		ret = -1;
	} else {
		start_vm(target_vm);
	}

	return ret;
}

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
int32_t hcall_pause_vm(uint16_t vmid)
{
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);
	int32_t ret;

	if (target_vm == NULL) {
	        ret = -1;
	} else {
		pause_vm(target_vm);
		ret = 0;
	}

	return ret;
}

/**
 * @brief create vcpu
 *
 * Create a vcpu based on parameter for a VM, it will allocate vcpu from
 * freed physical cpus, if there is no available pcpu, the function will
 * return -1.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to
 *              struct acrn_create_vcpu
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_create_vcpu(struct acrn_vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret;
	uint16_t pcpu_id;
	struct acrn_create_vcpu cv;
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);

	if ((target_vm == NULL) || (param == 0U)) {
	        ret = -1;
	} else if (copy_from_gpa(vm, &cv, param, sizeof(cv)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
	        ret = -1;
	} else {
		pcpu_id = allocate_pcpu();
		if (pcpu_id == INVALID_CPU_ID) {
			pr_err("%s: No physical available\n", __func__);
			ret = -1;
		} else {
			ret = prepare_vcpu(target_vm, pcpu_id);
		}
	}

	return ret;
}

/**
 * @brief reset virtual machine
 *
 * Reset a virtual machine, it will make target VM rerun from
 * pre-defined entry. Comparing to start vm, this function reset
 * each vcpu state and do some initialization for guest.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vmid ID of the VM
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_reset_vm(uint16_t vmid)
{
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);
	int32_t ret;

	if ((target_vm == NULL) || is_vm0(target_vm)) {
	        ret = -1;
	} else {
	        ret = reset_vm(target_vm);
	}
	return ret;
}

/**
 * @brief set vcpu regs
 *
 * Set the vcpu regs. It will set the vcpu init regs from DM. Now,
 * it's only applied to BSP. AP always uses fixed init regs.
 * The function will return -1 if the targat VM or BSP doesn't exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to
 *              struct acrn_vcpu_regs
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_vcpu_regs(struct acrn_vm *vm, uint16_t vmid, uint64_t param)
{
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);
	struct acrn_set_vcpu_regs vcpu_regs;
	struct acrn_vcpu *vcpu;
	int32_t ret;

	if ((target_vm == NULL) || (param == 0U) || is_vm0(target_vm) || (target_vm->state == VM_STARTED)) {
		/* Only allow setup init ctx while target_vm is inactive */
	        ret = -1;
	} else if (copy_from_gpa(vm, &vcpu_regs, param, sizeof(vcpu_regs)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
	        ret = -1;
	} else if (vcpu_regs.vcpu_id >= CONFIG_MAX_VCPUS_PER_VM) {
		pr_err("%s: invalid vcpu_id for set_vcpu_regs\n", __func__);
		ret = -1;
	} else {
		vcpu = vcpu_from_vid(target_vm, vcpu_regs.vcpu_id);
		set_vcpu_regs(vcpu, &(vcpu_regs.vcpu_regs));
		ret = 0;
	}

	return ret;
}

/**
 * @brief set or clear IRQ line
 *
 * Set or clear a virtual IRQ line for a VM, which could be from ISA
 * or IOAPIC, normally it triggers an edge IRQ.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param ops request command for IRQ set or clear
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_irqline(const struct acrn_vm *vm, uint16_t vmid,
				const struct acrn_irqline_ops *ops)
{
	uint32_t irq_pic;
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);
	int32_t ret;

	if (target_vm == NULL) {
	        ret = -EINVAL;
	} else if (ops->gsi >= vioapic_pincount(vm)) {
	        ret = -EINVAL;
	} else {
		if (ops->gsi < vpic_pincount()) {
			/*
			 * IRQ line for 8254 timer is connected to
			 * I/O APIC pin #2 but PIC pin #0,route GSI
			 * number #2 to PIC IRQ #0.
			 */
			irq_pic = (ops->gsi == 2U) ? 0U : ops->gsi;
			vpic_set_irqline(target_vm, irq_pic, ops->op);
	        }

		/* handle IOAPIC irqline */
		vioapic_set_irqline_lock(target_vm, ops->gsi, ops->op);
		ret = 0;
	}

	return ret;
}

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
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_inject_msi(struct acrn_vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret;
	struct acrn_msi_entry msi;
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm != NULL) {
		(void)memset((void *)&msi, 0U, sizeof(msi));
		if (copy_from_gpa(vm, &msi, param, sizeof(msi)) != 0) {
			pr_err("%s: Unable copy param to vm\n", __func__);
			ret = -1;
		} else {
			ret = vlapic_intr_msi(target_vm, msi.msi_addr, msi.msi_data);
		}
	} else {
		ret = -1;
	}

	return ret;
}

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
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_ioreq_buffer(struct acrn_vm *vm, uint16_t vmid, uint64_t param)
{
	uint64_t hpa;
	struct acrn_set_ioreq_buffer iobuf;
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);
	uint16_t i;
	int32_t ret;

	(void)memset((void *)&iobuf, 0U, sizeof(iobuf));
	if ((target_vm == NULL) || (copy_from_gpa(vm, &iobuf, param, sizeof(iobuf)) != 0)) {
		pr_err("%p %s: target_vm is not valid or Unable copy param to vm\n", target_vm, __func__);
	        ret = -1;
        } else {
		dev_dbg(ACRN_DBG_HYCALL, "[%d] SET BUFFER=0x%p",
				vmid, iobuf.req_buf);

		hpa = gpa2hpa(vm, iobuf.req_buf);
		if (hpa == INVALID_HPA) {
			pr_err("%s,vm[%hu] gpa 0x%llx,GPA is unmapping.",
				__func__, vm->vm_id, iobuf.req_buf);
			target_vm->sw.io_shared_page = NULL;
		        ret = -EINVAL;
		} else {
			target_vm->sw.io_shared_page = hpa2hva(hpa);
			for (i = 0U; i < VHM_REQUEST_MAX; i++) {
				set_vhm_req_state(target_vm, i, REQ_STATE_FREE);
			}

			ret = 0;
		}
	}

	return ret;
}

/**
 * @brief notify request done
 *
 * Notify the requestor VCPU for the completion of an ioreq.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vmid ID of the VM
 * @param vcpu_id vcpu ID of the requestor
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_notify_ioreq_finish(uint16_t vmid, uint16_t vcpu_id)
{
	struct acrn_vcpu *vcpu;
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);
	int32_t ret;

	/* make sure we have set req_buf */
	if ((target_vm == NULL) || (target_vm->sw.io_shared_page == NULL)) {
		pr_err("%p %s, invalid parameter\n", target_vm, __func__);
	        ret = -EINVAL;
	} else {
		dev_dbg(ACRN_DBG_HYCALL, "[%d] NOTIFY_FINISH for vcpu %d",
			vmid, vcpu_id);

		if (vcpu_id >= CONFIG_MAX_VCPUS_PER_VM) {
			pr_err("%s, failed to get VCPU %d context from VM %d\n",
				__func__, vcpu_id, target_vm->vm_id);
			ret = -EINVAL;
		} else {
			vcpu = vcpu_from_vid(target_vm, vcpu_id);
			emulate_io_post(vcpu);
			ret = 0;
		}
	}

	return ret;
}


static int32_t add_vm_memory_region(struct acrn_vm *vm, struct acrn_vm *target_vm,
				    const struct vm_memory_region *region,uint64_t *pml4_page)
{
	int32_t ret;
	uint64_t prot;
	uint64_t hpa, base_paddr;

	hpa = gpa2hpa(vm, region->vm0_gpa);
	if (hpa == INVALID_HPA) {
		pr_err("%s,vm[%hu] gpa 0x%llx,GPA is unmapping.",
			__func__, vm->vm_id, region->vm0_gpa);
		ret = -EINVAL;
	} else {
		base_paddr = get_hv_image_base();
		if (((hpa <= base_paddr) && ((hpa + region->size) > base_paddr)) ||
				((hpa >= base_paddr) && (hpa < (base_paddr + CONFIG_HV_RAM_SIZE)))) {
			pr_err("%s: overlap the HV memory region.", __func__);
			ret = -EFAULT;
		} else {
			prot = 0UL;
			/* access right */
			if ((region->prot & MEM_ACCESS_READ) != 0U) {
				prot |= EPT_RD;
			}
			if ((region->prot & MEM_ACCESS_WRITE) != 0U) {
				prot |= EPT_WR;
			}
			if ((region->prot & MEM_ACCESS_EXEC) != 0U) {
				prot |= EPT_EXE;
			}
			/* memory type */
			if ((region->prot & MEM_TYPE_WB) != 0U) {
				prot |= EPT_WB;
			} else if ((region->prot & MEM_TYPE_WT) != 0U) {
				prot |= EPT_WT;
			} else if ((region->prot & MEM_TYPE_WC) != 0U) {
				prot |= EPT_WC;
			} else if ((region->prot & MEM_TYPE_WP) != 0U) {
				prot |= EPT_WP;
			} else {
				prot |= EPT_UNCACHED;
			}
			/* create gpa to hpa EPT mapping */
			ept_mr_add(target_vm, pml4_page, hpa,
					region->gpa, region->size, prot);
			ret = 0;
		}
	}

	return ret;
}

/**
 *@pre Pointer vm shall point to VM0
 */
static int32_t set_vm_memory_region(struct acrn_vm *vm,
	struct acrn_vm *target_vm, const struct vm_memory_region *region)
{
	uint64_t gpa_end;
	uint64_t *pml4_page;
	int32_t ret;

	if ((region->size & (PAGE_SIZE - 1UL)) != 0UL) {
		pr_err("%s: [vm%d] map size 0x%x is not page aligned",
			__func__, target_vm->vm_id, region->size);
	        ret = -EINVAL;
	} else {
		gpa_end = region->gpa + region->size;
		if (gpa_end > vm->arch_vm.ept_mem_ops.info->ept.top_address_space) {
				pr_err("%s, invalid gpa: 0x%llx, size: 0x%llx, top_address_space: 0x%llx", __func__,
					region->gpa, region->size, vm->arch_vm.ept_mem_ops.info->ept.top_address_space);
				ret = 0;
		} else {
			dev_dbg(ACRN_DBG_HYCALL,
				"[vm%d] type=%d gpa=0x%x vm0_gpa=0x%x size=0x%x",
				target_vm->vm_id, region->type, region->gpa,
				region->vm0_gpa, region->size);

			pml4_page = (uint64_t *)target_vm->arch_vm.nworld_eptp;
			if (region->type != MR_DEL) {
				ret = add_vm_memory_region(vm, target_vm, region, pml4_page);
			} else {
				ept_mr_del(target_vm, pml4_page,
						region->gpa, region->size);
				ret = 0;
			}
		}
	}

	return ret;
}

/**
 * @brief setup ept memory mapping for multi regions
 *
 * @param vm Pointer to VM data structure
 * @param param guest physical address. This gpa points to
 *              struct set_memmaps
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_vm_memory_regions(struct acrn_vm *vm, uint64_t param)
{
	struct set_regions regions;
	struct vm_memory_region mr;
	struct acrn_vm *target_vm;
	uint32_t idx;
	int32_t ret = 0;


	(void)memset((void *)&regions, 0U, sizeof(regions));

	if (copy_from_gpa(vm, &regions, param, sizeof(regions)) != 0) {
		pr_err("%s: Unable copy param from vm\n", __func__);
	        ret = -EFAULT;
	} else {
		target_vm = get_vm_from_vmid(regions.vmid);
		if ((target_vm == NULL) || is_vm0(target_vm)) {
			pr_err("%p %s:target_vm is invalid or Targeting to service vm", target_vm, __func__);
		        ret = -EFAULT;
		} else {
			idx = 0U;
			while (idx < regions.mr_num) {
				if (copy_from_gpa(vm, &mr, regions.regions_gpa + idx * sizeof(mr), sizeof(mr)) != 0) {
					pr_err("%s: Copy mr entry fail from vm\n", __func__);
				        ret = -EFAULT;
					break;
				}

				ret = set_vm_memory_region(vm, target_vm, &mr);
				if (ret < 0) {
					break;
				}
				idx++;
			}
		}
	}

	return ret;
}

/**
 *@pre Pointer vm shall point to VM0
 */
static int32_t write_protect_page(struct acrn_vm *vm,const struct wp_data *wp)
{
	uint64_t hpa, base_paddr;
	uint64_t prot_set;
	uint64_t prot_clr;
	int32_t ret;

	hpa = gpa2hpa(vm, wp->gpa);
	if (hpa == INVALID_HPA) {
		pr_err("%s,vm[%hu] gpa 0x%llx,GPA is unmapping.",
			__func__, vm->vm_id, wp->gpa);
		ret = -EINVAL;
	} else {
		dev_dbg(ACRN_DBG_HYCALL, "[vm%d] gpa=0x%x hpa=0x%x",
				vm->vm_id, wp->gpa, hpa);

		base_paddr = get_hv_image_base();
		if (((hpa <= base_paddr) && ((hpa + PAGE_SIZE) > base_paddr)) ||
				((hpa >= base_paddr) &&
				(hpa < (base_paddr + CONFIG_HV_RAM_SIZE)))) {
			pr_err("%s: overlap the HV memory region.", __func__);
			ret = -EINVAL;
		} else {
			prot_set = (wp->set != 0U) ? 0UL : EPT_WR;
			prot_clr = (wp->set != 0U) ? EPT_WR : 0UL;

			ept_mr_modify(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
				wp->gpa, PAGE_SIZE, prot_set, prot_clr);
			ret = 0;
		}
	}

	return ret;
}

/**
 * @brief change guest memory page write permission
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param wp_gpa guest physical address. This gpa points to
 *              struct wp_data
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_write_protect_page(struct acrn_vm *vm, uint16_t vmid, uint64_t wp_gpa)
{
	struct wp_data wp;
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);
	int32_t ret;

	if ((target_vm == NULL) || is_vm0(target_vm)) {
		pr_err("%p %s: target_vm is invalid or Targeting to service vm", target_vm, __func__);
	        ret = -EINVAL;
	} else {
		(void)memset((void *)&wp, 0U, sizeof(wp));

		if (copy_from_gpa(vm, &wp, wp_gpa, sizeof(wp)) != 0) {
			pr_err("%s: Unable copy param to vm\n", __func__);
			ret = -EFAULT;
		} else {
			ret = write_protect_page(target_vm, &wp);
		}
	}

	return ret;
}

/**
 * @brief translate guest physical address to host physical address
 *
 * Translate guest physical address to host physical address for a VM.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to struct vm_gpa2hpa
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_gpa_to_hpa(struct acrn_vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret;
	struct vm_gpa2hpa v_gpa2hpa;
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);

	(void)memset((void *)&v_gpa2hpa, 0U, sizeof(v_gpa2hpa));
	if ((target_vm == NULL) || (copy_from_gpa(vm, &v_gpa2hpa, param, sizeof(v_gpa2hpa)) != 0)) {
		pr_err("target_vm is invalid or HCALL gpa2hpa: Unable copy param from vm\n");
	        ret = -1;
	} else {
		v_gpa2hpa.hpa = gpa2hpa(target_vm, v_gpa2hpa.gpa);
		if (v_gpa2hpa.hpa == INVALID_HPA) {
			pr_err("%s,vm[%hu] gpa 0x%llx,GPA is unmapping.",
				__func__, target_vm->vm_id, v_gpa2hpa.gpa);
			ret = -EINVAL;
		} else if (copy_to_gpa(vm, &v_gpa2hpa, param, sizeof(v_gpa2hpa)) != 0) {
			pr_err("%s: Unable copy param to vm\n", __func__);
		        ret = -1;
		} else {
			ret = 0;
		}
	}

	return ret;
}

/**
 * @brief Assign one passthrough dev to VM.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param the physical BDF of the assigning ptdev
 *              For the compatibility it still can be the guest physical address that
 *              points to the physical BDF of the assigning ptdev.(Depreciated)
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_assign_ptdev(struct acrn_vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret;
	uint16_t bdf;
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);
	bool bdf_valid = true;
	bool iommu_valid = true;

	if (target_vm != NULL) {
		if (param < 0x10000UL) {
			bdf = (uint16_t) param;
		} else {
			if (copy_from_gpa(vm, &bdf, param, sizeof(bdf)) != 0) {
				pr_err("%s: Unable copy param from vm %d\n",
				__func__, vm->vm_id);
				bdf_valid = false;
			        ret = -EIO;
		        }
	        }

		/* create a iommu domain for target VM if not created */
		if (bdf_valid && (target_vm->iommu == NULL)) {
			if (target_vm->arch_vm.nworld_eptp == NULL) {
				pr_err("%s, EPT of VM not set!\n",
					__func__, target_vm->vm_id);
				iommu_valid = false;
			        ret = -EPERM;
			} else {
				/* TODO: how to get vm's address width? */
				target_vm->iommu = create_iommu_domain(vmid,
						hva2hpa(target_vm->arch_vm.nworld_eptp), 48U);
				if (target_vm->iommu == NULL) {
					iommu_valid = false;
					ret = -ENODEV;
				}
			}
		}
		if (bdf_valid && iommu_valid) {
			ret = assign_iommu_device(target_vm->iommu,
				(uint8_t)(bdf >> 8U), (uint8_t)(bdf & 0xffU));
		}
	} else {
		pr_err("%s, vm is null\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

/**
 * @brief Deassign one passthrough dev from VM.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param the physical BDF of the deassigning ptdev
 *              To keep the compatibility it still can be the guest physical address that
 *              points to the physical BDF of the deassigning ptdev.(Depreciated)
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_deassign_ptdev(struct acrn_vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret = 0;
	uint16_t bdf;
	bool bdf_valid = true;
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm == NULL) {
	        ret = -1;
	} else {
		if (param < 0x10000UL) {
			bdf = (uint16_t) param;
		} else {
			if (copy_from_gpa(vm, &bdf, param, sizeof(bdf)) != 0) {
				pr_err("%s: Unable copy param to vm\n", __func__);
				bdf_valid = false;
			        ret = -1;
			}
		}

		if (bdf_valid) {
			ret = unassign_iommu_device(target_vm->iommu,
				(uint8_t)(bdf >> 8U), (uint8_t)(bdf & 0xffU));
		}
	}

	return ret;
}

/**
 * @brief Set interrupt mapping info of ptdev.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to data structure of
 *              hc_ptdev_irq including intr remapping info
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_ptdev_intr_info(struct acrn_vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret;
	struct hc_ptdev_irq irq;
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);

	(void)memset((void *)&irq, 0U, sizeof(irq));
	if (target_vm == NULL) {
	        ret = -1;
	} else if (copy_from_gpa(vm, &irq, param, sizeof(irq)) != 0) {
		pr_err("%s: Unable copy param to vm\n", __func__);
	        ret = -1;
	} else {
		/* Inform vPCI about the interupt info changes */
#ifndef CONFIG_PARTITION_MODE
		vpci_set_ptdev_intr_info(target_vm, irq.virt_bdf, irq.phys_bdf);
#endif

		if (irq.type == IRQ_INTX) {
			ret = ptirq_add_intx_remapping(target_vm, irq.is.intx.virt_pin,
					irq.is.intx.phys_pin, irq.is.intx.pic_pin);
		} else if ((irq.type == IRQ_MSI) || (irq.type == IRQ_MSIX)) {
			ret = ptirq_add_msix_remapping(target_vm,
					irq.virt_bdf, irq.phys_bdf,
					irq.is.msix.vector_cnt);
		} else {
			pr_err("%s: Invalid irq type: %u\n", __func__, irq.type);
			ret = -1;
		}
	}

	return ret;
}

/**
 * @brief Clear interrupt mapping info of ptdev.
 *
 * @param vm Pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to data structure of
 *              hc_ptdev_irq including intr remapping info
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t
hcall_reset_ptdev_intr_info(struct acrn_vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t ret = 0;
	struct hc_ptdev_irq irq;
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm != NULL) {
		(void)memset((void *)&irq, 0U, sizeof(irq));

		if (copy_from_gpa(vm, &irq, param, sizeof(irq)) != 0) {
			pr_err("%s: Unable copy param to vm\n", __func__);
			ret = -1;
		} else if (irq.type == IRQ_INTX) {
			ptirq_remove_intx_remapping(target_vm,
					irq.is.intx.virt_pin,
					irq.is.intx.pic_pin);
		} else if ((irq.type == IRQ_MSI) || (irq.type == IRQ_MSIX)) {

			/*
			 * Inform vPCI about the interupt info changes
			 * TODO: Need to add bdf info for IRQ_INTX type in devicemodel
			 */
#ifndef CONFIG_PARTITION_MODE
			vpci_reset_ptdev_intr_info(target_vm, irq.virt_bdf, irq.phys_bdf);
#endif

			ptirq_remove_msix_remapping(target_vm,
					irq.virt_bdf,
					irq.is.msix.vector_cnt);
		} else {
			pr_err("%s: Invalid irq type: %u\n", __func__, irq.type);
			ret = -1;
		}
	} else {
	        ret = -1;
	}

	return ret;
}

/**
 * @brief Get VCPU Power state.
 *
 * @param vm pointer to VM data structure
 * @param cmd cmd to show get which VCPU power state data
 * @param param VCPU power state data
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_get_cpu_pm_state(struct acrn_vm *vm, uint64_t cmd, uint64_t param)
{
	uint16_t target_vm_id;
	struct acrn_vm *target_vm;
	int32_t ret;

	target_vm_id = (uint16_t)((cmd & PMCMD_VMID_MASK) >> PMCMD_VMID_SHIFT);
	target_vm = get_vm_from_vmid(target_vm_id);

	if (target_vm == NULL) {
	        ret = -1;
	} else {

		switch (cmd & PMCMD_TYPE_MASK) {
		case PMCMD_GET_PX_CNT: {

			if (target_vm->pm.px_cnt == 0U) {
			        ret = -1;
			} else if (copy_to_gpa(vm, &(target_vm->pm.px_cnt), param,
						sizeof(target_vm->pm.px_cnt)) != 0) {
				pr_err("%s: Unable copy param to vm\n", __func__);
			        ret = -1;
			} else {
				ret = 0;
			}
			break;
		}
		case PMCMD_GET_PX_DATA: {
			uint8_t pn;
			struct cpu_px_data *px_data;

			/* For now we put px data as per-vm,
			 * If it is stored as per-cpu in the future,
			 * we need to check PMCMD_VCPUID_MASK in cmd.
			 */
			if (target_vm->pm.px_cnt == 0U) {
			        ret = -1;
				break;
			}

			pn = (uint8_t)((cmd & PMCMD_STATE_NUM_MASK) >> PMCMD_STATE_NUM_SHIFT);
			if (pn >= target_vm->pm.px_cnt) {
			        ret = -1;
				break;
			}

			px_data = target_vm->pm.px_data + pn;
			if (copy_to_gpa(vm, px_data, param,
							sizeof(struct cpu_px_data)) != 0) {
				pr_err("%s: Unable copy param to vm\n", __func__);
			        ret = -1;
				break;
			}

		        ret = 0;
			break;
		}
		case PMCMD_GET_CX_CNT: {

			if (target_vm->pm.cx_cnt == 0U) {
			        ret = -1;
			} else if (copy_to_gpa(vm, &(target_vm->pm.cx_cnt), param,
						sizeof(target_vm->pm.cx_cnt)) != 0) {
				pr_err("%s: Unable copy param to vm\n", __func__);
				ret = -1;
			} else {
				ret = 0;
			}
			break;
		}
		case PMCMD_GET_CX_DATA: {
			uint8_t cx_idx;
			struct cpu_cx_data *cx_data;

			if (target_vm->pm.cx_cnt == 0U) {
			        ret = -1;
				break;
			}

			cx_idx = (uint8_t)
				((cmd & PMCMD_STATE_NUM_MASK) >> PMCMD_STATE_NUM_SHIFT);
			if ((cx_idx == 0U) || (cx_idx > target_vm->pm.cx_cnt)) {
			        ret = -1;
				break;
			}

			cx_data = target_vm->pm.cx_data + cx_idx;

			if (copy_to_gpa(vm, cx_data, param,
							sizeof(struct cpu_cx_data)) != 0) {
				pr_err("%s: Unable copy param to vm\n", __func__);
			        ret = -1;
				break;
			}

		        ret = 0;
			break;
		}
		default:
		        ret = -1;
			break;

		}
	}

	return ret;
}

/**
 * @brief Get VCPU a VM's interrupt count data.
 *
 * @param vm pointer to VM data structure
 * @param vmid id of the VM
 * @param param guest physical address. This gpa points to data structure of
 *              acrn_intr_monitor
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_vm_intr_monitor(struct acrn_vm *vm, uint16_t vmid, uint64_t param)
{
	int32_t status = -EINVAL;
	struct acrn_intr_monitor *intr_hdr;
	uint64_t hpa;
	struct acrn_vm *target_vm = get_vm_from_vmid(vmid);

	if (target_vm != NULL) {

		/* the param for this hypercall is page aligned */
		hpa = gpa2hpa(vm, param);
		if (hpa != INVALID_HPA) {
			intr_hdr = (struct acrn_intr_monitor *)hpa2hva(hpa);
			stac();
			if (intr_hdr->buf_cnt <= (MAX_PTDEV_NUM * 2U)) {
				switch (intr_hdr->cmd) {
				case INTR_CMD_GET_DATA:
					intr_hdr->buf_cnt = ptirq_get_intr_data(target_vm,
						intr_hdr->buffer, intr_hdr->buf_cnt);
					break;

				case INTR_CMD_DELAY_INT:
					/* buffer[0] is the delay time (in MS), if 0 to cancel delay */
					target_vm->intr_inject_delay_delta =
						intr_hdr->buffer[0] * CYCLES_PER_MS;
					break;

				default:
					/* if cmd wrong it goes here should not happen */
					break;
				}

				status = 0;
				pr_dbg("intr monitor:%d, cnt=%d", intr_hdr->cmd, intr_hdr->buf_cnt);
			}
			clac();
		}
	}

	return status;
}

/**
 * @brief set upcall notifier vector
 *
 * This is the API that helps to switch the notifer vecotr. If this API is
 * not called, the hypervisor will use the default notifier vector(0xF7)
 * to notify the SOS kernel.
 *
 * @param vm Pointer to VM data structure
 * @param param the expected notifier vector from guest
 *
 * @pre Pointer vm shall point to VM0
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_callback_vector(const struct acrn_vm *vm, uint64_t param)
{
	int32_t ret;

	if (!is_vm0(vm)) {
		pr_err("%s: Targeting to service vm", __func__);
	        ret = -EPERM;
	} else if ((param > NR_MAX_VECTOR) || (param < VECTOR_DYNAMIC_START)) {
		pr_err("%s: Invalid passed vector\n");
		ret = -EINVAL;
	} else {
		acrn_vhm_vector = (uint32_t)param;
		ret = 0;
	}

	return ret;
}
