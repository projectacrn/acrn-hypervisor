/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * CSR definitions reference: opensbi riscv_encoding.h
 * The original code is licensed under BSD-2-Clause.
 */

#ifndef RISCV_CSR_H
#define RISCV_CSR_H

#ifndef ASSEMBLER

#include <types.h>

/* ===== Supervisor-level CSRs ===== */

/* Supervisor Trap Setup */
#define CSR_SSTATUS			0x100
#define CSR_SIE				0x104
#define CSR_STVEC			0x105
#define CSR_SCOUNTEREN			0x106

/* Supervisor Configuration */
#define CSR_SENVCFG			0x10a

/* Supervisor Conter Inhibit */
#define CSR_SCOUNTINHIBIT		0x120

/* Supervisor Trap Handling */
#define CSR_SSCRATCH			0x140
#define CSR_SEPC			0x141
#define CSR_SCAUSE			0x142
#define CSR_STVAL			0x143
#define CSR_SIP				0x144

/* Sstc extension */
#define CSR_STIMECMP			0x14D
#define CSR_STIMECMPH			0x15D

/* Supervisor Protection and Translation */
#define CSR_SATP			0x180

/* Supervisor Indirect Register Alias */
#define CSR_SISELECT			0x150
#define CSR_SIREG			0x151
#define CSR_SIREG2          		0x152
#define CSR_SIREG3          		0x153
#define CSR_SIREG4          		0x155
#define CSR_SIREG5          		0x156
#define CSR_SIREG6          		0x157

/* Supervisor-Level Interrupts (AIA) */
#define CSR_STOPEI			0x15c
#define CSR_STOPI			0xdb0

/* Supervisor-Level High-Half CSRs (AIA) */
#define CSR_SIEH			0x114
#define CSR_SIPH			0x154

/* Supervisor stateen CSRs */
#define CSR_SSTATEEN0			0x10C
#define CSR_SSTATEEN1			0x10D
#define CSR_SSTATEEN2			0x10E
#define CSR_SSTATEEN3			0x10F

/* Machine-Level Control transfer records CSRs */
#define CSR_MCTRCTL                     0x34e

/* Supervisor-Level Control transfer records CSRs */
#define CSR_SCTRCTL                     0x14e
#define CSR_SCTRSTATUS                  0x14f
#define CSR_SCTRDEPTH                   0x15f

/* VS-Level Control transfer records CSRs */
#define CSR_VSCTRCTL                    0x24e

/* ===== Hypervisor-level CSRs ===== */

/* Hypervisor Trap Setup (H-extension) */
#define CSR_HSTATUS			0x600
#define CSR_HEDELEG			0x602
#define CSR_HIDELEG			0x603
#define CSR_HIE				0x604
#define CSR_HCOUNTEREN			0x606
#define CSR_HGEIE			0x607

/* Hypervisor Configuration */
#define CSR_HENVCFG			0x60a
#define CSR_HENVCFGH			0x61a

/* Hypervisor Trap Handling (H-extension) */
#define CSR_HTVAL			0x643
#define CSR_HIP				0x644
#define CSR_HVIP			0x645
#define CSR_HTINST			0x64a
#define CSR_HGEIP			0xe12

/* Hypervisor Protection and Translation (H-extension) */
#define CSR_HGATP			0x680

/* Hypervisor Counter/Timer Virtualization Registers (H-extension) */
#define CSR_HTIMEDELTA			0x605
#define CSR_HTIMEDELTAH			0x615

/* Virtual Supervisor Registers (H-extension) */
#define CSR_VSSTATUS			0x200
#define CSR_VSIE			0x204
#define CSR_VSTVEC			0x205
#define CSR_VSSCRATCH			0x240
#define CSR_VSEPC			0x241
#define CSR_VSCAUSE			0x242
#define CSR_VSTVAL			0x243
#define CSR_VSIP			0x244
#define CSR_VSATP			0x280
#define CSR_VSTIMECMP 			0x24D

/* Virtual Supervisor Indirect Alias */
#define CSR_VSISELECT			0x250
#define CSR_VSIREG			0x251
#define CSR_VSIREG2         		0x252
#define CSR_VSIREG3         		0x253
#define CSR_VSIREG4         		0x255
#define CSR_VSIREG5         		0x256
#define CSR_VSIREG6         		0x257

/* Hypervisor stateen CSRs */
#define CSR_HSTATEEN0			0x60C
#define CSR_HSTATEEN0H			0x61C
#define CSR_HSTATEEN1			0x60D
#define CSR_HSTATEEN1H			0x61D
#define CSR_HSTATEEN2			0x60E
#define CSR_HSTATEEN2H			0x61E
#define CSR_HSTATEEN3			0x60F
#define CSR_HSTATEEN3H			0x61F

/* ===== Machine-level CSRs ===== */

/* Machine Information Registers */
#define CSR_MVENDORID			0xf11
#define CSR_MARCHID			0xf12
#define CSR_MIMPID			0xf13
#define CSR_MHARTID			0xf14
#define CSR_MCONFIGPTR			0xf15

/* Machine Trap Setup */
#define CSR_MSTATUS			0x300
#define CSR_MISA			0x301
#define CSR_MEDELEG			0x302
#define CSR_MIDELEG			0x303
#define CSR_MIE				0x304
#define CSR_MTVEC			0x305
#define CSR_MCOUNTEREN			0x306
#define CSR_MSTATUSH			0x310

/* Machine Configuration */
#define CSR_MENVCFG			0x30a
#define CSR_MENVCFGH			0x31a

/* Machine Trap Handling */
#define CSR_MSCRATCH			0x340
#define CSR_MEPC			0x341
#define CSR_MCAUSE			0x342
#define CSR_MTVAL			0x343
#define CSR_MIP				0x344
#define CSR_MTINST			0x34a
#define CSR_MTVAL2			0x34b

#define HSTATUS_VSXL        0x300000000UL
#define HSTATUS_VSXL_SHIFT  32
#define HSTATUS_VTSR        0x00400000UL
#define HSTATUS_VTW         0x00200000UL
#define HSTATUS_VTVM        0x00100000UL
#define HSTATUS_SPVP        0x00000100UL
#define HSTATUS_SPV         0x00000080UL
#define HSTATUS_VGEIN       0x0003f000UL
#define HSTATUS_VGEIN_SHIFT 12
#define HSTATUS_HU          0x00000200Ul
#define HSTATUS_GVA         0x00000040UL
#define HSTATUS_VSBE        0x00000020UL

#define MSTATUS_SIE			0x00000002UL
#define MSTATUS_MIE			0x00000008UL
#define MSTATUS_SPIE_SHIFT		5
#define MSTATUS_SPIE			(1UL << MSTATUS_SPIE_SHIFT)
#define MSTATUS_UBE			0x00000040UL
#define MSTATUS_MPIE			0x00000080UL
#define MSTATUS_SPP_SHIFT		8
#define MSTATUS_SPP			(1UL << MSTATUS_SPP_SHIFT)
#define MSTATUS_MPP_SHIFT		11
#define MSTATUS_MPP			(3UL << MSTATUS_MPP_SHIFT)
#define MSTATUS_FS			0x00006000UL
#define MSTATUS_XS			0x00018000UL
#define MSTATUS_VS			0x00000600UL
#define MSTATUS_MPRV			0x00020000UL
#define MSTATUS_SUM			0x00040000UL
#define MSTATUS_MXR			0x00080000UL
#define MSTATUS_TVM			0x00100000UL
#define MSTATUS_TW			0x00200000UL
#define MSTATUS_TSR			0x00400000UL
#define MSTATUS_SPELP			0x00800000UL
#define MSTATUS_SDT			0x01000000Ul
#define MSTATUS32_SD			0x80000000UL

#define MSTATUS_UXL			0x0000000300000000ULL
#define MSTATUS_SXL			0x0000000C00000000ULL
#define MSTATUS_SBE			0x0000001000000000ULL
#define MSTATUS_MBE			0x0000002000000000ULL
#define MSTATUS_GVA			0x0000004000000000ULL
#define MSTATUS_GVA_SHIFT		38
#define MSTATUS_MPV			0x0000008000000000ULL
#define MSTATUS_MPELP			0x0000020000000000ULL
#define MSTATUS_MDT			0x0000040000000000ULL

#define SSTATUS_SIE			MSTATUS_SIE
#define SSTATUS_SPIE_SHIFT		MSTATUS_SPIE_SHIFT
#define SSTATUS_SPIE			MSTATUS_SPIE
#define SSTATUS_SPP_SHIFT		MSTATUS_SPP_SHIFT
#define SSTATUS_SPP			MSTATUS_SPP
#define SSTATUS_FS			MSTATUS_FS
#define SSTATUS_XS			MSTATUS_XS
#define SSTATUS_VS			MSTATUS_VS
#define SSTATUS_SUM			MSTATUS_SUM
#define SSTATUS_MXR			MSTATUS_MXR

#define SCOUNTEREN_CY       0x1UL
#define SCOUNTEREN_TM       0x2UL
#define SCOUNTEREN_IR       0x4UL

#define ENVCFG_STCE			((1ULL) << 63)
#define ENVCFG_PBMTE			((1ULL) << 62)
#define ENVCFG_ADUE_SHIFT		61
#define ENVCFG_ADUE			((1ULL) << ENVCFG_ADUE_SHIFT)
#define ENVCFG_CDE			((1ULL) << 60)
#define ENVCFG_DTE_SHIFT		59
#define ENVCFG_DTE			((1ULL) << ENVCFG_DTE_SHIFT)
#define ENVCFG_PMM			((0x3ULL) << 32)
#define ENVCFG_PMM_PMLEN_0		((0x0ULL) << 32)
#define ENVCFG_PMM_PMLEN_7		((0x2ULL) << 32)
#define ENVCFG_PMM_PMLEN_16		((0x3ULL) << 32)
#define ENVCFG_CBZE			((1UL) << 7)
#define ENVCFG_CBCFE			((1UL) << 6)
#define ENVCFG_CBIE_SHIFT		4
#define ENVCFG_CBIE			((0x3UL) << ENVCFG_CBIE_SHIFT)
#define ENVCFG_CBIE_ILL			(0x0UL)
#define ENVCFG_CBIE_FLUSH		(0x1UL)
#define ENVCFG_CBIE_INV			(0x3UL)
#define ENVCFG_SSE_SHIFT		3
#define ENVCFG_SSE			((1UL) << ENVCFG_SSE_SHIFT)
#define ENVCFG_LPE_SHIFT		2
#define ENVCFG_LPE			((1UL) << ENVCFG_LPE_SHIFT)
#define ENVCFG_FIOM			(0x1UL)

static inline uint64_t cpu_csr_read(uint32_t csr) {
	uint64_t val;
	asm volatile("csrr %0, %1"
			: "=r"(val)
			: "i"(csr)
			: "memory");
	return val;
}

static inline void cpu_csr_write(uint32_t csr, uint64_t val) {
	asm volatile("csrw %0, %1"
			:
			: "i"(csr), "rK"(val)
			: "memory");
}

static inline void cpu_csr_set(uint32_t csr, uint64_t mask) {
	asm volatile("csrs %0, %1"
			:
			: "i"(csr), "rK"(mask)
			: "memory");
}

static inline void cpu_csr_clear(uint32_t csr, uint64_t mask) {
	asm volatile("csrc %0, %1"
			:
			: "i"(csr), "rK"(mask)
			: "memory");
}

/* Read old, set bit specified in mask */
static inline uint64_t cpu_csr_read_set(uint32_t csr, uint64_t mask) {
	uint64_t old;
	asm volatile("csrrs %0, %1, %2"
			: "=r"(old)
			: "i"(csr), "rK"(mask)
			: "memory");
	return old;
}

/* Read old, clear bit specified in mask */
static inline uint64_t cpu_csr_read_clear(uint32_t csr, uint64_t mask) {
	uint64_t old;
	asm volatile("csrrc %0, %1, %2"
			: "=r"(old)
			: "i"(csr), "rK"(mask)
			: "memory");
	return old;
}

/* Swap CSR and val (read old, write new) */
static inline uint64_t cpu_csr_swap(uint32_t csr, uint64_t val) {
	uint64_t old;
	asm volatile("csrrw %0, %1, %2"
			: "=r"(old)
			: "i"(csr), "r"(val)
			: "memory");
	return old;
}

#endif /* ASSEMBLER */

#endif /* RISCV_CSR_H */
