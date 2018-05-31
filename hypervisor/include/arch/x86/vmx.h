/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VMX_H_
#define VMX_H_

/* 16-bit control fields */
#define VMX_VPID						0x00000000
/* 16-bit guest-state fields */
#define VMX_GUEST_ES_SEL    0x00000800
#define VMX_GUEST_CS_SEL    0x00000802
#define VMX_GUEST_SS_SEL    0x00000804
#define VMX_GUEST_DS_SEL    0x00000806
#define VMX_GUEST_FS_SEL    0x00000808
#define VMX_GUEST_GS_SEL    0x0000080a
#define VMX_GUEST_LDTR_SEL    0x0000080c
#define VMX_GUEST_TR_SEL    0x0000080e
#define VMX_GUEST_INTR_STATUS 0x00000810
/* 16-bit host-state fields */
#define VMX_HOST_ES_SEL     0x00000c00
#define VMX_HOST_CS_SEL     0x00000c02
#define VMX_HOST_SS_SEL     0x00000c04
#define VMX_HOST_DS_SEL     0x00000c06
#define VMX_HOST_FS_SEL     0x00000c08
#define VMX_HOST_GS_SEL     0x00000c0a
#define VMX_HOST_TR_SEL     0x00000c0c
/* 64-bit control fields */
#define VMX_IO_BITMAP_A_FULL   0x00002000
#define VMX_IO_BITMAP_A_HIGH   0x00002001
#define VMX_IO_BITMAP_B_FULL   0x00002002
#define VMX_IO_BITMAP_B_HIGH   0x00002003
#define VMX_MSR_BITMAP_FULL             0x00002004
#define VMX_MSR_BITMAP_HIGH             0x00002005
#define VMX_EXIT_MSR_STORE_ADDR_FULL 0x00002006
#define VMX_EXIT_MSR_STORE_ADDR_HIGH 0x00002007
#define VMX_EXIT_MSR_LOAD_ADDR_FULL  0x00002008
#define VMX_EXIT_MSR_LOAD_ADDR_HIGH  0x00002009
#define VMX_ENTRY_MSR_LOAD_ADDR_FULL 0x0000200a
#define VMX_ENTRY_MSR_LOAD_ADDR_HIGH 0x0000200b
#define VMX_EXECUTIVE_VMCS_PTR_FULL     0x0000200c
#define VMX_EXECUTIVE_VMCS_PTR_HIGH     0x0000200d
#define VMX_TSC_OFFSET_FULL    0x00002010
#define VMX_TSC_OFFSET_HIGH    0x00002011
#define VMX_VIRTUAL_APIC_PAGE_ADDR_FULL 0x00002012
#define VMX_VIRTUAL_APIC_PAGE_ADDR_HIGH 0x00002013
#define VMX_APIC_ACCESS_ADDR_FULL  0x00002014
#define VMX_APIC_ACCESS_ADDR_HIGH  0x00002015
#define VMX_EPT_POINTER_FULL      0x0000201A
#define VMX_EPT_POINTER_HIGH      0x0000201B
#define	VMX_EOI_EXIT0_FULL			0x0000201C
#define	VMX_EOI_EXIT0_HIGH			0x0000201D
#define	VMX_EOI_EXIT1_FULL			0x0000201E
#define	VMX_EOI_EXIT1_HIGH			0x0000201F
#define	VMX_EOI_EXIT2_FULL			0x00002020
#define	VMX_EOI_EXIT2_HIGH			0x00002021
#define	VMX_EOI_EXIT3_FULL			0x00002022
#define	VMX_EOI_EXIT3_HIGH			0x00002023
#define	VMX_EOI_EXIT(vector)	(VMX_EOI_EXIT0_FULL + ((vector) / 64) * 2)
#define VMX_XSS_EXITING_BITMAP_FULL		0x0000202C
#define VMX_XSS_EXITING_BITMAP_HIGH		0x0000202D
/* 64-bit read-only data fields */
#define VMX_GUEST_PHYSICAL_ADDR_FULL 0x00002400
#define VMX_GUEST_PHYSICAL_ADDR_HIGH 0x00002401
/* 64-bit guest-state fields */
#define VMX_VMS_LINK_PTR_FULL   0x00002800
#define VMX_VMS_LINK_PTR_HIGH   0x00002801
#define VMX_GUEST_IA32_DEBUGCTL_FULL 0x00002802
#define VMX_GUEST_IA32_DEBUGCTL_HIGH 0x00002803
#define VMX_GUEST_IA32_PAT_FULL         0x00002804
#define VMX_GUEST_IA32_PAT_HIGH         0x00002805
#define VMX_GUEST_IA32_EFER_FULL        0x00002806
#define VMX_GUEST_IA32_EFER_HIGH  0x00002807
#define VMX_GUEST_IA32_PERF_CTL_FULL    0x00002808
#define VMX_GUEST_IA32_PERF_CTL_HIGH    0x00002809
#define VMX_GUEST_PDPTE0_FULL         0x0000280A
#define VMX_GUEST_PDPTE0_HIGH   0x0000280B
#define VMX_GUEST_PDPTE1_FULL   0x0000280C
#define VMX_GUEST_PDPTE1_HIGH   0x0000280D
#define VMX_GUEST_PDPTE2_FULL   0x0000280E
#define VMX_GUEST_PDPTE2_HIGH   0x0000280F
#define VMX_GUEST_PDPTE3_FULL   0x00002810
#define VMX_GUEST_PDPTE3_HIGH   0x00002811
/* 64-bit host-state fields */
#define VMX_HOST_IA32_PAT_FULL          0x00002C00
#define VMX_HOST_IA32_PAT_HIGH          0x00002C01
#define VMX_HOST_IA32_EFER_FULL         0x00002C02
#define VMX_HOST_IA32_EFER_HIGH         0x00002C03
#define VMX_HOST_IA32_PERF_CTL_FULL     0x00002C04
#define VMX_HOST_IA32_PERF_CTL_HIGH     0x00002C05
/* 32-bit control fields */
#define VMX_PIN_VM_EXEC_CONTROLS  0x00004000
#define VMX_PROC_VM_EXEC_CONTROLS  0x00004002
#define VMX_EXCEPTION_BITMAP   0x00004004
#define VMX_PF_ERROR_CODE_MASK     0x00004006
#define VMX_PF_ERROR_CODE_MATCH     0x00004008
#define VMX_CR3_TARGET_COUNT   0x0000400a
#define VMX_EXIT_CONTROLS    0x0000400c
#define VMX_EXIT_MSR_STORE_COUNT  0x0000400e
#define VMX_EXIT_MSR_LOAD_COUNT   0x00004010
#define VMX_ENTRY_CONTROLS    0x00004012
#define VMX_ENTRY_MSR_LOAD_COUNT  0x00004014
#define VMX_ENTRY_INT_INFO_FIELD  0x00004016
#define VMX_ENTRY_EXCEPTION_ERROR_CODE   0x00004018
#define VMX_ENTRY_INSTR_LENGTH   0x0000401a
#define VMX_TPR_THRESHOLD    0x0000401c
#define VMX_PROC_VM_EXEC_CONTROLS2  0x0000401E
#define VMX_PLE_GAP               0x00004020
#define VMX_PLE_WINDOW                  0x00004022
/* 32-bit read-only data fields */
#define VMX_INSTR_ERROR     0x00004400
#define VMX_EXIT_REASON     0x00004402
#define VMX_EXIT_INT_INFO    0x00004404
#define VMX_EXIT_INT_ERROR_CODE     0x00004406
#define VMX_IDT_VEC_INFO_FIELD   0x00004408
#define VMX_IDT_VEC_ERROR_CODE     0x0000440a
#define VMX_EXIT_INSTR_LEN    0x0000440c
#define VMX_INSTR_INFO     0x0000440e
/* 32-bit guest-state fields */
#define VMX_GUEST_ES_LIMIT    0x00004800
#define VMX_GUEST_CS_LIMIT    0x00004802
#define VMX_GUEST_SS_LIMIT    0x00004804
#define VMX_GUEST_DS_LIMIT    0x00004806
#define VMX_GUEST_FS_LIMIT    0x00004808
#define VMX_GUEST_GS_LIMIT    0x0000480a
#define VMX_GUEST_LDTR_LIMIT   0x0000480c
#define VMX_GUEST_TR_LIMIT    0x0000480e
#define VMX_GUEST_GDTR_LIMIT   0x00004810
#define VMX_GUEST_IDTR_LIMIT   0x00004812
#define VMX_GUEST_ES_ATTR    0x00004814
#define VMX_GUEST_CS_ATTR    0x00004816
#define VMX_GUEST_SS_ATTR    0x00004818
#define VMX_GUEST_DS_ATTR    0x0000481a
#define VMX_GUEST_FS_ATTR    0x0000481c
#define VMX_GUEST_GS_ATTR    0x0000481e
#define VMX_GUEST_LDTR_ATTR    0x00004820
#define VMX_GUEST_TR_ATTR    0x00004822
#define VMX_GUEST_INTERRUPTIBILITY_INFO 0x00004824
#define VMX_GUEST_ACTIVITY_STATE  0x00004826
#define VMX_GUEST_SMBASE                0x00004828
#define VMX_GUEST_IA32_SYSENTER_CS  0x0000482a
#define VMX_GUEST_TIMER                 0x0000482E
/* 32-bit host-state fields */
#define VMX_HOST_IA32_SYSENTER_CS  0x00004c00
/* natural-width control fields */
#define VMX_CR0_MASK     0x00006000
#define VMX_CR4_MASK     0x00006002
#define VMX_CR0_READ_SHADOW    0x00006004
#define VMX_CR4_READ_SHADOW    0x00006006
#define VMX_CR3_TARGET_0    0x00006008
#define VMX_CR3_TARGET_1    0x0000600a
#define VMX_CR3_TARGET_2    0x0000600c
#define VMX_CR3_TARGET_3    0x0000600e
/* natural-width read-only data fields */
#define VMX_EXIT_QUALIFICATION   0x00006400
#define VMX_IO_RCX      0x00006402
#define VMX_IO_RDI      0x00006406
#define VMX_GUEST_LINEAR_ADDR   0x0000640a
/* natural-width guest-state fields */
#define VMX_GUEST_CR0     0x00006800
#define VMX_GUEST_CR3     0x00006802
#define VMX_GUEST_CR4     0x00006804
#define VMX_GUEST_ES_BASE    0x00006806
#define VMX_GUEST_CS_BASE    0x00006808
#define VMX_GUEST_SS_BASE    0x0000680a
#define VMX_GUEST_DS_BASE    0x0000680c
#define VMX_GUEST_FS_BASE    0x0000680e
#define VMX_GUEST_GS_BASE    0x00006810
#define VMX_GUEST_LDTR_BASE    0x00006812
#define VMX_GUEST_TR_BASE    0x00006814
#define VMX_GUEST_GDTR_BASE    0x00006816
#define VMX_GUEST_IDTR_BASE    0x00006818
#define VMX_GUEST_DR7     0x0000681a
#define VMX_GUEST_RSP     0x0000681c
#define VMX_GUEST_RIP     0x0000681e
#define VMX_GUEST_RFLAGS    0x00006820
#define VMX_GUEST_PENDING_DEBUG_EXCEPT 0x00006822
#define VMX_GUEST_IA32_SYSENTER_ESP  0x00006824
#define VMX_GUEST_IA32_SYSENTER_EIP  0x00006826
/* natural-width host-state fields */
#define VMX_HOST_CR0     0x00006c00
#define VMX_HOST_CR3     0x00006c02
#define VMX_HOST_CR4     0x00006c04
#define VMX_HOST_FS_BASE    0x00006c06
#define VMX_HOST_GS_BASE    0x00006c08
#define VMX_HOST_TR_BASE    0x00006c0a
#define VMX_HOST_GDTR_BASE    0x00006c0c
#define VMX_HOST_IDTR_BASE    0x00006c0e
#define VMX_HOST_IA32_SYSENTER_ESP  0x00006c10
#define VMX_HOST_IA32_SYSENTER_EIP  0x00006c12
#define VMX_HOST_RSP                                                 0x00006c14
#define VMX_HOST_RIP                                                 0x00006c16
/*
 * Basic VM exit reasons
 */
#define VMX_EXIT_REASON_EXCEPTION_OR_NMI                             0x00000000
#define VMX_EXIT_REASON_EXTERNAL_INTERRUPT                           0x00000001
#define VMX_EXIT_REASON_TRIPLE_FAULT                                 0x00000002
#define VMX_EXIT_REASON_INIT_SIGNAL                                  0x00000003
#define VMX_EXIT_REASON_STARTUP_IPI                                  0x00000004
#define VMX_EXIT_REASON_IO_SMI                                       0x00000005
#define VMX_EXIT_REASON_OTHER_SMI                                    0x00000006
#define VMX_EXIT_REASON_INTERRUPT_WINDOW                             0x00000007
#define VMX_EXIT_REASON_NMI_WINDOW                                   0x00000008
#define VMX_EXIT_REASON_TASK_SWITCH                                  0x00000009
#define VMX_EXIT_REASON_CPUID                                        0x0000000A
#define VMX_EXIT_REASON_GETSEC                                       0x0000000B
#define VMX_EXIT_REASON_HLT                                          0x0000000C
#define VMX_EXIT_REASON_INVD                                         0x0000000D
#define VMX_EXIT_REASON_INVLPG                                       0x0000000E
#define VMX_EXIT_REASON_RDPMC                                        0x0000000F
#define VMX_EXIT_REASON_RDTSC                                        0x00000010
#define VMX_EXIT_REASON_RSM                                          0x00000011
#define VMX_EXIT_REASON_VMCALL                                       0x00000012
#define VMX_EXIT_REASON_VMCLEAR                                      0x00000013
#define VMX_EXIT_REASON_VMLAUNCH                                     0x00000014
#define VMX_EXIT_REASON_VMPTRLD                                      0x00000015
#define VMX_EXIT_REASON_VMPTRST                                      0x00000016
#define VMX_EXIT_REASON_VMREAD                                       0x00000017
#define VMX_EXIT_REASON_VMRESUME                                     0x00000018
#define VMX_EXIT_REASON_VMWRITE                                      0x00000019
#define VMX_EXIT_REASON_VMXOFF                                       0x0000001A
#define VMX_EXIT_REASON_VMXON                                        0x0000001B
#define VMX_EXIT_REASON_CR_ACCESS                                    0x0000001C
#define VMX_EXIT_REASON_DR_ACCESS                                    0x0000001D
#define VMX_EXIT_REASON_IO_INSTRUCTION                               0x0000001E
#define VMX_EXIT_REASON_RDMSR                                        0x0000001F
#define VMX_EXIT_REASON_WRMSR                                        0x00000020
#define VMX_EXIT_REASON_ENTRY_FAILURE_INVALID_GUEST_STATE            0x00000021
#define VMX_EXIT_REASON_ENTRY_FAILURE_MSR_LOADING                    0x00000022
	 /* entry 0x23 (35) is missing */
#define VMX_EXIT_REASON_MWAIT                                        0x00000024
#define VMX_EXIT_REASON_MONITOR_TRAP                                 0x00000025
	 /* entry 0x26 (38) is missing */
#define VMX_EXIT_REASON_MONITOR                                      0x00000027
#define VMX_EXIT_REASON_PAUSE                                        0x00000028
#define VMX_EXIT_REASON_ENTRY_FAILURE_MACHINE_CHECK                  0x00000029
	 /* entry 0x2A (42) is missing */
#define VMX_EXIT_REASON_TPR_BELOW_THRESHOLD                          0x0000002B
#define VMX_EXIT_REASON_APIC_ACCESS                                  0x0000002C
#define VMX_EXIT_REASON_VIRTUALIZED_EOI                              0x0000002D
#define VMX_EXIT_REASON_GDTR_IDTR_ACCESS                             0x0000002E
#define VMX_EXIT_REASON_LDTR_TR_ACCESS                               0x0000002F
#define VMX_EXIT_REASON_EPT_VIOLATION                                0x00000030
#define VMX_EXIT_REASON_EPT_MISCONFIGURATION                         0x00000031
#define VMX_EXIT_REASON_INVEPT                                       0x00000032
#define VMX_EXIT_REASON_RDTSCP                                       0x00000033
#define VMX_EXIT_REASON_VMX_PREEMPTION_TIMER_EXPIRED                 0x00000034
#define VMX_EXIT_REASON_INVVPID                                      0x00000035
#define VMX_EXIT_REASON_WBINVD                                       0x00000036
#define VMX_EXIT_REASON_XSETBV                                       0x00000037
#define VMX_EXIT_REASON_APIC_WRITE                                   0x00000038

/* VMX execution control bits (pin based) */
#define VMX_PINBASED_CTLS_IRQ_EXIT     (1<<0)
#define VMX_PINBASED_CTLS_NMI_EXIT     (1<<3)
#define VMX_PINBASED_CTLS_VIRT_NMI     (1<<5)
#define VMX_PINBASED_CTLS_ENABLE_PTMR  (1<<6)
#define VMX_PINBASED_CTLS_POST_IRQ     (1<<7)

/* VMX execution control bits (processor based) */
#define VMX_PROCBASED_CTLS_IRQ_WIN     (1<<2)
#define VMX_PROCBASED_CTLS_TSC_OFF     (1<<3)
#define VMX_PROCBASED_CTLS_HLT         (1<<7)
#define VMX_PROCBASED_CTLS_INVLPG      (1<<9)
#define VMX_PROCBASED_CTLS_MWAIT       (1<<10)
#define VMX_PROCBASED_CTLS_RDPMC       (1<<11)
#define VMX_PROCBASED_CTLS_RDTSC       (1<<12)
#define VMX_PROCBASED_CTLS_CR3_LOAD    (1<<15)
#define VMX_PROCBASED_CTLS_CR3_STORE   (1<<16)
#define VMX_PROCBASED_CTLS_CR8_LOAD    (1<<19)
#define VMX_PROCBASED_CTLS_CR8_STORE   (1<<20)
#define VMX_PROCBASED_CTLS_TPR_SHADOW  (1<<21)
#define VMX_PROCBASED_CTLS_NMI_WINEXIT (1<<22)
#define VMX_PROCBASED_CTLS_MOV_DR      (1<<23)
#define VMX_PROCBASED_CTLS_UNCOND_IO   (1<<24)
#define VMX_PROCBASED_CTLS_IO_BITMAP   (1<<25)
#define VMX_PROCBASED_CTLS_MON_TRAP    (1<<27)
#define VMX_PROCBASED_CTLS_MSR_BITMAP  (1<<28)
#define VMX_PROCBASED_CTLS_MONITOR     (1<<29)
#define VMX_PROCBASED_CTLS_PAUSE       (1<<30)
#define VMX_PROCBASED_CTLS_SECONDARY   (1<<31)
#define VMX_PROCBASED_CTLS2_VAPIC      (1<<0)
#define VMX_PROCBASED_CTLS2_EPT        (1<<1)
#define VMX_PROCBASED_CTLS2_DESC_TABLE (1<<2)
#define VMX_PROCBASED_CTLS2_RDTSCP     (1<<3)
#define VMX_PROCBASED_CTLS2_VX2APIC    (1<<4)
#define VMX_PROCBASED_CTLS2_VPID       (1<<5)
#define VMX_PROCBASED_CTLS2_WBINVD     (1<<6)
#define VMX_PROCBASED_CTLS2_UNRESTRICT (1<<7)
#define VMX_PROCBASED_CTLS2_VAPIC_REGS (1<<8)
#define VMX_PROCBASED_CTLS2_VIRQ       (1<<9)
#define VMX_PROCBASED_CTLS2_PAUSE_LOOP (1<<10)
#define VMX_PROCBASED_CTLS2_RDRAND     (1<<11)
#define VMX_PROCBASED_CTLS2_INVPCID    (1<<12)
#define VMX_PROCBASED_CTLS2_VM_FUNCS   (1<<13)
#define VMX_PROCBASED_CTLS2_VMCS_SHADW (1<<14)
#define VMX_PROCBASED_CTLS2_RDSEED     (1<<16)
#define VMX_PROCBASED_CTLS2_EPT_VE     (1<<18)
#define VMX_PROCBASED_CTLS2_XSVE_XRSTR (1<<20)

/* MSR_IA32_VMX_EPT_VPID_CAP: EPT and VPID capability bits */
#define VMX_EPT_EXECUTE_ONLY		(1 << 0)
#define VMX_EPT_PAGE_WALK_4		(1 << 6)
#define VMX_EPT_PAGE_WALK_5		(1 << 7)
#define VMX_EPTP_UC			(1 << 8)
#define VMX_EPTP_WB			(1 << 14)
#define VMX_EPT_2MB_PAGE		(1 << 16)
#define VMX_EPT_1GB_PAGE		(1 << 17)
#define VMX_EPT_INVEPT			(1 << 20)
#define VMX_EPT_AD			(1 << 21)
#define VMX_EPT_INVEPT_SINGLE_CONTEXT	(1 << 25)
#define VMX_EPT_INVEPT_GLOBAL_CONTEXT	(1 << 26)

#define VMX_MIN_NR_VPID			1
#define VMX_MAX_NR_VPID			(1 << 5)

#define VMX_VPID_TYPE_INDIVIDUAL_ADDR	0
#define VMX_VPID_TYPE_SINGLE_CONTEXT	1
#define VMX_VPID_TYPE_ALL_CONTEXT	2
#define VMX_VPID_TYPE_SINGLE_NON_GLOBAL	3

#define VMX_VPID_INVVPID			(1 << 0) /* (32 - 32) */
#define VMX_VPID_INVVPID_INDIVIDUAL_ADDR	(1 << 8) /* (40 - 32) */
#define VMX_VPID_INVVPID_SINGLE_CONTEXT		(1 << 9) /* (41 - 32) */
#define VMX_VPID_INVVPID_GLOBAL_CONTEXT		(1 << 10) /* (42 - 32) */
#define VMX_VPID_INVVPID_SINGLE_NON_GLOBAL	(1 << 11) /* (43 - 32) */

#define VMX_EPT_MT_EPTE_SHIFT		3
#define VMX_EPTP_PWL_MASK		0x38
#define VMX_EPTP_PWL_4			0x18
#define VMX_EPTP_PWL_5			0x20
#define VMX_EPTP_AD_ENABLE_BIT		(1 << 6)
#define VMX_EPTP_MT_MASK		0x7
#define VMX_EPTP_MT_WB			0x6
#define VMX_EPTP_MT_UC			0x0

/* VMX exit control bits */
#define VMX_EXIT_CTLS_SAVE_DBG         (1<<2)
#define VMX_EXIT_CTLS_HOST_ADDR64      (1<<9)
#define VMX_EXIT_CTLS_LOAD_PERF        (1<<12)
#define VMX_EXIT_CTLS_ACK_IRQ          (1<<15)
#define VMX_EXIT_CTLS_SAVE_PAT         (1<<18)
#define VMX_EXIT_CTLS_LOAD_PAT         (1<<19)
#define VMX_EXIT_CTLS_SAVE_EFER        (1<<20)
#define VMX_EXIT_CTLS_LOAD_EFER        (1<<21)
#define VMX_EXIT_CTLS_SAVE_PTMR        (1<<22)

/* VMX entry control bits */
#define VMX_ENTRY_CTLS_LOAD_DBG        (1<<2)
#define VMX_ENTRY_CTLS_IA32E_MODE      (1<<9)
#define VMX_ENTRY_CTLS_ENTRY_SMM       (1<<10)
#define VMX_ENTRY_CTLS_DEACT_DUAL      (1<<11)
#define VMX_ENTRY_CTLS_LOAD_PERF       (1<<13)
#define VMX_ENTRY_CTLS_LOAD_PAT        (1<<14)
#define VMX_ENTRY_CTLS_LOAD_EFER       (1<<15)

/* VMX entry/exit Interrupt info */
#define VMX_INT_INFO_ERR_CODE_VALID	(1<<11)
#define VMX_INT_INFO_VALID		(1<<31)
#define VMX_INT_TYPE_MASK		(0x700)
#define VMX_INT_TYPE_EXT_INT		0
#define VMX_INT_TYPE_NMI		2
#define VMX_INT_TYPE_HW_EXP		3
#define VMX_INT_TYPE_SW_EXP		6

/*VM exit qulifications for APIC-access
 * Access type:
 *  0  = linear access for a data read during instruction execution
 *  1  = linear access for a data write during instruction execution
 *  2  = linear access for an instruction fetch
 *  3  = linear access (read or write) during event delivery
 *  10 = guest-physical access during event delivery
 *  15 = guest-physical access for an instructon fetch or during
 *       instruction execution
 */
#define APIC_ACCESS_TYPE(qual)		(((qual) >> 12) & 0xF)
#define APIC_ACCESS_OFFSET(qual)	((qual) & 0xFFF)


#define VM_SUCCESS	0
#define VM_FAIL		-1

#define VMX_VMENTRY_FAIL 	0x80000000

#ifndef ASSEMBLER

#define RFLAGS_C (1<<0)
#define RFLAGS_Z (1<<6)

/* CR0 bits hv want to trap to track status change */
#define CR0_TRAP_MASK (CR0_PE | CR0_PG | CR0_WP)
#define CR0_RESERVED_MASK ~(CR0_PG | CR0_CD | CR0_NW | CR0_AM | CR0_WP | \
			   CR0_NE |  CR0_ET | CR0_TS | CR0_EM | CR0_MP | CR0_PE)

/* CR4 bits hv want to trap to track status change */
#define CR4_TRAP_MASK (CR4_PSE | CR4_PAE)

#define VMX_SUPPORT_UNRESTRICTED_GUEST (1<<5)

/* External Interfaces */
int exec_vmxon_instr(void);
uint64_t exec_vmread(uint32_t field);
uint64_t exec_vmread64(uint32_t field_full);
void exec_vmwrite(uint32_t field, uint64_t value);
void exec_vmwrite64(uint32_t field_full, uint64_t value);
int init_vmcs(struct vcpu *vcpu);

int exec_vmclear(void *addr);
int exec_vmptrld(void *addr);

int vmx_write_cr0(struct vcpu *vcpu, uint64_t cr0);
int vmx_write_cr3(struct vcpu *vcpu, uint64_t cr3);
int vmx_write_cr4(struct vcpu *vcpu, uint64_t cr4);

static inline uint8_t get_vcpu_mode(struct vcpu *vcpu)
{
	return vcpu->arch_vcpu.cpu_mode;
}

static inline bool cpu_has_vmx_unrestricted_guest_cap(void)
{
       return !!(msr_read(MSR_IA32_VMX_MISC) & VMX_SUPPORT_UNRESTRICTED_GUEST);
}

typedef struct _descriptor_table_{
	uint16_t limit;
	uint64_t base;
}__attribute__((packed)) descriptor_table;
#endif /* ASSEMBLER */

#endif /* VMX_H_ */
