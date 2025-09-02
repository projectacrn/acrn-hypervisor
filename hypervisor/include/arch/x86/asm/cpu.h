/*-
 * Copyright (c) 1989, 1990 William F. Jolitz
 * Copyright (c) 1990 The Regents of the University of California.
 * Copyright (c) 2017-2022 Intel Corporation.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)segments.h	7.1 (Berkeley) 5/9/91
 * $FreeBSD$
 */

#ifndef CPU_H
#define CPU_H
#include <types.h>
#include <util.h>
#include <acrn_common.h>
#include <asm/msr.h>
#include <errno.h>
#include <barrier.h>

/* Define CPU stack alignment */
#define CPU_STACK_ALIGN         16UL

/* CR0 register definitions */
#define CR0_PG                  (1UL<<31U)	/* paging enable */
#define CR0_CD                  (1UL<<30U)	/* cache disable */
#define CR0_NW                  (1UL<<29U)	/* not write through */
#define CR0_AM                  (1UL<<18U)	/* alignment mask */
#define CR0_WP                  (1UL<<16U)	/* write protect */
#define CR0_NE                  (1UL<<5U)	/* numeric error */
#define CR0_ET                  (1UL<<4U)	/* extension type */
#define CR0_TS                  (1UL<<3U)	/* task switched */
#define CR0_EM                  (1UL<<2U)	/* emulation */
#define CR0_MP                  (1UL<<1U)	/* monitor coprocessor */
#define CR0_PE                  (1UL<<0U)	/* protected mode enabled */

/* CR3 register definitions */
#define CR3_PWT                 (1UL<<3U)	/* page-level write through */
#define CR3_PCD                 (1UL<<4U)	/* page-level cache disable */

/* CR4 register definitions */
#define CR4_VME                 (1UL<<0U)	/* virtual 8086 mode extensions */
#define CR4_PVI                 (1UL<<1U)	/* protected mode virtual interrupts */
#define CR4_TSD                 (1UL<<2U)	/* time stamp disable */
#define CR4_DE                  (1UL<<3U)	/* debugging extensions */
#define CR4_PSE                 (1UL<<4U)	/* page size extensions */
#define CR4_PAE                 (1UL<<5U)	/* physical address extensions */
#define CR4_MCE                 (1UL<<6U)	/* machine check enable */
#define CR4_PGE                 (1UL<<7U)	/* page global enable */
#define CR4_PCE                 (1UL<<8U)
/* performance monitoring counter enable */
#define CR4_OSFXSR              (1UL<<9U)	/* OS support for FXSAVE/FXRSTOR */
#define CR4_OSXMMEXCPT          (1UL<<10U)
/* OS support for unmasked SIMD floating point exceptions */
#define CR4_UMIP                (1UL<<11U)	/* User-Mode Inst prevention */
#define CR4_LA57                (1UL<<12U)	/* 57-bit linear address */
#define CR4_VMXE                (1UL<<13U)	/* VMX enable */
#define CR4_SMXE                (1UL<<14U)	/* SMX enable */
#define CR4_FSGSBASE            (1UL<<16U)	/* RD(FS|GS|FS)BASE inst */
#define CR4_PCIDE               (1UL<<17U)	/* PCID enable */
/* XSAVE and Processor Extended States enable bit */
#define CR4_OSXSAVE             (1UL<<18U)
#define CR4_KL                  (1UL<<19U)      /* KeyLocker enable */
#define CR4_SMEP                (1UL<<20U)
#define CR4_SMAP                (1UL<<21U)
#define CR4_PKE                 (1UL<<22U)	/* Protect-key-enable */
#define CR4_CET                 (1UL<<23U)	/* Control-flow Enforcement Technology enable */
#define CR4_PKS                 (1UL<<24U)	/* Enable protection keys for supervisor-mode pages */

/* XCR0_SSE */
#define XCR0_SSE		(1UL<<1U)
/* XCR0_AVX */
#define XCR0_AVX		(1UL<<2U)
/* XCR0_BNDREGS */
#define XCR0_BNDREGS		(1UL<<3U)
/* XCR0_BNDCSR */
#define XCR0_BNDCSR		(1UL<<4U)
/* According to SDM Vol1 13.3:
 *   XCR0[63:10] and XCR0[8] are reserved. Executing the XSETBV instruction causes
 *   a general-protection fault if ECX = 0 and any corresponding bit in EDX:EAX
 *   is not 0.
 */
#define	XCR0_RESERVED_BITS	((~((1UL << 10U) - 1UL)) | (1UL << 8U))


/*
 * Entries in the Interrupt Descriptor Table (IDT)
 */
#define IDT_DE      0U   /* #DE: Divide Error */
#define IDT_DB      1U   /* #DB: Debug */
#define IDT_NMI     2U   /* Nonmaskable External Interrupt */
#define IDT_BP      3U   /* #BP: Breakpoint */
#define IDT_OF      4U   /* #OF: Overflow */
#define IDT_BR      5U   /* #BR: Bound Range Exceeded */
#define IDT_UD      6U   /* #UD: Undefined/Invalid Opcode */
#define IDT_NM      7U   /* #NM: No Math Coprocessor */
#define IDT_DF      8U   /* #DF: Double Fault */
#define IDT_FPUGP   9U   /* Coprocessor Segment Overrun */
#define IDT_TS      10U  /* #TS: Invalid TSS */
#define IDT_NP      11U  /* #NP: Segment Not Present */
#define IDT_SS      12U  /* #SS: Stack Segment Fault */
#define IDT_GP      13U  /* #GP: General Protection Fault */
#define IDT_PF      14U  /* #PF: Page Fault */
#define IDT_MF      16U  /* #MF: FPU Floating-Point Error */
#define IDT_AC      17U  /* #AC: Alignment Check */
#define IDT_MC      18U  /* #MC: Machine Check */
#define IDT_XF      19U  /* #XF: SIMD Floating-Point Exception */
#define IDT_VE      20U  /* #VE: Virtualization Exception */

/*Bits in EFER special registers */
#define EFER_LMA 0x00000400U    /* Long mode active (R) */

#define RFLAGS_C (1U<<0U)
#define RFLAGS_P (1U<<2U)
#define RFLAGS_A (1U<<4U)
#define RFLAGS_Z (1U<<6U)
#define RFLAGS_S (1U<<7U)
#define RFLAGS_O (1U<<11U)
#define RFLAGS_VM (1U<<17U)
#define RFLAGS_AC (1U<<18U)

/* CPU clock frequencies (FSB) */
#define CPU_FSB_83KHZ           83200
#define CPU_FSB_100KHZ          99840
#define CPU_FSB_133KHZ          133200
#define CPU_FSB_166KHZ          166400

/* Time conversions */
#define CPU_GHZ_TO_HZ           1000000000
#define CPU_GHZ_TO_KHZ          1000000
#define CPU_GHZ_TO_MHZ          1000
#define CPU_MHZ_TO_HZ           1000000
#define CPU_MHZ_TO_KHZ          1000


/* Number of GPRs saved / restored for guest in VCPU structure */
#define NUM_GPRS                            16U

#define XSAVE_STATE_AREA_SIZE			4096U
#define XSAVE_LEGACY_AREA_SIZE			512U
#define XSAVE_HEADER_AREA_SIZE			64U
#define XSAVE_EXTEND_AREA_SIZE			(XSAVE_STATE_AREA_SIZE - \
						XSAVE_HEADER_AREA_SIZE - \
						XSAVE_LEGACY_AREA_SIZE)
#define XSAVE_COMPACTED_FORMAT			(1UL << 63U)

#define XSAVE_FPU				(1UL << 0U)
#define XSAVE_SSE				(1UL << 1U)

#define	CPU_CONTEXT_OFFSET_RAX			0U
#define	CPU_CONTEXT_OFFSET_RCX			8U
#define	CPU_CONTEXT_OFFSET_RDX			16U
#define	CPU_CONTEXT_OFFSET_RBX			24U
#define	CPU_CONTEXT_OFFSET_RSP			32U
#define	CPU_CONTEXT_OFFSET_RBP			40U
#define	CPU_CONTEXT_OFFSET_RSI			48U
#define	CPU_CONTEXT_OFFSET_RDI			56U
#define	CPU_CONTEXT_OFFSET_R8			64U
#define	CPU_CONTEXT_OFFSET_R9			72U
#define	CPU_CONTEXT_OFFSET_R10			80U
#define	CPU_CONTEXT_OFFSET_R11			88U
#define	CPU_CONTEXT_OFFSET_R12			96U
#define	CPU_CONTEXT_OFFSET_R13			104U
#define	CPU_CONTEXT_OFFSET_R14			112U
#define	CPU_CONTEXT_OFFSET_R15			120U
#define	CPU_CONTEXT_OFFSET_CR0			128U
#define	CPU_CONTEXT_OFFSET_CR2			136U
#define	CPU_CONTEXT_OFFSET_CR4			144U
#define	CPU_CONTEXT_OFFSET_RIP			152U
#define	CPU_CONTEXT_OFFSET_RFLAGS		160U
#define	CPU_CONTEXT_OFFSET_IA32_SPEC_CTRL	168U
#define	CPU_CONTEXT_OFFSET_IA32_EFER		176U
#define	CPU_CONTEXT_OFFSET_EXTCTX_START		184U
#define	CPU_CONTEXT_OFFSET_CR3			184U
#define	CPU_CONTEXT_OFFSET_IDTR			192U
#define	CPU_CONTEXT_OFFSET_LDTR			216U

#ifndef ASSEMBLER

/**
 *
 * Identifiers for architecturally defined registers.
 *
 * These register names is used in condition statement.
 * Within the following groups,register name need to be
 * kept in order:
 * General register names group (CPU_REG_RAX~CPU_REG_R15);
 * Non general register names group (CPU_REG_CR0~CPU_REG_GDTR);
 * Segement register names group (CPU_REG_ES~CPU_REG_GS).
 */
enum cpu_reg_name {
	/* General purpose register layout should align with
	 * struct acrn_gp_regs
	 */
	CPU_REG_RAX,
	CPU_REG_RCX,
	CPU_REG_RDX,
	CPU_REG_RBX,
	CPU_REG_RSP,
	CPU_REG_RBP,
	CPU_REG_RSI,
	CPU_REG_RDI,
	CPU_REG_R8,
	CPU_REG_R9,
	CPU_REG_R10,
	CPU_REG_R11,
	CPU_REG_R12,
	CPU_REG_R13,
	CPU_REG_R14,
	CPU_REG_R15,

	CPU_REG_CR0,
	CPU_REG_CR2,
	CPU_REG_CR3,
	CPU_REG_CR4,
	CPU_REG_DR7,
	CPU_REG_RIP,
	CPU_REG_RFLAGS,
	/*CPU_REG_NATURAL_LAST*/
	CPU_REG_EFER,
	CPU_REG_PDPTE0,
	CPU_REG_PDPTE1,
	CPU_REG_PDPTE2,
	CPU_REG_PDPTE3,
	/*CPU_REG_64BIT_LAST,*/
	CPU_REG_ES,
	CPU_REG_CS,
	CPU_REG_SS,
	CPU_REG_DS,
	CPU_REG_FS,
	CPU_REG_GS,
	CPU_REG_LDTR,
	CPU_REG_TR,
	CPU_REG_IDTR,
	CPU_REG_GDTR
	/*CPU_REG_LAST*/
};

/* stack_frame is linked with the sequence of stack operation in arch_switch_to() */
struct stack_frame {
	uint64_t rdi;
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t rbp;
	uint64_t rbx;
	uint64_t rflag;
	uint64_t rip;
	uint64_t magic;
};

/**********************************/
/* EXTERNAL VARIABLES             */
/**********************************/

/* In trampoline range, hold the jump target which trampline will jump to */
extern uint64_t               main_entry[1];
extern uint64_t               secondary_cpu_stack[1];

/*
 * To support per_cpu access, we use a special struct "per_cpu_region" to hold
 * the pattern of per CPU data. And we allocate memory for per CPU data
 * according to multiple this struct size and pcpu number.
 *
 *   +-------------------+------------------+---+------------------+
 *   | percpu for pcpu0  | percpu for pcpu1 |...| percpu for pcpuX |
 *   +-------------------+------------------+---+------------------+
 *   ^                   ^
 *   |                   |
 *   <per_cpu_region size>
 *
 * To access per cpu data, we use:
 *   per_cpu_base_ptr + sizeof(struct per_cpu_region) * curr_pcpu_id
 *   + offset_of_member_per_cpu_region
 * to locate the per cpu data.
 */

/**
 *The broadcast id (BROADCAST_CPU_ID)
 *used to notify all valid phyiscal cpu
 *or virtual cpu.
 */
#define BROADCAST_CPU_ID 0xfffeU

#define DEL_MODE_INIT		(1U)
#define DEL_MODE_IPI		(2U)

#define IDLE_MODE_PAUSE		(1U)
#define IDLE_MODE_HLT		(2U)

struct descriptor_table {
	uint16_t limit;
	uint64_t base;
} __packed;

#define	NEED_OFFLINE		(1U)
#define	NEED_SHUTDOWN_VM	(2U)
void make_pcpu_offline(uint16_t pcpu_id);
bool need_offline(uint16_t pcpu_id);

struct segment_sel {
	uint16_t selector;
	uint64_t base;
	uint32_t limit;
	uint32_t attr;
};

/**
 * @brief registers info saved for vcpu running context
 */
struct run_context {
/* Contains the guest register set.
 * NOTE: This must be the first element in the structure, so that the offsets
 * in vmx_asm.S match
 */
	union cpu_regs_t {
		struct acrn_gp_regs regs;
		uint64_t longs[NUM_GPRS];
	} cpu_regs;

	/** The guests CR registers 0, 2, 3 and 4. */
	uint64_t cr0;

	/* CPU_CONTEXT_OFFSET_CR2 =
	 * offsetof(struct run_context, cr2) = 136
	 */
	uint64_t cr2;
	uint64_t cr4;

	uint64_t rip;
	uint64_t rflags;

	/* CPU_CONTEXT_OFFSET_IA32_SPEC_CTRL =
	 * offsetof(struct run_context, ia32_spec_ctrl) = 168
	 */
	uint64_t ia32_spec_ctrl;
	uint64_t ia32_efer;
};

union xsave_header {
	uint64_t value[XSAVE_HEADER_AREA_SIZE / sizeof(uint64_t)];
	struct {
		/* bytes 7:0 */
		uint64_t xstate_bv;
		/* bytes 15:8 */
		uint64_t xcomp_bv;
	} hdr;
};

struct xsave_area {
	uint64_t legacy_region[XSAVE_LEGACY_AREA_SIZE / sizeof(uint64_t)];
	union xsave_header xsave_hdr;
	uint64_t extend_region[XSAVE_EXTEND_AREA_SIZE / sizeof(uint64_t)];
} __aligned(64);
/*
 * extended context does not save/restore during vm exit/entry, it's mainly
 * used in trusty world switch
 */
struct ext_context {
	uint64_t cr3;

	/* segment registers */
	struct segment_sel idtr;
	struct segment_sel ldtr;
	struct segment_sel gdtr;
	struct segment_sel tr;
	struct segment_sel cs;
	struct segment_sel ss;
	struct segment_sel ds;
	struct segment_sel es;
	struct segment_sel fs;
	struct segment_sel gs;

	uint64_t ia32_star;
	uint64_t ia32_cstar;
	uint64_t ia32_lstar;
	uint64_t ia32_fmask;
	uint64_t ia32_kernel_gs_base;

	uint64_t ia32_pat;
	uint32_t ia32_sysenter_cs;
	uint64_t ia32_sysenter_esp;
	uint64_t ia32_sysenter_eip;
	uint64_t ia32_debugctl;

	uint64_t dr7;
	uint64_t tsc_offset;
	uint64_t tsc_aux;

	struct xsave_area xs_area;
	uint64_t xcr0;
};

struct cpu_context {
	struct run_context run_ctx;
	struct ext_context ext_ctx;
};

/*
 * Definition of the stack frame layout
 */
struct intr_excp_ctx {
	struct acrn_gp_regs gp_regs;
	uint64_t vector;
	uint64_t error_code;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
};

void dispatch_exception(struct intr_excp_ctx *ctx);

/**
 * @brief Handle NMI
 *
 * To handle an NMI
 *
 * @param ctx Pointer to interrupt exception context
 */
void handle_nmi(__unused struct intr_excp_ctx *ctx);

/* Function prototypes */
void cpu_do_idle(void);
void cpu_dead(void);
void trampoline_start16(void);
void load_pcpu_state_data(void);
void init_pcpu_pre(bool is_bsp);
/* The function should be called on the same CPU core as specified by pcpu_id,
 * hereby, pcpu_id is actually the current physcial cpu id.
 */
void init_pcpu_post(uint16_t pcpu_id);
void wait_pcpus_offline(uint64_t mask);
void stop_pcpus(void);
void wait_sync_change(volatile const uint64_t *sync, uint64_t wake_sync);

#define CPU_SEG_READ(seg, result_ptr)						\
{										\
	asm volatile ("mov %%" STRINGIFY(seg) ", %0": "=r" (*(result_ptr)));	\
}

/* Read control register */
#define CPU_CR_READ(cr, result_ptr)				\
{								\
	asm volatile ("mov %%" STRINGIFY(cr) ", %0"		\
			: "=r"(*(result_ptr)));			\
}

/* Write control register */
#define CPU_CR_WRITE(cr, value)					\
{								\
	asm volatile ("mov %0, %%" STRINGIFY(cr)		\
			: /* No output */			\
			: "r"(value));				\
}

#define CPU_XMM_READ(xmm, result_m128i_p)						\
{										\
	asm volatile ("movdqu %%" STRINGIFY(xmm) ", %0": "=m" (*(result_m128i_p)));	\
}

#define CPU_XMM_WRITE(xmm, input_m128i_p)					\
{								\
	asm volatile ("movdqu %0, %%" STRINGIFY(xmm)		\
			: /* No output */			\
			: "m"(*(input_m128i_p)));				\
}

static inline uint64_t sgdt(void)
{
	struct descriptor_table gdtb = {0U, 0UL};
	asm volatile ("sgdt %0":"=m"(gdtb)::"memory");
	return gdtb.base;
}

static inline uint64_t sidt(void)
{
	struct descriptor_table idtb = {0U, 0UL};
	asm volatile ("sidt %0":"=m"(idtb)::"memory");
	return idtb.base;
}

/* Read MSR */
static inline uint64_t cpu_msr_read(uint32_t reg)
{
	uint32_t  msrl, msrh;

	asm volatile (" rdmsr ":"=a"(msrl), "=d"(msrh) : "c" (reg));
	return (((uint64_t)msrh << 32U) | msrl);
}

/* Write MSR */
static inline void cpu_msr_write(uint32_t reg, uint64_t msr_val)
{
	asm volatile (" wrmsr " : : "c" (reg), "a" ((uint32_t)msr_val), "d" ((uint32_t)(msr_val >> 32U)));
}

static inline void arch_asm_pause(void)
{
	asm volatile ("pause" ::: "memory");
}

static inline void asm_hlt(void)
{
	asm volatile ("hlt");
}

/* interrupts remain inhibited on the instruction boundary following
 * an execution of STI. This will make sure pending interrupts will
 * wake hlt.
 */
static inline void asm_safe_hlt(void)
{
	asm volatile ("sti; hlt; cli" : : : "cc");
}

/* Enables interrupts on the current CPU
 * If CONFIG_KEEP_IRQ_DISABLED is 'y', all interrupts
 * received in root mode will be handled in external interrupt
 * vmexit after next VM entry. The postpone latency is negligible.
 * Permanently turning off interrupts in root mode can be useful in
 * many scenarios (e.g., x86_tee).
 */
static inline void arch_local_irq_enable(void)
{
#ifndef CONFIG_KEEP_IRQ_DISABLED
	asm volatile ("sti\n" : : : "cc");
#endif
}

/* Disables interrupts on the current CPU */
static inline void arch_local_irq_disable(void)
{
#ifndef CONFIG_KEEP_IRQ_DISABLED
	asm volatile ("cli\n" : : : "cc");
#endif
}

/* This macro writes the stack pointer. */
static inline void cpu_sp_write(uint64_t *stack_ptr)
{
	uint64_t rsp = (uint64_t)stack_ptr & ~(CPU_STACK_ALIGN - 1UL);

	asm volatile ("movq %0, %%rsp" : : "r"(rsp));
}

static inline void invlpg(unsigned long addr)
{
	asm volatile("invlpg (%0)" ::"r" (addr) : "memory");
}

static inline void wbinvd(void)
{
	asm volatile ("   wbinvd\n" : : : "memory");
}

static inline void clflush(const volatile void *p)
{
	asm volatile ("clflush (%0)" :: "r"(p));
}

static inline void clflushopt(const volatile void *p)
{
	asm volatile ("clflushopt (%0)" :: "r"(p));
}

/* Write the task register */
#define CPU_LTR_EXECUTE(ltr_ptr)                            \
{                                                           \
	asm volatile ("ltr %%ax\n" : : "a"(ltr_ptr));       \
}

/* Read time-stamp counter / processor ID */
static inline void
cpu_rdtscp_execute(uint64_t *timestamp_ptr, uint32_t *cpu_id_ptr)
{
	uint32_t tsl, tsh;

	asm volatile ("rdtscp":"=a"(tsl), "=d"(tsh), "=c"(*cpu_id_ptr));
	*timestamp_ptr = ((uint64_t)tsh << 32U) | tsl;
}

static inline void cpu_rflags_save(uint64_t *rflags_ptr)
{
	asm volatile (" pushf \n\t"
			" pop %0"
			: "=r" (*(rflags_ptr))
			: /* No inputs */);
}

static inline void cpu_rflags_restore(uint64_t rflags)
{
	asm volatile (" push %0\n\t"
			"popf	\n\t": : "r" (rflags)
			:"cc");
}

static inline void arch_local_irq_save(uint64_t *flags_ptr)
{
	cpu_rflags_save(flags_ptr);
	arch_local_irq_disable();
}

static inline void arch_local_irq_restore(uint64_t flags)
{
	cpu_rflags_restore(flags);
}

#define ACRN_PSEUDO_PCPUID_MSR		MSR_IA32_SYSENTER_CS

/*
 * Macro to get CPU ID
 * @pre: the return CPU ID would never equal or large than phys_cpu_num.
 */
static inline uint16_t arch_get_pcpu_id(void)
{
	return (uint16_t)cpu_msr_read(ACRN_PSEUDO_PCPUID_MSR);
}

static inline void arch_set_current_pcpu_id(uint16_t pcpu_id)
{
	cpu_msr_write(ACRN_PSEUDO_PCPUID_MSR, (uint32_t)pcpu_id);
}

static inline uint64_t cpu_rsp_get(void)
{
	uint64_t ret;

	asm volatile("movq %%rsp, %0" :  "=r"(ret));
	return ret;
}

static inline uint64_t cpu_rbp_get(void)
{
	uint64_t ret;

	asm volatile("movq %%rbp, %0" :  "=r"(ret));
	return ret;
}

static inline uint64_t msr_read(uint32_t reg_num)
{
	return cpu_msr_read(reg_num);
}

static inline void msr_write(uint32_t reg_num, uint64_t value64)
{
	cpu_msr_write(reg_num, value64);
}

static inline int32_t msr_write_safe(uint32_t reg_num, uint64_t value64, uint64_t rsvd)
{
	int32_t err = 0;

	if ((value64 & rsvd) == 0) {
		msr_write(reg_num, value64);
	} else {
		err = -EACCES;
	}
	return err;
}

/* wrmsr/rdmsr smp call data */
struct msr_data_struct {
	uint32_t msr_index;
	uint64_t read_val;
	uint64_t write_val;
};

void msr_write_pcpu(uint32_t msr_index, uint64_t value64, uint16_t pcpu_id);
uint64_t msr_read_pcpu(uint32_t msr_index, uint16_t pcpu_id);

static inline void write_xcr(int32_t reg, uint64_t val)
{
	asm volatile("xsetbv" : : "c" (reg), "a" ((uint32_t)val), "d" ((uint32_t)(val >> 32U)));
}

static inline uint64_t read_xcr(int32_t reg)
{
	uint32_t  xcrl, xcrh;

	asm volatile ("xgetbv ": "=a"(xcrl), "=d"(xcrh) : "c" (reg));
	return (((uint64_t)xcrh << 32U) | xcrl);
}

static inline void xsaves(struct xsave_area *region_addr, uint64_t mask)
{
	asm volatile("xsaves %0"
			: : "m" (*(region_addr)),
			"d" ((uint32_t)(mask >> 32U)),
			"a" ((uint32_t)mask):
			"memory");
}

static inline void xrstors(const struct xsave_area *region_addr, uint64_t mask)
{
	asm volatile("xrstors %0"
			: : "m" (*(region_addr)),
			"d" ((uint32_t)(mask >> 32U)),
			"a" ((uint32_t)mask):
			"memory");
}

static inline void asm_loadiwkey(uint32_t eax)
{
	asm volatile(".byte 0xf3, 0x0f, 0x38, 0xdc, 0xd1;": : "a" (eax));
}

static inline void read_xmm_0_2(uint64_t *xmm0_addr, uint64_t *xmm1_addr, uint64_t *xmm2_addr)
{
	CPU_XMM_READ(xmm0, xmm0_addr);
	CPU_XMM_READ(xmm1, xmm1_addr);
	CPU_XMM_READ(xmm2, xmm2_addr);
}

static inline void write_xmm_0_2(uint64_t *xmm0_addr, uint64_t *xmm1_addr, uint64_t *xmm2_addr)
{
	CPU_XMM_WRITE(xmm0, xmm0_addr);
	CPU_XMM_WRITE(xmm1, xmm1_addr);
	CPU_XMM_WRITE(xmm2, xmm2_addr);
}

/*
 * stac/clac pair is used to access guest's memory protected by SMAP,
 * following below flow:
 *
 *	stac();
 *	#access guest's memory.
 *	clac();
 *
 * Notes:Avoid inserting another stac/clac pair between stac and clac,
 *	As once clac after multiple stac will invalidate SMAP protection
 *	and hence Page Fault crash.
 *	Logging message to memory buffer will induce this case,
 *	please disable SMAP temporlly or don't log messages to shared
 *	memory buffer, if it is evitable for you for debug purpose.
 */
static inline void stac(void)
{
	asm volatile ("stac" : : : "memory");
}

static inline void clac(void)
{
	asm volatile ("clac" : : : "memory");
}

#else /* ASSEMBLER defined */

#endif /* ASSEMBLER defined */

#endif /* CPU_H */
