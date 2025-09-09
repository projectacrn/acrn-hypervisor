/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <sprintf.h>
#include <per_cpu.h>
#include <asm/lapic.h>
#include <asm/guest/vm.h>
#include <asm/guest/vm_reset.h>
#include <asm/guest/virq.h>
#include <asm/lib/bits.h>
#include <asm/e820.h>
#include <boot.h>
#include <asm/vtd.h>
#include <reloc.h>
#include <asm/guest/ept.h>
#include <asm/guest/guest_pm.h>
#include <console.h>
#include <ptdev.h>
#include <asm/guest/vmcs.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <logmsg.h>
#include <vboot.h>
#include <asm/board.h>
#include <asm/sgx.h>
#include <sbuf.h>
#include <asm/pci_dev.h>
#include <vacpi.h>
#include <asm/platform_caps.h>
#include <mmio_dev.h>
#include <asm/trampoline.h>
#include <asm/guest/assign.h>
#include <vgpio.h>
#include <asm/rtcm.h>
#include <asm/irq.h>
#include <uart16550.h>
#ifdef CONFIG_SECURITY_VM_FIXUP
#include <quirks/security_vm_fixup.h>
#endif
#include <asm/boot/ld_sym.h>
#include <asm/guest/optee.h>

/* Local variables */

/* pre-assumption: TRUSTY_RAM_SIZE is 2M aligned */
static struct page post_user_vm_sworld_memory[MAX_TRUSTY_VM_NUM][TRUSTY_RAM_SIZE >> PAGE_SHIFT] __aligned(MEM_2M);

static struct acrn_vm vm_array[CONFIG_MAX_VM_NUM] __aligned(PAGE_SIZE);

static struct acrn_vm *service_vm_ptr = NULL;

void *get_sworld_memory_base(void)
{
	return post_user_vm_sworld_memory;
}

uint16_t get_unused_vmid(void)
{
	uint16_t vm_id;
	struct acrn_vm_config *vm_config;

	for (vm_id = 0; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);
		if ((vm_config->name[0] == '\0') && ((vm_config->guest_flags & GUEST_FLAG_STATIC_VM) == 0U)) {
			break;
		}
	}
	return (vm_id < CONFIG_MAX_VM_NUM) ? (vm_id) : (ACRN_INVALID_VMID);
}

uint16_t get_vmid_by_name(const char *name)
{
	uint16_t vm_id;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		if ((*name != '\0') && vm_has_matched_name(vm_id, name)) {
			break;
		}
	}
	return (vm_id < CONFIG_MAX_VM_NUM) ? (vm_id) : (ACRN_INVALID_VMID);
}

/**
 * @pre vm != NULL
 */
bool is_poweroff_vm(const struct acrn_vm *vm)
{
	return (vm->state == VM_POWERED_OFF);
}

/**
 * @pre vm != NULL
 */
bool is_created_vm(const struct acrn_vm *vm)
{
	return (vm->state == VM_CREATED);
}

/**
 * @pre vm != NULL
 */
bool is_paused_vm(const struct acrn_vm *vm)
{
	return (vm->state == VM_PAUSED);
}

bool is_service_vm(const struct acrn_vm *vm)
{
	return (vm != NULL)  && (get_vm_config(vm->vm_id)->load_order == SERVICE_VM);
}

/**
 * @pre vm != NULL
 * @pre vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_postlaunched_vm(const struct acrn_vm *vm)
{
	return (get_vm_config(vm->vm_id)->load_order == POST_LAUNCHED_VM);
}


/**
 * @pre vm != NULL
 * @pre vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_prelaunched_vm(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config;

	vm_config = get_vm_config(vm->vm_id);
	return (vm_config->load_order == PRE_LAUNCHED_VM);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_lapic_pt_configured(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_LAPIC_PASSTHROUGH) != 0U);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_pmu_pt_configured(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_PMU_PASSTHROUGH) != 0U);
}


/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_rt_vm(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_RT) != 0U);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 *
 * Stateful VM refers to VM that has its own state (such as internal file cache),
 * and will experience state loss (file system corruption) if force powered down.
 */
bool is_stateful_vm(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	/* TEE VM has GUEST_FLAG_STATELESS set implicitly */
	return ((vm_config->guest_flags & GUEST_FLAG_STATELESS) == 0U);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_nvmx_configured(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_NVMX_ENABLED) != 0U);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_vcat_configured(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_VCAT_ENABLED) != 0U);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_static_configured_vm(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_STATIC_VM) != 0U);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_vhwp_configured(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_VHWP) != 0U);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_vtm_configured(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_VTM) != 0U);
}

/**
 * @brief VT-d PI posted mode can possibly be used for PTDEVs assigned
 * to this VM if platform supports VT-d PI AND lapic passthru is not configured
 * for this VM.
 * However, as we can only post single destination IRQ, so meeting these 2 conditions
 * does not necessarily mean posted mode will be used for all PTDEVs belonging
 * to the VM, unless the IRQ is single-destination for the specific PTDEV
 * @pre vm != NULL
 */
bool is_pi_capable(const struct acrn_vm *vm)
{
	return (platform_caps.pi && (!is_lapic_pt_configured(vm)));
}

struct acrn_vm *get_highest_severity_vm(bool runtime)
{
	uint16_t vm_id, highest_vm_id = 0U;

	for (vm_id = 1U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		if (runtime && is_poweroff_vm(get_vm_from_vmid(vm_id))) {
			/* If vm is non-existed or shutdown, it's not highest severity VM */
			continue;
		}

		if (get_vm_severity(vm_id) > get_vm_severity(highest_vm_id)) {
			highest_vm_id = vm_id;
		}
	}

	return get_vm_from_vmid(highest_vm_id);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool vm_hide_mtrr(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_HIDE_MTRR) != 0U);
}

/**
 * @brief Initialize the I/O bitmap for \p vm
 *
 * @param vm The VM whose I/O bitmap is to be initialized
 */
static void setup_io_bitmap(struct acrn_vm *vm)
{
	if (is_service_vm(vm)) {
		(void)memset(vm->arch_vm.io_bitmap, 0x00U, PAGE_SIZE * 2U);
	} else {
		/* block all IO port access from Guest */
		(void)memset(vm->arch_vm.io_bitmap, 0xFFU, PAGE_SIZE * 2U);
	}
}

/**
 * return a pointer to the virtual machine structure associated with
 * this VM ID
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM
 */
struct acrn_vm *get_vm_from_vmid(uint16_t vm_id)
{
	return &vm_array[vm_id];
}

/* return a pointer to the virtual machine structure of Service VM */
struct acrn_vm *get_service_vm(void)
{
	ASSERT(service_vm_ptr != NULL, "service_vm_ptr is NULL");

	return service_vm_ptr;
}

/**
 * @pre vm_config != NULL
 */
static inline uint16_t get_configured_bsp_pcpu_id(const struct acrn_vm_config *vm_config)
{
	/*
	 * The set least significant bit represents the pCPU ID for BSP
	 * vm_config->cpu_affinity has been sanitized to contain valid pCPU IDs
	 */
	return ffs64(vm_config->cpu_affinity);
}

/**
 * @pre vm != NULL && vm_config != NULL
 */
static void prepare_prelaunched_vm_memmap(struct acrn_vm *vm, const struct acrn_vm_config *vm_config)
{
	uint64_t base_hpa;
	uint64_t base_gpa;
	uint64_t remaining_entry_size;
	uint32_t hpa_index;
	uint64_t base_size;
	uint32_t i;
	struct vm_hpa_regions tmp_vm_hpa;
	const struct e820_entry *entry;

	hpa_index = 0U;
	tmp_vm_hpa = vm_config->memory.host_regions[0];

	for (i = 0U; i < vm->e820_entry_num; i++) {
		entry = &(vm->e820_entries[i]);

		if (entry->length == 0UL) {
			continue;
		} else {
			if (is_software_sram_enabled() && (entry->baseaddr == PRE_RTVM_SW_SRAM_BASE_GPA) &&
				((vm_config->guest_flags & GUEST_FLAG_RT) != 0U)){
				/* pass through Software SRAM to pre-RTVM */
				ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
					get_software_sram_base(), PRE_RTVM_SW_SRAM_BASE_GPA,
					get_software_sram_size(), EPT_RWX | EPT_WB);
				continue;
			}
		}

		if ((entry->type == E820_TYPE_RESERVED) && (entry->baseaddr > MEM_1M)) {
			continue;
		}

		base_gpa = entry->baseaddr;
		remaining_entry_size = entry->length;

		while ((hpa_index < vm_config->memory.region_num) && (remaining_entry_size > 0)) {

			base_hpa = tmp_vm_hpa.start_hpa;
			base_size = min(remaining_entry_size, tmp_vm_hpa.size_hpa);

			if (tmp_vm_hpa.size_hpa > remaining_entry_size) {
				/* from low to high */
				tmp_vm_hpa.start_hpa  += base_size;
				tmp_vm_hpa.size_hpa -= base_size;
			} else {
				hpa_index++;
				if (hpa_index < vm_config->memory.region_num) {
					tmp_vm_hpa = vm_config->memory.host_regions[hpa_index];
				}
			}

			if (entry->type != E820_TYPE_RESERVED) {
				ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, base_hpa, base_gpa,
						base_size, EPT_RWX | EPT_WB);
			} else {
				/* GPAs under 1MB are always backed by physical memory */
				ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, base_hpa, base_gpa,
						base_size, EPT_RWX | EPT_UNCACHED);
			}
			remaining_entry_size -= base_size;
			base_gpa += base_size;
		}
	}

	for (i = 0U; i < MAX_MMIO_DEV_NUM; i++) {
		/* Now we include the TPM2 event log region in ACPI NVS, so we need to
		 * delete this potential mapping first.
		 */
		(void)deassign_mmio_dev(vm, &vm_config->mmiodevs[i]);

		(void)assign_mmio_dev(vm, &vm_config->mmiodevs[i]);

#ifdef P2SB_VGPIO_DM_ENABLED
		if ((vm_config->pt_p2sb_bar) && (vm_config->mmiodevs[i].res[0].host_pa == P2SB_BAR_ADDR)) {
			register_vgpio_handler(vm, &vm_config->mmiodevs[i].res[0]);
		}
#endif
	}
}

static void deny_pci_bar_access(struct acrn_vm *service_vm, const struct pci_pdev *pdev)
{
	uint32_t idx;
	struct pci_vbar vbar = {};
	uint64_t base = 0UL, size = 0UL, mask;
	uint64_t *pml4_page;

	pml4_page = (uint64_t *)service_vm->arch_vm.nworld_eptp;

	for ( idx= 0; idx < pdev->nr_bars; idx++) {
		vbar.bar_type.bits = pdev->bars[idx].phy_bar;
		if (!is_pci_reserved_bar(&vbar)) {
			base = pdev->bars[idx].phy_bar;
			size = pdev->bars[idx].size_mask;
			if (is_pci_mem64lo_bar(&vbar)) {
				idx++;
				base |= (((uint64_t)pdev->bars[idx].phy_bar) << 32UL);
				size |= (((uint64_t)pdev->bars[idx].size_mask) << 32UL);
			}

			mask = (is_pci_io_bar(&vbar)) ? PCI_BASE_ADDRESS_IO_MASK : PCI_BASE_ADDRESS_MEM_MASK;
			base &= mask;
			size &= mask;
			size = size & ~(size - 1UL);

			if ((base != 0UL)) {
				if (is_pci_io_bar(&vbar)) {
					base &= 0xffffU;
					deny_guest_pio_access(service_vm, base, size);
				} else {
					/*for passthru device MMIO BAR base must be 4K aligned. This is the requirement of passthru devices.*/
					ASSERT((base & PAGE_MASK) != 0U, "%02x:%02x.%d bar[%d] 0x%lx, is not 4K aligned!",
						pdev->bdf.bits.b, pdev->bdf.bits.d, pdev->bdf.bits.f, idx, base);
					size =  round_page_up(size);
					ept_del_mr(service_vm, pml4_page, base, size);
				}
			}
		}
	}
}

static void deny_pdevs(struct acrn_vm *service_vm, struct acrn_vm_pci_dev_config *pci_devs, uint16_t pci_dev_num)
{
	uint16_t i;

	for (i = 0; i < pci_dev_num; i++) {
		if ( pci_devs[i].pdev != NULL) {
			deny_pci_bar_access(service_vm, pci_devs[i].pdev);
		}
	}
}

static void deny_hv_owned_devices(struct acrn_vm *service_vm)
{
	uint16_t pio_address;
	uint32_t nbytes, i;

	const struct pci_pdev **hv_owned = get_hv_owned_pdevs();

	for (i = 0U; i < get_hv_owned_pdev_num(); i++) {
		deny_pci_bar_access(service_vm, hv_owned[i]);
	}

	if (get_pio_dbg_uart_cfg(&pio_address, &nbytes)) {
		deny_guest_pio_access(service_vm, pio_address, nbytes);
	}
}

/**
 * @param[inout] vm pointer to a vm descriptor
 *
 * @retval 0 on success
 *
 * @pre vm != NULL
 * @pre is_service_vm(vm) == true
 */
static void prepare_service_vm_memmap(struct acrn_vm *vm)
{
	uint16_t vm_id;
	uint32_t i;
	uint64_t hv_hpa;
	uint64_t service_vm_high64_max_ram = MEM_4G;
	struct acrn_vm_config *vm_config;
	uint64_t *pml4_page = (uint64_t *)vm->arch_vm.nworld_eptp;
	struct epc_section* epc_secs;

	const struct e820_entry *entry;
	uint32_t entries_count = vm->e820_entry_num;
	const struct e820_entry *p_e820 = vm->e820_entries;
	struct pci_mmcfg_region *pci_mmcfg;
	uint64_t trampoline_memory_size = round_page_up((uint64_t)(&ld_trampoline_end - &ld_trampoline_start));

	pr_dbg("Service VM e820 layout:\n");
	for (i = 0U; i < entries_count; i++) {
		entry = p_e820 + i;
		pr_dbg("e820 table: %d type: 0x%x", i, entry->type);
		pr_dbg("BaseAddress: 0x%016lx length: 0x%016lx\n", entry->baseaddr, entry->length);
		service_vm_high64_max_ram = max((entry->baseaddr + entry->length), service_vm_high64_max_ram);
	}

	/* create real ept map for [0, service_vm_high64_max_ram) with UC */
	ept_add_mr(vm, pml4_page, 0UL, 0UL, service_vm_high64_max_ram, EPT_RWX | EPT_UNCACHED);

	/* update ram entries to WB attr */
	for (i = 0U; i < entries_count; i++) {
		entry = p_e820 + i;
		if (entry->type == E820_TYPE_RAM) {
			ept_modify_mr(vm, pml4_page, entry->baseaddr, entry->length, EPT_WB, EPT_MT_MASK);
		}
	}

	/* Unmap all platform EPC resource from Service VM.
	 * This part has already been marked as reserved by BIOS in E820
	 * will cause EPT violation if Service VM accesses EPC resource.
	 */
	epc_secs = get_phys_epc();
	for (i = 0U; (i < MAX_EPC_SECTIONS) && (epc_secs[i].size != 0UL); i++) {
		ept_del_mr(vm, pml4_page, epc_secs[i].base, epc_secs[i].size);
	}

	/* unmap hypervisor itself for safety
	 * will cause EPT violation if Service VM accesses hv memory
	 */
	hv_hpa = hva2hpa((void *)(get_hv_image_base()));
	ept_del_mr(vm, pml4_page, hv_hpa, get_hv_image_size());
	/* unmap prelaunch VM memory */
	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);
		if (vm_config->load_order == PRE_LAUNCHED_VM) {
			for (i = 0; i < vm_config->memory.region_num; i++){
				ept_del_mr(vm, pml4_page, vm_config->memory.host_regions[i].start_hpa, vm_config->memory.host_regions[i].size_hpa);
			}
			/* Remove MMIO/IO bars of pre-launched VM's ptdev */
			deny_pdevs(vm, vm_config->pci_devs, vm_config->pci_dev_num);
		}

		for (i = 0U; i < MAX_MMIO_DEV_NUM; i++) {
			(void)deassign_mmio_dev(vm, &vm_config->mmiodevs[i]);
		}
	}

	/* unmap AP trampoline code for security
	 * This buffer is guaranteed to be page aligned.
	 */
	ept_del_mr(vm, pml4_page, get_trampoline_start16_paddr(), trampoline_memory_size);

	/* unmap PCIe MMCONFIG region since it's owned by hypervisor */
	pci_mmcfg = get_mmcfg_region();
	ept_del_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, pci_mmcfg->address, get_pci_mmcfg_size(pci_mmcfg));

	if (is_software_sram_enabled()) {
		/*
		 * Native Software SRAM resources shall be assigned to either Pre-launched RTVM
		 * or Service VM. Software SRAM support for Post-launched RTVM is virtualized
		 * in Service VM.
		 *
		 * 1) Native Software SRAM resources are assigned to Pre-launched RTVM:
		 *     - Remove Software SRAM regions from Service VM EPT, to prevent
		 *       Service VM from using clflush to flush the Software SRAM cache.
		 *     - PRE_RTVM_SW_SRAM_MAX_SIZE is the size of Software SRAM that
		 *       Pre-launched RTVM uses, presumed to be starting from Software SRAM base.
		 *       For other cases, PRE_RTVM_SW_SRAM_MAX_SIZE should be defined as 0,
		 *       and no region will be removed from Service VM EPT.
		 *
		 * 2) Native Software SRAM resources are assigned to Service VM:
		 *     - Software SRAM regions are added to EPT of Service VM by default
		 *       with memory type UC.
		 *     - But, Service VM need to access Software SRAM regions
		 *       when virtualizing them for Post-launched RTVM.
		 *     - So memory type of Software SRAM regions in EPT shall be updated to EPT_WB.
		 */
#if (PRE_RTVM_SW_SRAM_MAX_SIZE > 0U)
		ept_del_mr(vm, pml4_page, service_vm_hpa2gpa(get_software_sram_base()), PRE_RTVM_SW_SRAM_MAX_SIZE);
#else
		ept_modify_mr(vm, pml4_page, service_vm_hpa2gpa(get_software_sram_base()),
			get_software_sram_size(), EPT_WB, EPT_MT_MASK);
#endif
	}

	/* unmap Intel IOMMU register pages for below reason:
	 * Service VM can detect IOMMU capability in its ACPI table hence it may access
	 * IOMMU hardware resources, which is not expected, as IOMMU hardware is owned by hypervisor.
	 */
	for (i = 0U; i < plat_dmar_info.drhd_count; i++) {
		ept_del_mr(vm, pml4_page, plat_dmar_info.drhd_units[i].reg_base_addr, PAGE_SIZE);
	}

}

/* Add EPT mapping of EPC reource for the VM */
static void prepare_epc_vm_memmap(struct acrn_vm *vm)
{
	struct epc_map* vm_epc_maps;
	uint32_t i;

	if (is_vsgx_supported(vm->vm_id)) {
		vm_epc_maps = get_epc_mapping(vm->vm_id);
		for (i = 0U; (i < MAX_EPC_SECTIONS) && (vm_epc_maps[i].size != 0UL); i++) {
			ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, vm_epc_maps[i].hpa,
				vm_epc_maps[i].gpa, vm_epc_maps[i].size, EPT_RWX | EPT_WB);
		}
	}
}

/**
 * @brief get bitmap of pCPUs whose vCPUs have LAPIC PT enabled
 *
 * @param[in] vm pointer to vm data structure
 * @pre vm != NULL
 *
 * @return pCPU bitmap
 */
static uint64_t lapic_pt_enabled_pcpu_bitmap(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu;
	uint64_t bitmap = 0UL;

	if (is_lapic_pt_configured(vm)) {
		foreach_vcpu(i, vm, vcpu) {
			if (is_x2apic_enabled(vcpu_vlapic(vcpu))) {
				bitmap_set_nolock(pcpuid_from_vcpu(vcpu), &bitmap);
			}
		}
	}

	return bitmap;
}

void prepare_vm_identical_memmap(struct acrn_vm *vm, uint16_t e820_entry_type, uint64_t prot_orig)
{
	const struct e820_entry *entry;
	const struct e820_entry *p_e820 = vm->e820_entries;
	uint32_t entries_count = vm->e820_entry_num;
	uint64_t *pml4_page = (uint64_t *)vm->arch_vm.nworld_eptp;
	uint32_t i;

	for (i = 0U; i < entries_count; i++) {
		entry = p_e820 + i;
		if (entry->type == e820_entry_type) {
			ept_add_mr(vm, pml4_page, entry->baseaddr,
				entry->baseaddr, entry->length,
				prot_orig);
		}
	}
}

/**
 * @pre vm_id < CONFIG_MAX_VM_NUM and should be trusty post-launched VM
 */
int32_t get_sworld_vm_index(uint16_t vm_id)
{
	int16_t i;
	int32_t vm_idx = MAX_TRUSTY_VM_NUM;
	struct acrn_vm_config *vm_config = get_vm_config(vm_id);

	if ((vm_config->guest_flags & GUEST_FLAG_SECURE_WORLD_ENABLED) != 0U) {
		vm_idx = 0;

		for (i = 0; i < vm_id; i++) {
			vm_config = get_vm_config(i);
			if ((vm_config->guest_flags & GUEST_FLAG_SECURE_WORLD_ENABLED) != 0U) {
				vm_idx += 1;
			}
		}
	}

	if (vm_idx >= (int32_t)MAX_TRUSTY_VM_NUM) {
		pr_err("Can't find sworld memory for vm id: %d", vm_id);
		vm_idx = -EINVAL;
	}

	return vm_idx;
}

/**
 * @pre vm_id < CONFIG_MAX_VM_NUM && vm_config != NULL && rtn_vm != NULL
 * @pre vm->state == VM_POWERED_OFF
 */
int32_t create_vm(uint16_t vm_id, uint64_t pcpu_bitmap, struct acrn_vm_config *vm_config, struct acrn_vm **rtn_vm)
{
	struct acrn_vm *vm = NULL;
	int32_t status = 0;
	uint16_t pcpu_id;

	/* Allocate memory for virtual machine */
	vm = &vm_array[vm_id];
	vm->vm_id = vm_id;
	vm->hw.created_vcpus = 0U;

	init_ept_pgtable(&vm->arch_vm.ept_pgtable, vm->vm_id);
	vm->arch_vm.nworld_eptp = pgtable_create_root(&vm->arch_vm.ept_pgtable);

	(void)memcpy_s(&vm->name[0], MAX_VM_NAME_LEN, &vm_config->name[0], MAX_VM_NAME_LEN);

	if (is_service_vm(vm)) {
		/* Only for Service VM */
		create_service_vm_e820(vm);
		prepare_service_vm_memmap(vm);

		status = init_vm_boot_info(vm);
	} else {
		/* For PRE_LAUNCHED_VM and POST_LAUNCHED_VM */
		if ((vm_config->guest_flags & GUEST_FLAG_SECURE_WORLD_ENABLED) != 0U) {
			vm->sworld_control.flag.supported = 1U;
		}
		if (vm->sworld_control.flag.supported != 0UL) {
			int32_t vm_idx = get_sworld_vm_index(vm_id);

			if (vm_idx >= 0)
			{
				ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
					hva2hpa(post_user_vm_sworld_memory[vm_idx]),
					TRUSTY_EPT_REBASE_GPA, TRUSTY_RAM_SIZE, EPT_WB | EPT_RWX);
			} else {
				status = -EINVAL;
			}
		}
		if (status == 0) {
			if (vm_config->name[0] == '\0') {
				/* if VM name is not configured, specify with VM ID */
				snprintf(vm_config->name, 16, "ACRN VM_%d", vm_id);
			}

			if (vm_config->load_order == PRE_LAUNCHED_VM) {
				/*
				 * If a prelaunched VM has the flag GUEST_FLAG_TEE set then it
				 * is a special prelaunched VM called TEE VM which need special
				 * memmap, e.g. mapping the REE VM into its space. Otherwise,
				 * just use the standard preplaunched VM memmap.
				 */
				if ((vm_config->guest_flags & GUEST_FLAG_TEE) != 0U) {
					prepare_tee_vm_memmap(vm, vm_config);
				} else {
					create_prelaunched_vm_e820(vm);
					prepare_prelaunched_vm_memmap(vm, vm_config);
				}
				status = init_vm_boot_info(vm);
			}
		}
	}

	if (status == 0) {
		prepare_epc_vm_memmap(vm);
		spinlock_init(&vm->vlapic_mode_lock);
		spinlock_init(&vm->ept_lock);
		spinlock_init(&vm->emul_mmio_lock);
		spinlock_init(&vm->arch_vm.iwkey_backup_lock);

		vm->arch_vm.vlapic_mode = VM_VLAPIC_XAPIC;
		vm->intr_inject_delay_delta = 0UL;
		vm->nr_emul_mmio_regions = 0U;
		vm->vcpuid_entry_nr = 0U;

		/* Set up IO bit-mask such that VM exit occurs on
		 * selected IO ranges
		 */
		setup_io_bitmap(vm);

		init_guest_pm(vm);

		if (is_nvmx_configured(vm)) {
			init_nested_vmx(vm);
		}

		if (!is_lapic_pt_configured(vm)) {
			vpic_init(vm);
		}

		if (is_rt_vm(vm) || !is_postlaunched_vm(vm)) {
			vrtc_init(vm);
		}

		if (is_service_vm(vm)) {
			deny_hv_owned_devices(vm);
		}

#ifdef CONFIG_SECURITY_VM_FIXUP
		passthrough_smbios(vm, get_acrn_boot_info());
#endif

		vm->sw.vm_event_sbuf = NULL;

		status = init_vpci(vm);
		if (status == 0) {
			enable_iommu();

			/* Create virtual uart;*/
			init_legacy_vuarts(vm, vm_config->vuart);

			register_reset_port_handler(vm);

			/* vpic wire_mode default is INTR */
			vm->wire_mode = VPIC_WIRE_INTR;

			/* Init full emulated vIOAPIC instance:
			* Present a virtual IOAPIC to guest, as a placeholder interrupt controller,
			* even if the guest uses PT LAPIC. This is to satisfy the guest OSes,
			* in some cases, though the functionality of vIOAPIC doesn't work.
			*/
			vioapic_init(vm);

			/* Populate return VM handle */
			*rtn_vm = vm;
			vm->sw.io_shared_page = NULL;
			vm->sw.asyncio_sbuf = NULL;
			if ((vm_config->load_order == POST_LAUNCHED_VM)
				&& ((vm_config->guest_flags & GUEST_FLAG_IO_COMPLETION_POLLING) != 0U)) {
				/* enable IO completion polling mode per its guest flags in vm_config. */
				vm->sw.is_polling_ioreq = true;
			}
			status = set_vcpuid_entries(vm);
			if (status == 0) {
				vm->state = VM_CREATED;
			}
		}
	}

	if (status == 0) {
		/* We have assumptions:
		 *   1) vcpus used by Service VM has been offlined by DM before User VM re-use it.
		 *   2) pcpu_bitmap passed sanitization is OK for vcpu creating.
		 */
		vm->hw.cpu_affinity = pcpu_bitmap;

		uint64_t tmp64 = pcpu_bitmap;
		while (tmp64 != 0UL) {
			pcpu_id = ffs64(tmp64);
			bitmap_clear_nolock(pcpu_id, &tmp64);
			status = prepare_vcpu(vm, pcpu_id);
			if (status != 0) {
				break;
			}
		}
	}

	if (status == 0) {
		uint16_t i;
		for (i = 0U; i < vm_config->pt_intx_num; i++) {
			status = ptirq_add_intx_remapping(vm, vm_config->pt_intx[i].virt_gsi,
								vm_config->pt_intx[i].phys_gsi, false);
			if (status != 0) {
				ptirq_remove_configured_intx_remappings(vm);
				break;
			}
		}
	}

	if ((status != 0) && (vm->arch_vm.nworld_eptp != NULL)) {
		(void)memset(vm->arch_vm.nworld_eptp, 0U, PAGE_SIZE);
	}

	return status;
}

static bool is_ready_for_system_shutdown(void)
{
	bool ret = true;
	uint16_t vm_id;
	struct acrn_vm *vm;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm = get_vm_from_vmid(vm_id);
		/* TODO: Update code to cover hybrid mode */
		if (!is_poweroff_vm(vm) && is_stateful_vm(vm)) {
			ret = false;
			break;
		}
	}

	return ret;
}

static int32_t offline_lapic_pt_enabled_pcpus(const struct acrn_vm *vm, uint64_t pcpu_mask)
{
	int32_t ret = 0;
	uint16_t i;
	uint64_t mask = pcpu_mask;
	const struct acrn_vcpu *vcpu = NULL;
	uint16_t this_pcpu_id = get_pcpu_id();

	if (bitmap_test(this_pcpu_id, &mask)) {
		bitmap_clear_nolock(this_pcpu_id, &mask);
		if (vm->state == VM_POWERED_OFF) {
			/*
			 * If the current pcpu needs to offline itself,
			 * it will be done after shutdown_vm() completes
			 * in the idle thread.
			 */
			make_pcpu_offline(this_pcpu_id);
		} else {
			/*
			 * The current pcpu can't reset itself
			 */
			pr_warn("%s: cannot offline self(%u)",
				__func__, this_pcpu_id);
			ret = -EINVAL;
		}
	}

	foreach_vcpu(i, vm, vcpu) {
		if (bitmap_test(pcpuid_from_vcpu(vcpu), &mask)) {
			make_pcpu_offline(pcpuid_from_vcpu(vcpu));
		}
	}

	wait_pcpus_offline(mask);
	if (!start_pcpus(mask)) {
		pr_fatal("Failed to start all cpus in mask(0x%lx)", mask);
		ret = -ETIMEDOUT;
	}
	return ret;
}

/*
 * @pre vm != NULL
 * @pre vm->state == VM_PAUSED
 */
int32_t shutdown_vm(struct acrn_vm *vm)
{
	uint16_t i;
	uint64_t mask;
	struct acrn_vcpu *vcpu = NULL;
	struct acrn_vm_config *vm_config = NULL;
	int32_t ret = 0;

	/* Only allow shutdown paused vm */
	vm->state = VM_POWERED_OFF;

	if (is_service_vm(vm)) {
		sbuf_reset();
	}

	ptirq_remove_configured_intx_remappings(vm);

	deinit_legacy_vuarts(vm);

	deinit_vpci(vm);

	deinit_emul_io(vm);

	/* Free EPT allocated resources assigned to VM */
	destroy_ept(vm);

	mask = lapic_pt_enabled_pcpu_bitmap(vm);
	if (mask != 0UL) {
		ret = offline_lapic_pt_enabled_pcpus(vm, mask);
	}

	foreach_vcpu(i, vm, vcpu) {
		offline_vcpu(vcpu);
	}

#ifdef CONFIG_HYPERV_ENABLED
	hyperv_page_destory(vm);
#endif

	/* after guest_flags not used, then clear it */
	vm_config = get_vm_config(vm->vm_id);
	vm_config->guest_flags &= ~DM_OWNED_GUEST_FLAG_MASK;
	if (!is_static_configured_vm(vm)) {
		memset(vm_config->name, 0U, MAX_VM_NAME_LEN);
	}

	if (is_ready_for_system_shutdown()) {
		/* If no any guest running, shutdown system */
		shutdown_system();
	}

	/* Return status to caller */
	return ret;
}

/**
 * @pre vm != NULL
 * @pre vm->state == VM_CREATED
 */
void start_vm(struct acrn_vm *vm)
{
	struct acrn_vcpu *bsp = NULL;

	vm->state = VM_RUNNING;

	/* Only start BSP (vid = 0) and let BSP start other APs */
	bsp = vcpu_from_vid(vm, BSP_CPU_ID);
	vcpu_make_request(bsp, ACRN_REQUEST_INIT_VMCS);
	launch_vcpu(bsp);
}

/**
 * @pre vm != NULL
 * @pre vm->state == VM_PAUSED
 */
int32_t reset_vm(struct acrn_vm *vm, enum reset_mode mode)
{
	uint16_t i;
	uint64_t mask;
	struct acrn_vcpu *vcpu = NULL;
	int32_t ret = 0;

	mask = lapic_pt_enabled_pcpu_bitmap(vm);
	if (mask != 0UL) {
		ret = offline_lapic_pt_enabled_pcpus(vm, mask);
	}

	foreach_vcpu(i, vm, vcpu) {
		reset_vcpu(vcpu, COLD_RESET);
	}

	/*
	 * Set VM vLAPIC state to VM_VLAPIC_XAPIC
	 */
	vm->arch_vm.vlapic_mode = VM_VLAPIC_XAPIC;

	if ((mode == POWER_ON_RESET) && is_service_vm(vm)) {
		(void)prepare_os_image(vm);
	}

	reset_vm_ioreqs(vm);
	reset_vioapics(vm);
	destroy_secure_world(vm, false);
	vm->sworld_control.flag.active = 0UL;
	vm->arch_vm.iwkey_backup_status = 0UL;
	vm->state = VM_CREATED;

	return ret;
}

/**
 * @pre vm != NULL
 */
void poweroff_if_rt_vm(struct acrn_vm *vm)
{
	if (is_rt_vm(vm) && !is_paused_vm(vm) && !is_poweroff_vm(vm)) {
		vm->state = VM_READY_TO_POWEROFF;
	}
}

/**
 * @pre vm != NULL
 */
void pause_vm(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu = NULL;

	if (((is_severity_pass(vm->vm_id)) && (vm->state == VM_RUNNING)) ||
			(vm->state == VM_READY_TO_POWEROFF) ||
			(vm->state == VM_CREATED)) {
		foreach_vcpu(i, vm, vcpu) {
			zombie_vcpu(vcpu, VCPU_ZOMBIE);
		}
		vm->state = VM_PAUSED;
	}
}

/**
 * @brief Resume vm from S3 state
 *
 * To resume vm after guest enter S3 state:
 * - reset BSP
 * - BSP will be put to real mode with entry set as wakeup_vec
 * - init_vmcs BSP. We could call init_vmcs here because we know current
 *   pcpu is mapped to BSP of vm.
 *
 * @vm[in]		vm pointer to vm data structure
 * @wakeup_vec[in]	The resume address of vm
 *
 * @pre vm != NULL
 * @pre is_service_vm(vm) && vm->state == VM_PAUSED
 */
void resume_vm_from_s3(struct acrn_vm *vm, uint32_t wakeup_vec)
{
	struct acrn_vcpu *bsp = vcpu_from_vid(vm, BSP_CPU_ID);

	reset_vm(vm, RESUME_FROM_S3);

	/* When Service VM resume from S3, it will return to real mode
	 * with entry set to wakeup_vec.
	 */
	set_vcpu_startup_entry(bsp, wakeup_vec);

	start_vm(vm);
}

/**
 * Prepare to create vm/vcpu for vm
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM && vm_config != NULL
 */
int32_t prepare_vm(uint16_t vm_id, struct acrn_vm_config *vm_config)
{
	int32_t err = -1;
	struct acrn_vm *vm = NULL;

#ifdef CONFIG_SECURITY_VM_FIXUP
	security_vm_fixup(vm_id);
#endif
	if (get_vmid_by_name(vm_config->name) != vm_id) {
		pr_err("Invalid VM name: %s", vm_config->name);
	} else {
		/* Service VM and pre-launched VMs launch on all pCPUs defined in vm_config->cpu_affinity */
		err = create_vm(vm_id, vm_config->cpu_affinity, vm_config, &vm);
		if (err == 0) {
			if (is_prelaunched_vm(vm)) {
				build_vrsdp(vm);
			}

			err = prepare_os_image(vm);
		}
	}

	return err;
}

/**
 * @pre vm_config != NULL
 * @Application constraint: The validity of vm_config->cpu_affinity should be guaranteed before run-time.
 */
void launch_vms(uint16_t pcpu_id)
{
	uint16_t vm_id;
	struct acrn_vm_config *vm_config;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);

		if (((vm_config->guest_flags & GUEST_FLAG_REE) != 0U) &&
		    ((vm_config->guest_flags & GUEST_FLAG_TEE) != 0U)) {
			ASSERT(false, "%s: Wrong VM (VM id: %u) configuration, can't set both REE and TEE flags",
				__func__, vm_id);
		}

		if ((vm_config->load_order == SERVICE_VM) || (vm_config->load_order == PRE_LAUNCHED_VM)) {
			if (pcpu_id == get_configured_bsp_pcpu_id(vm_config)) {
				if (vm_config->load_order == SERVICE_VM) {
					service_vm_ptr = &vm_array[vm_id];
				}

				/*
				 * We can only start a VM when there is no error in prepare_vm.
				 * Otherwise, print out the corresponding error.
				 *
				 * We can only start REE VM when get the notification from TEE VM.
				 * so skip "start_vm" here for REE, and start it in TEE hypercall
				 * HC_TEE_VCPU_BOOT_DONE.
				 */
				if (prepare_vm(vm_id, vm_config) == 0) {
					if ((vm_config->guest_flags & GUEST_FLAG_REE) != 0U) {
						/* Nothing need to do here, REE will start in TEE hypercall */
					} else {
						start_vm(get_vm_from_vmid(vm_id));
						pr_acrnlog("Start VM id: %x name: %s", vm_id, vm_config->name);
					}
				}
			}
		}
	}
}

/*
 * @brief Update state of vLAPICs of a VM
 * vLAPICs of VM switch between modes in an asynchronous fashion. This API
 * captures the "transition" state triggered when one vLAPIC switches mode.
 * When the VM is created, the state is set to "xAPIC" as all vLAPICs are setup
 * in xAPIC mode.
 *
 * Upon reset, all LAPICs switch to xAPIC mode accroding to SDM 10.12.5
 * Considering VM uses x2apic mode for vLAPIC, in reset or shutdown flow, vLAPIC state
 * moves to "xAPIC" directly without going thru "transition".
 *
 * VM_VLAPIC_X2APIC - All the online vCPUs/vLAPICs of this VM use x2APIC mode
 * VM_VLAPIC_XAPIC - All the online vCPUs/vLAPICs of this VM use xAPIC mode
 * VM_VLAPIC_DISABLED - All the online vCPUs/vLAPICs of this VM are in Disabled mode
 * VM_VLAPIC_TRANSITION - Online vCPUs/vLAPICs of this VM are in between transistion
 *
 * @pre vm != NULL
 */
void update_vm_vlapic_state(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu;
	uint16_t vcpus_in_x2apic, vcpus_in_xapic;
	enum vm_vlapic_mode vlapic_mode = VM_VLAPIC_XAPIC;

	vcpus_in_x2apic = 0U;
	vcpus_in_xapic = 0U;
	spinlock_obtain(&vm->vlapic_mode_lock);
	foreach_vcpu(i, vm, vcpu) {
		/* Skip vCPU in state outside of VCPU_RUNNING as it may be offline. */
		if (vcpu->state == VCPU_RUNNING) {
			if (is_x2apic_enabled(vcpu_vlapic(vcpu))) {
				vcpus_in_x2apic++;
			} else if (is_xapic_enabled(vcpu_vlapic(vcpu))) {
				vcpus_in_xapic++;
			} else {
				/*
				 * vCPU is using vLAPIC in Disabled mode
				 */
			}
		}
	}

	if ((vcpus_in_x2apic == 0U) && (vcpus_in_xapic == 0U)) {
		/*
		 * Check if the counts vcpus_in_x2apic and vcpus_in_xapic are zero
		 * VM_VLAPIC_DISABLED
		 */
		vlapic_mode = VM_VLAPIC_DISABLED;
	} else if ((vcpus_in_x2apic != 0U) && (vcpus_in_xapic != 0U)) {
		/*
		 * Check if the counts vcpus_in_x2apic and vcpus_in_xapic are non-zero
		 * VM_VLAPIC_TRANSITION
		 */
		vlapic_mode = VM_VLAPIC_TRANSITION;
	} else if (vcpus_in_x2apic != 0U) {
		/*
		 * Check if the counts vcpus_in_x2apic is non-zero
		 * VM_VLAPIC_X2APIC
		 */
		vlapic_mode = VM_VLAPIC_X2APIC;
	} else {
		/*
		 * Count vcpus_in_xapic is non-zero
		 * VM_VLAPIC_XAPIC
		 */
		vlapic_mode = VM_VLAPIC_XAPIC;
	}

	vm->arch_vm.vlapic_mode = vlapic_mode;
	spinlock_release(&vm->vlapic_mode_lock);
}

/*
 * @brief Check mode of vLAPICs of a VM
 *
 * @pre vm != NULL
 */
enum vm_vlapic_mode check_vm_vlapic_mode(const struct acrn_vm *vm)
{
	enum vm_vlapic_mode vlapic_mode;

	vlapic_mode = vm->arch_vm.vlapic_mode;
	return vlapic_mode;
}

/**
 * if there is RT VM return true otherwise return false.
 */
bool has_rt_vm(void)
{
	uint16_t vm_id;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		if (is_rt_vm(get_vm_from_vmid(vm_id))) {
			break;
		}
	}

	return (vm_id != CONFIG_MAX_VM_NUM);
}

void make_shutdown_vm_request(uint16_t pcpu_id)
{
	bitmap_set_lock(NEED_SHUTDOWN_VM, &per_cpu(pcpu_flag, pcpu_id));
	if (get_pcpu_id() != pcpu_id) {
		kick_pcpu(pcpu_id);
	}
}

bool need_shutdown_vm(uint16_t pcpu_id)
{
	return bitmap_test_and_clear_lock(NEED_SHUTDOWN_VM, &per_cpu(pcpu_flag, pcpu_id));
}

/*
 * @pre vm != NULL
 */
void get_vm_lock(struct acrn_vm *vm)
{
	spinlock_obtain(&vm->vm_state_lock);
}
/*
 * @pre vm != NULL
 */
void put_vm_lock(struct acrn_vm *vm)
{
	spinlock_release(&vm->vm_state_lock);
}
