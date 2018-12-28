/*-
 * Copyright (c) 1989, 1990 William F. Jolitz
 * Copyright (c) 1990 The Regents of the University of California.
 * Copyright (c) 2017 Intel Corporation
 * All rights reserved.
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
#define CR4_VMXE                (1UL<<13U)	/* VMX enable */
#define CR4_SMXE                (1UL<<14U)	/* SMX enable */
#define CR4_FSGSBASE            (1UL<<16U)	/* RD(FS|GS|FS)BASE inst */
#define CR4_PCIDE               (1UL<<17U)	/* PCID enable */
#define CR4_OSXSAVE             (1UL<<18U)
/* XSAVE and Processor Extended States enable bit */
#define CR4_SMEP                (1UL<<20U)
#define CR4_SMAP                (1UL<<21U)
#define CR4_PKE                 (1UL<<22U)	/* Protect-key-enable */


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

/* Boot CPU ID */
#define BOOT_CPU_ID             0U

#ifndef ASSEMBLER

#define	BUS_LOCK	"lock ; "

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

/**********************************/
/* EXTERNAL VARIABLES             */
/**********************************/
extern uint8_t		ld_bss_start;
extern uint8_t		ld_bss_end;

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
 *The invalid cpu_id (INVALID_CPU_ID) is error
 *code for error handling, this means that
 *caller can't find a valid physical cpu
 *or virtual cpu.
 */
#define INVALID_CPU_ID 0xffffU
/**
 *The broadcast id (BROADCAST_CPU_ID)
 *used to notify all valid phyiscal cpu
 *or virtual cpu.
 */
#define BROADCAST_CPU_ID 0xfffeU

struct descriptor_table {
	uint16_t limit;
	uint64_t base;
} __packed;

/* CPU states defined */
enum pcpu_boot_state {
	PCPU_STATE_RESET = 0U,
	PCPU_STATE_INITIALIZING,
	PCPU_STATE_RUNNING,
	PCPU_STATE_HALTED,
	PCPU_STATE_DEAD,
};

/* Function prototypes */
void cpu_do_idle(void);
void cpu_dead(void);
void trampoline_start16(void);
void load_cpu_state_data(void);
void init_cpu_pre(uint16_t pcpu_id_args);
void init_cpu_post(uint16_t pcpu_id);
void start_cpus(void);
void stop_cpus(void);
void wait_sync_change(uint64_t *sync, uint64_t wake_sync);

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

static inline void asm_pause(void)
{
	asm volatile ("pause" ::: "memory");
}

static inline void asm_hlt(void)
{
	asm volatile ("hlt");
}

#ifdef CONFIG_PARTITION_MODE
#define CPU_IRQ_DISABLE()
#else
/* Disables interrupts on the current CPU */
#define CPU_IRQ_DISABLE()                                   \
{                                                           \
	asm volatile ("cli\n" : : : "cc");                  \
}
#endif

#ifdef CONFIG_PARTITION_MODE
#define CPU_IRQ_ENABLE()
#else
/* Enables interrupts on the current CPU */
#define CPU_IRQ_ENABLE()                                    \
{                                                           \
	asm volatile ("sti\n" : : : "cc");                  \
}
#endif

/* This macro writes the stack pointer. */
static inline void cpu_sp_write(uint64_t *stack_ptr)
{
	uint64_t rsp = (uint64_t)stack_ptr & ~(CPU_STACK_ALIGN - 1UL);

	asm volatile ("movq %0, %%rsp" : : "r"(rsp));
}

/* Synchronizes all read and write accesses to/from memory */
static inline void cpu_memory_barrier(void)
{
	asm volatile ("mfence\n" : : : "memory");
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

/* Macro to save rflags register */
#define CPU_RFLAGS_SAVE(rflags_ptr)                     \
{                                                       \
	asm volatile (" pushf");                        \
	asm volatile (" pop %0"                         \
			: "=r" (*(rflags_ptr))          \
			: /* No inputs */);             \
}

/* Macro to restore rflags register */
#define CPU_RFLAGS_RESTORE(rflags)                      \
{                                                       \
	asm volatile (" push %0\n\t"			\
			"popf	\n\t": : "r" (rflags)	\
			:"cc");				\
}

/* This macro locks out interrupts and saves the current architecture status
 * register / state register to the specified address.  This function does not
 * attempt to mask any bits in the return register value and can be used as a
 * quick method to guard a critical section.
 * NOTE:  This macro is used in conjunction with CPU_INT_ALL_RESTORE
 *        defined below and CPU_INT_CONTROL_VARS defined above.
 */

#define CPU_INT_ALL_DISABLE(p_rflags)               \
{                                                   \
	CPU_RFLAGS_SAVE(p_rflags);	             \
	CPU_IRQ_DISABLE();                          \
}

/* This macro restores the architecture status / state register used to lockout
 * interrupts to the value provided.  The intent of this function is to be a
 * fast mechanism to restore the interrupt level at the end of a critical
 * section to its original level.
 * NOTE:  This macro is used in conjunction with CPU_INT_ALL_DISABLE
 *        and CPU_INT_CONTROL_VARS defined above.
 */
#define CPU_INT_ALL_RESTORE(rflags)                 \
{                                                   \
	CPU_RFLAGS_RESTORE(rflags);                 \
}

/*
 * Macro to get CPU ID
 * @pre: the return CPU ID would never equal or large than phys_cpu_num.
 */
static inline uint16_t get_cpu_id(void)
{
	uint32_t tsl, tsh, cpu_id;

	asm volatile ("rdtscp":"=a" (tsl), "=d"(tsh), "=c"(cpu_id)::);
	return (uint16_t)cpu_id;
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

static inline void write_xcr(int32_t reg, uint64_t val)
{
	asm volatile("xsetbv" : : "c" (reg), "a" ((uint32_t)val), "d" ((uint32_t)(val >> 32U)));
}

static inline void stac(void)
{
	asm volatile ("stac" : : : "memory");
}

static inline void clac(void)
{
	asm volatile ("clac" : : : "memory");
}

uint16_t get_pcpu_nums(void);
bool is_pcpu_active(uint16_t pcpu_id);
uint64_t get_active_pcpu_bitmap(void);
#else /* ASSEMBLER defined */

#endif /* ASSEMBLER defined */

#endif /* CPU_H */
