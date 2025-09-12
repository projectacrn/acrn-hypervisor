/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef X86_VM_H_
#define X86_VM_H_

/* Defines for VM Launch and Resume */
#define VM_RESUME		0
#define VM_LAUNCH		1

#ifndef ASSEMBLER

#include <bits.h>
#include <spinlock.h>
#include <asm/pgtable.h>
#include <vcpu.h>
#include <vioapic.h>
#include <vpic.h>
#include <asm/guest/vmx_io.h>
#include <vuart.h>
#include <vrtc.h>
#include <asm/guest/trusty.h>
#include <asm/guest/vcpuid.h>
#include <vpci.h>
#include <asm/cpu_caps.h>
#include <asm/e820.h>
#include <asm/vm_config.h>
#include <io_req.h>
#include <mmu.h>
#ifdef CONFIG_HYPERV_ENABLED
#include <asm/guest/hyperv.h>
#endif

enum reset_mode {
	POWER_ON_RESET,		/* reset by hardware Power-on */
	COLD_RESET,		/* hardware cold reset */
	WARM_RESET,		/* behavior slightly differ from cold reset, that some MSRs might be retained. */
	INIT_RESET,		/* reset by INIT */
	SOFTWARE_RESET,		/* reset by software disable<->enable */
	RESUME_FROM_S3,		/* reset core states after resuming from S3 */
};

struct vm_pm_info {
	uint8_t			px_cnt;		/* count of all Px states */
	struct acrn_pstate_data	px_data[MAX_PSTATE];
	uint8_t			cx_cnt;		/* count of all Cx entries */
	struct acrn_cstate_data	cx_data[MAX_CSTATE];
	struct pm_s_state_data	*sx_state_data;	/* data for S3/S5 implementation */
};

enum vm_vlapic_mode {
	VM_VLAPIC_DISABLED = 0U,
	VM_VLAPIC_XAPIC,
	VM_VLAPIC_X2APIC,
	VM_VLAPIC_TRANSITION
};

struct vm_arch {
	/* I/O bitmaps A and B for this VM, MUST be 4-Kbyte aligned */
	uint8_t io_bitmap[PAGE_SIZE*2];

	struct vm_pm_info pm;	/* Reference to this VM's PM information */
	uint32_t e820_entry_num;
	struct e820_entry *e820_entries;

	enum vpic_wire_mode wire_mode;

	spinlock_t wbinvd_lock;		/* Spin-lock used to serialize wbinvd emulation */
	spinlock_t vlapic_mode_lock;	/* Spin-lock used to protect vlapic_mode modifications for a VM */
	struct secure_world_control sworld_control;

	/* Secure World's snapshot
	 * Currently, Secure World is only running on vcpu[0],
	 * so the snapshot only stores the vcpu0's run_context
	 * of secure world.
	 */
	struct guest_cpu_context sworld_snapshot;

	uint32_t vcpuid_entry_nr, vcpuid_level, vcpuid_xlevel;
	struct vcpuid_entry vcpuid_entries[MAX_VM_VCPUID_ENTRIES];

	uint64_t intr_inject_delay_delta; /* delay of intr injection */
	uint32_t reset_control;

	/* EPT hierarchy for Secure World
	 * Secure world can access Normal World's memory,
	 * but Normal World can not access Secure World's memory.
	 */
	void *sworld_eptp;

	struct acrn_vioapics vioapics;	/* Virtual IOAPIC/s */
	struct acrn_vpic vpic;      /* Virtual PIC */
#ifdef CONFIG_HYPERV_ENABLED
	struct acrn_hyperv hyperv;
#endif
	enum vm_vlapic_mode vlapic_mode; /* Represents vLAPIC mode across vCPUs*/

	/*
	 * Keylocker spec 4.5:
	 * Bit 0 - Backup/restore valid.
	 * Bit 1 - Reserved.
	 * Bit 2 - Backup key storage read/write error.
	 * Bit 3 - IWKeyBackup consumed.
	 * Bit 63:4 - Reserved.
	 */
	uint64_t iwkey_backup_status;
	spinlock_t iwkey_backup_lock;	/* Spin-lock used to protect internal key backup/restore */
	struct iwkey iwkey_backup;

} __aligned(PAGE_SIZE);

/* Convert relative vm id to absolute vm id */
static inline uint16_t rel_vmid_2_vmid(uint16_t service_vmid, uint16_t rel_vmid) {
	return (service_vmid + rel_vmid);
}

/* Convert absolute vm id to relative vm id */
static inline uint16_t vmid_2_rel_vmid(uint16_t service_vmid, uint16_t vmid) {
	return (vmid - service_vmid);
}

static inline bool is_severity_pass(uint16_t target_vmid)
{
	return SEVERITY_SERVICE_VM >= get_vm_severity(target_vmid);
}

void make_shutdown_vm_request(uint16_t pcpu_id);
bool need_shutdown_vm(uint16_t pcpu_id);
void poweroff_if_rt_vm(struct acrn_vm *vm);
void resume_vm_from_s3(struct acrn_vm *vm, uint32_t wakeup_vec);
int32_t reset_vm(struct acrn_vm *vm, enum reset_mode mode);
bool is_created_vm(const struct acrn_vm *vm);
bool is_service_vm(const struct acrn_vm *vm);
bool is_postlaunched_vm(const struct acrn_vm *vm);
bool is_prelaunched_vm(const struct acrn_vm *vm);
uint16_t get_vmid_by_name(const char *name);
struct acrn_vm *get_service_vm(void);

void create_service_vm_e820(struct acrn_vm *vm);
void create_prelaunched_vm_e820(struct acrn_vm *vm);
void prepare_vm_identical_memmap(struct acrn_vm *vm, uint16_t e820_entry_type, uint64_t prot_orig);
uint64_t find_space_from_ve820(struct acrn_vm *vm, uint32_t size, uint64_t min_addr, uint64_t max_addr);

int32_t prepare_os_image(struct acrn_vm *vm);

void suspend_vrtc(void);
void resume_vrtc(void);
void vrtc_init(struct acrn_vm *vm);

bool is_lapic_pt_configured(const struct acrn_vm *vm);
bool is_pmu_pt_configured(const struct acrn_vm *vm);
bool is_rt_vm(const struct acrn_vm *vm);
bool is_stateful_vm(const struct acrn_vm *vm);
bool is_nvmx_configured(const struct acrn_vm *vm);
bool is_vcat_configured(const struct acrn_vm *vm);
bool is_static_configured_vm(const struct acrn_vm *vm);
uint16_t get_unused_vmid(void);
bool is_pi_capable(const struct acrn_vm *vm);
bool has_rt_vm(void);
struct acrn_vm *get_highest_severity_vm(bool runtime);
bool vm_hide_mtrr(const struct acrn_vm *vm);
void update_vm_vlapic_state(struct acrn_vm *vm);
enum vm_vlapic_mode check_vm_vlapic_mode(const struct acrn_vm *vm);
bool is_vhwp_configured(const struct acrn_vm *vm);
bool is_vtm_configured(const struct acrn_vm *vm);
/*
 * @pre vm != NULL
 */
void get_vm_lock(struct acrn_vm *vm);

/*
 * @pre vm != NULL
 */
void put_vm_lock(struct acrn_vm *vm);

void *get_sworld_memory_base(void);
#endif /* !ASSEMBLER */

#endif /* X86_VM_H_ */
