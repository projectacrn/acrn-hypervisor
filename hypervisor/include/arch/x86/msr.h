/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MSR_H
#define MSR_H

/* architectural (common) MSRs */
#define MSR_IA32_P5_MC_ADDR                 0x00000000
/* Machine check address for MC exception handler */
#define MSR_IA32_P5_MC_TYPE                 0x00000001
/* Machine check error type for MC exception handler */
#define MSR_IA32_MONITOR_FILTER_SIZE        0x00000006
/* System coherence line size for MWAIT/MONITOR */
#define MSR_IA32_TIME_STAMP_COUNTER         0x00000010	/* TSC as MSR */
#define MSR_IA32_PLATFORM_ID                0x00000017	/* Platform ID */
#define MSR_IA32_APIC_BASE                  0x0000001B
/* Information about LAPIC */
#define MSR_IA32_FEATURE_CONTROL            0x0000003A
/* Speculation Control */
#define MSR_IA32_SPEC_CTRL                  0x00000048
/* Prediction Command */
#define MSR_IA32_PRED_CMD                   0x00000049
/* Control Features in Intel 64 processor */
#define MSR_IA32_ADJUST_TSC                 0x0000003B	/* Adjust TSC value */
#define MSR_IA32_BIOS_UPDT_TRIG             0x00000079
/* BIOS update trigger */
#define MSR_IA32_BIOS_SIGN_ID               0x0000008B
/* BIOS update signature */
#define MSR_IA32_SMM_MONITOR_CTL            0x0000009B
/* SMM monitor configuration */
#define MSR_IA32_PMC0                       0x000000C1
/* General performance counter 0 */
#define MSR_IA32_PMC1                       0x000000C2
/* General performance counter 1 */
#define MSR_IA32_PMC2                       0x000000C3
/* General performance counter 2 */
#define MSR_IA32_PMC3                       0x000000C4
/* General performance counter 3 */
#define MSR_IA32_MPERF                      0x000000E7
/* Max. qualified performance clock counter */
#define MSR_IA32_APERF                      0x000000E8
/* Actual performance clock counter */
#define MSR_IA32_MTRR_CAP                   0x000000FE	/* MTRR capability */
#define MSR_IA32_SYSENTER_CS                0x00000174	/* CS for sysenter */
#define MSR_IA32_SYSENTER_ESP               0x00000175	/* ESP for sysenter */
#define MSR_IA32_SYSENTER_EIP               0x00000176	/* EIP for sysenter */
#define MSR_IA32_MCG_CAP                    0x00000179
/* Global machine check capability */
#define MSR_IA32_MCG_STATUS                 0x0000017A
/* Global machine check status */
#define MSR_IA32_MCG_CTL                    0x0000017B
/* Global machine check  control */
#define MSR_IA32_PERFEVTSEL0                0x00000186
/* Performance Event Select Register 0 */
#define MSR_IA32_PERFEVTSEL1                0x00000187
/* Performance Event Select Register 1 */
#define MSR_IA32_PERFEVTSEL2                0x00000188
/* Performance Event Select Register 2 */
#define MSR_IA32_PERFEVTSEL3                0x00000189
/* Performance Event Select Register 3 */
#define MSR_IA32_PERF_STATUS                0x00000198
/* Current performance state */
#define MSR_IA32_PERF_CTL                   0x00000199
/* Performance control */
#define MSR_IA32_CLOCK_MODULATION           0x0000019A
/* Clock modulation control */
#define MSR_IA32_THERM_INTERRUPT            0x0000019B
/* Thermal interrupt control */
#define MSR_IA32_THERM_STATUS               0x0000019C
/* Thermal status information */
#define MSR_IA32_MISC_ENABLE                0x000001A0
/* Enable misc. processor features */
#define MSR_IA32_ENERGY_PERF_BIAS           0x000001B0
/* Performance energy bias hint */
#define MSR_IA32_DEBUGCTL                   0x000001D9
/* Trace/Profile resource control */
#define MSR_IA32_SMRR_PHYSBASE              0x000001F2	/* SMRR base address */
#define MSR_IA32_SMRR_PHYSMASK              0x000001F3	/* SMRR range mask */
#define MSR_IA32_PLATFORM_DCA_CAP           0x000001F8	/* DCA capability */
#define MSR_IA32_CPU_DCA_CAP                0x000001F9
/* Prefetch hint type capability */
#define MSR_IA32_DCA_0_CAP                  0x000001FA
/* DCA type 0 status/control */
#define MSR_IA32_MTRR_PHYSBASE_0            0x00000200
/* variable range MTRR base 0 */
#define MSR_IA32_MTRR_PHYSMASK_0            0x00000201
/* variable range MTRR mask 0 */
#define MSR_IA32_MTRR_PHYSBASE_1            0x00000202
/* variable range MTRR base 1 */
#define MSR_IA32_MTRR_PHYSMASK_1            0x00000203
/* variable range MTRR mask 1 */
#define MSR_IA32_MTRR_PHYSBASE_2            0x00000204
/* variable range MTRR base 2 */
#define MSR_IA32_MTRR_PHYSMASK_2            0x00000205
/* variable range MTRR mask 2 */
#define MSR_IA32_MTRR_PHYSBASE_3            0x00000206
/* variable range MTRR base 3 */
#define MSR_IA32_MTRR_PHYSMASK_3            0x00000207
/* variable range MTRR mask 3 */
#define MSR_IA32_MTRR_PHYSBASE_4            0x00000208
/* variable range MTRR base 4 */
#define MSR_IA32_MTRR_PHYSMASK_4            0x00000209
/* variable range MTRR mask 4 */
#define MSR_IA32_MTRR_PHYSBASE_5            0x0000020A
/* variable range MTRR base 5 */
#define MSR_IA32_MTRR_PHYSMASK_5            0x0000020B
/* variable range MTRR mask 5 */
#define MSR_IA32_MTRR_PHYSBASE_6            0x0000020C
/* variable range MTRR base 6 */
#define MSR_IA32_MTRR_PHYSMASK_6            0x0000020D
/* variable range MTRR mask 6 */
#define MSR_IA32_MTRR_PHYSBASE_7            0x0000020E
/* variable range MTRR base 7 */
#define MSR_IA32_MTRR_PHYSMASK_7            0x0000020F
/* variable range MTRR mask 7 */
#define MSR_IA32_MTRR_PHYSBASE_8            0x00000210
/* variable range MTRR base 8 */
#define MSR_IA32_MTRR_PHYSMASK_8            0x00000211
/* variable range MTRR mask 8 */
#define MSR_IA32_MTRR_PHYSBASE_9            0x00000212
/* variable range MTRR base 9 */
#define MSR_IA32_MTRR_PHYSMASK_9            0x00000213
/* variable range MTRR mask 9 */
#define MSR_IA32_MTRR_FIX64K_00000          0x00000250
/* fixed range MTRR 16K/0x00000 */
#define MSR_IA32_MTRR_FIX16K_80000          0x00000258
/* fixed range MTRR 16K/0x80000 */
#define MSR_IA32_MTRR_FIX16K_A0000          0x00000259
/* fixed range MTRR 16K/0xA0000 */
#define MSR_IA32_MTRR_FIX4K_C0000           0x00000268
/* fixed range MTRR 4K/0xC0000 */
#define MSR_IA32_MTRR_FIX4K_C8000           0x00000269
/* fixed range MTRR 4K/0xC8000 */
#define MSR_IA32_MTRR_FIX4K_D0000           0x0000026A
/* fixed range MTRR 4K/0xD0000 */
#define MSR_IA32_MTRR_FIX4K_D8000           0x0000026B
/* fixed range MTRR 4K/0xD8000 */
#define MSR_IA32_MTRR_FIX4K_E0000           0x0000026C
/* fixed range MTRR 4K/0xE0000 */
#define MSR_IA32_MTRR_FIX4K_E8000           0x0000026D
/* fixed range MTRR 4K/0xE8000 */
#define MSR_IA32_MTRR_FIX4K_F0000           0x0000026E
/* fixed range MTRR 4K/0xF0000 */
#define MSR_IA32_MTRR_FIX4K_F8000           0x0000026F
/* fixed range MTRR 4K/0xF8000 */
#define MSR_IA32_PAT                        0x00000277	/* PAT */
#define MSR_IA32_MC0_CTL2                   0x00000280
/* Corrected error count threshold 0 */
#define MSR_IA32_MC1_CTL2                   0x00000281
/* Corrected error count threshold 1 */
#define MSR_IA32_MC2_CTL2                   0x00000282
/* Corrected error count threshold 2 */
#define MSR_IA32_MC3_CTL2                   0x00000283
/* Corrected error count threshold 3 */
#define MSR_IA32_MC4_CTL2                   0x00000284
/* Corrected error count threshold 4 */
#define MSR_IA32_MC5_CTL2                   0x00000285
/* Corrected error count threshold 5 */
#define MSR_IA32_MC6_CTL2                   0x00000286
/* Corrected error count threshold 6 */
#define MSR_IA32_MC7_CTL2                   0x00000287
/* Corrected error count threshold 7 */
#define MSR_IA32_MC8_CTL2                   0x00000288
/* Corrected error count threshold 8 */
#define MSR_IA32_MC9_CTL2                   0x00000289
/* Corrected error count threshold 9 */
#define MSR_IA32_MC10_CTL2                  0x0000028A
/* Corrected error count threshold 10 */
#define MSR_IA32_MC11_CTL2                  0x0000028B
/* Corrected error count threshold 11 */
#define MSR_IA32_MC12_CTL2                  0x0000028C
/* Corrected error count threshold 12 */
#define MSR_IA32_MC13_CTL2                  0x0000028D
/* Corrected error count threshold 13 */
#define MSR_IA32_MC14_CTL2                  0x0000028E
/* Corrected error count threshold 14 */
#define MSR_IA32_MC15_CTL2                  0x0000028F
/* Corrected error count threshold 15 */
#define MSR_IA32_MC16_CTL2                  0x00000290
/* Corrected error count threshold 16 */
#define MSR_IA32_MC17_CTL2                  0x00000291
/* Corrected error count threshold 17 */
#define MSR_IA32_MC18_CTL2                  0x00000292
/* Corrected error count threshold 18 */
#define MSR_IA32_MC19_CTL2                  0x00000293
/* Corrected error count threshold 19 */
#define MSR_IA32_MC20_CTL2                  0x00000294
/* Corrected error count threshold 20 */
#define MSR_IA32_MC21_CTL2                  0x00000295
/* Corrected error count threshold 21 */
#define MSR_IA32_MTRR_DEF_TYPE              0x000002FF
/* Default memory type/MTRR control */
#define MSR_IA32_FIXED_CTR0                 0x00000309
/* Fixed-function performance counter 0 */
#define MSR_IA32_FIXED_CTR1                 0x0000030A
/* Fixed-function performance counter 1 */
#define MSR_IA32_FIXED_CTR2                 0x0000030B
/* Fixed-function performance counter 2 */
#define MSR_IA32_PERF_CAPABILITIES          0x00000345
/* Performance capability */
#define MSR_IA32_FIXED_CTR_CTL              0x0000038D
/* Fixed-function performance counter control */
#define MSR_IA32_PERF_GLOBAL_STATUS         0x0000038E
/* Global performance counter status */
#define MSR_IA32_PERF_GLOBAL_CTRL           0x0000038F
/* Global performance counter control */
#define MSR_IA32_PERF_GLOBAL_OVF_CTRL       0x00000390
/* Global performance counter overflow control */
#define MSR_IA32_PEBS_ENABLE                0x000003F1	/* PEBS control */
#define MSR_IA32_MC0_CTL                    0x00000400	/* MC 0 control */
#define MSR_IA32_MC0_STATUS                 0x00000401	/* MC 0 status */
#define MSR_IA32_MC0_ADDR                   0x00000402	/* MC 0 address */
#define MSR_IA32_MC0_MISC                   0x00000403	/* MC 0 misc. */
#define MSR_IA32_MC1_CTL                    0x00000404	/* MC 1 control */
#define MSR_IA32_MC1_STATUS                 0x00000405	/* MC 1 status */
#define MSR_IA32_MC1_ADDR                   0x00000406	/* MC 1 address */
#define MSR_IA32_MC1_MISC                   0x00000407	/* MC 1 misc. */
#define MSR_IA32_MC2_CTL                    0x00000408	/* MC 2 control */
#define MSR_IA32_MC2_STATUS                 0x00000409	/* MC 2 status */
#define MSR_IA32_MC2_ADDR                   0x0000040A	/* MC 2 address */
#define MSR_IA32_MC2_MISC                   0x0000040B	/* MC 2 misc. */
#define MSR_IA32_MC3_CTL                    0x0000040C	/* MC 3 control */
#define MSR_IA32_MC3_STATUS                 0x0000040D	/* MC 3 status */
#define MSR_IA32_MC3_ADDR                   0x0000040E	/* MC 3 address */
#define MSR_IA32_MC3_MISC                   0x0000040F	/* MC 3 misc. */
#define MSR_IA32_MC4_CTL                    0x00000410	/* MC 4 control */
#define MSR_IA32_MC4_STATUS                 0x00000411	/* MC 4 status */
#define MSR_IA32_MC4_ADDR                   0x00000412	/* MC 4 address */
#define MSR_IA32_MC4_MISC                   0x00000413	/* MC 4 misc. */
#define MSR_IA32_MC5_CTL                    0x00000414	/* MC 5 control */
#define MSR_IA32_MC5_STATUS                 0x00000415	/* MC 5 status */
#define MSR_IA32_MC5_ADDR                   0x00000416	/* MC 5 address */
#define MSR_IA32_MC5_MISC                   0x00000417	/* MC 5 misc. */
#define MSR_IA32_MC6_CTL                    0x00000418	/* MC 6 control */
#define MSR_IA32_MC6_STATUS                 0x00000419	/* MC 6 status */
#define MSR_IA32_MC6_ADDR                   0x0000041A	/* MC 6 address */
#define MSR_IA32_MC6_MISC                   0x0000041B	/* MC 6 misc. */
#define MSR_IA32_MC7_CTL                    0x0000041C	/* MC 7 control */
#define MSR_IA32_MC7_STATUS                 0x0000041D	/* MC 7 status */
#define MSR_IA32_MC7_ADDR                   0x0000041E	/* MC 7 address */
#define MSR_IA32_MC7_MISC                   0x0000041F	/* MC 7 misc. */
#define MSR_IA32_MC8_CTL                    0x00000420	/* MC 8 control */
#define MSR_IA32_MC8_STATUS                 0x00000421	/* MC 8 status */
#define MSR_IA32_MC8_ADDR                   0x00000422	/* MC 8 address */
#define MSR_IA32_MC8_MISC                   0x00000423	/* MC 8 misc. */
#define MSR_IA32_MC9_CTL                    0x00000424	/* MC 9 control */
#define MSR_IA32_MC9_STATUS                 0x00000425	/* MC 9 status */
#define MSR_IA32_MC9_ADDR                   0x00000426	/* MC 9 address */
#define MSR_IA32_MC9_MISC                   0x00000427	/* MC 9 misc. */
#define MSR_IA32_MC10_CTL                   0x00000428	/* MC 10 control */
#define MSR_IA32_MC10_STATUS                0x00000429	/* MC 10 status */
#define MSR_IA32_MC10_ADDR                  0x0000042A	/* MC 10 address */
#define MSR_IA32_MC10_MISC                  0x0000042B	/* MC 10 misc. */
#define MSR_IA32_MC11_CTL                   0x0000042C	/* MC 11 control */
#define MSR_IA32_MC11_STATUS                0x0000042D	/* MC 11 status */
#define MSR_IA32_MC11_ADDR                  0x0000042E	/* MC 11 address */
#define MSR_IA32_MC11_MISC                  0x0000042F	/* MC 11 misc. */
#define MSR_IA32_MC12_CTL                   0x00000430	/* MC 12 control */
#define MSR_IA32_MC12_STATUS                0x00000431	/* MC 12 status */
#define MSR_IA32_MC12_ADDR                  0x00000432	/* MC 12 address */
#define MSR_IA32_MC12_MISC                  0x00000433	/* MC 12 misc. */
#define MSR_IA32_MC13_CTL                   0x00000434	/* MC 13 control */
#define MSR_IA32_MC13_STATUS                0x00000435	/* MC 13 status */
#define MSR_IA32_MC13_ADDR                  0x00000436	/* MC 13 address */
#define MSR_IA32_MC13_MISC                  0x00000437	/* MC 13 misc. */
#define MSR_IA32_MC14_CTL                   0x00000438	/* MC 14 control */
#define MSR_IA32_MC14_STATUS                0x00000439	/* MC 14 status */
#define MSR_IA32_MC14_ADDR                  0x0000043A	/* MC 14 address */
#define MSR_IA32_MC14_MISC                  0x0000043B	/* MC 14 misc. */
#define MSR_IA32_MC15_CTL                   0x0000043C	/* MC 15 control */
#define MSR_IA32_MC15_STATUS                0x0000043D	/* MC 15 status */
#define MSR_IA32_MC15_ADDR                  0x0000043E	/* MC 15 address */
#define MSR_IA32_MC15_MISC                  0x0000043F	/* MC 15 misc. */
#define MSR_IA32_MC16_CTL                   0x00000440	/* MC 16 control */
#define MSR_IA32_MC16_STATUS                0x00000441	/* MC 16 status */
#define MSR_IA32_MC16_ADDR                  0x00000442	/* MC 16 address */
#define MSR_IA32_MC16_MISC                  0x00000443	/* MC 16 misc. */
#define MSR_IA32_MC17_CTL                   0x00000444	/* MC 17 control */
#define MSR_IA32_MC17_STATUS                0x00000445	/* MC 17 status */
#define MSR_IA32_MC17_ADDR                  0x00000446	/* MC 17 address */
#define MSR_IA32_MC17_MISC                  0x00000447	/* MC 17 misc. */
#define MSR_IA32_MC18_CTL                   0x00000448	/* MC 18 control */
#define MSR_IA32_MC18_STATUS                0x00000449	/* MC 18 status */
#define MSR_IA32_MC18_ADDR                  0x0000044A	/* MC 18 address */
#define MSR_IA32_MC18_MISC                  0x0000044B	/* MC 18 misc. */
#define MSR_IA32_MC19_CTL                   0x0000044C	/* MC 19 control */
#define MSR_IA32_MC19_STATUS                0x0000044D	/* MC 19 status */
#define MSR_IA32_MC19_ADDR                  0x0000044E	/* MC 19 address */
#define MSR_IA32_MC19_MISC                  0x0000044F	/* MC 19 misc. */
#define MSR_IA32_MC20_CTL                   0x00000450	/* MC 20 control */
#define MSR_IA32_MC20_STATUS                0x00000451	/* MC 20 status */
#define MSR_IA32_MC20_ADDR                  0x00000452	/* MC 20 address */
#define MSR_IA32_MC20_MISC                  0x00000453	/* MC 20 misc. */
#define MSR_IA32_MC21_CTL                   0x00000454	/* MC 21 control */
#define MSR_IA32_MC21_STATUS                0x00000455	/* MC 21 status */
#define MSR_IA32_MC21_ADDR                  0x00000456	/* MC 21 address */
#define MSR_IA32_MC21_MISC                  0x00000457	/* MC 21 misc. */
#define MSR_IA32_VMX_BASIC                  0x00000480
/* Capability reporting register basic VMX capabilities */
#define MSR_IA32_VMX_PINBASED_CTLS          0x00000481
/* Capability reporting register pin based VM execution controls */
#define MSR_IA32_VMX_PROCBASED_CTLS         0x00000482
/* Capability reporting register primary processor based VM execution controls*/
#define MSR_IA32_VMX_EXIT_CTLS              0x00000483
/* Capability reporting register VM exit controls */
#define MSR_IA32_VMX_ENTRY_CTLS             0x00000484
/* Capability reporting register VM entry controls */
#define MSR_IA32_VMX_MISC                   0x00000485
/* Reporting register misc. VMX capabilities */
#define MSR_IA32_VMX_CR0_FIXED0             0x00000486
/* Capability reporting register of CR0 bits fixed to 0 */
#define MSR_IA32_VMX_CR0_FIXED1             0x00000487
/* Capability reporting register of CR0 bits fixed to 1 */
#define MSR_IA32_VMX_CR4_FIXED0             0x00000488
/* Capability reporting register of CR4 bits fixed to 0 */
#define MSR_IA32_VMX_CR4_FIXED1            0x00000489
/* Capability reporting register of CR4 bits fixed to 1 */
#define MSR_IA32_VMX_VMCS_ENUM             0x0000048A
/* Capability reporting register of VMCS field enumeration */
#define MSR_IA32_VMX_PROCBASED_CTLS2        0x0000048B
/* Capability reporting register of secondary processor based VM execution
 * controls
 */
#define MSR_IA32_VMX_EPT_VPID_CAP           0x0000048C
/* Capability reporting register of EPT and VPID */
#define MSR_IA32_VMX_TRUE_PINBASED_CTLS     0x0000048D
/* Capability reporting register of pin based VM execution flex controls */
#define MSR_IA32_VMX_TRUE_PROCBASED_CTLS    0x0000048E
/* Capability reporting register of primary processor based VM execution flex
 * controls
 */
#define MSR_IA32_VMX_TRUE_EXIT_CTLS         0x0000048F
/* Capability reporting register of VM exit flex controls */
#define MSR_IA32_VMX_TRUE_ENTRY_CTLS        0x00000490
/* Capability reporting register of VM entry flex controls */
#define MSR_IA32_DS_AREA                   0x00000600   /* DS save area */
/* APIC TSC deadline MSR */
#define MSR_IA32_TSC_DEADLINE               0x000006E0
#define MSR_IA32_EXT_XAPICID                0x00000802	/* x2APIC ID */
#define MSR_IA32_EXT_APIC_VERSION           0x00000803	/* x2APIC version */
#define MSR_IA32_EXT_APIC_TPR               0x00000808
/* x2APIC task priority */
#define MSR_IA32_EXT_APIC_PPR               0x0000080A
/* x2APIC processor priority */
#define MSR_IA32_EXT_APIC_EOI               0x0000080B	/* x2APIC EOI */
#define MSR_IA32_EXT_APIC_LDR               0x0000080D
/* x2APIC logical destination */
#define MSR_IA32_EXT_APIC_SIVR              0x0000080F
/* x2APIC spurious interrupt vector */
#define MSR_IA32_EXT_APIC_ISR0              0x00000810
/* x2APIC in-service register 0 */
#define MSR_IA32_EXT_APIC_ISR1              0x00000811
/* x2APIC in-service register 1 */
#define MSR_IA32_EXT_APIC_ISR2              0x00000812
/* x2APIC in-service register 2 */
#define MSR_IA32_EXT_APIC_ISR3              0x00000813
/* x2APIC in-service register 3 */
#define MSR_IA32_EXT_APIC_ISR4              0x00000814
/* x2APIC in-service register 4 */
#define MSR_IA32_EXT_APIC_ISR5              0x00000815
/* x2APIC in-service register 5 */
#define MSR_IA32_EXT_APIC_ISR6              0x00000816
/* x2APIC in-service register 6 */
#define MSR_IA32_EXT_APIC_ISR7              0x00000817
/* x2APIC in-service register 7 */
#define MSR_IA32_EXT_APIC_TMR0              0x00000818
/* x2APIC trigger mode register 0 */
#define MSR_IA32_EXT_APIC_TMR1              0x00000819
/* x2APIC trigger mode register 1 */
#define MSR_IA32_EXT_APIC_TMR2              0x0000081A
/* x2APIC trigger mode register 2 */
#define MSR_IA32_EXT_APIC_TMR3              0x0000081B
/* x2APIC trigger mode register 3 */
#define MSR_IA32_EXT_APIC_TMR4              0x0000081C
/* x2APIC trigger mode register 4 */
#define MSR_IA32_EXT_APIC_TMR5              0x0000081D
/* x2APIC trigger mode register 5 */
#define MSR_IA32_EXT_APIC_TMR6              0x0000081E
/* x2APIC trigger mode register 6 */
#define MSR_IA32_EXT_APIC_TMR7              0x0000081F
/* x2APIC trigger mode register 7 */
#define MSR_IA32_EXT_APIC_IRR0              0x00000820
/* x2APIC interrupt request register 0 */
#define MSR_IA32_EXT_APIC_IRR1              0x00000821
/* x2APIC interrupt request register 1 */
#define MSR_IA32_EXT_APIC_IRR2              0x00000822
/* x2APIC interrupt request register 2 */
#define MSR_IA32_EXT_APIC_IRR3              0x00000823
/* x2APIC interrupt request register 3 */
#define MSR_IA32_EXT_APIC_IRR4              0x00000824
/* x2APIC interrupt request register 4 */
#define MSR_IA32_EXT_APIC_IRR5              0x00000825
/* x2APIC interrupt request register 5 */
#define MSR_IA32_EXT_APIC_IRR6              0x00000826
/* x2APIC interrupt request register 6 */
#define MSR_IA32_EXT_APIC_IRR7              0x00000827
/* x2APIC interrupt request register 7 */
#define MSR_IA32_EXT_APIC_ESR               0x00000828
/* x2APIC error status */
#define MSR_IA32_EXT_APIC_LVT_CMCI          0x0000082F
/* x2APIC LVT corrected machine check interrupt register */
#define MSR_IA32_EXT_APIC_ICR               0x00000830
/* x2APIC interrupt command register */
#define MSR_IA32_EXT_APIC_LVT_TIMER         0x00000832
/* x2APIC LVT timer interrupt register */
#define MSR_IA32_EXT_APIC_LVT_THERMAL       0x00000833
/* x2APIC LVT thermal sensor interrupt register */
#define MSR_IA32_EXT_APIC_LVT_PMI           0x00000834
/* x2APIC LVT performance monitor interrupt register */
#define MSR_IA32_EXT_APIC_LVT_LINT0         0x00000835
/* x2APIC LVT LINT0 register */
#define MSR_IA32_EXT_APIC_LVT_LINT1         0x00000836
/* x2APIC LVT LINT1 register */
#define MSR_IA32_EXT_APIC_LVT_ERROR         0x00000837
/* x2APIC LVT error register */
#define MSR_IA32_EXT_APIC_INIT_COUNT        0x00000838
/* x2APIC initial count register */
#define MSR_IA32_EXT_APIC_CUR_COUNT         0x00000839
/* x2APIC current count  register */
#define MSR_IA32_EXT_APIC_DIV_CONF          0x0000083E
/* x2APIC divide configuration register */
#define MSR_IA32_EXT_APIC_SELF_IPI          0x0000083F
/* x2APIC self IPI register */
#define MSR_IA32_EFER                       0xC0000080
/* Extended feature enables */
#define MSR_IA32_STAR                       0xC0000081
/* System call target address */
#define MSR_IA32_LSTAR                      0xC0000082
/* IA-32e mode system call target address */
#define MSR_IA32_FMASK                      0xC0000084
/* System call flag mask */
#define MSR_IA32_FS_BASE                    0xC0000100
/* Map of BASE address of FS */
#define MSR_IA32_GS_BASE                    0xC0000101
/* Map of BASE address of GS */
#define MSR_IA32_KERNEL_GS_BASE             0xC0000102
/* Swap target of BASE address of GS */
#define MSR_IA32_TSC_AUX                    0xC0000103	/* Auxiliary TSC */

/* ATOM specific MSRs */
#define MSR_ATOM_EBL_CR_POWERON             0x0000002A
/* Processor hard power-on configuration */
#define MSR_ATOM_LASTBRANCH_0_FROM_IP       0x00000040
/* Last branch record 0 from IP */
#define MSR_ATOM_LASTBRANCH_1_FROM_IP       0x00000041
/* Last branch record 1 from IP */
#define MSR_ATOM_LASTBRANCH_2_FROM_IP       0x00000042
/* Last branch record 2 from IP */
#define MSR_ATOM_LASTBRANCH_3_FROM_IP       0x00000043
/* Last branch record 3 from IP */
#define MSR_ATOM_LASTBRANCH_4_FROM_IP       0x00000044
/* Last branch record 4 from IP */
#define MSR_ATOM_LASTBRANCH_5_FROM_IP       0x00000045
/* Last branch record 5 from IP */
#define MSR_ATOM_LASTBRANCH_6_FROM_IP       0x00000046
/* Last branch record 6 from IP */
#define MSR_ATOM_LASTBRANCH_7_FROM_IP       0x00000047
/* Last branch record 7 from IP */
#define MSR_ATOM_LASTBRANCH_0_TO_LIP        0x00000060
/* Last branch record 0 to IP */
#define MSR_ATOM_LASTBRANCH_1_TO_LIP        0x00000061
/* Last branch record 1 to IP */
#define MSR_ATOM_LASTBRANCH_2_TO_LIP        0x00000062
/* Last branch record 2 to IP */
#define MSR_ATOM_LASTBRANCH_3_TO_LIP        0x00000063
/* Last branch record 3 to IP */
#define MSR_ATOM_LASTBRANCH_4_TO_LIP        0x00000064
/* Last branch record 4 to IP */
#define MSR_ATOM_LASTBRANCH_5_TO_LIP        0x00000065
/* Last branch record 5 to IP */
#define MSR_ATOM_LASTBRANCH_6_TO_LIP        0x00000066
/* Last branch record 6 to IP */
#define MSR_ATOM_LASTBRANCH_7_TO_LIP        0x00000067
/* Last branch record 7 to IP */
#define MSR_ATOM_FSB_FREQ                   0x000000CD	/* Scalable bus speed */
#define MSR_PLATFORM_INFO                   0x000000CE
/* Maximum resolved bus ratio */
#define MSR_ATOM_BBL_CR_CTL3                0x0000011E /* L2 hardware enabled */
#define MSR_ATOM_THERM2_CTL                 0x0000019D
/* Mode of automatic thermal monitor */
#define MSR_ATOM_LASTBRANCH_TOS             0x000001C9
/* Last branch record stack TOS */
#define MSR_ATOM_LER_FROM_LIP               0x000001DD
/* Last exception record from linear IP */
#define MSR_ATOM_LER_TO_LIP                 0x000001DE
/* Last exception record to linear IP */

/* LINCROFT specific MSRs */
#define MSR_LNC_BIOS_CACHE_AS_RAM           0x000002E0	/* Configure CAR */

/* MSR_IA32_VMX_EPT_VPID_CAP: EPT and VPID capability bits */
#define MSR_VMX_EPT_VPID_CAP_1GB           (1UL << 17)/* EPT 1GB page */
#define MSR_VMX_INVEPT                    (1UL << 20)/* INVEPT */
#define MSR_VMX_INVEPT_SINGLE_CONTEXT     (1UL << 25)/* INVEPT Single */
#define MSR_VMX_INVEPT_GLOBAL_CONTEXT     (1UL << 26)/* INVEPT Global */
#define MSR_VMX_INVVPID                   (1UL << 32)/* INVVPID */
#define MSR_VMX_INVVPID_SINGLE_CONTEXT    (1UL << 41)/* INVVPID Single */
#define MSR_VMX_INVVPID_GLOBAL_CONTEXT    (1UL << 42)/* INVVPID Global */

/* EFER bits */
#define MSR_IA32_EFER_SCE_BIT                   (1<<0)
#define MSR_IA32_EFER_LME_BIT                   (1<<8)	/* IA32e mode enable */
#define MSR_IA32_EFER_LMA_BIT                   (1<<10)	/* IA32e mode active */
#define MSR_IA32_EFER_NXE_BIT                   (1<<11)

/* FEATURE CONTROL bits */
#define MSR_IA32_FEATURE_CONTROL_LOCK           (1<<0)
#define MSR_IA32_FEATURE_CONTROL_VMX_SMX        (1<<1)
#define MSR_IA32_FEATURE_CONTROL_VMX_NO_SMX     (1<<2)

/* PAT memory type definitions */
#define PAT_MEM_TYPE_UC                     0x00	/* uncached */
#define PAT_MEM_TYPE_WC                     0x01	/* write combining */
#define PAT_MEM_TYPE_WT                     0x04	/* write through */
#define PAT_MEM_TYPE_WP                     0x05	/* write protected */
#define PAT_MEM_TYPE_WB                     0x06	/* writeback */
#define PAT_MEM_TYPE_UCM                    0x07	/* uncached minus */

/* MTRR memory type definitions */
#define MTRR_MEM_TYPE_UC             0x00	/* uncached */
#define MTRR_MEM_TYPE_WC             0x01	/* write combining */
#define MTRR_MEM_TYPE_WT             0x04	/* write through */
#define MTRR_MEM_TYPE_WP             0x05	/* write protected */
#define MTRR_MEM_TYPE_WB             0x06	/* writeback */

/* misc. MTRR flag definitions */
#define MTRR_ENABLE                  0x800	/* MTRR enable */
#define MTRR_FIX_ENABLE              0x400	/* fixed range MTRR enable */
#define MTRR_VALID                   0x800	/* MTRR setting is  valid */

/* SPEC & PRED bit */
#define SPEC_ENABLE_IBRS		(1<<0)
#define SPEC_ENABLE_STIBP		(1<<1)
#define PRED_SET_IBPB			(1<<0)

#endif /* MSR_H */
