/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VMX_H_
#define VMX_H_

/* 16-bit control fields */
#define VMX_VPID						0x00000000U
#define VMX_POSTED_INTR_VECTOR	0x00000002U
/* 16-bit guest-state fields */
#define VMX_GUEST_ES_SEL    0x00000800U
#define VMX_GUEST_CS_SEL    0x00000802U
#define VMX_GUEST_SS_SEL    0x00000804U
#define VMX_GUEST_DS_SEL    0x00000806U
#define VMX_GUEST_FS_SEL    0x00000808U
#define VMX_GUEST_GS_SEL    0x0000080aU
#define VMX_GUEST_LDTR_SEL    0x0000080cU
#define VMX_GUEST_TR_SEL    0x0000080eU
#define VMX_GUEST_INTR_STATUS 0x00000810U
/* 16-bit host-state fields */
#define VMX_HOST_ES_SEL     0x00000c00U
#define VMX_HOST_CS_SEL     0x00000c02U
#define VMX_HOST_SS_SEL     0x00000c04U
#define VMX_HOST_DS_SEL     0x00000c06U
#define VMX_HOST_FS_SEL     0x00000c08U
#define VMX_HOST_GS_SEL     0x00000c0aU
#define VMX_HOST_TR_SEL     0x00000c0cU
/* 64-bit control fields */
#define VMX_IO_BITMAP_A_FULL   0x00002000U
#define VMX_IO_BITMAP_A_HIGH   0x00002001U
#define VMX_IO_BITMAP_B_FULL   0x00002002U
#define VMX_IO_BITMAP_B_HIGH   0x00002003U
#define VMX_MSR_BITMAP_FULL             0x00002004U
#define VMX_MSR_BITMAP_HIGH             0x00002005U
#define VMX_EXIT_MSR_STORE_ADDR_FULL 0x00002006U
#define VMX_EXIT_MSR_STORE_ADDR_HIGH 0x00002007U
#define VMX_EXIT_MSR_LOAD_ADDR_FULL  0x00002008U
#define VMX_EXIT_MSR_LOAD_ADDR_HIGH  0x00002009U
#define VMX_ENTRY_MSR_LOAD_ADDR_FULL 0x0000200aU
#define VMX_ENTRY_MSR_LOAD_ADDR_HIGH 0x0000200bU
#define VMX_EXECUTIVE_VMCS_PTR_FULL     0x0000200cU
#define VMX_EXECUTIVE_VMCS_PTR_HIGH     0x0000200dU
#define VMX_TSC_OFFSET_FULL    0x00002010U
#define VMX_TSC_OFFSET_HIGH    0x00002011U
#define VMX_VIRTUAL_APIC_PAGE_ADDR_FULL 0x00002012U
#define VMX_VIRTUAL_APIC_PAGE_ADDR_HIGH 0x00002013U
#define VMX_APIC_ACCESS_ADDR_FULL  0x00002014U
#define VMX_APIC_ACCESS_ADDR_HIGH  0x00002015U
#define VMX_PIR_DESC_ADDR_FULL	0x00002016U
#define VMX_PIR_DESC_ADDR_HIGH	0x00002017U
#define VMX_EPT_POINTER_FULL      0x0000201AU
#define VMX_EPT_POINTER_HIGH      0x0000201BU
#define	VMX_EOI_EXIT0_FULL			0x0000201CU
#define	VMX_EOI_EXIT0_HIGH			0x0000201DU
#define	VMX_EOI_EXIT1_FULL			0x0000201EU
#define	VMX_EOI_EXIT1_HIGH			0x0000201FU
#define	VMX_EOI_EXIT2_FULL			0x00002020U
#define	VMX_EOI_EXIT2_HIGH			0x00002021U
#define	VMX_EOI_EXIT3_FULL			0x00002022U
#define	VMX_EOI_EXIT3_HIGH			0x00002023U

#define VMX_XSS_EXITING_BITMAP_FULL		0x0000202CU
#define VMX_XSS_EXITING_BITMAP_HIGH		0x0000202DU
/* 64-bit read-only data fields */
#define VMX_GUEST_PHYSICAL_ADDR_FULL 0x00002400U
#define VMX_GUEST_PHYSICAL_ADDR_HIGH 0x00002401U
/* 64-bit guest-state fields */
#define VMX_VMS_LINK_PTR_FULL   0x00002800U
#define VMX_VMS_LINK_PTR_HIGH   0x00002801U
#define VMX_GUEST_IA32_DEBUGCTL_FULL 0x00002802U
#define VMX_GUEST_IA32_DEBUGCTL_HIGH 0x00002803U
#define VMX_GUEST_IA32_PAT_FULL         0x00002804U
#define VMX_GUEST_IA32_PAT_HIGH         0x00002805U
#define VMX_GUEST_IA32_EFER_FULL        0x00002806U
#define VMX_GUEST_IA32_EFER_HIGH  0x00002807U
#define VMX_GUEST_IA32_PERF_CTL_FULL    0x00002808U
#define VMX_GUEST_IA32_PERF_CTL_HIGH    0x00002809U
#define VMX_GUEST_PDPTE0_FULL         0x0000280AU
#define VMX_GUEST_PDPTE0_HIGH   0x0000280BU
#define VMX_GUEST_PDPTE1_FULL   0x0000280CU
#define VMX_GUEST_PDPTE1_HIGH   0x0000280DU
#define VMX_GUEST_PDPTE2_FULL   0x0000280EU
#define VMX_GUEST_PDPTE2_HIGH   0x0000280FU
#define VMX_GUEST_PDPTE3_FULL   0x00002810U
#define VMX_GUEST_PDPTE3_HIGH   0x00002811U
/* 64-bit host-state fields */
#define VMX_HOST_IA32_PAT_FULL          0x00002C00U
#define VMX_HOST_IA32_PAT_HIGH          0x00002C01U
#define VMX_HOST_IA32_EFER_FULL         0x00002C02U
#define VMX_HOST_IA32_EFER_HIGH         0x00002C03U
#define VMX_HOST_IA32_PERF_CTL_FULL     0x00002C04U
#define VMX_HOST_IA32_PERF_CTL_HIGH     0x00002C05U
/* 32-bit control fields */
#define VMX_PIN_VM_EXEC_CONTROLS  0x00004000U
#define VMX_PROC_VM_EXEC_CONTROLS  0x00004002U
#define VMX_EXCEPTION_BITMAP   0x00004004U
#define VMX_PF_ERROR_CODE_MASK     0x00004006U
#define VMX_PF_ERROR_CODE_MATCH     0x00004008U
#define VMX_CR3_TARGET_COUNT   0x0000400aU
#define VMX_EXIT_CONTROLS    0x0000400cU
#define VMX_EXIT_MSR_STORE_COUNT  0x0000400eU
#define VMX_EXIT_MSR_LOAD_COUNT   0x00004010U
#define VMX_ENTRY_CONTROLS    0x00004012U
#define VMX_ENTRY_MSR_LOAD_COUNT  0x00004014U
#define VMX_ENTRY_INT_INFO_FIELD  0x00004016U
#define VMX_ENTRY_EXCEPTION_ERROR_CODE   0x00004018U
#define VMX_ENTRY_INSTR_LENGTH   0x0000401aU
#define VMX_TPR_THRESHOLD    0x0000401cU
#define VMX_PROC_VM_EXEC_CONTROLS2  0x0000401EU
#define VMX_PLE_GAP               0x00004020U
#define VMX_PLE_WINDOW                  0x00004022U
/* 32-bit read-only data fields */
#define VMX_INSTR_ERROR     0x00004400U
#define VMX_EXIT_REASON     0x00004402U
#define VMX_EXIT_INT_INFO    0x00004404U
#define VMX_EXIT_INT_ERROR_CODE     0x00004406U
#define VMX_IDT_VEC_INFO_FIELD   0x00004408U
#define VMX_IDT_VEC_ERROR_CODE     0x0000440aU
#define VMX_EXIT_INSTR_LEN    0x0000440cU
#define VMX_INSTR_INFO     0x0000440eU
/* 32-bit guest-state fields */
#define VMX_GUEST_ES_LIMIT    0x00004800U
#define VMX_GUEST_CS_LIMIT    0x00004802U
#define VMX_GUEST_SS_LIMIT    0x00004804U
#define VMX_GUEST_DS_LIMIT    0x00004806U
#define VMX_GUEST_FS_LIMIT    0x00004808U
#define VMX_GUEST_GS_LIMIT    0x0000480aU
#define VMX_GUEST_LDTR_LIMIT   0x0000480cU
#define VMX_GUEST_TR_LIMIT    0x0000480eU
#define VMX_GUEST_GDTR_LIMIT   0x00004810U
#define VMX_GUEST_IDTR_LIMIT   0x00004812U
#define VMX_GUEST_ES_ATTR    0x00004814U
#define VMX_GUEST_CS_ATTR    0x00004816U
#define VMX_GUEST_SS_ATTR    0x00004818U
#define VMX_GUEST_DS_ATTR    0x0000481aU
#define VMX_GUEST_FS_ATTR    0x0000481cU
#define VMX_GUEST_GS_ATTR    0x0000481eU
#define VMX_GUEST_LDTR_ATTR    0x00004820U
#define VMX_GUEST_TR_ATTR    0x00004822U
#define VMX_GUEST_INTERRUPTIBILITY_INFO 0x00004824U
#define VMX_GUEST_ACTIVITY_STATE  0x00004826U
#define VMX_GUEST_SMBASE                0x00004828U
#define VMX_GUEST_IA32_SYSENTER_CS  0x0000482aU
#define VMX_GUEST_TIMER                 0x0000482EU
/* 32-bit host-state fields */
#define VMX_HOST_IA32_SYSENTER_CS  0x00004c00U
/* natural-width control fields */
#define VMX_CR0_MASK     0x00006000U
#define VMX_CR4_MASK     0x00006002U
#define VMX_CR0_READ_SHADOW    0x00006004U
#define VMX_CR4_READ_SHADOW    0x00006006U
#define VMX_CR3_TARGET_0    0x00006008U
#define VMX_CR3_TARGET_1    0x0000600aU
#define VMX_CR3_TARGET_2    0x0000600cU
#define VMX_CR3_TARGET_3    0x0000600eU
/* natural-width read-only data fields */
#define VMX_EXIT_QUALIFICATION   0x00006400U
#define VMX_IO_RCX      0x00006402U
#define VMX_IO_RDI      0x00006406U
#define VMX_GUEST_LINEAR_ADDR   0x0000640aU
/* natural-width guest-state fields */
#define VMX_GUEST_CR0     0x00006800U
#define VMX_GUEST_CR3     0x00006802U
#define VMX_GUEST_CR4     0x00006804U
#define VMX_GUEST_ES_BASE    0x00006806U
#define VMX_GUEST_CS_BASE    0x00006808U
#define VMX_GUEST_SS_BASE    0x0000680aU
#define VMX_GUEST_DS_BASE    0x0000680cU
#define VMX_GUEST_FS_BASE    0x0000680eU
#define VMX_GUEST_GS_BASE    0x00006810U
#define VMX_GUEST_LDTR_BASE    0x00006812U
#define VMX_GUEST_TR_BASE    0x00006814U
#define VMX_GUEST_GDTR_BASE    0x00006816U
#define VMX_GUEST_IDTR_BASE    0x00006818U
#define VMX_GUEST_DR7     0x0000681aU
#define VMX_GUEST_RSP     0x0000681cU
#define VMX_GUEST_RIP     0x0000681eU
#define VMX_GUEST_RFLAGS    0x00006820U
#define VMX_GUEST_PENDING_DEBUG_EXCEPT 0x00006822U
#define VMX_GUEST_IA32_SYSENTER_ESP  0x00006824U
#define VMX_GUEST_IA32_SYSENTER_EIP  0x00006826U
/* natural-width host-state fields */
#define VMX_HOST_CR0     0x00006c00U
#define VMX_HOST_CR3     0x00006c02U
#define VMX_HOST_CR4     0x00006c04U
#define VMX_HOST_FS_BASE    0x00006c06U
#define VMX_HOST_GS_BASE    0x00006c08U
#define VMX_HOST_TR_BASE    0x00006c0aU
#define VMX_HOST_GDTR_BASE    0x00006c0cU
#define VMX_HOST_IDTR_BASE    0x00006c0eU
#define VMX_HOST_IA32_SYSENTER_ESP  0x00006c10U
#define VMX_HOST_IA32_SYSENTER_EIP  0x00006c12U
#define VMX_HOST_RSP                                                 0x00006c14U
#define VMX_HOST_RIP                                                 0x00006c16U
/*
 * Basic VM exit reasons
 */
#define VMX_EXIT_REASON_EXCEPTION_OR_NMI                             0x00000000U
#define VMX_EXIT_REASON_EXTERNAL_INTERRUPT                           0x00000001U
#define VMX_EXIT_REASON_TRIPLE_FAULT                                 0x00000002U
#define VMX_EXIT_REASON_INIT_SIGNAL                                  0x00000003U
#define VMX_EXIT_REASON_STARTUP_IPI                                  0x00000004U
#define VMX_EXIT_REASON_IO_SMI                                       0x00000005U
#define VMX_EXIT_REASON_OTHER_SMI                                    0x00000006U
#define VMX_EXIT_REASON_INTERRUPT_WINDOW                             0x00000007U
#define VMX_EXIT_REASON_NMI_WINDOW                                   0x00000008U
#define VMX_EXIT_REASON_TASK_SWITCH                                  0x00000009U
#define VMX_EXIT_REASON_CPUID                                        0x0000000AU
#define VMX_EXIT_REASON_GETSEC                                       0x0000000BU
#define VMX_EXIT_REASON_HLT                                          0x0000000CU
#define VMX_EXIT_REASON_INVD                                         0x0000000DU
#define VMX_EXIT_REASON_INVLPG                                       0x0000000EU
#define VMX_EXIT_REASON_RDPMC                                        0x0000000FU
#define VMX_EXIT_REASON_RDTSC                                        0x00000010U
#define VMX_EXIT_REASON_RSM                                          0x00000011U
#define VMX_EXIT_REASON_VMCALL                                       0x00000012U
#define VMX_EXIT_REASON_VMCLEAR                                      0x00000013U
#define VMX_EXIT_REASON_VMLAUNCH                                     0x00000014U
#define VMX_EXIT_REASON_VMPTRLD                                      0x00000015U
#define VMX_EXIT_REASON_VMPTRST                                      0x00000016U
#define VMX_EXIT_REASON_VMREAD                                       0x00000017U
#define VMX_EXIT_REASON_VMRESUME                                     0x00000018U
#define VMX_EXIT_REASON_VMWRITE                                      0x00000019U
#define VMX_EXIT_REASON_VMXOFF                                       0x0000001AU
#define VMX_EXIT_REASON_VMXON                                        0x0000001BU
#define VMX_EXIT_REASON_CR_ACCESS                                    0x0000001CU
#define VMX_EXIT_REASON_DR_ACCESS                                    0x0000001DU
#define VMX_EXIT_REASON_IO_INSTRUCTION                               0x0000001EU
#define VMX_EXIT_REASON_RDMSR                                        0x0000001FU
#define VMX_EXIT_REASON_WRMSR                                        0x00000020U
#define VMX_EXIT_REASON_ENTRY_FAILURE_INVALID_GUEST_STATE            0x00000021U
#define VMX_EXIT_REASON_ENTRY_FAILURE_MSR_LOADING                    0x00000022U
	 /* entry 0x23 (35) is missing */
#define VMX_EXIT_REASON_MWAIT                                        0x00000024U
#define VMX_EXIT_REASON_MONITOR_TRAP                                 0x00000025U
	 /* entry 0x26 (38) is missing */
#define VMX_EXIT_REASON_MONITOR                                      0x00000027U
#define VMX_EXIT_REASON_PAUSE                                        0x00000028U
#define VMX_EXIT_REASON_ENTRY_FAILURE_MACHINE_CHECK                  0x00000029U
	 /* entry 0x2A (42) is missing */
#define VMX_EXIT_REASON_TPR_BELOW_THRESHOLD                          0x0000002BU
#define VMX_EXIT_REASON_APIC_ACCESS                                  0x0000002CU
#define VMX_EXIT_REASON_VIRTUALIZED_EOI                              0x0000002DU
#define VMX_EXIT_REASON_GDTR_IDTR_ACCESS                             0x0000002EU
#define VMX_EXIT_REASON_LDTR_TR_ACCESS                               0x0000002FU
#define VMX_EXIT_REASON_EPT_VIOLATION                                0x00000030U
#define VMX_EXIT_REASON_EPT_MISCONFIGURATION                         0x00000031U
#define VMX_EXIT_REASON_INVEPT                                       0x00000032U
#define VMX_EXIT_REASON_RDTSCP                                       0x00000033U
#define VMX_EXIT_REASON_VMX_PREEMPTION_TIMER_EXPIRED                 0x00000034U
#define VMX_EXIT_REASON_INVVPID                                      0x00000035U
#define VMX_EXIT_REASON_WBINVD                                       0x00000036U
#define VMX_EXIT_REASON_XSETBV                                       0x00000037U
#define VMX_EXIT_REASON_APIC_WRITE                                   0x00000038U
#define VMX_EXIT_REASON_RDRAND                                       0x00000039U
#define VMX_EXIT_REASON_INVPCID                                      0x0000003AU
#define VMX_EXIT_REASON_VMFUNC                                       0x0000003BU
#define VMX_EXIT_REASON_ENCLS                                        0x0000003CU
#define VMX_EXIT_REASON_RDSEED                                       0x0000003DU
#define VMX_EXIT_REASON_PAGE_MODIFICATION_LOG_FULL                   0x0000003EU
#define VMX_EXIT_REASON_XSAVES                                       0x0000003FU
#define VMX_EXIT_REASON_XRSTORS                                      0x00000040U

/* VMX execution control bits (pin based) */
#define VMX_PINBASED_CTLS_IRQ_EXIT     (1U<<0U)
#define VMX_PINBASED_CTLS_NMI_EXIT     (1U<<3U)
#define VMX_PINBASED_CTLS_VIRT_NMI     (1U<<5U)
#define VMX_PINBASED_CTLS_ENABLE_PTMR  (1U<<6U)
#define VMX_PINBASED_CTLS_POST_IRQ     (1U<<7U)

/* VMX execution control bits (processor based) */
#define VMX_PROCBASED_CTLS_IRQ_WIN     (1U<<2U)
#define VMX_PROCBASED_CTLS_TSC_OFF     (1U<<3U)
#define VMX_PROCBASED_CTLS_HLT         (1U<<7U)
#define VMX_PROCBASED_CTLS_INVLPG      (1U<<9U)
#define VMX_PROCBASED_CTLS_MWAIT       (1U<<10U)
#define VMX_PROCBASED_CTLS_RDPMC       (1U<<11U)
#define VMX_PROCBASED_CTLS_RDTSC       (1U<<12U)
#define VMX_PROCBASED_CTLS_CR3_LOAD    (1U<<15U)
#define VMX_PROCBASED_CTLS_CR3_STORE   (1U<<16U)
#define VMX_PROCBASED_CTLS_CR8_LOAD    (1U<<19U)
#define VMX_PROCBASED_CTLS_CR8_STORE   (1U<<20U)
#define VMX_PROCBASED_CTLS_TPR_SHADOW  (1U<<21U)
#define VMX_PROCBASED_CTLS_NMI_WINEXIT (1U<<22U)
#define VMX_PROCBASED_CTLS_MOV_DR      (1U<<23U)
#define VMX_PROCBASED_CTLS_UNCOND_IO   (1U<<24U)
#define VMX_PROCBASED_CTLS_IO_BITMAP   (1U<<25U)
#define VMX_PROCBASED_CTLS_MON_TRAP    (1U<<27U)
#define VMX_PROCBASED_CTLS_MSR_BITMAP  (1U<<28U)
#define VMX_PROCBASED_CTLS_MONITOR     (1U<<29U)
#define VMX_PROCBASED_CTLS_PAUSE       (1U<<30U)
#define VMX_PROCBASED_CTLS_SECONDARY   (1U<<31U)
#define VMX_PROCBASED_CTLS2_VAPIC      (1U<<0U)
#define VMX_PROCBASED_CTLS2_EPT        (1U<<1U)
#define VMX_PROCBASED_CTLS2_DESC_TABLE (1U<<2U)
#define VMX_PROCBASED_CTLS2_RDTSCP     (1U<<3U)
#define VMX_PROCBASED_CTLS2_VX2APIC    (1U<<4U)
#define VMX_PROCBASED_CTLS2_VPID       (1U<<5U)
#define VMX_PROCBASED_CTLS2_WBINVD     (1U<<6U)
#define VMX_PROCBASED_CTLS2_UNRESTRICT (1U<<7U)
#define VMX_PROCBASED_CTLS2_VAPIC_REGS (1U<<8U)
#define VMX_PROCBASED_CTLS2_VIRQ       (1U<<9U)
#define VMX_PROCBASED_CTLS2_PAUSE_LOOP (1U<<10U)
#define VMX_PROCBASED_CTLS2_RDRAND     (1U<<11U)
#define VMX_PROCBASED_CTLS2_INVPCID    (1U<<12U)
#define VMX_PROCBASED_CTLS2_VM_FUNCS   (1U<<13U)
#define VMX_PROCBASED_CTLS2_VMCS_SHADW (1U<<14U)
#define VMX_PROCBASED_CTLS2_RDSEED     (1U<<16U)
#define VMX_PROCBASED_CTLS2_EPT_VE     (1U<<18U)
#define VMX_PROCBASED_CTLS2_XSVE_XRSTR (1U<<20U)

/* MSR_IA32_VMX_EPT_VPID_CAP: EPT and VPID capability bits */
#define VMX_EPT_EXECUTE_ONLY		(1U << 0U)
#define VMX_EPT_PAGE_WALK_4		(1U << 6U)
#define VMX_EPT_PAGE_WALK_5		(1U << 7U)
#define VMX_EPTP_UC			(1U << 8U)
#define VMX_EPTP_WB			(1U << 14U)
#define VMX_EPT_2MB_PAGE		(1U << 16U)
#define VMX_EPT_1GB_PAGE		(1U << 17U)
#define VMX_EPT_INVEPT  		(1U << 20U)
#define VMX_EPT_AD			(1U << 21U)
#define VMX_EPT_INVEPT_SINGLE_CONTEXT	(1U << 25U)
#define VMX_EPT_INVEPT_GLOBAL_CONTEXT	(1U << 26U)

#define VMX_MIN_NR_VPID 		1U
#define VMX_MAX_NR_VPID 		(1U << 5U)

#define VMX_VPID_TYPE_INDIVIDUAL_ADDR	0UL
#define VMX_VPID_TYPE_SINGLE_CONTEXT	1UL
#define VMX_VPID_TYPE_ALL_CONTEXT	2UL
#define VMX_VPID_TYPE_SINGLE_NON_GLOBAL	3UL

#define VMX_VPID_INVVPID			(1U << 0U) /* (32 - 32) */
#define VMX_VPID_INVVPID_INDIVIDUAL_ADDR	(1U << 8U) /* (40 - 32) */
#define VMX_VPID_INVVPID_SINGLE_CONTEXT 	(1U << 9U) /* (41 - 32) */
#define VMX_VPID_INVVPID_GLOBAL_CONTEXT 	(1U << 10U) /* (42 - 32) */
#define VMX_VPID_INVVPID_SINGLE_NON_GLOBAL	(1U << 11U) /* (43 - 32) */

#define VMX_EPT_MT_EPTE_SHIFT		3U
#define VMX_EPTP_PWL_MASK		0x38UL
#define VMX_EPTP_PWL_4  		0x18UL
#define VMX_EPTP_PWL_5  		0x20UL
#define VMX_EPTP_AD_ENABLE_BIT  	(1UL << 6U)
#define VMX_EPTP_MT_MASK		0x7UL
#define VMX_EPTP_MT_WB  		0x6UL
#define VMX_EPTP_MT_UC  		0x0UL

/* VMX exit control bits */
#define VMX_EXIT_CTLS_SAVE_DBG         (1U<<2U)
#define VMX_EXIT_CTLS_HOST_ADDR64      (1U<<9U)
#define VMX_EXIT_CTLS_LOAD_PERF        (1U<<12U)
#define VMX_EXIT_CTLS_ACK_IRQ          (1U<<15U)
#define VMX_EXIT_CTLS_SAVE_PAT         (1U<<18U)
#define VMX_EXIT_CTLS_LOAD_PAT         (1U<<19U)
#define VMX_EXIT_CTLS_SAVE_EFER        (1U<<20U)
#define VMX_EXIT_CTLS_LOAD_EFER        (1U<<21U)
#define VMX_EXIT_CTLS_SAVE_PTMR        (1U<<22U)

/* VMX entry control bits */
#define VMX_ENTRY_CTLS_LOAD_DBG        (1U<<2U)
#define VMX_ENTRY_CTLS_IA32E_MODE      (1U<<9U)
#define VMX_ENTRY_CTLS_ENTRY_SMM       (1U<<10U)
#define VMX_ENTRY_CTLS_DEACT_DUAL      (1U<<11U)
#define VMX_ENTRY_CTLS_LOAD_PERF       (1U<<13U)
#define VMX_ENTRY_CTLS_LOAD_PAT        (1U<<14U)
#define VMX_ENTRY_CTLS_LOAD_EFER       (1U<<15U)

/* VMX entry/exit Interrupt info */
#define VMX_INT_INFO_ERR_CODE_VALID	(1U<<11U)
#define VMX_INT_INFO_VALID		(1U<<31U)
#define VMX_INT_TYPE_MASK		(0x700U)
#define VMX_INT_TYPE_EXT_INT		0U
#define VMX_INT_TYPE_NMI		2U
#define VMX_INT_TYPE_HW_EXP		3U
#define VMX_INT_TYPE_SW_EXP		6U

#define VM_SUCCESS	0
#define VM_FAIL		-1

#define VMX_VMENTRY_FAIL	0x80000000U

#ifndef ASSEMBLER

static inline uint32_t vmx_eoi_exit(uint32_t vector)
{
	return (VMX_EOI_EXIT0_FULL + ((vector >> 6U) * 2U));
}

/* VM exit qulifications for APIC-access
 * Access type:
 *  0  = linear access for a data read during instruction execution
 *  1  = linear access for a data write during instruction execution
 *  2  = linear access for an instruction fetch
 *  3  = linear access (read or write) during event delivery
 *  10 = guest-physical access during event delivery
 *  15 = guest-physical access for an instructon fetch or during
 *       instruction execution
 */
static inline uint64_t apic_access_type(uint64_t qual)
{
	return ((qual >> 12U) & 0xFUL);
}

static inline uint64_t apic_access_offset(uint64_t qual)
{
	return (qual & 0xFFFUL);
}

#define RFLAGS_C (1U<<0U)
#define RFLAGS_Z (1U<<6U)
#define RFLAGS_AC (1U<<18U)

/* CR0 bits hv want to trap to track status change */
#define CR0_TRAP_MASK (CR0_PE | CR0_PG | CR0_WP | CR0_CD | CR0_NW )
#define CR0_RESERVED_MASK ~(CR0_PG | CR0_CD | CR0_NW | CR0_AM | CR0_WP | \
			   CR0_NE |  CR0_ET | CR0_TS | CR0_EM | CR0_MP | CR0_PE)

/* CR4 bits hv want to trap to track status change */
#define CR4_TRAP_MASK (CR4_PSE | CR4_PAE | CR4_VMXE | CR4_PCIDE)
#define	CR4_RESERVED_MASK ~(CR4_VME | CR4_PVI | CR4_TSD | CR4_DE | CR4_PSE | \
				CR4_PAE | CR4_MCE | CR4_PGE | CR4_PCE |	     \
				CR4_OSFXSR | CR4_PCIDE | CR4_OSXSAVE |       \
				CR4_SMEP | CR4_FSGSBASE | CR4_VMXE |         \
				CR4_OSXMMEXCPT | CR4_SMAP | CR4_PKE |        \
				CR4_SMXE | CR4_UMIP )

#define VMX_SUPPORT_UNRESTRICTED_GUEST (1U<<5U)

/* External Interfaces */
void exec_vmxon_instr(uint16_t pcpu_id);

/**
 * Read field from VMCS.
 *
 * Refer to Chapter 24, Vol. 3 in SDM for the width of VMCS fields.
 *
 * @return full contents in IA-32e mode for 64-bit fields.
 * @return the lower 32-bit outside IA-32e mode for 64-bit fields.
 * @return full contents for 32-bit fields, with higher 32-bit set to 0.
 */
uint16_t exec_vmread16(uint32_t field);
uint32_t exec_vmread32(uint32_t field);
uint64_t exec_vmread64(uint32_t field_full);
#define exec_vmread exec_vmread64

void exec_vmwrite16(uint32_t field, uint16_t value);
void exec_vmwrite32(uint32_t field, uint32_t value);
void exec_vmwrite64(uint32_t field_full, uint64_t value);
#define exec_vmwrite exec_vmwrite64

void init_vmcs(struct acrn_vcpu *vcpu);

void vmx_off(uint16_t pcpu_id);

void exec_vmclear(void *addr);
void exec_vmptrld(void *addr);

uint64_t vmx_rdmsr_pat(const struct acrn_vcpu *vcpu);
int vmx_wrmsr_pat(struct acrn_vcpu *vcpu, uint64_t value);

void vmx_write_cr0(struct acrn_vcpu *vcpu, uint64_t cr0);
void vmx_write_cr4(struct acrn_vcpu *vcpu, uint64_t cr4);
bool is_vmx_disabled(void);
void switch_apicv_mode_x2apic(struct acrn_vcpu *vcpu);

static inline enum vm_cpu_mode get_vcpu_mode(const struct acrn_vcpu *vcpu)
{
	return vcpu->arch.cpu_mode;
}

static inline bool cpu_has_vmx_unrestricted_guest_cap(void)
{
	return ((msr_read(MSR_IA32_VMX_MISC) & VMX_SUPPORT_UNRESTRICTED_GUEST)
									!= 0UL);
}

typedef struct _descriptor_table_{
	uint16_t limit;
	uint64_t base;
}__attribute__((packed)) descriptor_table;
#endif /* ASSEMBLER */

#endif /* VMX_H_ */
