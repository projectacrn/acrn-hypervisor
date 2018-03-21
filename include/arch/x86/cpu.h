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

/* Define page size */
#define CPU_PAGE_SHIFT          12
#define CPU_PAGE_SIZE           0x1000
#define CPU_PAGE_MASK           0xFFFFFFFFFFFFF000

/* Define CPU stack alignment */
#define CPU_STACK_ALIGN         16

/* CR0 register definitions */
#define CR0_PG                  (1<<31)	/* paging enable */
#define CR0_CD                  (1<<30)	/* cache disable */
#define CR0_NW                  (1<<29)	/* not write through */
#define CR0_AM                  (1<<18)	/* alignment mask */
#define CR0_WP                  (1<<16)	/* write protect */
#define CR0_NE                  (1<<5)	/* numeric error */
#define CR0_ET                  (1<<4)	/* extension type */
#define CR0_TS                  (1<<3)	/* task switched */
#define CR0_EM                  (1<<2)	/* emulation */
#define CR0_MP                  (1<<1)	/* monitor coprocessor */
#define CR0_PE                  (1<<0)	/* protected mode enabled */

/* CR3 register definitions */
#define CR3_PWT                 (1<<3)	/* page-level write through */
#define CR3_PCD                 (1<<4)	/* page-level cache disable */

/* CR4 register definitions */
#define CR4_VME                 (1<<0)	/* virtual 8086 mode extensions */
#define CR4_PVI                 (1<<1)	/* protected mode virtual interrupts */
#define CR4_TSD                 (1<<2)	/* time stamp disable */
#define CR4_DE                  (1<<3)	/* debugging extensions */
#define CR4_PSE                 (1<<4)	/* page size extensions */
#define CR4_PAE                 (1<<5)	/* physical address extensions */
#define CR4_MCE                 (1<<6)	/* machine check enable */
#define CR4_PGE                 (1<<7)	/* page global enable */
#define CR4_PCE                 (1<<8)
/* performance monitoring counter enable */
#define CR4_OSFXSR              (1<<9)	/* OS support for FXSAVE/FXRSTOR */
#define CR4_OSXMMEXCPT          (1<<10)
/* OS support for unmasked SIMD floating point exceptions */
#define CR4_VMXE                (1<<13)	/* VMX enable */
#define CR4_SMXE                (1<<14)	/* SMX enable */
#define CR4_PCIDE               (1<<17)	/* PCID enable */
#define CR4_OSXSAVE             (1<<18)
/* XSAVE and Processor Extended States enable bit */


/*
 * Entries in the Interrupt Descriptor Table (IDT)
 */
#define IDT_DE      0   /* #DE: Divide Error */
#define IDT_DB      1   /* #DB: Debug */
#define IDT_NMI     2   /* Nonmaskable External Interrupt */
#define IDT_BP      3   /* #BP: Breakpoint */
#define IDT_OF      4   /* #OF: Overflow */
#define IDT_BR      5   /* #BR: Bound Range Exceeded */
#define IDT_UD      6   /* #UD: Undefined/Invalid Opcode */
#define IDT_NM      7   /* #NM: No Math Coprocessor */
#define IDT_DF      8   /* #DF: Double Fault */
#define IDT_FPUGP   9   /* Coprocessor Segment Overrun */
#define IDT_TS      10  /* #TS: Invalid TSS */
#define IDT_NP      11  /* #NP: Segment Not Present */
#define IDT_SS      12  /* #SS: Stack Segment Fault */
#define IDT_GP      13  /* #GP: General Protection Fault */
#define IDT_PF      14  /* #PF: Page Fault */
#define IDT_MF      16  /* #MF: FPU Floating-Point Error */
#define IDT_AC      17  /* #AC: Alignment Check */
#define IDT_MC      18  /* #MC: Machine Check */
#define IDT_XF      19  /* #XF: SIMD Floating-Point Exception */

/*Bits in EFER special registers */
#define EFER_LMA 0x000000400    /* Long mode active (R) */

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
#define CPU_BOOT_ID             0

/* CPU states defined */
#define CPU_STATE_RESET         0
#define CPU_STATE_INITIALIZING  1
#define CPU_STATE_RUNNING       2
#define CPU_STATE_HALTED        3
#define CPU_STATE_DEAD          4

/* hypervisor stack bottom magic('intl') */
#define SP_BOTTOM_MAGIC    0x696e746c

/* type of speculation control
 * 0 - no speculation control support
 * 1 - raw IBRS + IPBP support
 * 2 - with STIBP optimization support
 */
#define IBRS_NONE	0
#define IBRS_RAW	1
#define IBRS_OPT	2

#ifndef ASSEMBLER

/**********************************/
/* EXTERNAL VARIABLES             */
/**********************************/
extern const uint8_t          _ld_cpu_secondary_reset_load[];
extern uint8_t                _ld_cpu_secondary_reset_start[];
extern const uint64_t         _ld_cpu_secondary_reset_size;
extern uint8_t                _ld_bss_start[];
extern uint8_t                _ld_bss_end[];
extern uint8_t		      _ld_cpu_data_start[];
extern uint8_t                _ld_cpu_data_end[];

extern int ibrs_type;

/*
 * To support per_cpu access, we use a special section ".cpu_data" to define
 * the pattern of per CPU data. And we allocate memory for per CPU data
 * according to multiple this section size and pcpu number.
 *
 *   +------------------+------------------+---+------------------+
 *   | percpu for pcpu0 | percpu for pcpu1 |...| percpu for pcpuX |
 *   +------------------+------------------+---+------------------+
 *   ^                  ^
 *   |                  |
 *    --.cpu_data size--
 *
 * To access per cpu data, we use:
 *   per_cpu_data_base_ptr + curr_pcpu_id * cpu_data_section_size +
 *        offset_of_symbol_in_cpu_data_section
 * to locate the per cpu data.
 */

/* declare per cpu data */
#define EXTERN_CPU_DATA(type, name)					\
	extern __typeof__(type) cpu_data_##name

EXTERN_CPU_DATA(uint8_t, lapic_id);
EXTERN_CPU_DATA(void *, vcpu);
EXTERN_CPU_DATA(uint8_t[STACK_SIZE], stack) __aligned(16);

/* define per cpu data */
#define DEFINE_CPU_DATA(type, name)					\
	__typeof__(type) cpu_data_##name				\
	__attribute__((__section__(".cpu_data")))

extern void *per_cpu_data_base_ptr;
extern int phy_cpu_num;

#define	PER_CPU_DATA_OFFSET(sym_addr)					\
	((uint64_t)(sym_addr) - (uint64_t)(_ld_cpu_data_start))

#define	PER_CPU_DATA_SIZE						\
	((uint64_t)_ld_cpu_data_end - (uint64_t)(_ld_cpu_data_start))

/*
 * get percpu data for pcpu_id.
 *
 * It returns:
 *   per_cpu_data_##name[pcpu_id];
 */
#define per_cpu(name, pcpu_id)						\
	(*({ uint64_t base = (uint64_t)per_cpu_data_base_ptr;		\
		uint64_t off = PER_CPU_DATA_OFFSET(&cpu_data_##name);	\
		((typeof(&cpu_data_##name))(base +			\
		  (pcpu_id) * PER_CPU_DATA_SIZE + off));		\
	}))

/* get percpu data for current pcpu */
#define get_cpu_var(name)	per_cpu(name, get_cpu_id())

/* Function prototypes */
void cpu_halt(uint32_t logical_id);
uint64_t cpu_cycles_per_second(void);
uint64_t tsc_cycles_in_period(uint16_t timer_period_in_us);
void cpu_secondary_reset(void);
int hv_main(int cpu_id);
bool check_tsc_adjust_support(void);
bool check_ibrs_ibpb_support(void);
bool check_stibp_support(void);
bool is_vapic_supported(void);
bool is_vapic_intr_delivery_supported(void);
bool is_vapic_virt_reg_supported(void);

/* Read control register */
#define CPU_CR_READ(cr, result_ptr)                         \
{                                                           \
	asm volatile ("mov %%" __CPP_STRING(cr) ", %0"      \
			: "=r"(*result_ptr));               \
}

/* Write control register */
#define CPU_CR_WRITE(cr, value)                             \
{                                                           \
	asm volatile ("mov %0, %%" __CPP_STRING(cr)         \
			: /* No output */                   \
			: "r"(value));                      \
}

/* Read MSR */
#define CPU_MSR_READ(reg, msr_val_ptr)                      \
{                                                           \
	uint32_t  msrl, msrh;                                 \
	asm volatile (" rdmsr ":"=a"(msrl),                 \
			"=d"(msrh) : "c" (reg));            \
	*msr_val_ptr = ((uint64_t)msrh<<32) | msrl;           \
}

/* Write MSR */
#define CPU_MSR_WRITE(reg, msr_val)                         \
{                                                           \
	uint32_t msrl, msrh;                                  \
	msrl = (uint32_t)msr_val;                             \
	msrh = (uint32_t)(msr_val >> 32);                     \
	asm volatile (" wrmsr " : : "c" (reg),              \
			"a" (msrl), "d" (msrh));            \
}

/* Disables interrupts on the current CPU */
#define CPU_IRQ_DISABLE()                                   \
{                                                           \
	asm volatile ("cli\n" : : : "cc");                  \
}

/* Enables interrupts on the current CPU */
#define CPU_IRQ_ENABLE()                                    \
{                                                           \
	asm volatile ("sti\n" : : : "cc");                  \
}

/* This macro writes the stack pointer. */
#define CPU_SP_WRITE(stack_ptr)                             \
{                                                           \
	uint64_t rsp = (uint64_t)stack_ptr & ~(CPU_STACK_ALIGN - 1);  \
	asm volatile ("movq %0, %%rsp" : : "r"(rsp));             \
}

/* Synchronizes all read accesses from memory */
#define CPU_MEMORY_READ_BARRIER()                           \
{                                                           \
	asm volatile ("lfence\n" : : : "memory");           \
}

/* Synchronizes all write accesses to memory */
#define CPU_MEMORY_WRITE_BARRIER()                          \
{                                                           \
	asm volatile ("sfence\n" : : : "memory");           \
}

/* Synchronizes all read and write accesses to/from memory */
#define CPU_MEMORY_BARRIER()                                \
{                                                           \
	asm volatile ("mfence\n" : : : "memory");           \
}

/* Write the task register */
#define CPU_LTR_EXECUTE(ltr_ptr)                            \
{                                                           \
	asm volatile ("ltr %%ax\n" : : "a"(ltr_ptr));       \
}

/* Read time-stamp counter / processor ID */
#define CPU_RDTSCP_EXECUTE(timestamp_ptr, cpu_id_ptr)       \
{                                                           \
	uint32_t  tsl, tsh;                                   \
	asm volatile ("rdtscp":"=a"(tsl), "=d"(tsh),        \
			"=c"(*cpu_id_ptr));                 \
	*timestamp_ptr = ((uint64_t)tsh << 32) | tsl;         \
}

/* Define variable(s) required to save / restore architecture interrupt state.
 * These variable(s) are used in conjunction with the ESAL_AR_INT_ALL_DISABLE()
 * and ESAL_AR_INT_ALL_RESTORE() macros to hold any data that must be preserved
 * in order to allow these macros to function correctly.
 */
#define         CPU_INT_CONTROL_VARS                uint64_t    cpu_int_value

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
	asm volatile (" push %0" : : "r" (rflags));     \
	asm volatile (" popf");                         \
}

/* This macro locks out interrupts and saves the current architecture status
 * register / state register to the specified address.  This function does not
 * attempt to mask any bits in the return register value and can be used as a
 * quick method to guard a critical section.
 * NOTE:  This macro is used in conjunction with CPU_INT_ALL_RESTORE
 *        defined below and CPU_INT_CONTROL_VARS defined above.
 */

#define CPU_INT_ALL_DISABLE()                       \
{                                                   \
	CPU_RFLAGS_SAVE(&cpu_int_value);            \
	CPU_IRQ_DISABLE();                          \
}

/* This macro restores the architecture status / state register used to lockout
 * interrupts to the value provided.  The intent of this function is to be a
 * fast mechanism to restore the interrupt level at the end of a critical
 * section to its original level.
 * NOTE:  This macro is used in conjunction with CPU_INT_ALL_DISABLE
 *        and CPU_INT_CONTROL_VARS defined above.
 */
#define CPU_INT_ALL_RESTORE()                       \
{                                                   \
	CPU_RFLAGS_RESTORE(cpu_int_value);          \
}

/* Macro to get CPU ID */
static inline uint32_t get_cpu_id(void)
{
	uint32_t tsl, tsh, cpu_id;

	asm volatile ("rdtscp":"=a" (tsl), "=d"(tsh), "=c"(cpu_id)::);
	return cpu_id;
}

static inline uint64_t cpu_rsp_get(void)
{
	uint64_t ret;

	asm volatile("movq %%rsp, %0"
			:  "=r"(ret));
	return ret;
}

static inline uint64_t cpu_rbp_get(void)
{
	uint64_t ret;

	asm volatile("movq %%rbp, %0"
			:  "=r"(ret));
	return ret;
}



static inline uint64_t
msr_read(uint32_t reg_num)
{
	uint64_t msr_val;

	CPU_MSR_READ(reg_num, &msr_val);
	return msr_val;
}

static inline void
msr_write(uint32_t reg_num, uint64_t value64)
{
	CPU_MSR_WRITE(reg_num, value64);
}

#else /* ASSEMBLER defined */

#endif /* ASSEMBLER defined */

#endif /* CPU_H */
