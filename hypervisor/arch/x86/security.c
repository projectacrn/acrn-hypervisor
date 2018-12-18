/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <msr.h>
#include <cpufeatures.h>
#include <spinlock.h>
#include <cpu.h>
#include <per_cpu.h>
#include <cpu_caps.h>
#include <security.h>
#include <logmsg.h>

static bool skip_l1dfl_vmentry;
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
	if (cpu_has_cap(X86_FEATURE_IBRS_IBPB)) {
		ibrs_type = IBRS_RAW;
		if (cpu_has_cap(X86_FEATURE_STIBP)) {
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
	uint64_t x86_arch_capabilities;

	detect_ibrs();

	if (cpu_has_cap(X86_FEATURE_ARCH_CAP)) {
		x86_arch_capabilities = msr_read(MSR_IA32_ARCH_CAPABILITIES);
		skip_l1dfl_vmentry = ((x86_arch_capabilities
			& IA32_ARCH_CAP_SKIP_L1DFL_VMENTRY) != 0UL);
	} else {
		return false;
	}

	if ((!cpu_has_cap(X86_FEATURE_L1D_FLUSH)) && (!skip_l1dfl_vmentry)) {
		return false;
	}

	if ((!cpu_has_cap(X86_FEATURE_IBRS_IBPB)) &&
		(!cpu_has_cap(X86_FEATURE_STIBP))) {
		return false;
	}

	return true;
}

void cpu_l1d_flush(void)
{
	/*
	 * 'skip_l1dfl_vmentry' will be true on platform that
	 * is not affected by L1TF.
	 *
	 */
	if (!skip_l1dfl_vmentry) {
		if (cpu_has_cap(X86_FEATURE_L1D_FLUSH)) {
			msr_write(MSR_IA32_FLUSH_CMD, IA32_L1D_FLUSH);
		}
	}

}

#ifdef STACK_PROTECTOR
static uint64_t get_random_value(void)
{
	uint64_t random = 0UL;

	asm volatile ("1: rdrand %%rax\n"
			"jnc 1b\n"
			"mov %%rax, %0\n"
			: "=r"(random)
			:
			:"%rax");
	return random;
}

void set_fs_base(void)
{
	struct stack_canary *psc = &get_cpu_var(stk_canary);

	psc->canary = get_random_value();
	msr_write(MSR_IA32_FS_BASE, (uint64_t)psc);
}
#endif
