/*
 * Copyright (C) 2018-2020 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <vmx.h>
#include <schedule.h>
#include <version.h>
#include <reloc.h>
#include <vtd.h>
#include <per_cpu.h>
#include <lapic.h>
#include <assign.h>
#include <ept.h>
#include <mmu.h>
#include <hypercall.h>
#include <errno.h>
#include <logmsg.h>
#include <ioapic.h>
#include <mmio_dev.h>
#include <ivshmem.h>
#include <vmcs9900.h>
#include <ptcm.h>

#define DBG_LEVEL_HYCALL	6U

typedef int32_t (*emul_dev_create) (struct acrn_vm *vm, struct acrn_emul_dev *dev);
typedef int32_t (*emul_dev_destroy) (struct pci_vdev *vdev);
struct emul_dev_ops {
	/*
	 * The low 32 bits represent the vendor id and device id of PCI device,
	 * and the high 32 bits represent the device number of the legacy device
	 */
	uint64_t dev_id;
	/* TODO: to re-use vdev_init/vdev_deinit directly in hypercall */
	emul_dev_create create;
	emul_dev_destroy destroy;
};

static struct emul_dev_ops emul_dev_ops_tbl[] = {
#ifdef CONFIG_IVSHMEM_ENABLED
	{(IVSHMEM_VENDOR_ID | (IVSHMEM_DEVICE_ID << 16U)), create_ivshmem_vdev , destroy_ivshmem_vdev},
#else
	{(IVSHMEM_VENDOR_ID | (IVSHMEM_DEVICE_ID << 16U)), NULL, NULL},
#endif
	{(MCS9900_VENDOR | (MCS9900_DEV << 16U)), create_vmcs9900_vdev, destroy_vmcs9900_vdev},
};

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

inline static bool is_severity_pass(uint16_t target_vmid)
{
	return SEVERITY_SOS >= get_vm_severity(target_vmid);
}

/**
 * @brief offline vcpu from SOS
 *
 * The function offline specific vcpu from SOS.
 *
 * @param vm Pointer to VM data structure
 * @param param1 lapic id of the vcpu which wants to offline
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_sos_offline_cpu(struct acrn_vm *vm, __unused struct acrn_vm *target_vm,
		uint64_t param1, __unused uint64_t param2)
{
	struct acrn_vcpu *vcpu;
	uint16_t i;
	int32_t ret = 0;
	uint64_t lapicid = param1;

	pr_info("sos offline cpu with lapicid %ld", lapicid);

	foreach_vcpu(i, vm, vcpu) {
		if (vlapic_get_apicid(vcpu_vlapic(vcpu)) == lapicid) {
			/* should not offline BSP */
			if (vcpu->vcpu_id == BSP_CPU_ID) {
				ret = -1;
				break;
			}
			zombie_vcpu(vcpu, VCPU_ZOMBIE);
			offline_vcpu(vcpu);
		}
	}

	return ret;
}

/**
 * @brief Get hypervisor api version
 *
 * The function only return api version information when VM is SOS_VM.
 *
 * @param vm Pointer to VM data structure
 * @param param1 guest physical memory address. The api version returned
 *              will be copied to this gpa
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_get_api_version(struct acrn_vm *vm, __unused struct acrn_vm *target_vm,
		uint64_t param1, __unused uint64_t param2)
{
	struct hc_api_version version;

	version.major_version = HV_API_MAJOR_VERSION;
	version.minor_version = HV_API_MINOR_VERSION;

	return copy_to_gpa(vm, &version, param1, sizeof(version));
}

/**
 * @brief Get basic platform information.
 *
 * The function returns basic hardware or configuration information
 * for the current platform.
 *
 * @param vm Pointer to VM data structure.
 * @param param1 GPA pointer to struct hc_platform_info.
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non zero in case of error.
 */
int32_t hcall_get_platform_info(struct acrn_vm *vm, __unused struct acrn_vm *target_vm,
		uint64_t param1, __unused uint64_t param2)
{
	struct hc_platform_info pi = { 0 };
	uint32_t entry_size = sizeof(struct acrn_vm_config);
	int32_t ret;

	/* to get the vm_config_info pointer */
	ret = copy_from_gpa(vm, &pi, param1, sizeof(pi));
	if (ret == 0) {
		pi.cpu_num = get_pcpu_nums();
		pi.version = 0x100;  /* version 1.0; byte[1:0] = major:minor version */
		pi.max_vcpus_per_vm = MAX_VCPUS_PER_VM;
		pi.max_kata_containers = CONFIG_MAX_KATA_VM_NUM;
		pi.max_vms = CONFIG_MAX_VM_NUM;
		pi.vm_config_entry_size = entry_size;

		/* If it wants to get the vm_configs info */
		if (pi.vm_configs_addr != 0UL) {
			ret = copy_to_gpa(vm, (void *)get_vm_config(0U), pi.vm_configs_addr, entry_size * pi.max_vms);
		}

		if (ret == 0) {
			ret = copy_to_gpa(vm, &pi, param1, sizeof(pi));
		}
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
 * @param target_vm Pointer to target VM data structure
 * @param param1 guest physical memory address. This gpa points to
 *              struct acrn_create_vm
 *
 * @pre Pointer vm shall point to SOS_VM, vm_config != NULL
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_create_vm(struct acrn_vm *vm, struct acrn_vm *target_vm, uint64_t param1, __unused uint64_t param2)
{
	uint16_t vmid = target_vm->vm_id;
	int32_t ret = -1;
	struct acrn_vm *tgt_vm = NULL;
	struct acrn_create_vm cv;
	struct acrn_vm_config* vm_config = NULL;

	if (copy_from_gpa(vm, &cv, param1, sizeof(cv)) == 0) {
		if (is_poweroff_vm(get_vm_from_vmid(vmid))) {

			vm_config = get_vm_config(vmid);

			/* Filter out the bits should not set by DM and then assign it to guest_flags */
			vm_config->guest_flags |= (cv.vm_flag & DM_OWNED_GUEST_FLAG_MASK);

			/* post-launched VM is allowed to choose pCPUs from vm_config->cpu_affinity only */
			if ((cv.cpu_affinity & ~(vm_config->cpu_affinity)) == 0UL) {
				/* By default launch VM with all the configured pCPUs */
				uint64_t pcpu_bitmap = vm_config->cpu_affinity;

				if (cv.cpu_affinity != 0UL) {
					/* overwrite the statically configured CPU affinity */
					pcpu_bitmap = cv.cpu_affinity;
				}

				/*
				 * GUEST_FLAG_RT must be set if we have GUEST_FLAG_LAPIC_PASSTHROUGH
				 * set in guest_flags
				 */
				if (((vm_config->guest_flags & GUEST_FLAG_LAPIC_PASSTHROUGH) != 0UL)
						&& ((vm_config->guest_flags & GUEST_FLAG_RT) == 0UL)) {
					pr_err("Wrong guest flags 0x%lx\n", vm_config->guest_flags);
				} else {
					if (create_vm(vmid, pcpu_bitmap, vm_config, &tgt_vm) == 0) {
						/* return a relative vm_id from SOS view */
						cv.vmid = vmid_2_rel_vmid(vm->vm_id, vmid);
						cv.vcpu_num = tgt_vm->hw.created_vcpus;
					} else {
						dev_dbg(DBG_LEVEL_HYCALL, "HCALL: Create VM failed");
						cv.vmid = ACRN_INVALID_VMID;
					}

					ret = copy_to_gpa(vm, &cv, param1, sizeof(cv));
				}
			} else {
				pr_err("Post-launched VM%u chooses invalid pCPUs(0x%llx).",
						vmid, cv.cpu_affinity);
			}
		}

	}

	return ret;
}

/**
 * @brief destroy virtual machine
 *
 * Destroy a virtual machine, it will pause target VM then shutdown it.
 * The function will return -1 if the target VM does not exist.
 *
 * @param target_vm Pointer to target VM data structure
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_destroy_vm(__unused struct acrn_vm *vm, struct acrn_vm *target_vm,
		__unused uint64_t param1, __unused uint64_t param2)
{
	int32_t ret = -1;

	if (is_paused_vm(target_vm)) {
		/* TODO: check target_vm guest_flags */
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
 * @param target_vm Pointer to target VM data structure
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_start_vm(__unused struct acrn_vm *vm, struct acrn_vm *target_vm,
		__unused uint64_t param1, __unused uint64_t param2)
{
	int32_t ret = -1;

	if ((is_created_vm(target_vm)) && (target_vm->sw.io_shared_page != NULL)) {
		/* TODO: check target_vm guest_flags */
		start_vm(target_vm);
		ret = 0;
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
 * @param target_vm Pointer to target VM data structure
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_pause_vm(__unused struct acrn_vm *vm, struct acrn_vm *target_vm,
		__unused uint64_t param1, __unused uint64_t param2)
{
	int32_t ret = -1;

	if (!is_poweroff_vm(target_vm)) {
		/* TODO: check target_vm guest_flags */
		pause_vm(target_vm);
		ret = 0;
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
 * @param target_vm Pointer to target VM data structure
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_reset_vm(__unused struct acrn_vm *vm, struct acrn_vm *target_vm,
		__unused uint64_t param1, __unused uint64_t param2)
{
	int32_t ret = -1;

	if (is_paused_vm(target_vm)) {
		/* TODO: check target_vm guest_flags */
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
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to
 *              struct acrn_vcpu_regs
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_vcpu_regs(struct acrn_vm *vm, struct acrn_vm *target_vm, __unused uint64_t param1, uint64_t param2)
{
	struct acrn_set_vcpu_regs vcpu_regs;
	struct acrn_vcpu *vcpu;
	int32_t ret = -1;

	/* Only allow setup init ctx while target_vm is inactive */
	if ((!is_poweroff_vm(target_vm)) && (param2 != 0U) && (target_vm->state != VM_RUNNING)) {
		if (copy_from_gpa(vm, &vcpu_regs, param2, sizeof(vcpu_regs)) != 0) {
		} else if (vcpu_regs.vcpu_id >= MAX_VCPUS_PER_VM) {
			pr_err("%s: invalid vcpu_id for set_vcpu_regs\n", __func__);
		} else {
			vcpu = vcpu_from_vid(target_vm, vcpu_regs.vcpu_id);
			if (vcpu->state != VCPU_OFFLINE) {
				set_vcpu_regs(vcpu, &(vcpu_regs.vcpu_regs));
				ret = 0;
			}
		}
	}

	return ret;
}

int32_t hcall_create_vcpu(__unused struct acrn_vm *vm, __unused struct acrn_vm *target_vm,
		__unused uint64_t param1, __unused uint64_t param2)
{
	return 0;
}

/**
 * @brief set or clear IRQ line
 *
 * Set or clear a virtual IRQ line for a VM, which could be from ISA
 * or IOAPIC, normally it triggers an edge IRQ.
 * The function will return -1 if the target VM does not exist.
 *
 * @param vm Pointer to VM data structure
 * @param target_vm Pointer to target VM data structure
 * @param param2 info for irqline
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_irqline(struct acrn_vm *vm, struct acrn_vm *target_vm, __unused uint64_t param1, uint64_t param2)
{
	uint32_t irq_pic;
	int32_t ret = -1;
	struct acrn_irqline_ops *ops = (struct acrn_irqline_ops *)&param2;

	if (is_severity_pass(target_vm->vm_id) && !is_poweroff_vm(target_vm)) {
		if (ops->gsi < get_vm_gsicount(vm)) {
			if (ops->gsi < vpic_pincount()) {
				/*
				 * IRQ line for 8254 timer is connected to
				 * I/O APIC pin #2 but PIC pin #0,route GSI
				 * number #2 to PIC IRQ #0.
				 */
				irq_pic = (ops->gsi == 2U) ? 0U : ops->gsi;
				vpic_set_irqline(vm_pic(target_vm), irq_pic, ops->op);
			}

			/* handle IOAPIC irqline */
			vioapic_set_irqline_lock(target_vm, ops->gsi, ops->op);
			ret = 0;
		}
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
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to struct acrn_msi_entry
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_inject_msi(struct acrn_vm *vm, struct acrn_vm *target_vm, __unused uint64_t param1, uint64_t param2)
{
	int32_t ret = -1;

	if (is_severity_pass(target_vm->vm_id) && !is_poweroff_vm(target_vm)) {
		struct acrn_msi_entry msi;

		if (copy_from_gpa(vm, &msi, param2, sizeof(msi)) == 0) {
			ret = vlapic_inject_msi(target_vm, msi.msi_addr, msi.msi_data);
		}
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
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to
 *              struct acrn_set_ioreq_buffer
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_ioreq_buffer(struct acrn_vm *vm, struct acrn_vm *target_vm, __unused uint64_t param1, uint64_t param2)
{
	uint64_t hpa;
	uint16_t i;
	int32_t ret = -1;

	if (is_created_vm(target_vm)) {
		struct acrn_set_ioreq_buffer iobuf;

		if (copy_from_gpa(vm, &iobuf, param2, sizeof(iobuf)) == 0) {
			dev_dbg(DBG_LEVEL_HYCALL, "[%d] SET BUFFER=0x%p",
					target_vm->vm_id, iobuf.req_buf);

			hpa = gpa2hpa(vm, iobuf.req_buf);
			if (hpa == INVALID_HPA) {
				pr_err("%s,vm[%hu] gpa 0x%lx,GPA is unmapping.",
					__func__, vm->vm_id, iobuf.req_buf);
				target_vm->sw.io_shared_page = NULL;
			} else {
				target_vm->sw.io_shared_page = hpa2hva(hpa);
				for (i = 0U; i < VHM_REQUEST_MAX; i++) {
					set_vhm_req_state(target_vm, i, REQ_STATE_FREE);
				}
				ret = 0;
			}
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
 * @param target_vm Pointer to target VM data structure
 * @param param2 vcpu ID of the requestor
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_notify_ioreq_finish(__unused struct acrn_vm *vm, struct acrn_vm *target_vm,
		__unused uint64_t param1, uint64_t param2)
{
	struct acrn_vcpu *vcpu;
	int32_t ret = -1;
	uint16_t vcpu_id = (uint16_t)param2;

	/* make sure we have set req_buf */
	if (is_severity_pass(target_vm->vm_id) &&
	    (!is_poweroff_vm(target_vm)) && (target_vm->sw.io_shared_page != NULL)) {
		dev_dbg(DBG_LEVEL_HYCALL, "[%d] NOTIFY_FINISH for vcpu %d",
			target_vm->vm_id, vcpu_id);

		if (vcpu_id >= target_vm->hw.created_vcpus) {
			pr_err("%s, failed to get VCPU %d context from VM %d\n",
				__func__, vcpu_id, target_vm->vm_id);
		} else {
			vcpu = vcpu_from_vid(target_vm, vcpu_id);
			if (!vcpu->vm->sw.is_polling_ioreq) {
				signal_event(&vcpu->events[VCPU_EVENT_IOREQ]);
			}
			ret = 0;
		}
	}

	return ret;
}

/**
 *@pre Pointer vm shall point to SOS_VM
 */
static int32_t add_vm_memory_region(struct acrn_vm *vm, struct acrn_vm *target_vm,
				const struct vm_memory_region *region,uint64_t *pml4_page)
{
	int32_t ret;
	uint64_t prot;
	uint64_t hpa, base_paddr;

	hpa = gpa2hpa(vm, region->sos_vm_gpa);
	if (hpa == INVALID_HPA) {
		pr_err("%s,vm[%hu] gpa 0x%lx,GPA is unmapping.",
			__func__, vm->vm_id, region->sos_vm_gpa);
		ret = -EINVAL;
	} else {
		base_paddr = hva2hpa((void *)(get_hv_image_base()));
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
			/* If pSRAM is initialized, and HV received a request to map pSRAM area to guest,
			 * we should add EPT_WB flag to make pSRAM effective.
			 * Assumption: SOS must assign the PSRAM area as a whole and as a separate memory
			 * region whose base address is PSRAM_BASE_HPA
			 * TODO: We can enforce WB for any region has overlap with pSRAM, for simplicity,
			 * and leave it to SOS to make sure it won't violate.
			 */
			if (hpa == PSRAM_BASE_HPA && is_psram_initialized == true) {
				prot |= EPT_WB;
			}
			/* create gpa to hpa EPT mapping */
			ept_add_mr(target_vm, pml4_page, hpa,
					region->gpa, region->size, prot);
			ret = 0;
		}
	}

	return ret;
}

/**
 *@pre Pointer vm shall point to SOS_VM
 */
static int32_t set_vm_memory_region(struct acrn_vm *vm,
	struct acrn_vm *target_vm, const struct vm_memory_region *region)
{
	uint64_t *pml4_page;
	int32_t ret;

	if ((region->size & (PAGE_SIZE - 1UL)) != 0UL) {
		pr_err("%s: [vm%d] map size 0x%x is not page aligned",
			__func__, target_vm->vm_id, region->size);
		ret = -EINVAL;
	} else {
		if (!ept_is_mr_valid(target_vm, region->gpa, region->size)) {
			pr_err("%s, invalid gpa: 0x%lx, size: 0x%lx, top_address_space: 0x%lx", __func__,
				region->gpa, region->size,
				target_vm->arch_vm.ept_mem_ops.info->ept.top_address_space);
			ret = 0;
		} else {
			dev_dbg(DBG_LEVEL_HYCALL,
				"[vm%d] type=%d gpa=0x%x sos_vm_gpa=0x%x size=0x%x",
				target_vm->vm_id, region->type, region->gpa,
				region->sos_vm_gpa, region->size);

			pml4_page = (uint64_t *)target_vm->arch_vm.nworld_eptp;
			if (region->type != MR_DEL) {
				ret = add_vm_memory_region(vm, target_vm, region, pml4_page);
			} else {
				ept_del_mr(target_vm, pml4_page,
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
 * @param target_vm Pointer to target VM data structure
 * @param param1 guest physical address. This gpa points to
 *              struct set_memmaps
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_vm_memory_regions(struct acrn_vm *vm, struct acrn_vm *target_vm,
		uint64_t param1, __unused uint64_t param2)
{
	struct set_regions regions;
	struct vm_memory_region mr;
	uint32_t idx;
	int32_t ret = -1;

	if (copy_from_gpa(vm, &regions, param1, sizeof(regions)) == 0) {

		if (!is_poweroff_vm(target_vm) &&
		    (is_severity_pass(target_vm->vm_id) || (target_vm->state != VM_RUNNING))) {
			idx = 0U;
			while (idx < regions.mr_num) {
				if (copy_from_gpa(vm, &mr, regions.regions_gpa + idx * sizeof(mr), sizeof(mr)) != 0) {
					pr_err("%s: Copy mr entry fail from vm\n", __func__);
					break;
				}

				ret = set_vm_memory_region(vm, target_vm, &mr);
				if (ret < 0) {
					break;
				}
				idx++;
			}
		} else {
			pr_err("%p %s:target_vm is invalid or Targeting to service vm", target_vm, __func__);
		}
	}

	return ret;
}

/**
 *@pre Pointer vm shall point to SOS_VM
 */
static int32_t write_protect_page(struct acrn_vm *vm,const struct wp_data *wp)
{
	uint64_t hpa, base_paddr;
	uint64_t prot_set;
	uint64_t prot_clr;
	int32_t ret = -EINVAL;

	if (is_severity_pass(vm->vm_id)) {
		if ((!mem_aligned_check(wp->gpa, PAGE_SIZE)) ||
				(!ept_is_mr_valid(vm, wp->gpa, PAGE_SIZE))) {
			pr_err("%s,vm[%hu] gpa 0x%lx,GPA is invalid or not page size aligned.",
					__func__, vm->vm_id, wp->gpa);
		} else {
			hpa = gpa2hpa(vm, wp->gpa);
			if (hpa == INVALID_HPA) {
				pr_err("%s,vm[%hu] gpa 0x%lx,GPA is unmapping.",
						__func__, vm->vm_id, wp->gpa);
			} else {
				dev_dbg(DBG_LEVEL_HYCALL, "[vm%d] gpa=0x%x hpa=0x%x",
						vm->vm_id, wp->gpa, hpa);

				base_paddr = hva2hpa((void *)(get_hv_image_base()));
				if (((hpa <= base_paddr) && ((hpa + PAGE_SIZE) > base_paddr)) ||
						((hpa >= base_paddr) &&
						 (hpa < (base_paddr + CONFIG_HV_RAM_SIZE)))) {
					pr_err("%s: overlap the HV memory region.", __func__);
				} else {
					prot_set = (wp->set != 0U) ? 0UL : EPT_WR;
					prot_clr = (wp->set != 0U) ? EPT_WR : 0UL;

					ept_modify_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
							wp->gpa, PAGE_SIZE, prot_set, prot_clr);
					ret = 0;
				}
			}
		}
	}

	return ret;
}

/**
 * @brief change guest memory page write permission
 *
 * @param vm Pointer to VM data structure
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to
 *              struct wp_data
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_write_protect_page(struct acrn_vm *vm, struct acrn_vm *target_vm,
		__unused uint64_t param1, uint64_t param2)
{
	int32_t ret = -1;
	uint64_t wp_gpa = param2;

	if (!is_poweroff_vm(target_vm)) {
		struct wp_data wp;

		if (copy_from_gpa(vm, &wp, wp_gpa, sizeof(wp)) == 0) {
			ret = write_protect_page(target_vm, &wp);
		}
	} else {
		pr_err("%p %s: target_vm is invalid", target_vm, __func__);
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
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to struct vm_gpa2hpa
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_gpa_to_hpa(struct acrn_vm *vm, struct acrn_vm *target_vm, __unused uint64_t param1, uint64_t param2)
{
	int32_t ret = -1;
	struct vm_gpa2hpa v_gpa2hpa;

	(void)memset((void *)&v_gpa2hpa, 0U, sizeof(v_gpa2hpa));
	if (!is_poweroff_vm(target_vm) &&
			(copy_from_gpa(vm, &v_gpa2hpa, param2, sizeof(v_gpa2hpa)) == 0)) {
		v_gpa2hpa.hpa = gpa2hpa(target_vm, v_gpa2hpa.gpa);
		if (v_gpa2hpa.hpa == INVALID_HPA) {
			pr_err("%s,vm[%hu] gpa 0x%lx,GPA is unmapping.",
				__func__, target_vm->vm_id, v_gpa2hpa.gpa);
		} else {
			ret = copy_to_gpa(vm, &v_gpa2hpa, param2, sizeof(v_gpa2hpa));
		}
	} else {
		pr_err("target_vm is invalid or HCALL gpa2hpa: Unable copy param from vm\n");
	}

	return ret;
}

/**
 * @brief Assign one PCI dev to a VM.
 *
 * @param vm Pointer to VM data structure
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_assign_pcidev including assign PCI device info
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_assign_pcidev(struct acrn_vm *vm, struct acrn_vm *target_vm, __unused uint64_t param1, uint64_t param2)
{
	int32_t ret = -EINVAL;
	struct acrn_assign_pcidev pcidev;

	/* We should only assign a device to a post-launched VM at creating time for safety, not runtime or other cases*/
	if (is_created_vm(target_vm)) {
		if (copy_from_gpa(vm, &pcidev, param2, sizeof(pcidev)) == 0) {
			ret = vpci_assign_pcidev(target_vm, &pcidev);
		}
	} else {
		pr_err("%s, vm[%d] is not a postlaunched VM, or not in CREATED status to be assigned with a pcidev\n", __func__, vm->vm_id);
	}

	return ret;
}

/**
 * @brief Deassign one PCI dev from a VM.
 *
 * @param vm Pointer to VM data structure
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_assign_pcidev including deassign PCI device info
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_deassign_pcidev(struct acrn_vm *vm, struct acrn_vm *target_vm, __unused uint64_t param1, uint64_t param2)
{
	int32_t ret = -EINVAL;
	struct acrn_assign_pcidev pcidev;

	/* We should only de-assign a device from a post-launched VM at creating/shutdown/reset time */
	if ((is_paused_vm(target_vm) || is_created_vm(target_vm))) {
		if (copy_from_gpa(vm, &pcidev, param2, sizeof(pcidev)) == 0) {
			ret = vpci_deassign_pcidev(target_vm, &pcidev);
		}
	} else {
		pr_err("%s, vm[%d] is not a postlaunched VM, or not in PAUSED/CREATED status to be deassigned from a pcidev\n", __func__, vm->vm_id);
	}

	return ret;
}

static bool is_mmiodev_valid(struct acrn_mmiodev *mmiodev)
{
	bool ret = true;

	/* TODO: we should define the mmiodev which we could pt in the vmconfig,
	 *       then check the mmiodev whether the address and size are valid
	 *       according the vmconfig. Now we only could pt the TPM acpi device.
	 */
	if ((mmiodev->base_gpa != 0xFED40000UL) ||
		(mmiodev->base_hpa != 0xFED40000UL) ||
		(mmiodev->size != 0x00005000UL)) {
		ret = false;
		pr_fatal("%s, invalid mmiodev gpa: 0x%lx, hpa: 0x%lx, size: 0x%lx",
			__func__, mmiodev->base_gpa, mmiodev->base_hpa, mmiodev->size);
	}

	return ret;
}

/**
 * @brief Assign one MMIO dev to a VM.
 *
 * @param vm Pointer to VM data structure
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_mmiodev including assign MMIO device info
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_assign_mmiodev(struct acrn_vm *vm, struct acrn_vm *target_vm, __unused uint64_t param1, uint64_t param2)
{
	int32_t ret = -EINVAL;
	struct acrn_mmiodev mmiodev;

	/* We should only assign a device to a post-launched VM at creating time for safety, not runtime or other cases*/
	if (is_created_vm(target_vm)) {
		if (copy_from_gpa(vm, &mmiodev, param2, sizeof(mmiodev)) == 0) {
			if (is_mmiodev_valid(&mmiodev)) {
				ret = deassign_mmio_dev(vm, &mmiodev);
				if (ret == 0) {
					ret = assign_mmio_dev(target_vm, &mmiodev);
				}
			}
		}
	} else {
		pr_err("vm[%d] %s failed!\n",target_vm->vm_id,  __func__);
	}

	return ret;
}

/**
 * @brief Deassign one MMIO dev from a VM.
 *
 * @param vm Pointer to VM data structure
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_mmiodev including deassign MMIO device info
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_deassign_mmiodev(struct acrn_vm *vm, struct acrn_vm *target_vm, __unused uint64_t param1, uint64_t param2)
{
	int32_t ret = -EINVAL;
	struct acrn_mmiodev mmiodev;

	/* We should only de-assign a device from a post-launched VM at creating/shutdown/reset time */
	if ((is_paused_vm(target_vm) || is_created_vm(target_vm))) {
		if (copy_from_gpa(vm, &mmiodev, param2, sizeof(mmiodev)) == 0) {
			if (is_mmiodev_valid(&mmiodev)) {
				ret = deassign_mmio_dev(target_vm, &mmiodev);
				if (ret == 0) {
					ret = assign_mmio_dev(vm, &mmiodev);
				}
			}
		}
	} else {
		pr_err("vm[%d] %s failed!\n",target_vm->vm_id,  __func__);
	}

	return ret;
}

/**
 * @brief Set interrupt mapping info of ptdev.
 *
 * @param vm Pointer to VM data structure
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to data structure of
 *              hc_ptdev_irq including intr remapping info
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_ptdev_intr_info(struct acrn_vm *vm, struct acrn_vm *target_vm,
	       __unused uint64_t param1, uint64_t param2)
{
	int32_t ret = -1;

	if (is_created_vm(target_vm)) {
		struct hc_ptdev_irq irq;

		if (copy_from_gpa(vm, &irq, param2, sizeof(irq)) == 0) {
			if (irq.type == IRQ_INTX) {
				struct pci_vdev *vdev;
				union pci_bdf bdf = {.value = irq.virt_bdf};
				struct acrn_vpci *vpci = &target_vm->vpci;

				spinlock_obtain(&vpci->lock);
				vdev = pci_find_vdev(vpci, bdf);
				spinlock_release(&vpci->lock);
				/*
				 * TODO: Change the hc_ptdev_irq structure member names
				 * virt_pin to virt_gsi
				 * phys_pin to phys_gsi
				 */
				if ((vdev != NULL) && (vdev->pdev->bdf.value == irq.phys_bdf)) {
					if ((((!irq.intx.pic_pin) && (irq.intx.virt_pin < get_vm_gsicount(target_vm)))
						|| ((irq.intx.pic_pin) && (irq.intx.virt_pin < vpic_pincount())))
							&& is_gsi_valid(irq.intx.phys_pin)) {
						ret = ptirq_add_intx_remapping(target_vm, irq.intx.virt_pin,
								irq.intx.phys_pin, irq.intx.pic_pin);
					} else {
						pr_err("%s: Invalid phys pin or virt pin\n", __func__);
					}
				}
			} else {
				pr_err("%s: Invalid irq type: %u\n", __func__, irq.type);
			}
		}
	}
	return ret;
}

/**
 * @brief Clear interrupt mapping info of ptdev.
 *
 * @param vm Pointer to VM data structure
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to data structure of
 *              hc_ptdev_irq including intr remapping info
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_reset_ptdev_intr_info(struct acrn_vm *vm, struct acrn_vm *target_vm,
		__unused uint64_t param1, uint64_t param2)
{
	int32_t ret = -1;

	if (is_created_vm(target_vm) || is_paused_vm(target_vm)) {
		struct hc_ptdev_irq irq;

		if (copy_from_gpa(vm, &irq, param2, sizeof(irq)) == 0) {
			if (irq.type == IRQ_INTX) {
				struct pci_vdev *vdev;
				union pci_bdf bdf = {.value = irq.virt_bdf};
				struct acrn_vpci *vpci = &target_vm->vpci;

				spinlock_obtain(&vpci->lock);
				vdev = pci_find_vdev(vpci, bdf);
				spinlock_release(&vpci->lock);
				/*
				 * TODO: Change the hc_ptdev_irq structure member names
				 * virt_pin to virt_gsi
				 * phys_pin to phys_gsi
				 */
				if ((vdev != NULL) && (vdev->pdev->bdf.value == irq.phys_bdf)) {
					if (((!irq.intx.pic_pin) && (irq.intx.virt_pin < get_vm_gsicount(target_vm))) ||
						((irq.intx.pic_pin) && (irq.intx.virt_pin < vpic_pincount()))) {
						ptirq_remove_intx_remapping(target_vm, irq.intx.virt_pin, irq.intx.pic_pin);
						ret = 0;
					} else {
						pr_err("%s: Invalid virt pin\n", __func__);
					}
				}
			} else {
				pr_err("%s: Invalid irq type: %u\n", __func__, irq.type);
			}
		}
	}

	return ret;
}

/**
 * @brief Get VCPU Power state.
 *
 * @param vm pointer to VM data structure
 * @param target_vm Pointer to target VM data structure
 * @param param1 cmd to show get which VCPU power state data
 * @param param2 VCPU power state data
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_get_cpu_pm_state(struct acrn_vm *vm, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2)
{
	int32_t ret = -1;
	uint64_t cmd = param1;

	if (is_created_vm(target_vm)) {
		switch (cmd & PMCMD_TYPE_MASK) {
		case PMCMD_GET_PX_CNT: {
			if (target_vm->pm.px_cnt != 0U) {
				ret = copy_to_gpa(vm, &(target_vm->pm.px_cnt), param2, sizeof(target_vm->pm.px_cnt));
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
				break;
			}

			pn = (uint8_t)((cmd & PMCMD_STATE_NUM_MASK) >> PMCMD_STATE_NUM_SHIFT);
			if (pn >= target_vm->pm.px_cnt) {
				break;
			}

			px_data = target_vm->pm.px_data + pn;
			ret = copy_to_gpa(vm, px_data, param2, sizeof(struct cpu_px_data));
			break;
		}
		case PMCMD_GET_CX_CNT: {
			if (target_vm->pm.cx_cnt != 0U) {
				ret = copy_to_gpa(vm, &(target_vm->pm.cx_cnt), param2, sizeof(target_vm->pm.cx_cnt));
			}
			break;
		}
		case PMCMD_GET_CX_DATA: {
			uint8_t cx_idx;
			struct cpu_cx_data *cx_data;

			if (target_vm->pm.cx_cnt == 0U) {
				break;
			}

			cx_idx = (uint8_t)
				((cmd & PMCMD_STATE_NUM_MASK) >> PMCMD_STATE_NUM_SHIFT);
			if ((cx_idx == 0U) || (cx_idx > target_vm->pm.cx_cnt)) {
				break;
			}

			cx_data = target_vm->pm.cx_data + cx_idx;
			ret = copy_to_gpa(vm, cx_data, param2, sizeof(struct cpu_cx_data));
			break;
		}
		default:
			/* invalid command */
			break;
		}
	}

	return ret;
}

/**
 * @brief Get VCPU a VM's interrupt count data.
 *
 * @param vm pointer to VM data structure
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_intr_monitor
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_vm_intr_monitor(struct acrn_vm *vm, struct acrn_vm *target_vm, __unused uint64_t param1, uint64_t param2)
{
	int32_t status = -EINVAL;
	struct acrn_intr_monitor *intr_hdr;
	uint64_t hpa;

	if (!is_poweroff_vm(target_vm)) {
		/* the param for this hypercall is page aligned */
		hpa = gpa2hpa(vm, param2);
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
 * not called, the hypervisor will use the default notifier vector(0xF3)
 * to notify the SOS kernel.
 *
 * @param vm Pointer to VM data structure
 * @param param1 the expected notifier vector from guest
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_callback_vector(__unused struct acrn_vm *vm, __unused struct acrn_vm *target_vm,
		uint64_t param1, __unused uint64_t param2)
{
	int32_t ret;

	if ((param1 > NR_MAX_VECTOR) || (param1 < VECTOR_DYNAMIC_START)) {
		pr_err("%s: Invalid passed vector\n", __func__);
		ret = -EINVAL;
	} else {
		set_vhm_notification_vector((uint32_t)param1);
		ret = 0;
	}

	return ret;
}

/*
 * @pre dev != NULL
 */
static struct emul_dev_ops *find_emul_dev_ops(struct acrn_emul_dev *dev)
{
	struct emul_dev_ops *op = NULL;
	uint32_t i;

	for (i = 0U; i < ARRAY_SIZE(emul_dev_ops_tbl); i++) {
		if (emul_dev_ops_tbl[i].dev_id == dev->dev_id.value) {
			op = &emul_dev_ops_tbl[i];
			break;
		}
	}
	return op;
}

/**
 * @brief Create an emulated device in hypervisor.
 *
 * @param vm pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to data structure of
 *              acrn_emul_dev including information about PCI or legacy devices
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_create_vdev(struct acrn_vm *vm, struct acrn_vm *target_vm, __unused uint64_t param1, uint64_t param2)
{
	int32_t ret = -EINVAL;
	struct acrn_emul_dev dev;
	struct emul_dev_ops *op;

	/* We should only create a device to a post-launched VM at creating time for safety, not runtime or other cases*/
	if (is_created_vm(target_vm)) {
		if (copy_from_gpa(vm, &dev, param2, sizeof(dev)) == 0) {
			op = find_emul_dev_ops(&dev);
			if ((op != NULL) && (op->create != NULL)) {
				ret = op->create(target_vm, &dev);
			}
		}
	} else {
		pr_err("%s, vm[%d] is not a postlaunched VM, or not in CREATED status to create a vdev\n", __func__, target_vm->vm_id);
	}
	return ret;
}

/**
 * @brief Destroy an emulated device in hypervisor.
 *
 * @param vm pointer to VM data structure
 * @param vmid ID of the VM
 * @param param guest physical address. This gpa points to data structure of
 *              acrn_emul_dev including information about PCI or legacy devices
 *
 * @pre Pointer vm shall point to SOS_VM
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_destroy_vdev(struct acrn_vm *vm, struct acrn_vm *target_vm, __unused uint64_t param1, uint64_t param2)
{
	int32_t ret = -EINVAL;
	struct acrn_emul_dev dev;
	struct pci_vdev *vdev;
	struct emul_dev_ops *op;
	union pci_bdf bdf;

	/* We should only destroy a device to a post-launched VM at creating or pausing time for safety, not runtime or other cases*/
	if (is_created_vm(target_vm) || is_paused_vm(target_vm)) {
		if (copy_from_gpa(vm, &dev, param2, sizeof(dev)) == 0) {
			op = find_emul_dev_ops(&dev);
			if (op != NULL) {
				bdf.value = (uint16_t) dev.slot;
				vdev = pci_find_vdev(&target_vm->vpci, bdf);
				if (vdev != NULL) {
					vdev->pci_dev_config->vbdf.value = UNASSIGNED_VBDF;
					if (op->destroy != NULL) {
						ret = op->destroy(vdev);
					} else {
						ret = 0;
					}
				} else {
					pr_warn("%s, failed to destroy emulated device %x:%x.%x\n",
						__func__, bdf.bits.b, bdf.bits.d, bdf.bits.f);
				}
			}
		}
	} else {
		pr_err("%s, vm[%d] is not a postlaunched VM, or not in CREATED/PAUSED status to destroy a vdev\n", __func__, target_vm->vm_id);
	}
	return ret;
}
