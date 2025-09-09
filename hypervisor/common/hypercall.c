/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/guest/vm.h>
#include <asm/vmx.h>
#include <schedule.h>
#include <version.h>
#include <reloc.h>
#include <asm/vtd.h>
#include <per_cpu.h>
#include <asm/lapic.h>
#include <asm/guest/assign.h>
#include <asm/guest/ept.h>
#include <asm/guest/vm.h>
#include <asm/mmu.h>
#include <hypercall.h>
#include <errno.h>
#include <logmsg.h>
#include <asm/ioapic.h>
#include <mmio_dev.h>
#include <ivshmem.h>
#include <vmcs9900.h>
#include <asm/rtcm.h>
#include <asm/irq.h>
#include <ticks.h>
#include <asm/cpuid.h>
#include <vroot_port.h>

#define DBG_LEVEL_HYCALL	6U

typedef int32_t (*emul_dev_create) (struct acrn_vm *vm, struct acrn_vdev *dev);
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
	{(VRP_VENDOR | (VRP_DEVICE << 16U)), create_vrp, destroy_vrp},
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

/**
 * @brief offline vcpu from Service VM
 *
 * The function offline specific vcpu from Service VM.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param param1 lapic id of the vcpu which wants to offline
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_service_vm_offline_cpu(struct acrn_vcpu *vcpu, __unused struct acrn_vm *target_vm,
		uint64_t param1, __unused uint64_t param2)
{
	struct acrn_vcpu *target_vcpu;
	uint16_t i;
	int32_t ret = 0;
	uint32_t lapicid = (uint32_t)param1;

	pr_info("Service VM offline cpu with lapicid %ld", lapicid);

	foreach_vcpu(i, vcpu->vm, target_vcpu) {
		if (vlapic_get_apicid(vcpu_vlapic(target_vcpu)) == lapicid) {
			/* should not offline BSP */
			if (target_vcpu->vcpu_id == BSP_CPU_ID) {
				ret = -1;
				break;
			}
			zombie_vcpu(target_vcpu, VCPU_ZOMBIE);
			offline_vcpu(target_vcpu);
		}
	}

	return ret;
}

/**
 * @brief Get hypervisor api version
 *
 * The function only return api version information when VM is Service VM.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param param1 guest physical memory address. The api version returned
 *              will be copied to this gpa
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_get_api_version(struct acrn_vcpu *vcpu, __unused struct acrn_vm *target_vm,
		uint64_t param1, __unused uint64_t param2)
{
	struct hc_api_version version;

	version.major_version = HV_API_MAJOR_VERSION;
	version.minor_version = HV_API_MINOR_VERSION;

	return copy_to_gpa(vcpu->vm, &version, param1, sizeof(version));
}

/*
 * nearest_pow2(n) is the nearest power of 2 integer that is not less than n
 * The last (most significant) bit set of (n*2-1) matches the above definition
 */
static uint32_t nearest_pow2(uint32_t n)
{
	uint32_t p = n;

	if (n >= 2U) {
		p = fls32(2U*n - 1U);
	}

	return p;
}

void get_cache_shift(uint32_t *l2_shift, uint32_t *l3_shift)
{
	uint32_t subleaf;

	*l2_shift = 0U;
	*l3_shift = 0U;

	for (subleaf = 0U;; subleaf++) {
		uint32_t eax, ebx, ecx, edx;
		uint32_t cache_type, cache_level, id, shift;

		cpuid_subleaf(0x4U, subleaf, &eax, &ebx, &ecx, &edx);

		cache_type = eax & 0x1fU;
		cache_level = (eax >> 5U) & 0x7U;
		id = (eax >> 14U) & 0xfffU;
		shift = nearest_pow2(id + 1U);

		/* No more caches */
		if ((cache_type == 0U) || (cache_type >= 4U)) {
			break;
		}

		if (cache_level == 2U) {
			*l2_shift = shift;
		} else if (cache_level == 3U) {
			*l3_shift = shift;
		} else {
			/* this api only for L2 & L3 cache */
		}
	}
}

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
 *
 * @pre is_service_vm(vcpu->vm)
 * @pre get_vm_config(target_vm->vm_id) != NULL
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_create_vm(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, __unused uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
	uint16_t vmid = target_vm->vm_id;
	int32_t ret = -1;
	struct acrn_vm *tgt_vm = NULL;
	struct acrn_vm_creation cv;
	struct acrn_vm_config *vm_config = get_vm_config(vmid);

	if (copy_from_gpa(vm, &cv, param1, sizeof(cv)) == 0) {
		if (is_poweroff_vm(get_vm_from_vmid(vmid))) {

			/* Filter out the bits should not set by DM and then assign it to guest_flags */
			vm_config->guest_flags &= ~DM_OWNED_GUEST_FLAG_MASK;
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
						/* return a relative vm_id from Service VM view */
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

	if (((ret != 0) || (cv.vmid == ACRN_INVALID_VMID)) && (!is_static_configured_vm(target_vm))) {
		memset(vm_config->name, 0U, MAX_VM_NAME_LEN);
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
int32_t hcall_destroy_vm(__unused struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
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
int32_t hcall_start_vm(__unused struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
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
int32_t hcall_pause_vm(__unused struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
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
int32_t hcall_reset_vm(__unused struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		__unused uint64_t param1, __unused uint64_t param2)
{
	int32_t ret = -1;

	if (is_paused_vm(target_vm)) {
		/* TODO: check target_vm guest_flags */
		ret = reset_vm(target_vm, COLD_RESET);
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
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to
 *              struct acrn_vcpu_regs
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_vcpu_regs(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		__unused uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
	struct acrn_vcpu_regs vcpu_regs;
	struct acrn_vcpu *target_vcpu;
	int32_t ret = -1;

	/* Only allow setup init ctx while target_vm is inactive */
	if ((!is_poweroff_vm(target_vm)) && (param2 != 0U) && (target_vm->state != VM_RUNNING)) {
		if (copy_from_gpa(vm, &vcpu_regs, param2, sizeof(vcpu_regs)) != 0) {
		} else if (vcpu_regs.vcpu_id >= MAX_VCPUS_PER_VM) {
			pr_err("%s: invalid vcpu_id for set_vcpu_regs\n", __func__);
		} else {
			target_vcpu = vcpu_from_vid(target_vm, vcpu_regs.vcpu_id);
			if (target_vcpu->state != VCPU_OFFLINE) {
				if (is_valid_cr0_cr4(vcpu_regs.vcpu_regs.cr0, vcpu_regs.vcpu_regs.cr4)) {
					set_vcpu_regs(target_vcpu, &(vcpu_regs.vcpu_regs));
					ret = 0;
				}
			}
		}
	}

	return ret;
}

int32_t hcall_create_vcpu(__unused struct acrn_vcpu *vcpu, __unused struct acrn_vm *target_vm,
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
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param2 info for irqline
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_irqline(__unused struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
	__unused uint64_t param1, uint64_t param2)
{
	uint32_t irq_pic;
	int32_t ret = -1;
	struct acrn_irqline_ops *ops = (struct acrn_irqline_ops *)&param2;

	if (is_severity_pass(target_vm->vm_id) && !is_poweroff_vm(target_vm)) {
		if (ops->gsi < get_vm_gsicount(target_vm)) {
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
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to struct acrn_msi_entry
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_inject_msi(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, __unused uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
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
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to buffer address
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_ioreq_buffer(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		__unused uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
	uint64_t hpa;
	uint16_t i;
	int32_t ret = -1;

	if (is_created_vm(target_vm)) {
		uint64_t iobuf;

		if (copy_from_gpa(vm, &iobuf, param2, sizeof(iobuf)) == 0) {
			dev_dbg(DBG_LEVEL_HYCALL, "[%d] SET BUFFER=0x%p",
					target_vm->vm_id, iobuf);

			hpa = gpa2hpa(vm, iobuf);
			if (hpa == INVALID_HPA) {
				pr_err("%s,vm[%hu] gpa 0x%lx,GPA is unmapping.",
					__func__, vm->vm_id, iobuf);
				target_vm->sw.io_shared_page = NULL;
			} else {
				target_vm->sw.io_shared_page = hpa2hva(hpa);
				for (i = 0U; i < ACRN_IO_REQUEST_MAX; i++) {
					set_io_req_state(target_vm, i, ACRN_IOREQ_STATE_FREE);
				}
				ret = 0;
			}
		}
	}

	return ret;
}

/**
 * @brief Setup a share buffer for a VM.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param param1 guest physical address. This gpa points to
 *              struct sbuf_setup_param
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_setup_sbuf(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		__unused uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
	struct acrn_sbuf_param asp;
	uint64_t *hva;
	int ret = -1;

	if (copy_from_gpa(vm, &asp, param2, sizeof(asp)) == 0) {
		if (asp.gpa != 0U) {
			hva = (uint64_t *)gpa2hva(vm, asp.gpa);
			ret = sbuf_setup_common(target_vm, asp.cpu_id, asp.sbuf_id, hva);
		}
	}
	return ret;
}

int32_t hcall_asyncio_assign(__unused struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		 __unused uint64_t param1, uint64_t param2)
{
	struct acrn_asyncio_info asyncio_info;
	struct acrn_vm *vm = vcpu->vm;
	int ret = -1;

	if (copy_from_gpa(vm, &asyncio_info, param2, sizeof(asyncio_info)) == 0) {
		add_asyncio(target_vm, &asyncio_info);
		ret = 0;
	}
	return ret;
}

int32_t hcall_asyncio_deassign(__unused struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		 __unused uint64_t param1, uint64_t param2)
{
	struct acrn_asyncio_info asyncio_info;
	struct acrn_vm *vm = vcpu->vm;
	int ret = -1;

	if (copy_from_gpa(vm, &asyncio_info, param2, sizeof(asyncio_info)) == 0) {
		remove_asyncio(target_vm, &asyncio_info);
		ret = 0;
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
int32_t hcall_notify_ioreq_finish(__unused struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		__unused uint64_t param1, uint64_t param2)
{
	struct acrn_vcpu *target_vcpu;
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
			target_vcpu = vcpu_from_vid(target_vm, vcpu_id);
			if (!target_vcpu->vm->sw.is_polling_ioreq) {
				signal_event(&target_vcpu->events[VCPU_EVENT_IOREQ]);
			}
			ret = 0;
		}
	}

	return ret;
}

/**
 *@pre is_service_vm(vm)
 *@pre gpa2hpa(vm, region->service_vm_gpa) != INVALID_HPA
 */
static void add_vm_memory_region(struct acrn_vm *vm, struct acrn_vm *target_vm,
				const struct vm_memory_region *region,uint64_t *pml4_page)
{
	uint64_t prot = 0UL, base_paddr;
	uint64_t hpa = gpa2hpa(vm, region->service_vm_gpa);

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

	/* If Software SRAM is initialized, and HV received a request to map Software SRAM
	 * area to guest, we should add EPT_WB flag to make Software SRAM effective.
	 * TODO: We can enforce WB for any region has overlap with Software SRAM, for simplicity,
	 * and leave it to Service VM to make sure it won't violate.
	 */
	if (is_software_sram_enabled()) {
		base_paddr = get_software_sram_base();
		if ((hpa >= base_paddr) &&
			((hpa + region->size) <= (base_paddr + get_software_sram_size()))) {
			prot |= EPT_WB;
		}
	}

	/* create gpa to hpa EPT mapping */
	ept_add_mr(target_vm, pml4_page, hpa, region->gpa, region->size, prot);
}

/**
 *@pre is_service_vm(vm)
 */
static int32_t set_vm_memory_region(struct acrn_vm *vm,
	struct acrn_vm *target_vm, const struct vm_memory_region *region)
{
	uint64_t *pml4_page;
	int32_t ret = -EINVAL;

	if ((region->size & (PAGE_SIZE - 1UL)) == 0UL) {
		pml4_page = (uint64_t *)target_vm->arch_vm.nworld_eptp;
		if (region->type == MR_ADD) {
			/* if the GPA range is Service VM valid GPA or not */
			if (ept_is_valid_mr(vm, region->service_vm_gpa, region->size)) {
				/* FIXME: how to filter the alias mapping ? */
				add_vm_memory_region(vm, target_vm, region, pml4_page);
				ret = 0;
			}
		} else {
			if (ept_is_valid_mr(target_vm, region->gpa, region->size)) {
				ept_del_mr(target_vm, pml4_page, region->gpa, region->size);
				ret = 0;
			}
		}
	}

	dev_dbg((ret == 0) ? DBG_LEVEL_HYCALL : LOG_ERROR,
			"[vm%d] type=%d gpa=0x%x service_vm_gpa=0x%x sz=0x%x",
			target_vm->vm_id, region->type, region->gpa,
			region->service_vm_gpa, region->size);
	return ret;
}

/**
 * @brief setup ept memory mapping for multi regions
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param1 guest physical address. This gpa points to
 *              struct set_memmaps
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_vm_memory_regions(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		uint64_t param1, __unused uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
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
 *@pre is_service_vm(vm)
 */
static int32_t write_protect_page(struct acrn_vm *vm,const struct wp_data *wp)
{
	uint64_t hpa, base_paddr;
	uint64_t prot_set;
	uint64_t prot_clr;
	int32_t ret = -EINVAL;

	if (is_severity_pass(vm->vm_id)) {
		if ((!mem_aligned_check(wp->gpa, PAGE_SIZE)) ||
				(!ept_is_valid_mr(vm, wp->gpa, PAGE_SIZE))) {
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
						 (hpa < (base_paddr + get_hv_image_size())))) {
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
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to
 *              struct wp_data
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_write_protect_page(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		__unused uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
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
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to struct vm_gpa2hpa
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_gpa_to_hpa(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, __unused uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
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
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_pcidev including assign PCI device info
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_assign_pcidev(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		__unused uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
	int32_t ret = -EINVAL;
	struct acrn_pcidev pcidev;

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
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_pcidev including deassign PCI device info
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_deassign_pcidev(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		__unused uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
	int32_t ret = -EINVAL;
	struct acrn_pcidev pcidev;

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

/**
 * @brief Assign one MMIO dev to a VM.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_mmiodev including assign MMIO device info
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_assign_mmiodev(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		__unused uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
	int32_t ret = -EINVAL;
	struct acrn_mmiodev mmiodev;

	/* We should only assign a device to a post-launched VM at creating time for safety, not runtime or other cases*/
	if (is_created_vm(target_vm)) {
		if (copy_from_gpa(vm, &mmiodev, param2, sizeof(mmiodev)) == 0) {
			ret = deassign_mmio_dev(vm, &mmiodev);
			if (ret == 0) {
				ret = assign_mmio_dev(target_vm, &mmiodev);
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
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_mmiodev including deassign MMIO device info
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_deassign_mmiodev(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		__unused uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
	int32_t ret = -EINVAL;
	struct acrn_mmiodev mmiodev;

	/* We should only de-assign a device from a post-launched VM at creating/shutdown/reset time */
	if ((is_paused_vm(target_vm) || is_created_vm(target_vm))) {
		if (copy_from_gpa(vm, &mmiodev, param2, sizeof(mmiodev)) == 0) {
			ret = deassign_mmio_dev(target_vm, &mmiodev);
			if (ret == 0) {
				ret = assign_mmio_dev(vm, &mmiodev);
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
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to data structure of
 *              hc_ptdev_irq including intr remapping info
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_ptdev_intr_info(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
	       __unused uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
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
						ptirq_remove_intx_remapping(get_service_vm(), irq.intx.phys_pin, false, true);
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
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to data structure of
 *              hc_ptdev_irq including intr remapping info
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_reset_ptdev_intr_info(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		__unused uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
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
						ptirq_remove_intx_remapping(target_vm, irq.intx.virt_pin, irq.intx.pic_pin, false);
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

static bool is_pt_pstate(__unused const struct acrn_vm *vm)
{
	/* Currently VM's CPU frequency is managed in hypervisor. So no pass through for all VMs. */
	return false;
}

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
int32_t hcall_get_cpu_pm_state(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
	int32_t ret = -1;
	uint64_t cmd = param1;

	if (is_created_vm(target_vm) || is_paused_vm(target_vm)) {
		switch (cmd & PMCMD_TYPE_MASK) {
		case ACRN_PMCMD_GET_PX_CNT: {
			uint8_t px_cnt;
			/* If the VM supports vHWP, then the guest is having continuous p-state. Thus it doesn't have a specific
			 * px_cnt. The hypercall returns success and px_cnt = 0.
			 * If the VM's p-state is hidden or hv doesn't have its p-state info, the hypercall returns -1.
			 */
			if (is_vhwp_configured(target_vm)) {
				px_cnt = 0U;
			} else if (!is_pt_pstate(target_vm)) {
				break;
			} else if (target_vm->pm.px_cnt == 0U) {
				break;
			} else {
				px_cnt = target_vm->pm.px_cnt;
			}
			ret = copy_to_gpa(vm, &px_cnt, param2, sizeof(px_cnt));
			break;
		}
		case ACRN_PMCMD_GET_PX_DATA: {
			uint8_t pn;
			struct acrn_pstate_data *px_data;

			if (!is_pt_pstate(target_vm)) {
				break;
			}

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
			ret = copy_to_gpa(vm, px_data, param2, sizeof(struct acrn_pstate_data));
			break;
		}
		case ACRN_PMCMD_GET_CX_CNT: {
			ret = copy_to_gpa(vm, &(target_vm->pm.cx_cnt), param2, sizeof(target_vm->pm.cx_cnt));
			break;
		}
		case ACRN_PMCMD_GET_CX_DATA: {
			uint8_t cx_idx;
			struct acrn_cstate_data *cx_data;

			if (target_vm->pm.cx_cnt == 0U) {
				break;
			}

			cx_idx = (uint8_t)
				((cmd & PMCMD_STATE_NUM_MASK) >> PMCMD_STATE_NUM_SHIFT);
			if ((cx_idx == 0U) || (cx_idx > target_vm->pm.cx_cnt)) {
				break;
			}

			cx_data = target_vm->pm.cx_data + cx_idx;
			ret = copy_to_gpa(vm, cx_data, param2, sizeof(struct acrn_cstate_data));
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
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param2 guest physical address. This gpa points to data structure of
 *              acrn_intr_monitor
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_vm_intr_monitor(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm,
		__unused uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
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
				status = 0;

				switch (intr_hdr->cmd) {
				case INTR_CMD_GET_DATA:
					intr_hdr->buf_cnt = ptirq_get_intr_data(target_vm,
						intr_hdr->buffer, intr_hdr->buf_cnt);
					break;

				case INTR_CMD_DELAY_INT:
					/* buffer[0] is the delay time (in MS), if 0 to cancel delay */
					target_vm->intr_inject_delay_delta =
						intr_hdr->buffer[0] * TICKS_PER_MS;
					break;

				default:
					/* if cmd wrong it goes here should not happen */
					status = -EINVAL;
					break;
				}
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
 * to notify the Service VM kernel.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param param1 the expected notifier vector from guest
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_set_callback_vector(__unused struct acrn_vcpu *vcpu, __unused struct acrn_vm *target_vm,
		uint64_t param1, __unused uint64_t param2)
{
	int32_t ret;

	if ((param1 > NR_MAX_VECTOR) || (param1 < VECTOR_DYNAMIC_START)) {
		pr_err("%s: Invalid passed vector\n", __func__);
		ret = -EINVAL;
	} else {
		set_hsm_notification_vector((uint32_t)param1);
		ret = 0;
	}

	return ret;
}

/*
 * @pre dev != NULL
 */
static struct emul_dev_ops *find_emul_dev_ops(struct acrn_vdev *dev)
{
	struct emul_dev_ops *op = NULL;
	uint32_t i;

	for (i = 0U; i < ARRAY_SIZE(emul_dev_ops_tbl); i++) {
		if (emul_dev_ops_tbl[i].dev_id == dev->id.value) {
			op = &emul_dev_ops_tbl[i];
			break;
		}
	}
	return op;
}

/**
 * @brief Add an emulated device in hypervisor.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param guest physical address. This gpa points to data structure of
 *              acrn_vdev including information about PCI or legacy devices
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_add_vdev(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, __unused uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
	int32_t ret = -EINVAL;
	struct acrn_vdev dev;
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
 * @brief Remove an emulated device in hypervisor.
 *
 * @param vcpu Pointer to vCPU that initiates the hypercall
 * @param target_vm Pointer to target VM data structure
 * @param param guest physical address. This gpa points to data structure of
 *              acrn_vdev including information about PCI or legacy devices
 *
 * @pre is_service_vm(vcpu->vm)
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_remove_vdev(struct acrn_vcpu *vcpu, struct acrn_vm *target_vm, __unused uint64_t param1, uint64_t param2)
{
	struct acrn_vm *vm = vcpu->vm;
	int32_t ret = -EINVAL;
	struct acrn_vdev dev;
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
