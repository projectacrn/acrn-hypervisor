/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file vcpu.h
 *
 * @brief public APIs for vcpu operations
 */

#ifndef VCPU_H
#define VCPU_H


#ifndef ASSEMBLER

#include <acrn_common.h>
#include <x86/guest/guest_memory.h>
#include <x86/guest/virtual_cr.h>
#include <x86/guest/vlapic.h>
#include <x86/guest/vmtrr.h>
#include <schedule.h>
#include <event.h>
#include <io_req.h>
#include <x86/msr.h>
#include <x86/cpu.h>
#include <x86/guest/instr_emul.h>
#include <x86/vmx.h>

/**
 * @brief vcpu
 *
 * @defgroup acrn_vcpu ACRN vcpu
 * @{
 */

/*
 * VCPU related APIs
 */

/**
 * @defgroup virt_int_injection Event ID supported for virtual interrupt injection
 *
 * This is a group that includes Event ID supported for virtual interrupt injection.
 *
 * @{
 */

/**
 * @brief Request for exception injection
 */
#define ACRN_REQUEST_EXCP			0U

/**
 * @brief Request for vLAPIC event
 */
#define ACRN_REQUEST_EVENT			1U

/**
 * @brief Request for external interrupt from vPIC
 */
#define ACRN_REQUEST_EXTINT			2U

/**
 * @brief Request for non-maskable interrupt
 */
#define ACRN_REQUEST_NMI			3U

/**
 * @brief Request for EOI exit bitmap update
 */
#define ACRN_REQUEST_EOI_EXIT_BITMAP_UPDATE	4U

/**
 * @brief Request for EPT flush
 */
#define ACRN_REQUEST_EPT_FLUSH			5U

/**
 * @brief Request for triple fault
 */
#define ACRN_REQUEST_TRP_FAULT			6U

/**
 * @brief Request for VPID TLB flush
 */
#define ACRN_REQUEST_VPID_FLUSH			7U

/**
 * @brief Request for initilizing VMCS
 */
#define ACRN_REQUEST_INIT_VMCS			8U

/**
 * @brief Request for sync waiting WBINVD
 */
#define ACRN_REQUEST_WAIT_WBINVD		9U

/**
 * @brief Request for split lock operation
 */
#define ACRN_REQUEST_SPLIT_LOCK			10U

/**
 * @}
 */
/* End of virt_int_injection */

#define save_segment(seg, SEG_NAME)				\
{								\
	(seg).selector = exec_vmread16(SEG_NAME##_SEL);		\
	(seg).base = exec_vmread(SEG_NAME##_BASE);		\
	(seg).limit = exec_vmread32(SEG_NAME##_LIMIT);		\
	(seg).attr = exec_vmread32(SEG_NAME##_ATTR);		\
}

#define load_segment(seg, SEG_NAME)				\
{								\
	exec_vmwrite16(SEG_NAME##_SEL, (seg).selector);		\
	exec_vmwrite(SEG_NAME##_BASE, (seg).base);		\
	exec_vmwrite32(SEG_NAME##_LIMIT, (seg).limit);		\
	exec_vmwrite32(SEG_NAME##_ATTR, (seg).attr);		\
}

/* Define segments constants for guest */
#define REAL_MODE_BSP_INIT_CODE_SEL     (0xf000U)
#define REAL_MODE_DATA_SEG_AR           (0x0093U)
#define REAL_MODE_CODE_SEG_AR           (0x009fU)
#define PROTECTED_MODE_DATA_SEG_AR      (0xc093U)
#define PROTECTED_MODE_CODE_SEG_AR      (0xc09bU)
#define REAL_MODE_SEG_LIMIT             (0xffffU)
#define PROTECTED_MODE_SEG_LIMIT        (0xffffffffU)
#define DR7_INIT_VALUE                  (0x400UL)
#define LDTR_AR                         (0x0082U) /* LDT, type must be 2, refer to SDM Vol3 26.3.1.2 */
#define TR_AR                           (0x008bU) /* TSS (busy), refer to SDM Vol3 26.3.1.2 */

#define foreach_vcpu(idx, vm, vcpu)				\
	for ((idx) = 0U, (vcpu) = &((vm)->hw.vcpu_array[(idx)]);	\
		(idx) < (vm)->hw.created_vcpus;			\
		(idx)++, (vcpu) = &((vm)->hw.vcpu_array[(idx)])) \
		if (vcpu->state != VCPU_OFFLINE)

enum vcpu_state {
	VCPU_OFFLINE = 0U,
	VCPU_INIT,
	VCPU_RUNNING,
	VCPU_ZOMBIE,
};

enum vm_cpu_mode {
	CPU_MODE_REAL,
	CPU_MODE_PROTECTED,
	CPU_MODE_COMPATIBILITY,		/* IA-32E mode (CS.L = 0) */
	CPU_MODE_64BIT,			/* IA-32E mode (CS.L = 1) */
};

#define	VCPU_EVENT_IOREQ		0
#define	VCPU_EVENT_VIRTUAL_INTERRUPT	1
#define	VCPU_EVENT_SYNC_WBINVD		2
#define VCPU_EVENT_SPLIT_LOCK		3
#define	VCPU_EVENT_NUM			4


enum reset_mode;

/* 2 worlds: 0 for Normal World, 1 for Secure World */
#define NR_WORLD	2
#define NORMAL_WORLD	0
#define SECURE_WORLD	1

#define NUM_WORLD_MSRS		2U
#define NUM_COMMON_MSRS		21U
#define NUM_GUEST_MSRS		(NUM_WORLD_MSRS + NUM_COMMON_MSRS)

#define EOI_EXIT_BITMAP_SIZE	256U

struct guest_cpu_context {
	struct run_context run_ctx;
	struct ext_context ext_ctx;

	/* per world MSRs, need isolation between secure and normal world */
	uint32_t world_msrs[NUM_WORLD_MSRS];
};

/* Intel SDM 24.8.2, the address must be 16-byte aligned */
struct msr_store_entry {
	uint32_t msr_index;
	uint32_t reserved;
	uint64_t value;
} __aligned(16);

enum {
	MSR_AREA_TSC_AUX = 0,
	MSR_AREA_IA32_PQR_ASSOC,
	MSR_AREA_COUNT,
};

struct msr_store_area {
	struct msr_store_entry guest[MSR_AREA_COUNT];
	struct msr_store_entry host[MSR_AREA_COUNT];
	uint32_t count;	/* actual count of entries to be loaded/restored during VMEntry/VMExit */
};

struct iwkey {
	/* 256bit encryption key */
	uint64_t encryption_key[4];
	/* 128bit integration key */
	uint64_t integrity_key[2];
};

struct acrn_vcpu_arch {
	/* vmcs region for this vcpu, MUST be 4KB-aligned */
	uint8_t vmcs[PAGE_SIZE];

	/* MSR bitmap region for this vcpu, MUST be 4-Kbyte aligned */
	uint8_t msr_bitmap[PAGE_SIZE];

	/* per vcpu lapic */
	struct acrn_vlapic vlapic;

	/* pid MUST be 64 bytes aligned */
	struct pi_desc pid __aligned(64);

	struct acrn_vmtrr vmtrr;

	int32_t cur_context;
	struct guest_cpu_context contexts[NR_WORLD];

	/* common MSRs, world_msrs[] is a subset of it */
	uint64_t guest_msrs[NUM_GUEST_MSRS];

	uint16_t vpid;

	/* Holds the information needed for IRQ/exception handling. */
	struct {
		/* The number of the exception to raise. */
		uint32_t exception;

		/* The error number for the exception. */
		uint32_t error;
	} exception_info;

	uint8_t lapic_mask;
	bool irq_window_enabled;
	bool emulating_lock;
	uint32_t nrexits;

	/* VCPU context state information */
	uint32_t exit_reason;
	uint32_t idt_vectoring_info;
	uint64_t exit_qualification;
	uint32_t proc_vm_exec_ctrls;
	uint32_t inst_len;

	/* Information related to secondary / AP VCPU start-up */
	enum vm_cpu_mode cpu_mode;
	uint8_t nr_sipi;

	/* interrupt injection information */
	uint64_t pending_req;

	/* List of MSRS to be stored and loaded on VM exits or VM entries */
	struct msr_store_area msr_area;

	/* EOI_EXIT_BITMAP buffer, for the bitmap update */
	uint64_t eoi_exit_bitmap[EOI_EXIT_BITMAP_SIZE >> 6U];

	/* Keylocker */
	struct iwkey IWKey;
	bool cr4_kl_enabled;
	/*
	 * Keylocker spec 4.4:
	 * Bit 0 - Status of most recent copy to or from IWKeyBackup.
	 * Bit 63:1 - Reserved.
	 */
	uint64_t iwkey_copy_status;
} __aligned(PAGE_SIZE);

struct acrn_vm;
struct acrn_vcpu {
	uint8_t stack[CONFIG_STACK_SIZE] __aligned(16);

	/* Architecture specific definitions for this VCPU */
	struct acrn_vcpu_arch arch;
	uint16_t vcpu_id;	/* virtual identifier for VCPU */
	struct acrn_vm *vm;		/* Reference to the VM this VCPU belongs to */

	volatile enum vcpu_state state;	/* State of this VCPU */

	struct thread_object thread_obj;
	bool launched; /* Whether the vcpu is launched on target pcpu */

	struct instr_emul_ctxt inst_ctxt;
	struct io_request req; /* used by io/ept emulation */

	uint64_t reg_cached;
	uint64_t reg_updated;

	struct sched_event events[VCPU_EVENT_NUM];
} __aligned(PAGE_SIZE);

struct vcpu_dump {
	struct acrn_vcpu *vcpu;
	char *str;
	uint32_t str_max;
};

struct guest_mem_dump {
	struct acrn_vcpu *vcpu;
	uint64_t gva;
	uint64_t len;
};

static inline bool is_vcpu_bsp(const struct acrn_vcpu *vcpu)
{
	return (vcpu->vcpu_id == BSP_CPU_ID);
}

static inline enum vm_cpu_mode get_vcpu_mode(const struct acrn_vcpu *vcpu)
{
	return vcpu->arch.cpu_mode;
}

/* do not update Guest RIP for next VM Enter */
static inline void vcpu_retain_rip(struct acrn_vcpu *vcpu)
{
	(vcpu)->arch.inst_len = 0U;
}

static inline struct acrn_vlapic *vcpu_vlapic(struct acrn_vcpu *vcpu)
{
	return &(vcpu->arch.vlapic);
}

/**
 * @brief Get pointer to PI description.
 *
 * @param[in] vcpu Target vCPU
 *
 * @return pointer to PI description
 *
 * @pre vcpu != NULL
 */
static inline struct pi_desc *get_pi_desc(struct acrn_vcpu *vcpu)
{
	return &(vcpu->arch.pid);
}

uint16_t pcpuid_from_vcpu(const struct acrn_vcpu *vcpu);
void default_idle(__unused struct thread_object *obj);
void vcpu_thread(struct thread_object *obj);

int32_t vmx_vmrun(struct run_context *context, int32_t ops, int32_t ibrs);

/* External Interfaces */

/**
 * @brief get vcpu register value
 *
 * Get target vCPU's general purpose registers value in run_context.
 *
 * @param[in] vcpu pointer to vcpu data structure
 * @param[in] reg register of the vcpu
 *
 * @return the value of the register.
 */
uint64_t vcpu_get_gpreg(const struct acrn_vcpu *vcpu, uint32_t reg);

/**
 * @brief set vcpu register value
 *
 * Set target vCPU's general purpose registers value in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] reg register of the vcpu
 * @param[in] val the value set the register of the vcpu
 *
 * @return None
 */
void vcpu_set_gpreg(struct acrn_vcpu *vcpu, uint32_t reg, uint64_t val);

/**
 * @brief get vcpu RIP value
 *
 * Get & cache target vCPU's RIP in run_context.
 *
 * @param[in] vcpu pointer to vcpu data structure
 *
 * @return the value of RIP.
 */
uint64_t vcpu_get_rip(struct acrn_vcpu *vcpu);

/**
 * @brief set vcpu RIP value
 *
 * Update target vCPU's RIP in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] val the value set RIP
 *
 * @return None
 */
void vcpu_set_rip(struct acrn_vcpu *vcpu, uint64_t val);

/**
 * @brief get vcpu RSP value
 *
 * Get & cache target vCPU's RSP in run_context.
 *
 * @param[in] vcpu pointer to vcpu data structure
 *
 * @return the value of RSP.
 */
uint64_t vcpu_get_rsp(const struct acrn_vcpu *vcpu);

/**
 * @brief set vcpu RSP value
 *
 * Update target vCPU's RSP in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] val the value set RSP
 *
 * @return None
 */
void vcpu_set_rsp(struct acrn_vcpu *vcpu, uint64_t val);

/**
 * @brief get vcpu EFER value
 *
 * Get & cache target vCPU's EFER in run_context.
 *
 * @param[in] vcpu pointer to vcpu data structure
 *
 * @return the value of EFER.
 */
uint64_t vcpu_get_efer(struct acrn_vcpu *vcpu);

/**
 * @brief set vcpu EFER value
 *
 * Update target vCPU's EFER in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] val the value set EFER
 *
 * @return None
 */
void vcpu_set_efer(struct acrn_vcpu *vcpu, uint64_t val);

/**
 * @brief get vcpu RFLAG value
 *
 * Get & cache target vCPU's RFLAGS in run_context.
 *
 * @param[in] vcpu pointer to vcpu data structure
 *
 * @return the value of RFLAGS.
 */
uint64_t vcpu_get_rflags(struct acrn_vcpu *vcpu);

/**
 * @brief set vcpu RFLAGS value
 *
 * Update target vCPU's RFLAGS in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] val the value set RFLAGS
 *
 * @return None
 */
void vcpu_set_rflags(struct acrn_vcpu *vcpu, uint64_t val);

/**
 * @brief get guest emulated MSR
 *
 * Get the content of emulated guest MSR
 *
 * @param[in] vcpu pointer to vcpu data structure
 * @param[in] msr the guest MSR
 *
 * @return the value of emulated MSR.
 */
uint64_t vcpu_get_guest_msr(const struct acrn_vcpu *vcpu, uint32_t msr);

/**
 * @brief set guest emulated MSR
 *
 * Update the content of emulated guest MSR
 *
 * @param[in] vcpu pointer to vcpu data structure
 * @param[in] msr the guest MSR
 * @param[in] val the value to set the target MSR
 *
 * @return None
 */
void vcpu_set_guest_msr(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t val);

/**
 * @brief write eoi_exit_bitmap to VMCS fields
 *
 * @param[in] vcpu pointer to vcpu data structure
 *
 * @return None
 */
void vcpu_set_vmcs_eoi_exit(const struct acrn_vcpu *vcpu);

/**
 * @brief reset all eoi_exit_bitmaps
 *
 * @param[in] vcpu pointer to vcpu data structure
 *
 * @return None
 */

void vcpu_reset_eoi_exit_bitmaps(struct acrn_vcpu *vcpu);

/**
 * @brief set eoi_exit_bitmap bit
 *
 * Set corresponding bit of vector in eoi_exit_bitmap
 *
 * @param[in] vcpu pointer to vcpu data structure
 * @param[in] vector
 *
 * @return None
 */
void vcpu_set_eoi_exit_bitmap(struct acrn_vcpu *vcpu, uint32_t vector);
/**
 * @brief clear eoi_exit_bitmap bit
 *
 * Clear corresponding bit of vector in eoi_exit_bitmap
 *
 * @param[in] vcpu pointer to vcpu data structure
 * @param[in] vector
 *
 * @return None
 */
void vcpu_clear_eoi_exit_bitmap(struct acrn_vcpu *vcpu, uint32_t vector);
/**
 * @brief set all the vcpu registers
 *
 * Update target vCPU's all registers in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] vcpu_regs all the registers' value
 *
 * @return None
 */
void set_vcpu_regs(struct acrn_vcpu *vcpu, struct acrn_vcpu_regs *vcpu_regs);

/**
 * @brief reset all the vcpu registers
 *
 * Reset target vCPU's all registers in run_context to initial values.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 *
 * @return None
 */
void reset_vcpu_regs(struct acrn_vcpu *vcpu);

bool sanitize_cr0_cr4_pattern(void);

/**
 * @brief Initialize the protect mode vcpu registers
 *
 * Initialize vCPU's all registers in run_context to initial protece mode values.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] vgdt_base_gpa guest physical address of gdt for guest
 *
 * @return None
 */
void init_vcpu_protect_mode_regs(struct acrn_vcpu *vcpu, uint64_t vgdt_base_gpa);

/**
 * @brief set the vCPU startup entry
 *
 * Set target vCPU's startup entry in run_context.
 *
 * @param[inout] vcpu pointer to vCPU data structure
 * @param[in] entry startup entry for the vCPU
 *
 * @return None
 */
void set_vcpu_startup_entry(struct acrn_vcpu *vcpu, uint64_t entry);

static inline bool is_long_mode(struct acrn_vcpu *vcpu)
{
	return (vcpu_get_efer(vcpu) & MSR_IA32_EFER_LMA_BIT) != 0UL;
}

static inline bool is_paging_enabled(struct acrn_vcpu *vcpu)
{
	return (vcpu_get_cr0(vcpu) & CR0_PG) != 0UL;
}

static inline bool is_pae(struct acrn_vcpu *vcpu)
{
	return (vcpu_get_cr4(vcpu) & CR4_PAE) != 0UL;
}

struct acrn_vcpu *get_running_vcpu(uint16_t pcpu_id);
struct acrn_vcpu *get_ever_run_vcpu(uint16_t pcpu_id);

void save_xsave_area(struct acrn_vcpu *vcpu, struct ext_context *ectx);
void rstore_xsave_area(const struct acrn_vcpu *vcpu, const struct ext_context *ectx);
void load_iwkey(struct acrn_vcpu *vcpu);

/**
 * @brief create a vcpu for the target vm
 *
 * Creates/allocates a vCPU instance, with initialization for its vcpu_id,
 * vpid, vmcs, vlapic, etc. It sets the init vCPU state to VCPU_INIT
 *
 * @param[in] pcpu_id created vcpu will run on this pcpu
 * @param[in] vm pointer to vm data structure, this vcpu will owned by this vm.
 * @param[out] rtn_vcpu_handle pointer to the created vcpu
 *
 * @retval 0 vcpu created successfully, other values failed.
 */
int32_t create_vcpu(uint16_t pcpu_id, struct acrn_vm *vm, struct acrn_vcpu **rtn_vcpu_handle);

/**
 * @brief run into non-root mode based on vcpu setting
 *
 * An interface in vCPU thread to implement VM entry and VM exit.
 * A CPU switches between VMX root mode and non-root mode based on it.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @pre vcpu != NULL
 *
 * @retval 0 vcpu run successfully, other values failed.
 */
int32_t run_vcpu(struct acrn_vcpu *vcpu);

/**
 * @brief unmap the vcpu with pcpu and free its vlapic
 *
 * Unmap the vcpu with pcpu and free its vlapic, and set the vcpu state to offline
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @pre vcpu != NULL
 * @pre vcpu->state == VCPU_ZOMBIE
 * @return None
 */
void offline_vcpu(struct acrn_vcpu *vcpu);

/**
 * @brief reset vcpu state and values
 *
 * Reset all fields in a vCPU instance, the vCPU state is reset to VCPU_INIT.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] mode the reset mode
 * @pre vcpu != NULL
 * @pre vcpu->state == VCPU_ZOMBIE
 * @return None
 */
void reset_vcpu(struct acrn_vcpu *vcpu, enum reset_mode mode);

/**
 * @brief pause the vcpu and set new state
 *
 * Change a vCPU state to VCPU_ZOMBIE, and make a reschedule request for it.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] new_state the state to set vcpu
 *
 * @return None
 */
void zombie_vcpu(struct acrn_vcpu *vcpu, enum vcpu_state new_state);

/**
 * @brief set the vcpu to running state, then it will be scheculed.
 *
 * Adds a vCPU into the run queue and make a reschedule request for it. It sets the vCPU state to VCPU_RUNNING.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @pre vcpu != NULL
 * @pre vcpu->state == VCPU_INIT
 * @return None
 */
void launch_vcpu(struct acrn_vcpu *vcpu);

/**
 * @brief kick the vcpu and let it handle pending events
 *
 * Kick a vCPU to handle the pending events.
 *
 * @param[in] vcpu pointer to vcpu data structure
 *
 * @return None
 */
void kick_vcpu(struct acrn_vcpu *vcpu);

/**
 * @brief create a vcpu for the vm and mapped to the pcpu.
 *
 * Create a vcpu for the vm, and mapped to the pcpu.
 *
 * @param[inout] vm pointer to vm data structure
 * @param[in] pcpu_id which the vcpu will be mapped
 *
 * @retval 0 on success
 * @retval -EINVAL if the vCPU ID is invalid
 */
int32_t prepare_vcpu(struct acrn_vm *vm, uint16_t pcpu_id);

/**
 * @brief get physical destination cpu mask
 *
 * get the corresponding physical destination cpu mask for the vm and virtual destination cpu mask
 *
 * @param[in] vm pointer to vm data structure
 * @param[in] vdmask virtual destination cpu mask
 *
 * @return The physical destination CPU mask
 */
uint64_t vcpumask2pcpumask(struct acrn_vm *vm, uint64_t vdmask);
bool is_lapic_pt_enabled(struct acrn_vcpu *vcpu);

/**
 * @brief handle posted interrupts
 *
 * VT-d PI handler, find the corresponding vCPU for this IRQ,
 * if the associated PID's bit ON is set, wake it up.
 *
 * @param[in] vcpu_index a zero based index of where the vCPU is located in the vCPU list for current pCPU
 * @pre vcpu_index < CONFIG_MAX_VM_NUM
 *
 * @return None
 */
void vcpu_handle_pi_notification(uint32_t vcpu_index);

/*
 * @brief Update the state of vCPU and state of vlapic
 *
 * The vlapic state of VM shall be updated for some vCPU
 * state update cases, such as from VCPU_INIT to VCPU_RUNNING.

 * @pre (vcpu != NULL)
 */
void vcpu_set_state(struct acrn_vcpu *vcpu, enum vcpu_state new_state);

/**
 * @}
 */
/* End of acrn_vcpu */

#endif /* ASSEMBLER */

#endif /* VCPU_H */
