/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MSR_H
#define MSR_H

/* architectural (common) MSRs */
#define MSR_IA32_P5_MC_ADDR                 0x00000000U
/* Machine check address for MC exception handler */
#define MSR_IA32_P5_MC_TYPE                 0x00000001U
/* Machine check error type for MC exception handler */
#define MSR_IA32_MONITOR_FILTER_SIZE        0x00000006U
/* System coherence line size for MWAIT/MONITOR */
#define MSR_IA32_TIME_STAMP_COUNTER         0x00000010U	/* TSC as MSR */
#define MSR_IA32_PLATFORM_ID                0x00000017U	/* Platform ID */
#define MSR_IA32_APIC_BASE                  0x0000001BU
/* Information about LAPIC */
#define MSR_IA32_FEATURE_CONTROL            0x0000003AU
/* Speculation Control */
#define MSR_IA32_SPEC_CTRL                  0x00000048U
/* Prediction Command */
#define MSR_IA32_PRED_CMD                   0x00000049U
/* Control Features in Intel 64 processor */
#define MSR_IA32_ADJUST_TSC                 0x0000003BU	/* Adjust TSC value */
#define MSR_IA32_BIOS_UPDT_TRIG             0x00000079U
/* BIOS update trigger */
#define MSR_IA32_BIOS_SIGN_ID               0x0000008BU
/* BIOS update signature */
#define MSR_IA32_SMM_MONITOR_CTL            0x0000009BU
/* SMM monitor configuration */
#define MSR_IA32_PMC0                       0x000000C1U
/* General performance counter 0 */
#define MSR_IA32_PMC1                       0x000000C2U
/* General performance counter 1 */
#define MSR_IA32_PMC2                       0x000000C3U
/* General performance counter 2 */
#define MSR_IA32_PMC3                       0x000000C4U
/* General performance counter 3 */
#define MSR_IA32_MPERF                      0x000000E7U
/* Max. qualified performance clock counter */
#define MSR_IA32_APERF                      0x000000E8U
/* Actual performance clock counter */
#define MSR_IA32_MTRR_CAP                   0x000000FEU	/* MTRR capability */
#define MSR_IA32_SYSENTER_CS                0x00000174U	/* CS for sysenter */
#define MSR_IA32_SYSENTER_ESP               0x00000175U	/* ESP for sysenter */
#define MSR_IA32_SYSENTER_EIP               0x00000176U	/* EIP for sysenter */
#define MSR_IA32_MCG_CAP                    0x00000179U
/* Global machine check capability */
#define MSR_IA32_MCG_STATUS                 0x0000017AU
/* Global machine check status */
#define MSR_IA32_MCG_CTL                    0x0000017BU
/* Global machine check  control */
#define MSR_IA32_PERFEVTSEL0                0x00000186U
/* Performance Event Select Register 0 */
#define MSR_IA32_PERFEVTSEL1                0x00000187U
/* Performance Event Select Register 1 */
#define MSR_IA32_PERFEVTSEL2                0x00000188U
/* Performance Event Select Register 2 */
#define MSR_IA32_PERFEVTSEL3                0x00000189U
/* Performance Event Select Register 3 */
#define MSR_IA32_PERF_STATUS                0x00000198U
/* Current performance state */
#define MSR_IA32_PERF_CTL                   0x00000199U
/* Performance control */
#define MSR_IA32_CLOCK_MODULATION           0x0000019AU
/* Clock modulation control */
#define MSR_IA32_THERM_INTERRUPT            0x0000019BU
/* Thermal interrupt control */
#define MSR_IA32_THERM_STATUS               0x0000019CU
/* Thermal status information */
#define MSR_IA32_MISC_ENABLE                0x000001A0U
/* Enable misc. processor features */
#define MSR_IA32_ENERGY_PERF_BIAS           0x000001B0U
/* Performance energy bias hint */
#define MSR_IA32_DEBUGCTL                   0x000001D9U
/* Trace/Profile resource control */
#define MSR_IA32_SMRR_PHYSBASE              0x000001F2U	/* SMRR base address */
#define MSR_IA32_SMRR_PHYSMASK              0x000001F3U	/* SMRR range mask */
#define MSR_IA32_PLATFORM_DCA_CAP           0x000001F8U	/* DCA capability */
#define MSR_IA32_CPU_DCA_CAP                0x000001F9U
/* Prefetch hint type capability */
#define MSR_IA32_DCA_0_CAP                  0x000001FAU
/* DCA type 0 status/control */
#define MSR_IA32_MTRR_PHYSBASE_0            0x00000200U
/* variable range MTRR base 0 */
#define MSR_IA32_MTRR_PHYSMASK_0            0x00000201U
/* variable range MTRR mask 0 */
#define MSR_IA32_MTRR_PHYSBASE_1            0x00000202U
/* variable range MTRR base 1 */
#define MSR_IA32_MTRR_PHYSMASK_1            0x00000203U
/* variable range MTRR mask 1 */
#define MSR_IA32_MTRR_PHYSBASE_2            0x00000204U
/* variable range MTRR base 2 */
#define MSR_IA32_MTRR_PHYSMASK_2            0x00000205U
/* variable range MTRR mask 2 */
#define MSR_IA32_MTRR_PHYSBASE_3            0x00000206U
/* variable range MTRR base 3 */
#define MSR_IA32_MTRR_PHYSMASK_3            0x00000207U
/* variable range MTRR mask 3 */
#define MSR_IA32_MTRR_PHYSBASE_4            0x00000208U
/* variable range MTRR base 4 */
#define MSR_IA32_MTRR_PHYSMASK_4            0x00000209U
/* variable range MTRR mask 4 */
#define MSR_IA32_MTRR_PHYSBASE_5            0x0000020AU
/* variable range MTRR base 5 */
#define MSR_IA32_MTRR_PHYSMASK_5            0x0000020BU
/* variable range MTRR mask 5 */
#define MSR_IA32_MTRR_PHYSBASE_6            0x0000020CU
/* variable range MTRR base 6 */
#define MSR_IA32_MTRR_PHYSMASK_6            0x0000020DU
/* variable range MTRR mask 6 */
#define MSR_IA32_MTRR_PHYSBASE_7            0x0000020EU
/* variable range MTRR base 7 */
#define MSR_IA32_MTRR_PHYSMASK_7            0x0000020FU
/* variable range MTRR mask 7 */
#define MSR_IA32_MTRR_PHYSBASE_8            0x00000210U
/* variable range MTRR base 8 */
#define MSR_IA32_MTRR_PHYSMASK_8            0x00000211U
/* variable range MTRR mask 8 */
#define MSR_IA32_MTRR_PHYSBASE_9            0x00000212U
/* variable range MTRR base 9 */
#define MSR_IA32_MTRR_PHYSMASK_9            0x00000213U
/* variable range MTRR mask 9 */
#define MSR_IA32_MTRR_FIX64K_00000          0x00000250U
/* fixed range MTRR 16K/0x00000 */
#define MSR_IA32_MTRR_FIX16K_80000          0x00000258U
/* fixed range MTRR 16K/0x80000 */
#define MSR_IA32_MTRR_FIX16K_A0000          0x00000259U
/* fixed range MTRR 16K/0xA0000 */
#define MSR_IA32_MTRR_FIX4K_C0000           0x00000268U
/* fixed range MTRR 4K/0xC0000 */
#define MSR_IA32_MTRR_FIX4K_C8000           0x00000269U
/* fixed range MTRR 4K/0xC8000 */
#define MSR_IA32_MTRR_FIX4K_D0000           0x0000026AU
/* fixed range MTRR 4K/0xD0000 */
#define MSR_IA32_MTRR_FIX4K_D8000           0x0000026BU
/* fixed range MTRR 4K/0xD8000 */
#define MSR_IA32_MTRR_FIX4K_E0000           0x0000026CU
/* fixed range MTRR 4K/0xE0000 */
#define MSR_IA32_MTRR_FIX4K_E8000           0x0000026DU
/* fixed range MTRR 4K/0xE8000 */
#define MSR_IA32_MTRR_FIX4K_F0000           0x0000026EU
/* fixed range MTRR 4K/0xF0000 */
#define MSR_IA32_MTRR_FIX4K_F8000           0x0000026FU
/* fixed range MTRR 4K/0xF8000 */
#define MSR_IA32_PAT                        0x00000277U	/* PAT */
#define MSR_IA32_MC0_CTL2                   0x00000280U
/* Corrected error count threshold 0 */
#define MSR_IA32_MC1_CTL2                   0x00000281U
/* Corrected error count threshold 1 */
#define MSR_IA32_MC2_CTL2                   0x00000282U
/* Corrected error count threshold 2 */
#define MSR_IA32_MC3_CTL2                   0x00000283U
/* Corrected error count threshold 3 */
#define MSR_IA32_MC4_CTL2                   0x00000284U
/* Corrected error count threshold 4 */
#define MSR_IA32_MC5_CTL2                   0x00000285U
/* Corrected error count threshold 5 */
#define MSR_IA32_MC6_CTL2                   0x00000286U
/* Corrected error count threshold 6 */
#define MSR_IA32_MC7_CTL2                   0x00000287U
/* Corrected error count threshold 7 */
#define MSR_IA32_MC8_CTL2                   0x00000288U
/* Corrected error count threshold 8 */
#define MSR_IA32_MC9_CTL2                   0x00000289U
/* Corrected error count threshold 9 */
#define MSR_IA32_MC10_CTL2                  0x0000028AU
/* Corrected error count threshold 10 */
#define MSR_IA32_MC11_CTL2                  0x0000028BU
/* Corrected error count threshold 11 */
#define MSR_IA32_MC12_CTL2                  0x0000028CU
/* Corrected error count threshold 12 */
#define MSR_IA32_MC13_CTL2                  0x0000028DU
/* Corrected error count threshold 13 */
#define MSR_IA32_MC14_CTL2                  0x0000028EU
/* Corrected error count threshold 14 */
#define MSR_IA32_MC15_CTL2                  0x0000028FU
/* Corrected error count threshold 15 */
#define MSR_IA32_MC16_CTL2                  0x00000290U
/* Corrected error count threshold 16 */
#define MSR_IA32_MC17_CTL2                  0x00000291U
/* Corrected error count threshold 17 */
#define MSR_IA32_MC18_CTL2                  0x00000292U
/* Corrected error count threshold 18 */
#define MSR_IA32_MC19_CTL2                  0x00000293U
/* Corrected error count threshold 19 */
#define MSR_IA32_MC20_CTL2                  0x00000294U
/* Corrected error count threshold 20 */
#define MSR_IA32_MC21_CTL2                  0x00000295U
/* Corrected error count threshold 21 */
#define MSR_IA32_MTRR_DEF_TYPE              0x000002FFU
/* Default memory type/MTRR control */
#define MSR_IA32_FIXED_CTR0                 0x00000309U
/* Fixed-function performance counter 0 */
#define MSR_IA32_FIXED_CTR1                 0x0000030AU
/* Fixed-function performance counter 1 */
#define MSR_IA32_FIXED_CTR2                 0x0000030BU
/* Fixed-function performance counter 2 */
#define MSR_IA32_PERF_CAPABILITIES          0x00000345U
/* Performance capability */
#define MSR_IA32_FIXED_CTR_CTL              0x0000038DU
/* Fixed-function performance counter control */
#define MSR_IA32_PERF_GLOBAL_STATUS         0x0000038EU
/* Global performance counter status */
#define MSR_IA32_PERF_GLOBAL_CTRL           0x0000038FU
/* Global performance counter control */
#define MSR_IA32_PERF_GLOBAL_OVF_CTRL       0x00000390U
/* Global performance counter overflow control */
#define MSR_IA32_PEBS_ENABLE                0x000003F1U    /* PEBS control */
#define MSR_IA32_MC0_CTL                    0x00000400U    /* MC 0 control */
#define MSR_IA32_MC0_STATUS                 0x00000401U    /* MC 0 status */
#define MSR_IA32_MC0_ADDR                   0x00000402U    /* MC 0 address */
#define MSR_IA32_MC0_MISC                   0x00000403U    /* MC 0 misc. */
#define MSR_IA32_MC1_CTL                    0x00000404U    /* MC 1 control */
#define MSR_IA32_MC1_STATUS                 0x00000405U    /* MC 1 status */
#define MSR_IA32_MC1_ADDR                   0x00000406U    /* MC 1 address */
#define MSR_IA32_MC1_MISC                   0x00000407U    /* MC 1 misc. */
#define MSR_IA32_MC2_CTL                    0x00000408U    /* MC 2 control */
#define MSR_IA32_MC2_STATUS                 0x00000409U    /* MC 2 status */
#define MSR_IA32_MC2_ADDR                   0x0000040AU    /* MC 2 address */
#define MSR_IA32_MC2_MISC                   0x0000040BU    /* MC 2 misc. */
#define MSR_IA32_MC3_CTL                    0x0000040CU    /* MC 3 control */
#define MSR_IA32_MC3_STATUS                 0x0000040DU    /* MC 3 status */
#define MSR_IA32_MC3_ADDR                   0x0000040EU    /* MC 3 address */
#define MSR_IA32_MC3_MISC                   0x0000040FU    /* MC 3 misc. */
#define MSR_IA32_MC4_CTL                    0x00000410U    /* MC 4 control */
#define MSR_IA32_MC4_STATUS                 0x00000411U    /* MC 4 status */
#define MSR_IA32_MC4_ADDR                   0x00000412U    /* MC 4 address */
#define MSR_IA32_MC4_MISC                   0x00000413U    /* MC 4 misc. */
#define MSR_IA32_MC5_CTL                    0x00000414U    /* MC 5 control */
#define MSR_IA32_MC5_STATUS                 0x00000415U    /* MC 5 status */
#define MSR_IA32_MC5_ADDR                   0x00000416U    /* MC 5 address */
#define MSR_IA32_MC5_MISC                   0x00000417U    /* MC 5 misc. */
#define MSR_IA32_MC6_CTL                    0x00000418U    /* MC 6 control */
#define MSR_IA32_MC6_STATUS                 0x00000419U    /* MC 6 status */
#define MSR_IA32_MC6_ADDR                   0x0000041AU    /* MC 6 address */
#define MSR_IA32_MC6_MISC                   0x0000041BU    /* MC 6 misc. */
#define MSR_IA32_MC7_CTL                    0x0000041CU    /* MC 7 control */
#define MSR_IA32_MC7_STATUS                 0x0000041DU    /* MC 7 status */
#define MSR_IA32_MC7_ADDR                   0x0000041EU    /* MC 7 address */
#define MSR_IA32_MC7_MISC                   0x0000041FU    /* MC 7 misc. */
#define MSR_IA32_MC8_CTL                    0x00000420U    /* MC 8 control */
#define MSR_IA32_MC8_STATUS                 0x00000421U    /* MC 8 status */
#define MSR_IA32_MC8_ADDR                   0x00000422U    /* MC 8 address */
#define MSR_IA32_MC8_MISC                   0x00000423U    /* MC 8 misc. */
#define MSR_IA32_MC9_CTL                    0x00000424U    /* MC 9 control */
#define MSR_IA32_MC9_STATUS                 0x00000425U    /* MC 9 status */
#define MSR_IA32_MC9_ADDR                   0x00000426U    /* MC 9 address */
#define MSR_IA32_MC9_MISC                   0x00000427U    /* MC 9 misc. */
#define MSR_IA32_MC10_CTL                   0x00000428U    /* MC 10 control */
#define MSR_IA32_MC10_STATUS                0x00000429U    /* MC 10 status */
#define MSR_IA32_MC10_ADDR                  0x0000042AU    /* MC 10 address */
#define MSR_IA32_MC10_MISC                  0x0000042BU    /* MC 10 misc. */
#define MSR_IA32_MC11_CTL                   0x0000042CU    /* MC 11 control */
#define MSR_IA32_MC11_STATUS                0x0000042DU    /* MC 11 status */
#define MSR_IA32_MC11_ADDR                  0x0000042EU    /* MC 11 address */
#define MSR_IA32_MC11_MISC                  0x0000042FU    /* MC 11 misc. */
#define MSR_IA32_MC12_CTL                   0x00000430U    /* MC 12 control */
#define MSR_IA32_MC12_STATUS                0x00000431U    /* MC 12 status */
#define MSR_IA32_MC12_ADDR                  0x00000432U    /* MC 12 address */
#define MSR_IA32_MC12_MISC                  0x00000433U    /* MC 12 misc. */
#define MSR_IA32_MC13_CTL                   0x00000434U    /* MC 13 control */
#define MSR_IA32_MC13_STATUS                0x00000435U    /* MC 13 status */
#define MSR_IA32_MC13_ADDR                  0x00000436U    /* MC 13 address */
#define MSR_IA32_MC13_MISC                  0x00000437U    /* MC 13 misc. */
#define MSR_IA32_MC14_CTL                   0x00000438U    /* MC 14 control */
#define MSR_IA32_MC14_STATUS                0x00000439U    /* MC 14 status */
#define MSR_IA32_MC14_ADDR                  0x0000043AU    /* MC 14 address */
#define MSR_IA32_MC14_MISC                  0x0000043BU    /* MC 14 misc. */
#define MSR_IA32_MC15_CTL                   0x0000043CU    /* MC 15 control */
#define MSR_IA32_MC15_STATUS                0x0000043DU    /* MC 15 status */
#define MSR_IA32_MC15_ADDR                  0x0000043EU    /* MC 15 address */
#define MSR_IA32_MC15_MISC                  0x0000043FU    /* MC 15 misc. */
#define MSR_IA32_MC16_CTL                   0x00000440U    /* MC 16 control */
#define MSR_IA32_MC16_STATUS                0x00000441U    /* MC 16 status */
#define MSR_IA32_MC16_ADDR                  0x00000442U    /* MC 16 address */
#define MSR_IA32_MC16_MISC                  0x00000443U    /* MC 16 misc. */
#define MSR_IA32_MC17_CTL                   0x00000444U    /* MC 17 control */
#define MSR_IA32_MC17_STATUS                0x00000445U    /* MC 17 status */
#define MSR_IA32_MC17_ADDR                  0x00000446U    /* MC 17 address */
#define MSR_IA32_MC17_MISC                  0x00000447U    /* MC 17 misc. */
#define MSR_IA32_MC18_CTL                   0x00000448U    /* MC 18 control */
#define MSR_IA32_MC18_STATUS                0x00000449U    /* MC 18 status */
#define MSR_IA32_MC18_ADDR                  0x0000044AU    /* MC 18 address */
#define MSR_IA32_MC18_MISC                  0x0000044BU    /* MC 18 misc. */
#define MSR_IA32_MC19_CTL                   0x0000044CU    /* MC 19 control */
#define MSR_IA32_MC19_STATUS                0x0000044DU    /* MC 19 status */
#define MSR_IA32_MC19_ADDR                  0x0000044EU    /* MC 19 address */
#define MSR_IA32_MC19_MISC                  0x0000044FU    /* MC 19 misc. */
#define MSR_IA32_MC20_CTL                   0x00000450U    /* MC 20 control */
#define MSR_IA32_MC20_STATUS                0x00000451U    /* MC 20 status */
#define MSR_IA32_MC20_ADDR                  0x00000452U    /* MC 20 address */
#define MSR_IA32_MC20_MISC                  0x00000453U    /* MC 20 misc. */
#define MSR_IA32_MC21_CTL                   0x00000454U    /* MC 21 control */
#define MSR_IA32_MC21_STATUS                0x00000455U    /* MC 21 status */
#define MSR_IA32_MC21_ADDR                  0x00000456U    /* MC 21 address */
#define MSR_IA32_MC21_MISC                  0x00000457U    /* MC 21 misc. */
#define MSR_IA32_VMX_BASIC                  0x00000480U
/* Capability reporting register basic VMX capabilities */
#define MSR_IA32_VMX_PINBASED_CTLS          0x00000481U
/* Capability reporting register pin based VM execution controls */
#define MSR_IA32_VMX_PROCBASED_CTLS         0x00000482U
/* Capability reporting register primary processor based VM execution controls*/
#define MSR_IA32_VMX_EXIT_CTLS              0x00000483U
/* Capability reporting register VM exit controls */
#define MSR_IA32_VMX_ENTRY_CTLS             0x00000484U
/* Capability reporting register VM entry controls */
#define MSR_IA32_VMX_MISC                   0x00000485U
/* Reporting register misc. VMX capabilities */
#define MSR_IA32_VMX_CR0_FIXED0             0x00000486U
/* Capability reporting register of CR0 bits fixed to 0 */
#define MSR_IA32_VMX_CR0_FIXED1             0x00000487U
/* Capability reporting register of CR0 bits fixed to 1 */
#define MSR_IA32_VMX_CR4_FIXED0             0x00000488U
/* Capability reporting register of CR4 bits fixed to 0 */
#define MSR_IA32_VMX_CR4_FIXED1            0x00000489U
/* Capability reporting register of CR4 bits fixed to 1 */
#define MSR_IA32_VMX_VMCS_ENUM             0x0000048AU
/* Capability reporting register of VMCS field enumeration */
#define MSR_IA32_VMX_PROCBASED_CTLS2        0x0000048BU
/* Capability reporting register of secondary processor based VM execution
 * controls
 */
#define MSR_IA32_VMX_EPT_VPID_CAP           0x0000048CU
/* Capability reporting register of EPT and VPID */
#define MSR_IA32_VMX_TRUE_PINBASED_CTLS     0x0000048DU
/* Capability reporting register of pin based VM execution flex controls */
#define MSR_IA32_VMX_TRUE_PROCBASED_CTLS    0x0000048EU
/* Capability reporting register of primary processor based VM execution flex
 * controls
 */
#define MSR_IA32_VMX_TRUE_EXIT_CTLS         0x0000048FU
/* Capability reporting register of VM exit flex controls */
#define MSR_IA32_VMX_TRUE_ENTRY_CTLS        0x00000490U
/* Capability reporting register of VM entry flex controls */
#define MSR_IA32_DS_AREA                   0x00000600U   /* DS save area */
/* APIC TSC deadline MSR */
#define MSR_IA32_TSC_DEADLINE               0x000006E0U
#define MSR_IA32_EXT_XAPICID                0x00000802U	/* x2APIC ID */
#define MSR_IA32_EXT_APIC_VERSION           0x00000803U	/* x2APIC version */
#define MSR_IA32_EXT_APIC_TPR               0x00000808U
/* x2APIC task priority */
#define MSR_IA32_EXT_APIC_PPR               0x0000080AU
/* x2APIC processor priority */
#define MSR_IA32_EXT_APIC_EOI               0x0000080BU	/* x2APIC EOI */
#define MSR_IA32_EXT_APIC_LDR               0x0000080DU
/* x2APIC logical destination */
#define MSR_IA32_EXT_APIC_SIVR              0x0000080FU
/* x2APIC spurious interrupt vector */
#define MSR_IA32_EXT_APIC_ISR0              0x00000810U
/* x2APIC in-service register 0 */
#define MSR_IA32_EXT_APIC_ISR1              0x00000811U
/* x2APIC in-service register 1 */
#define MSR_IA32_EXT_APIC_ISR2              0x00000812U
/* x2APIC in-service register 2 */
#define MSR_IA32_EXT_APIC_ISR3              0x00000813U
/* x2APIC in-service register 3 */
#define MSR_IA32_EXT_APIC_ISR4              0x00000814U
/* x2APIC in-service register 4 */
#define MSR_IA32_EXT_APIC_ISR5              0x00000815U
/* x2APIC in-service register 5 */
#define MSR_IA32_EXT_APIC_ISR6              0x00000816U
/* x2APIC in-service register 6 */
#define MSR_IA32_EXT_APIC_ISR7              0x00000817U
/* x2APIC in-service register 7 */
#define MSR_IA32_EXT_APIC_TMR0              0x00000818U
/* x2APIC trigger mode register 0 */
#define MSR_IA32_EXT_APIC_TMR1              0x00000819U
/* x2APIC trigger mode register 1 */
#define MSR_IA32_EXT_APIC_TMR2              0x0000081AU
/* x2APIC trigger mode register 2 */
#define MSR_IA32_EXT_APIC_TMR3              0x0000081BU
/* x2APIC trigger mode register 3 */
#define MSR_IA32_EXT_APIC_TMR4              0x0000081CU
/* x2APIC trigger mode register 4 */
#define MSR_IA32_EXT_APIC_TMR5              0x0000081DU
/* x2APIC trigger mode register 5 */
#define MSR_IA32_EXT_APIC_TMR6              0x0000081EU
/* x2APIC trigger mode register 6 */
#define MSR_IA32_EXT_APIC_TMR7              0x0000081FU
/* x2APIC trigger mode register 7 */
#define MSR_IA32_EXT_APIC_IRR0              0x00000820U
/* x2APIC interrupt request register 0 */
#define MSR_IA32_EXT_APIC_IRR1              0x00000821U
/* x2APIC interrupt request register 1 */
#define MSR_IA32_EXT_APIC_IRR2              0x00000822U
/* x2APIC interrupt request register 2 */
#define MSR_IA32_EXT_APIC_IRR3              0x00000823U
/* x2APIC interrupt request register 3 */
#define MSR_IA32_EXT_APIC_IRR4              0x00000824U
/* x2APIC interrupt request register 4 */
#define MSR_IA32_EXT_APIC_IRR5              0x00000825U
/* x2APIC interrupt request register 5 */
#define MSR_IA32_EXT_APIC_IRR6              0x00000826U
/* x2APIC interrupt request register 6 */
#define MSR_IA32_EXT_APIC_IRR7              0x00000827U
/* x2APIC interrupt request register 7 */
#define MSR_IA32_EXT_APIC_ESR               0x00000828U
/* x2APIC error status */
#define MSR_IA32_EXT_APIC_LVT_CMCI          0x0000082FU
/* x2APIC LVT corrected machine check interrupt register */
#define MSR_IA32_EXT_APIC_ICR               0x00000830U
/* x2APIC interrupt command register */
#define MSR_IA32_EXT_APIC_LVT_TIMER         0x00000832U
/* x2APIC LVT timer interrupt register */
#define MSR_IA32_EXT_APIC_LVT_THERMAL       0x00000833U
/* x2APIC LVT thermal sensor interrupt register */
#define MSR_IA32_EXT_APIC_LVT_PMI           0x00000834U
/* x2APIC LVT performance monitor interrupt register */
#define MSR_IA32_EXT_APIC_LVT_LINT0         0x00000835U
/* x2APIC LVT LINT0 register */
#define MSR_IA32_EXT_APIC_LVT_LINT1         0x00000836U
/* x2APIC LVT LINT1 register */
#define MSR_IA32_EXT_APIC_LVT_ERROR         0x00000837U
/* x2APIC LVT error register */
#define MSR_IA32_EXT_APIC_INIT_COUNT        0x00000838U
/* x2APIC initial count register */
#define MSR_IA32_EXT_APIC_CUR_COUNT         0x00000839U
/* x2APIC current count  register */
#define MSR_IA32_EXT_APIC_DIV_CONF          0x0000083EU
/* x2APIC divide configuration register */
#define MSR_IA32_EXT_APIC_SELF_IPI          0x0000083FU
/* x2APIC self IPI register */
#define MSR_IA32_EFER                       0xC0000080U
/* Extended feature enables */
#define MSR_IA32_STAR                       0xC0000081U
/* System call target address */
#define MSR_IA32_LSTAR                      0xC0000082U
/* IA-32e mode system call target address */
#define MSR_IA32_FMASK                      0xC0000084U
/* System call flag mask */
#define MSR_IA32_FS_BASE                    0xC0000100U
/* Map of BASE address of FS */
#define MSR_IA32_GS_BASE                    0xC0000101U
/* Map of BASE address of GS */
#define MSR_IA32_KERNEL_GS_BASE             0xC0000102U
/* Swap target of BASE address of GS */
#define MSR_IA32_TSC_AUX                    0xC0000103U    /* Auxiliary TSC */

/* ATOM specific MSRs */
#define MSR_ATOM_EBL_CR_POWERON             0x0000002AU
/* Processor hard power-on configuration */
#define MSR_ATOM_LASTBRANCH_0_FROM_IP       0x00000040U
/* Last branch record 0 from IP */
#define MSR_ATOM_LASTBRANCH_1_FROM_IP       0x00000041U
/* Last branch record 1 from IP */
#define MSR_ATOM_LASTBRANCH_2_FROM_IP       0x00000042U
/* Last branch record 2 from IP */
#define MSR_ATOM_LASTBRANCH_3_FROM_IP       0x00000043U
/* Last branch record 3 from IP */
#define MSR_ATOM_LASTBRANCH_4_FROM_IP       0x00000044U
/* Last branch record 4 from IP */
#define MSR_ATOM_LASTBRANCH_5_FROM_IP       0x00000045U
/* Last branch record 5 from IP */
#define MSR_ATOM_LASTBRANCH_6_FROM_IP       0x00000046U
/* Last branch record 6 from IP */
#define MSR_ATOM_LASTBRANCH_7_FROM_IP       0x00000047U
/* Last branch record 7 from IP */
#define MSR_ATOM_LASTBRANCH_0_TO_LIP        0x00000060U
/* Last branch record 0 to IP */
#define MSR_ATOM_LASTBRANCH_1_TO_LIP        0x00000061U
/* Last branch record 1 to IP */
#define MSR_ATOM_LASTBRANCH_2_TO_LIP        0x00000062U
/* Last branch record 2 to IP */
#define MSR_ATOM_LASTBRANCH_3_TO_LIP        0x00000063U
/* Last branch record 3 to IP */
#define MSR_ATOM_LASTBRANCH_4_TO_LIP        0x00000064U
/* Last branch record 4 to IP */
#define MSR_ATOM_LASTBRANCH_5_TO_LIP        0x00000065U
/* Last branch record 5 to IP */
#define MSR_ATOM_LASTBRANCH_6_TO_LIP        0x00000066U
/* Last branch record 6 to IP */
#define MSR_ATOM_LASTBRANCH_7_TO_LIP        0x00000067U
/* Last branch record 7 to IP */
#define MSR_ATOM_FSB_FREQ                   0x000000CDU    /* Scalable bus speed */
#define MSR_PLATFORM_INFO                   0x000000CEU
/* Maximum resolved bus ratio */
#define MSR_ATOM_BBL_CR_CTL3                0x0000011EU    /* L2 hardware enabled */
#define MSR_ATOM_THERM2_CTL                 0x0000019DU
/* Mode of automatic thermal monitor */
#define MSR_ATOM_LASTBRANCH_TOS             0x000001C9U
/* Last branch record stack TOS */
#define MSR_ATOM_LER_FROM_LIP               0x000001DDU
/* Last exception record from linear IP */
#define MSR_ATOM_LER_TO_LIP                 0x000001DEU
/* Last exception record to linear IP */

/* LINCROFT specific MSRs */
#define MSR_LNC_BIOS_CACHE_AS_RAM           0x000002E0U    /* Configure CAR */

/* EFER bits */
#define MSR_IA32_EFER_SCE_BIT                   (1U<<0)
#define MSR_IA32_EFER_LME_BIT                   (1U<<8)    /* IA32e mode enable */
#define MSR_IA32_EFER_LMA_BIT                   (1U<<10)   /* IA32e mode active */
#define MSR_IA32_EFER_NXE_BIT                   (1U<<11)

/* FEATURE CONTROL bits */
#define MSR_IA32_FEATURE_CONTROL_LOCK           (1U<<0)
#define MSR_IA32_FEATURE_CONTROL_VMX_SMX        (1U<<1)
#define MSR_IA32_FEATURE_CONTROL_VMX_NO_SMX     (1U<<2)

/* PAT memory type definitions */
#define PAT_MEM_TYPE_UC                     0x00U	/* uncached */
#define PAT_MEM_TYPE_WC                     0x01U	/* write combining */
#define PAT_MEM_TYPE_WT                     0x04U	/* write through */
#define PAT_MEM_TYPE_WP                     0x05U	/* write protected */
#define PAT_MEM_TYPE_WB                     0x06U	/* writeback */
#define PAT_MEM_TYPE_UCM                    0x07U	/* uncached minus */

/* MTRR memory type definitions */
#define MTRR_MEM_TYPE_UC             0x00U	/* uncached */
#define MTRR_MEM_TYPE_WC             0x01U	/* write combining */
#define MTRR_MEM_TYPE_WT             0x04U	/* write through */
#define MTRR_MEM_TYPE_WP             0x05U	/* write protected */
#define MTRR_MEM_TYPE_WB             0x06U	/* writeback */

/* misc. MTRR flag definitions */
#define MTRR_ENABLE                  0x800U	/* MTRR enable */
#define MTRR_FIX_ENABLE              0x400U	/* fixed range MTRR enable */
#define MTRR_VALID                   0x800U	/* MTRR setting is  valid */

/* SPEC & PRED bit */
#define SPEC_ENABLE_IBRS		(1U<<0)
#define SPEC_ENABLE_STIBP		(1U<<1)
#define PRED_SET_IBPB			(1U<<0)

#endif /* MSR_H */
