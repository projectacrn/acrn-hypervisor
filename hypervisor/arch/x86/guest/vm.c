/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <bsp_extern.h>
#include <multiboot.h>
#include <e820.h>
#include <vtd.h>
#include <vm0_boot.h>

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

static inline void free_vm_id(const struct acrn_vm *vm)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	if (vm_config != NULL) {
		vm_config->type = UNDEFINED_VM;
	}
}

bool is_sos_vm(const struct acrn_vm *vm)
{
	return (vm != NULL) && (vm == sos_vm_ptr);
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
uint16_t get_vm_pcpu_nums(struct acrn_vm_config *vm_config)
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
#ifdef CONFIG_PARTITION_MODE
	/* Map Virtual Machine to its VM Description */
	vm->vm_config = vm_config;
#endif
	vm->hw.created_vcpus = 0U;
	vm->emul_mmio_regions = 0U;
	vm->snoopy_mem = true;

	/* gpa_lowtop are used for system start up */
	vm->hw.gpa_lowtop = 0UL;

	init_ept_mem_ops(vm);
	vm->arch_vm.nworld_eptp = vm->arch_vm.ept_mem_ops.get_pml4_page(vm->arch_vm.ept_mem_ops.info);
	sanitize_pte((uint64_t *)vm->arch_vm.nworld_eptp);

	/* Only for SOS: Configure VM software information */
	/* For UOS: This VM software information is configure in DM */
	if (is_sos_vm(vm)) {
		vm->snoopy_mem = false;
		rebuild_sos_vm_e820();
		prepare_sos_vm_memmap(vm);

#ifndef CONFIG_EFI_STUB
		status = init_vm_boot_info(vm);
#else
		status = efi_boot_init();
#endif
		if (status == 0) {
			init_iommu_sos_vm_domain(vm);
		} else {
			need_cleanup = true;
		}

	} else {
		/* populate UOS vm fields according to vm_config */
		if ((vm_config->guest_flags & SECURE_WORLD_ENABLED) != 0U) {
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
#ifdef CONFIG_PARTITION_MODE
		ept_mr_add(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
			vm_config->start_hpa, 0UL, vm_config->mem_size,
			EPT_RWX|EPT_WB);
		init_vm_boot_info(vm);
#endif
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
		vrtc_init(vm);
#endif

		vpci_init(vm);

		/* vpic wire_mode default is INTR */
		vm->wire_mode = VPIC_WIRE_INTR;

		/* Init full emulated vIOAPIC instance */
		vioapic_init(vm);

		/* Populate return VM handle */
		*rtn_vm = vm;
		vm->sw.io_shared_page = NULL;
#ifdef CONFIG_IOREQ_POLLING
		/* Now, enable IO completion polling mode for all VMs with CONFIG_IOREQ_POLLING. */
		vm->sw.is_completion_polling = true;
#endif
		status = set_vcpuid_entries(vm);
		if (status == 0) {
			vm->state = VM_CREATED;
		} else {
			need_cleanup = true;
		}
	}

	if (need_cleanup && (vm != NULL)) {
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
		foreach_vcpu(i, vm, vcpu) {
			reset_vcpu(vcpu);
			offline_vcpu(vcpu);
		}

		ptdev_release_all_entries(vm);

		/* Free EPT allocated resources assigned to VM */
		destroy_ept(vm);

		/* Free iommu */
		if (vm->iommu != NULL) {
			destroy_iommu_domain(vm->iommu);
		}

		/* Free vm id */
		free_vm_id(vm);

		vpci_cleanup(vm);
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
		vioapic_reset(vm_ioapic(vm));
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
		vm->state = VM_PAUSED;

		foreach_vcpu(i, vm, vcpu) {
			pause_vcpu(vcpu, VCPU_ZOMBIE);
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
		mptable_build(vm);
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
