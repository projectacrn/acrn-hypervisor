/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <sprintf.h>
#include <vm.h>
#include <bits.h>
#include <uart16550.h>
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
#include <cat.h>
#include <firmware.h>
#include <board.h>

vm_sw_loader_t vm_sw_loader;

/* Local variables */

static struct acrn_vm vm_array[CONFIG_MAX_VM_NUM] __aligned(PAGE_SIZE);

static struct acrn_vm *sos_vm_ptr = NULL;

uint16_t find_free_vm_id(void)
{
	uint16_t id;
	struct acrn_vm_config *vm_config;

	for (id = 0U; id < CONFIG_MAX_VM_NUM; id++) {
		vm_config = get_vm_config(id);
		if (vm_config->type == UNDEFINED_VM) {
			break;
		}
	}
	return (vm_config->type == UNDEFINED_VM) ? id : INVALID_VM_ID;
}

/**
 * @pre vm != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
static inline void free_vm_id(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	vm_config->type = UNDEFINED_VM;
}

bool is_valid_vm(const struct acrn_vm *vm)
{
	return (vm != NULL) && (vm->state != VM_STATE_INVALID);
}

bool is_sos_vm(const struct acrn_vm *vm)
{
	return (vm != NULL)  && (get_vm_config(vm->vm_id)->type == SOS_VM);
}

/**
 * @pre vm != NULL && vm_config != NULL && vm->vmid < CONFIG_MAX_VM_NUM
 */
bool is_lapic_pt(const struct acrn_vm *vm)
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
static inline uint16_t get_vm_bsp_pcpu_id(const struct acrn_vm_config *vm_config)
{
	uint16_t cpu_id = INVALID_CPU_ID;

	cpu_id = ffs64(vm_config->pcpu_bitmap);

	return (cpu_id < get_pcpu_nums()) ? cpu_id : INVALID_CPU_ID;
}

#ifdef CONFIG_PARTITION_MODE
/**
 * @pre vm_config != NULL
 */
uint16_t get_vm_pcpu_nums(const struct acrn_vm_config *vm_config)
{
	uint16_t i, host_pcpu_num, pcpu_num = 0U;
	uint64_t cpu_bitmap = vm_config->pcpu_bitmap;

	host_pcpu_num = get_pcpu_nums();

	for (i = 0U; i < host_pcpu_num ; i++) {
		if (bitmap_test(i, &cpu_bitmap)) {
			pcpu_num++;
		}
	}
	return pcpu_num;
}
#endif

/**
 * @pre vm != NULL && vm_config != NULL
 */
static void prepare_prelaunched_vm_memmap(struct acrn_vm *vm, const struct acrn_vm_config *vm_config)
{
	uint64_t base_hpa = vm_config->memory.start_hpa;
	uint32_t i;

	for (i = 0U; i < vm->e820_entry_num; i++) {
		struct e820_entry *entry = &(vm->e820_entries[i]);

		if (entry->length == 0UL) {
			break;
		}

		/* Do EPT mapping for GPAs that are backed by physical memory */
		if (entry->type == E820_TYPE_RAM) {
			ept_mr_add(vm, (uint64_t *)vm->arch_vm.nworld_eptp, base_hpa, entry->baseaddr,
				entry->length, EPT_RWX | EPT_WB);

			base_hpa += entry->length;
		}

		/* GPAs under 1MB are always backed by physical memory */
		if ((entry->type != E820_TYPE_RAM) && (entry->baseaddr < (uint64_t)MEM_1M)) {
			ept_mr_add(vm, (uint64_t *)vm->arch_vm.nworld_eptp, base_hpa, entry->baseaddr,
				entry->length, EPT_RWX | EPT_UNCACHED);

			base_hpa += entry->length;
		}
	}
}

/**
 * before boot sos_vm(service OS), call it to hide the HV RAM entry in e820 table from sos_vm
 *
 * @pre vm != NULL && entry != NULL && p_e820_mem != NULL
 */
static void create_sos_vm_e820(struct acrn_vm *vm)
{
	uint32_t i;
	uint64_t entry_start;
	uint64_t entry_end;
	uint64_t hv_start_pa = hva2hpa((void *)(get_hv_image_base()));
	uint64_t hv_end_pa  = hv_start_pa + CONFIG_HV_RAM_SIZE;
	uint32_t entries_count = get_e820_entries_count();
	struct e820_entry *entry, new_entry = {0};
	struct e820_entry *p_e820 = (struct e820_entry *)get_e820_entry();
	struct e820_mem_params *p_e820_mem = (struct e820_mem_params *)get_e820_mem_info();

	/* hypervisor mem need be filter out from e820 table
	 * it's hv itself + other hv reserved mem like vgt etc
	 */
	for (i = 0U; i < entries_count; i++) {
		entry = p_e820 + i;
		entry_start = entry->baseaddr;
		entry_end = entry->baseaddr + entry->length;

		/* No need handle in these cases*/
		if ((entry->type != E820_TYPE_RAM) || (entry_end <= hv_start_pa) || (entry_start >= hv_end_pa)) {
			continue;
		}

		/* filter out hv mem and adjust length of this entry*/
		if ((entry_start < hv_start_pa) && (entry_end <= hv_end_pa)) {
			entry->length = hv_start_pa - entry_start;
			continue;
		}

		/* filter out hv mem and need to create a new entry*/
		if ((entry_start < hv_start_pa) && (entry_end > hv_end_pa)) {
			entry->length = hv_start_pa - entry_start;
			new_entry.baseaddr = hv_end_pa;
			new_entry.length = entry_end - hv_end_pa;
			new_entry.type = E820_TYPE_RAM;
			continue;
		}

		/* This entry is within the range of hv mem
		 * change to E820_TYPE_RESERVED
		 */
		if ((entry_start >= hv_start_pa) && (entry_end <= hv_end_pa)) {
			entry->type = E820_TYPE_RESERVED;
			continue;
		}

		if ((entry_start >= hv_start_pa) && (entry_start < hv_end_pa) && (entry_end > hv_end_pa)) {
			entry->baseaddr = hv_end_pa;
			entry->length = entry_end - hv_end_pa;
			continue;
		}
	}

	if (new_entry.length > 0UL) {
		entries_count++;
		ASSERT(entries_count <= E820_MAX_ENTRIES, "e820 entry overflow");
		entry = p_e820 + entries_count - 1;
		entry->baseaddr = new_entry.baseaddr;
		entry->length = new_entry.length;
		entry->type = new_entry.type;
	}

	p_e820_mem->total_mem_size -= CONFIG_HV_RAM_SIZE;

	vm->e820_entry_num = entries_count;
	vm->e820_entries = p_e820;
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
	uint32_t i;
	uint64_t attr_uc = (EPT_RWX | EPT_UNCACHED);
	uint64_t hv_hpa;
	uint64_t *pml4_page = (uint64_t *)vm->arch_vm.nworld_eptp;

	const struct e820_entry *entry;
	uint32_t entries_count = vm->e820_entry_num;
	struct e820_entry *p_e820 = vm->e820_entries;
	const struct e820_mem_params *p_e820_mem_info = get_e820_mem_info();

	pr_dbg("sos_vm: bottom memory - 0x%llx, top memory - 0x%llx\n",
		p_e820_mem_info->mem_bottom, p_e820_mem_info->mem_top);

	if (p_e820_mem_info->mem_top > EPT_ADDRESS_SPACE(CONFIG_SOS_RAM_SIZE)) {
		panic("Please configure SOS_VM_ADDRESS_SPACE correctly!\n");
	}

	/* create real ept map for all ranges with UC */
	ept_mr_add(vm, pml4_page, p_e820_mem_info->mem_bottom, p_e820_mem_info->mem_bottom,
			(p_e820_mem_info->mem_top - p_e820_mem_info->mem_bottom), attr_uc);

	/* update ram entries to WB attr */
	for (i = 0U; i < entries_count; i++) {
		entry = p_e820 + i;
		if (entry->type == E820_TYPE_RAM) {
			ept_mr_modify(vm, pml4_page, entry->baseaddr, entry->length, EPT_WB, EPT_MT_MASK);
		}
	}

	pr_dbg("SOS_VM e820 layout:\n");
	for (i = 0U; i < entries_count; i++) {
		entry = p_e820 + i;
		pr_dbg("e820 table: %d type: 0x%x", i, entry->type);
		pr_dbg("BaseAddress: 0x%016llx length: 0x%016llx\n", entry->baseaddr, entry->length);
	}

	/* unmap hypervisor itself for safety
	 * will cause EPT violation if sos accesses hv memory
	 */
	hv_hpa = hva2hpa((void *)(get_hv_image_base()));
	ept_mr_del(vm, pml4_page, hv_hpa, CONFIG_HV_RAM_SIZE);
}

/**
 * @pre vm_id < CONFIG_MAX_VM_NUM && vm_config != NULL && rtn_vm != NULL
 */
int32_t create_vm(uint16_t vm_id, struct acrn_vm_config *vm_config, struct acrn_vm **rtn_vm)
{
	struct acrn_vm *vm = NULL;
	int32_t status = 0;
	bool need_cleanup = false;

	/* Allocate memory for virtual machine */
	vm = &vm_array[vm_id];
	(void)memset((void *)vm, 0U, sizeof(struct acrn_vm));
	vm->vm_id = vm_id;
	vm->hw.created_vcpus = 0U;
	vm->emul_mmio_regions = 0U;

	init_ept_mem_ops(vm);
	vm->arch_vm.nworld_eptp = vm->arch_vm.ept_mem_ops.get_pml4_page(vm->arch_vm.ept_mem_ops.info);
	sanitize_pte((uint64_t *)vm->arch_vm.nworld_eptp);

	/* Register default handlers for PIO & MMIO if it is SOS VM or Pre-launched VM */
	if ((vm_config->type == SOS_VM) || (vm_config->type == PRE_LAUNCHED_VM)) {
		register_pio_default_emulation_handler(vm);
		register_mmio_default_emulation_handler(vm);
	}

	if (is_sos_vm(vm)) {
		/* Only for SOS_VM */
		create_sos_vm_e820(vm);
		prepare_sos_vm_memmap(vm);

		status = firmware_init_vm_boot_info(vm);
		if (status == 0) {
			init_fallback_iommu_domain(vm->iommu, vm->vm_id, vm->arch_vm.nworld_eptp);
		} else {
			need_cleanup = true;
		}

	} else {
		/* For PRE_LAUNCHED_VM and NORMAL_VM */
		if ((vm_config->guest_flags & GUEST_FLAG_SECURE_WORLD_ENABLED) != 0U) {
			vm->sworld_control.flag.supported = 1U;
		}
		if (vm->sworld_control.flag.supported != 0UL) {
			struct memory_ops *ept_mem_ops = &vm->arch_vm.ept_mem_ops;

			ept_mr_add(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
				hva2hpa(ept_mem_ops->get_sworld_memory_base(ept_mem_ops->info)),
				TRUSTY_EPT_REBASE_GPA, TRUSTY_RAM_SIZE, EPT_WB | EPT_RWX);
		}
		if (vm_config->name[0] == '\0') {
			/* if VM name is not configured, specify with VM ID */
			snprintf(vm_config->name, 16, "ACRN VM_%d", vm_id);
		}

		(void)memcpy_s(&vm->GUID[0], sizeof(vm->GUID),
			&vm_config->GUID[0], sizeof(vm_config->GUID));

		 if (vm_config->type == PRE_LAUNCHED_VM) {
			create_prelaunched_vm_e820(vm);
			prepare_prelaunched_vm_memmap(vm, vm_config);
			(void)firmware_init_vm_boot_info(vm);
		 }
	}

	if (status == 0) {
		enable_iommu();

		INIT_LIST_HEAD(&vm->softirq_dev_entry_list);
		spinlock_init(&vm->softirq_dev_lock);
		vm->intr_inject_delay_delta = 0UL;

		/* Set up IO bit-mask such that VM exit occurs on
		 * selected IO ranges
		 */
		setup_io_bitmap(vm);

		vm_setup_cpu_state(vm);

		if (is_sos_vm(vm)) {
			/* Load pm S state data */
			if (vm_load_pm_s_state(vm) == 0) {
				register_pm1ab_handler(vm);
			}

			/* Create virtual uart; just when uart enabled, vuart can work */
			if (is_dbg_uart_enabled()) {
				vuart_init(vm);
			}
		}
		vpic_init(vm);

#ifdef CONFIG_PARTITION_MODE
		/* Create virtual uart; just when uart enabled, vuart can work */
		if (vm_config->vm_vuart && is_dbg_uart_enabled()) {
			vuart_init(vm);
		}
#endif
		vrtc_init(vm);

		vpci_init(vm);

		/* vpic wire_mode default is INTR */
		vm->wire_mode = VPIC_WIRE_INTR;

		/* Init full emulated vIOAPIC instance */
		vioapic_init(vm);

		/* Intercept the virtual pm port for RTVM */
		if (is_rt_vm(vm)) {
			register_rt_vm_pm1a_ctl_handler(vm);
		}

		/* Populate return VM handle */
		*rtn_vm = vm;
		vm->sw.io_shared_page = NULL;
		if ((vm_config->guest_flags & GUEST_FLAG_IO_COMPLETION_POLLING) != 0U) {
			/* enable IO completion polling mode per its guest flags in vm_config. */
			vm->sw.is_completion_polling = true;
		}
		status = set_vcpuid_entries(vm);
		if (status == 0) {
			vm->state = VM_CREATED;
		} else {
			need_cleanup = true;
		}
	}

	if (need_cleanup && is_valid_vm(vm)) {
		if (vm->arch_vm.nworld_eptp != NULL) {
			(void)memset(vm->arch_vm.nworld_eptp, 0U, PAGE_SIZE);
		}
		free_vm_id(vm);
	}
	return status;
}

/*
 * @pre vm != NULL
 */
int32_t shutdown_vm(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu = NULL;
	int32_t ret;

	pause_vm(vm);

	/* Only allow shutdown paused vm */
	if (vm->state == VM_PAUSED) {
		vm->state = VM_STATE_INVALID;

		foreach_vcpu(i, vm, vcpu) {
			reset_vcpu(vcpu);
			offline_vcpu(vcpu);
		}

		ptdev_release_all_entries(vm);

		vpci_cleanup(vm);

		/* Free iommu */
		if (vm->iommu != NULL) {
			destroy_iommu_domain(vm->iommu);
		}

		/* Free EPT allocated resources assigned to VM */
		destroy_ept(vm);

		/* Free vm id */
		free_vm_id(vm);

		ret = 0;
	} else {
	        ret = -EINVAL;
	}

	/* Return status to caller */
	return ret;
}

/**
 *  * @pre vm != NULL
 */
void start_vm(struct acrn_vm *vm)
{
	struct acrn_vcpu *vcpu = NULL;

	vm->state = VM_STARTED;

	/* Only start BSP (vid = 0) and let BSP start other APs */
	vcpu = vcpu_from_vid(vm, 0U);
	schedule_vcpu(vcpu);
}

/**
 *  * @pre vm != NULL
 */
int32_t reset_vm(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu = NULL;
	int32_t ret;

	if (vm->state == VM_PAUSED) {
		foreach_vcpu(i, vm, vcpu) {
			reset_vcpu(vcpu);
		}

		if (is_sos_vm(vm)) {
			(void )vm_sw_loader(vm);
		}

		reset_vm_ioreqs(vm);
		vioapic_reset(vm);
		destroy_secure_world(vm, false);
		vm->sworld_control.flag.active = 0UL;
		ret = 0;
	} else {
		ret = -1;
	}

	return ret;
}

/**
 *  * @pre vm != NULL
 */
void pause_vm(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu = NULL;

	if (vm->state != VM_PAUSED) {
		if (is_rt_vm(vm)) {
			/* Only when RTVM is powering off by itself, we can pause vcpu */
			if (vm->state == VM_POWERING_OFF) {
				foreach_vcpu(i, vm, vcpu) {
					pause_vcpu(vcpu, VCPU_ZOMBIE);
				}

				vm->state = VM_PAUSED;
			}
		} else {
			foreach_vcpu(i, vm, vcpu) {
				pause_vcpu(vcpu, VCPU_ZOMBIE);
			}

			vm->state = VM_PAUSED;
		}
	}
}

/**
 *  * @pre vm != NULL
 */
void resume_vm(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu = NULL;

	foreach_vcpu(i, vm, vcpu) {
		resume_vcpu(vcpu);
	}

	vm->state = VM_STARTED;
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
 */
void resume_vm_from_s3(struct acrn_vm *vm, uint32_t wakeup_vec)
{
	struct acrn_vcpu *bsp = vcpu_from_vid(vm, 0U);

	vm->state = VM_STARTED;

	reset_vcpu(bsp);

	/* When SOS resume from S3, it will return to real mode
	 * with entry set to wakeup_vec.
	 */
	set_ap_entry(bsp, wakeup_vec);

	init_vmcs(bsp);
	schedule_vcpu(bsp);
}

/**
 * Prepare to create vm/vcpu for vm
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM && vm_config != NULL
 */
void prepare_vm(uint16_t vm_id, struct acrn_vm_config *vm_config)
{
	int32_t err = 0;
	uint16_t i;
	struct acrn_vm *vm = NULL;

	err = create_vm(vm_id, vm_config, &vm);

	if (err == 0) {
#ifdef CONFIG_PARTITION_MODE
		(void)mptable_build(vm);
#endif

		for (i = 0U; i < get_pcpu_nums(); i++) {
			if (bitmap_test(i, &vm_config->pcpu_bitmap)) {
				err = prepare_vcpu(vm, i);
				if (err != 0) {
					break;
				}
			}
		}

	}

	if (err == 0) {
		if (vm_sw_loader == NULL) {
			vm_sw_loader = general_sw_loader;
		}

		(void )vm_sw_loader(vm);

		/* start vm BSP automatically */
		start_vm(vm);

		pr_acrnlog("Start VM id: %x name: %s", vm_id, vm_config->name);
	}
}

/**
 * @pre vm_config != NULL
 */
void launch_vms(uint16_t pcpu_id)
{
	uint16_t vm_id, bsp_id;
	struct acrn_vm_config *vm_config;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);
		if ((vm_config->type == SOS_VM) || (vm_config->type == PRE_LAUNCHED_VM)) {
			if (vm_config->type == SOS_VM) {
				sos_vm_ptr = &vm_array[vm_id];
			}

			bsp_id = get_vm_bsp_pcpu_id(vm_config);
			if (pcpu_id == bsp_id) {
				prepare_vm(vm_id, vm_config);
			}
		}
	}
}
