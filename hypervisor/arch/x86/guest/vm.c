/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <sprintf.h>
#include <per_cpu.h>
#include <lapic.h>
#include <vm.h>
#include <vm_reset.h>
#include <bits.h>
#include <e820.h>
#include <multiboot.h>
#include <vtd.h>
#include <reloc.h>
#include <ept.h>
#include <guest_pm.h>
#include <console.h>
#include <ptdev.h>
#include <vmcs.h>
#include <pgtable.h>
#include <mmu.h>
#include <logmsg.h>
#include <vboot_info.h>
#include <board.h>
#include <sgx.h>
#include <sbuf.h>
#include <pci_dev.h>
#include <vacpi.h>
#include <platform_caps.h>
#include <mmio_dev.h>
#include <trampoline.h>
#include <assign.h>
#include <vgpio.h>
#include <rtcm.h>

/* Local variables */

static struct acrn_vm vm_array[CONFIG_MAX_VM_NUM] __aligned(PAGE_SIZE);

static struct acrn_vm *sos_vm_ptr = NULL;

uint16_t get_vmid_by_uuid(const uint8_t *uuid)
{
	uint16_t vm_id = 0U;

	while (!vm_has_matched_uuid(vm_id, uuid)) {
		vm_id++;
		if (vm_id == CONFIG_MAX_VM_NUM) {
			break;
		}
	}
	return vm_id;
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

bool is_sos_vm(const struct acrn_vm *vm)
{
	return (vm != NULL)  && (get_vm_config(vm->vm_id)->load_order == SOS_VM);
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
bool is_rt_vm(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	return ((vm_config->guest_flags & GUEST_FLAG_RT) != 0U);
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
	if (is_sos_vm(vm)) {
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

/* return a pointer to the virtual machine structure of SOS VM */
struct acrn_vm *get_sos_vm(void)
{
	ASSERT(sos_vm_ptr != NULL, "sos_vm_ptr is NULL");

	return sos_vm_ptr;
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
	bool is_hpa1 = true;
	uint64_t base_hpa = vm_config->memory.start_hpa;
	uint64_t remaining_hpa_size = vm_config->memory.size;
	uint32_t i;

	for (i = 0U; i < vm->e820_entry_num; i++) {
		const struct e820_entry *entry = &(vm->e820_entries[i]);

		if (entry->length == 0UL) {
			continue;
		} else {
			if (is_sw_sram_initialized && (entry->baseaddr == PRE_RTVM_SW_SRAM_BASE_GPA) &&
				((vm_config->guest_flags & GUEST_FLAG_RT) != 0U)){
				/* pass through Software SRAM to pre-RTVM */
				ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
					get_software_sram_base(), PRE_RTVM_SW_SRAM_BASE_GPA,
					get_software_sram_size(), EPT_RWX | EPT_WB);
				continue;
			}
		}

		if (remaining_hpa_size >= entry->length) {
			/* Do EPT mapping for GPAs that are backed by physical memory */
			if ((entry->type == E820_TYPE_RAM) || (entry->type == E820_TYPE_ACPI_RECLAIM)
					|| (entry->type == E820_TYPE_ACPI_NVS)) {
				ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, base_hpa, entry->baseaddr,
					entry->length, EPT_RWX | EPT_WB);
				base_hpa += entry->length;
				remaining_hpa_size -= entry->length;
			}

			/* GPAs under 1MB are always backed by physical memory */
			if ((entry->type != E820_TYPE_RAM) && (entry->baseaddr < (uint64_t)MEM_1M)) {
				ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, base_hpa, entry->baseaddr,
					entry->length, EPT_RWX | EPT_UNCACHED);
				base_hpa += entry->length;
				remaining_hpa_size -= entry->length;
			}
		} else if (entry->type == E820_TYPE_RAM) {
			pr_warn("%s: HPA size incorrectly configured in v820\n", __func__);
		}

		if ((remaining_hpa_size == 0UL) && (is_hpa1)) {
			is_hpa1 = false;
			base_hpa = vm_config->memory.start_hpa2;
			remaining_hpa_size = vm_config->memory.size_hpa2;
		}
	}

	for (i = 0U; i < MAX_MMIO_DEV_NUM; i++) {
		(void)assign_mmio_dev(vm, &vm_config->mmiodevs[i]);

#ifdef P2SB_VGPIO_DM_ENABLED
		if ((vm_config->pt_p2sb_bar) && (vm_config->mmiodevs[i].base_hpa == P2SB_BAR_ADDR)) {
			register_vgpio_handler(vm, &vm_config->mmiodevs[i]);
		}
#endif
	}
}

/**
 * @param[inout] vm pointer to a vm descriptor
 *
 * @retval 0 on success
 *
 * @pre vm != NULL
 * @pre is_sos_vm(vm) == true
 */
static void prepare_sos_vm_memmap(struct acrn_vm *vm)
{
	uint16_t vm_id;
	uint32_t i;
	uint64_t attr_uc = (EPT_RWX | EPT_UNCACHED);
	uint64_t hv_hpa;
	struct acrn_vm_config *vm_config;
	uint64_t *pml4_page = (uint64_t *)vm->arch_vm.nworld_eptp;
	struct epc_section* epc_secs;

	const struct e820_entry *entry;
	uint32_t entries_count = vm->e820_entry_num;
	const struct e820_entry *p_e820 = vm->e820_entries;
	const struct mem_range *p_mem_range_info = get_mem_range_info();
	struct pci_mmcfg_region *pci_mmcfg;

	pr_dbg("sos_vm: bottom memory - 0x%lx, top memory - 0x%lx\n",
		p_mem_range_info->mem_bottom, p_mem_range_info->mem_top);

	if (p_mem_range_info->mem_top > EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE)) {
		panic("Please configure SOS_VM_ADDRESS_SPACE correctly!\n");
	}

	/* create real ept map for all ranges with UC */
	ept_add_mr(vm, pml4_page, p_mem_range_info->mem_bottom, p_mem_range_info->mem_bottom,
			(p_mem_range_info->mem_top - p_mem_range_info->mem_bottom), attr_uc);

	/* update ram entries to WB attr */
	for (i = 0U; i < entries_count; i++) {
		entry = p_e820 + i;
		if (entry->type == E820_TYPE_RAM) {
			ept_modify_mr(vm, pml4_page, entry->baseaddr, entry->length, EPT_WB, EPT_MT_MASK);
		}
	}

	pr_dbg("SOS_VM e820 layout:\n");
	for (i = 0U; i < entries_count; i++) {
		entry = p_e820 + i;
		pr_dbg("e820 table: %d type: 0x%x", i, entry->type);
		pr_dbg("BaseAddress: 0x%016lx length: 0x%016lx\n", entry->baseaddr, entry->length);
	}

	/* Unmap all platform EPC resource from SOS.
	 * This part has already been marked as reserved by BIOS in E820
	 * will cause EPT violation if sos accesses EPC resource.
	 */
	epc_secs = get_phys_epc();
	for (i = 0U; (i < MAX_EPC_SECTIONS) && (epc_secs[i].size != 0UL); i++) {
		ept_del_mr(vm, pml4_page, epc_secs[i].base, epc_secs[i].size);
	}

	/* unmap hypervisor itself for safety
	 * will cause EPT violation if sos accesses hv memory
	 */
	hv_hpa = hva2hpa((void *)(get_hv_image_base()));
	ept_del_mr(vm, pml4_page, hv_hpa, CONFIG_HV_RAM_SIZE);
	/* unmap prelaunch VM memory */
	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);
		if (vm_config->load_order == PRE_LAUNCHED_VM) {
			ept_del_mr(vm, pml4_page, vm_config->memory.start_hpa, vm_config->memory.size);
		}

		for (i = 0U; i < MAX_MMIO_DEV_NUM; i++) {
			(void)deassign_mmio_dev(vm, &vm_config->mmiodevs[i]);
		}
	}

	/* unmap AP trampoline code for security
	 * This buffer is guaranteed to be page aligned.
	 */
	ept_del_mr(vm, pml4_page, get_trampoline_start16_paddr(), CONFIG_LOW_RAM_SIZE);

	/* unmap PCIe MMCONFIG region since it's owned by hypervisor */
	pci_mmcfg = get_mmcfg_region();
	ept_del_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, pci_mmcfg->address, get_pci_mmcfg_size(pci_mmcfg));

	/* TODO: remove Software SRAM from SOS prevent SOS to use clflush to flush the Software SRAM cache.
	 * If we remove this EPT mapping from the SOS, the ACRN-DM can't do Software SRAM EPT mapping
	 * because the SOS can't get the HPA of this memory region.
	 */
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

	init_ept_mem_ops(&vm->arch_vm.ept_mem_ops, vm->vm_id);
	vm->arch_vm.nworld_eptp = vm->arch_vm.ept_mem_ops.get_pml4_page(vm->arch_vm.ept_mem_ops.info);
	sanitize_pte((uint64_t *)vm->arch_vm.nworld_eptp, &vm->arch_vm.ept_mem_ops);

	(void)memcpy_s(&vm->uuid[0], sizeof(vm->uuid),
		&vm_config->uuid[0], sizeof(vm_config->uuid));

	if (is_sos_vm(vm)) {
		/* Only for SOS_VM */
		create_sos_vm_e820(vm);
		prepare_sos_vm_memmap(vm);

		status = init_vm_boot_info(vm);
	} else {
		/* For PRE_LAUNCHED_VM and POST_LAUNCHED_VM */
		if ((vm_config->guest_flags & GUEST_FLAG_SECURE_WORLD_ENABLED) != 0U) {
			vm->sworld_control.flag.supported = 1U;
		}
		if (vm->sworld_control.flag.supported != 0UL) {
			struct memory_ops *ept_mem_ops = &vm->arch_vm.ept_mem_ops;

			ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
				hva2hpa(ept_mem_ops->get_sworld_memory_base(ept_mem_ops->info)),
				TRUSTY_EPT_REBASE_GPA, TRUSTY_RAM_SIZE, EPT_WB | EPT_RWX);
		}
		if (vm_config->name[0] == '\0') {
			/* if VM name is not configured, specify with VM ID */
			snprintf(vm_config->name, 16, "ACRN VM_%d", vm_id);
		}

		 if (vm_config->load_order == PRE_LAUNCHED_VM) {
			create_prelaunched_vm_e820(vm);
			prepare_prelaunched_vm_memmap(vm, vm_config);
			status = init_vm_boot_info(vm);
		 }
	}

	if (status == 0) {
		prepare_epc_vm_memmap(vm);
		spinlock_init(&vm->vlapic_mode_lock);
		spinlock_init(&vm->ept_lock);
		spinlock_init(&vm->emul_mmio_lock);

		vm->arch_vm.vlapic_mode = VM_VLAPIC_XAPIC;
		vm->intr_inject_delay_delta = 0UL;
		vm->nr_emul_mmio_regions = 0U;
		vm->vcpuid_entry_nr = 0U;

		/* Set up IO bit-mask such that VM exit occurs on
		 * selected IO ranges
		 */
		setup_io_bitmap(vm);

		init_guest_pm(vm);

		if (!is_lapic_pt_configured(vm)) {
			vpic_init(vm);
		}

		if (is_rt_vm(vm) || !is_postlaunched_vm(vm)) {
			vrtc_init(vm);
		}

		init_vpci(vm);
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
		if ((vm_config->load_order == POST_LAUNCHED_VM) && ((vm_config->guest_flags & GUEST_FLAG_IO_COMPLETION_POLLING) != 0U)) {
			/* enable IO completion polling mode per its guest flags in vm_config. */
			vm->sw.is_polling_ioreq = true;
		}
		status = set_vcpuid_entries(vm);
		if (status == 0) {
			vm->state = VM_CREATED;
		}
	}

	if (status == 0) {
		/* We have assumptions:
		 *   1) vcpus used by SOS has been offlined by DM before UOS re-use it.
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
		uint32_t i;
		for (i = 0; i < vm_config->pt_intx_num; i++) {
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
		if (!is_poweroff_vm(vm)) {
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

	if (is_sos_vm(vm)) {
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

	/* after guest_flags not used, then clear it */
	vm_config = get_vm_config(vm->vm_id);
	vm_config->guest_flags &= ~DM_OWNED_GUEST_FLAG_MASK;

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
int32_t reset_vm(struct acrn_vm *vm)
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

	if (is_sos_vm(vm)) {
		(void)vm_sw_loader(vm);
	}

	reset_vm_ioreqs(vm);
	reset_vioapics(vm);
	destroy_secure_world(vm, false);
	vm->sworld_control.flag.active = 0UL;
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

	/* For RTVM, we can only pause its vCPUs when it is powering off by itself */
	if (((!is_rt_vm(vm)) && (vm->state == VM_RUNNING)) ||
			((is_rt_vm(vm)) && (vm->state == VM_READY_TO_POWEROFF)) ||
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
 * @pre is_sos_vm(vm) && vm->state == VM_PAUSED
 */
void resume_vm_from_s3(struct acrn_vm *vm, uint32_t wakeup_vec)
{
	struct acrn_vcpu *bsp = vcpu_from_vid(vm, BSP_CPU_ID);

	vm->state = VM_RUNNING;

	reset_vcpu(bsp, POWER_ON_RESET);

	/* When SOS resume from S3, it will return to real mode
	 * with entry set to wakeup_vec.
	 */
	set_vcpu_startup_entry(bsp, wakeup_vec);

	init_vmcs(bsp);
	launch_vcpu(bsp);
}

/**
 * Prepare to create vm/vcpu for vm
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM && vm_config != NULL
 */
void prepare_vm(uint16_t vm_id, struct acrn_vm_config *vm_config)
{
	int32_t err = 0;
	struct acrn_vm *vm = NULL;

	/* SOS and pre-launched VMs launch on all pCPUs defined in vm_config->cpu_affinity */
	err = create_vm(vm_id, vm_config->cpu_affinity, vm_config, &vm);

	if (err == 0) {
		if (is_prelaunched_vm(vm)) {
			build_vrsdp(vm);
		}

		(void)vm_sw_loader(vm);

		/* start vm BSP automatically */
		start_vm(vm);

		pr_acrnlog("Start VM id: %x name: %s", vm_id, vm_config->name);
	}
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
		if ((vm_config->load_order == SOS_VM) || (vm_config->load_order == PRE_LAUNCHED_VM)) {
			if (pcpu_id == get_configured_bsp_pcpu_id(vm_config)) {
				if (vm_config->load_order == SOS_VM) {
					sos_vm_ptr = &vm_array[vm_id];
				}
				prepare_vm(vm_id, vm_config);
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
		send_single_ipi(pcpu_id, NOTIFY_VCPU_VECTOR);
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
