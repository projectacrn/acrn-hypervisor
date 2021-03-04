/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <x86/msr.h>
#include <x86/cpufeatures.h>
#include <x86/cpu.h>
#include <x86/per_cpu.h>
#include <x86/cpu_caps.h>
#include <x86/security.h>
#include <logmsg.h>

static bool skip_l1dfl_vmentry;
static bool cpu_md_clear;
static int32_t ibrs_type;

static void detect_ibrs(void)
{
	/* For speculation defence.
	 * The default way is to set IBRS at vmexit and then do IBPB at vcpu
	 * context switch(ibrs_type == IBRS_RAW).
	 * Now provide an optimized way (ibrs_type == IBRS_OPT) which set
	 * STIBP and do IBPB at vmexit,since having STIBP always set has less
	 * impact than having IBRS always set. Also since IBPB is already done
	 * at vmexit, it is no necessary to do so at vcpu context switch then.
	 */
	ibrs_type = IBRS_NONE;

	/* Currently for APL, if we enabled retpoline, then IBRS should not
	 * take effect
	 * TODO: add IA32_ARCH_CAPABILITIES[1] check, if this bit is set, IBRS
	 * should be set all the time instead of relying on retpoline
	 */
#ifndef CONFIG_RETPOLINE
	if (pcpu_has_cap(X86_FEATURE_IBRS_IBPB)) {
		ibrs_type = IBRS_RAW;
		if (pcpu_has_cap(X86_FEATURE_STIBP)) {
			ibrs_type = IBRS_OPT;
		}
	}
#endif
}

int32_t get_ibrs_type(void)
{
	return ibrs_type;
}

bool check_cpu_security_cap(void)
{
	bool ret = true;
	bool mds_no = false;
	bool ssb_no = false;
	uint64_t x86_arch_capabilities;

	detect_ibrs();

	if (pcpu_has_cap(X86_FEATURE_ARCH_CAP)) {
		x86_arch_capabilities = msr_read(MSR_IA32_ARCH_CAPABILITIES);
		skip_l1dfl_vmentry = ((x86_arch_capabilities
			& IA32_ARCH_CAP_SKIP_L1DFL_VMENTRY) != 0UL);

		mds_no = ((x86_arch_capabilities & IA32_ARCH_CAP_MDS_NO) != 0UL);

		/* SSB_NO: Processor is not susceptble to Speculative Store Bypass(SSB) */
		ssb_no = ((x86_arch_capabilities & IA32_ARCH_CAP_SSB_NO) != 0UL);
	}

	if ((!pcpu_has_cap(X86_FEATURE_L1D_FLUSH)) && (!skip_l1dfl_vmentry)) {
		/* Processor is affected by L1TF CPU vulnerability,
		 * but no L1D_FLUSH command support.
		 */
		ret = false;
	}

	if ((!pcpu_has_cap(X86_FEATURE_SSBD)) && (!ssb_no)) {
		/* Processor is susceptble to Speculative Store Bypass(SSB),
		 * but no support for Speculative Store Bypass Disable(SSBD).
		 */
		ret = false;
	}

	if ((!pcpu_has_cap(X86_FEATURE_IBRS_IBPB)) && (!pcpu_has_cap(X86_FEATURE_STIBP))) {
		ret = false;
	}

	if (!mds_no) { /* Processor is affected by MDS vulnerability.*/
		if (pcpu_has_cap(X86_FEATURE_MDS_CLEAR)) {
			cpu_md_clear = true;
#ifdef CONFIG_L1D_FLUSH_VMENTRY_ENABLED
			if (!skip_l1dfl_vmentry) {
				/* L1D cache flush will also overwrite CPU internal buffers,
				 * additional MDS buffers clear operation is not required.
				 */
				cpu_md_clear = false;
			}
#endif
		} else {
			/* Processor is affected by MDS but no mitigation software
			 * interface is enumerated, CPU microcode need to be udpated.
			 */
			ret = false;
		}
	}

	return ret;
}

void cpu_l1d_flush(void)
{
	/*
	 * 'skip_l1dfl_vmentry' will be true on platform that
	 * is not affected by L1TF.
	 *
	 */
	if (!skip_l1dfl_vmentry) {
		if (pcpu_has_cap(X86_FEATURE_L1D_FLUSH)) {
			msr_write(MSR_IA32_FLUSH_CMD, IA32_L1D_FLUSH);
		}
	}

}

/*
 * VERW instruction (with microcode update) will overwrite
 * CPU internal buffers.
 */
static inline void verw_buffer_overwriting(void)
{
	uint16_t ds = HOST_GDT_RING0_DATA_SEL;

	asm volatile ("verw %[ds]" : : [ds] "m" (ds) : "cc");
}

/*
 * On processors that enumerate MD_CLEAR:CPUID.(EAX=7H,ECX=0):EDX[MD_CLEAR=10],
 * the VERW instruction or L1D_FLUSH command should be used to cause the
 * processor to overwrite buffer values that are affected by MDS
 * (Microarchitectural Data Sampling) vulnerabilities.
 *
 * The VERW instruction and L1D_FLUSH command will overwrite below buffer values:
 *  - Store buffer value for the current logical processor on processors affected
 *    by MSBDS (Microarchitectural Store Buffer Data Sampling).
 *  - Fill buffer for all logical processors on the physical core for processors
 *    affected by MFBDS (Microarchitectural Fill Buffer Data Sampling).
 *  - Load port for all logical processors on the physical core for processors
 *    affected by MLPDS(Microarchitectural Load Port Data Sampling).
 *
 * If processor is affected by L1TF vulnerability and the mitigation is enabled,
 * L1D_FLUSH will overwrite internal buffers on processors affected by MDS, no
 * additional buffer overwriting is required before VM entry. For other cases,
 * VERW instruction is used to overwrite buffer values for processors affected
 * by MDS.
 */
void cpu_internal_buffers_clear(void)
{
	if (cpu_md_clear) {
		verw_buffer_overwriting();
	}
}

uint64_t get_random_value(void)
{
	uint64_t random;

	asm volatile ("1: rdrand %%rax\n"
			"jnc 1b\n"
			"mov %%rax, %0\n"
			: "=r"(random)
			:
			:"%rax");
	return random;
}

#ifdef STACK_PROTECTOR
void set_fs_base(void)
{
	struct stack_canary *psc = &get_cpu_var(stk_canary);

	psc->canary = get_random_value();
	msr_write(MSR_IA32_FS_BASE, (uint64_t)psc);
}
#endif

#ifdef CONFIG_MCE_ON_PSC_WORKAROUND_DISABLED
bool is_ept_force_4k_ipage(void)
{
	return false;
}
#else
bool is_ept_force_4k_ipage(void)
{
	bool force_4k_ipage = true;
	const struct cpuinfo_x86 *info = get_pcpu_info();
	uint64_t x86_arch_capabilities;

	if (info->displayfamily == 0x6U) {
		switch (info->displaymodel) {
		case 0x26U:
		case 0x27U:
		case 0x35U:
		case 0x36U:
		case 0x37U:
		case 0x86U:
		case 0x1CU:
		case 0x4AU:
		case 0x4CU:
		case 0x4DU:
		case 0x5AU:
		case 0x5CU:
		case 0x5DU:
		case 0x5FU:
		case 0x6EU:
		case 0x7AU:
			/* Atom processor is not affected by the issue
			 * "Machine Check Error on Page Size Change"
			 */
			force_4k_ipage = false;
			break;
		default:
			force_4k_ipage = true;
			break;
		}
	}

	if (pcpu_has_cap(X86_FEATURE_ARCH_CAP)) {
		x86_arch_capabilities = msr_read(MSR_IA32_ARCH_CAPABILITIES);
		if ((x86_arch_capabilities & IA32_ARCH_CAP_IF_PSCHANGE_MC_NO) != 0UL) {
			force_4k_ipage = false;
		}
	}

	return force_4k_ipage;
}
#endif
