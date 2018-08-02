/*-
 * Copyright (c) 1996, by Peter Wemm and Steve Passe
 * Copyright (c) 2017 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _APICREG_H_
#define _APICREG_H_

/*
 * Local && I/O APIC definitions.
 */

/*
 * Pentium P54C+ Built-in APIC
 * (Advanced programmable Interrupt Controller)
 *
 * Base Address of Built-in APIC in memory location
 * is 0xfee00000.
 *
 * Map of APIC Registers:
 *
 * Offset (hex)    Description                     Read/Write state
 * 000             Reserved
 * 010             Reserved
 * 020 ID          Local APIC ID                   R/W
 * 030 VER         Local APIC Version              R
 * 040             Reserved
 * 050             Reserved
 * 060             Reserved
 * 070             Reserved
 * 080             Task Priority Register          R/W
 * 090             Arbitration Priority Register   R
 * 0A0             Processor Priority Register     R
 * 0B0             EOI Register                    W
 * 0C0 RRR         Remote read                     R
 * 0D0             Logical Destination             R/W
 * 0E0             Destination Format Register     0..27 R;  28..31 R/W
 * 0F0 SVR         Spurious Interrupt Vector Reg.  0..3  R;  4..9   R/W
 * 100             ISR  000-031                    R
 * 110             ISR  032-063                    R
 * 120             ISR  064-095                    R
 * 130             ISR  095-128                    R
 * 140             ISR  128-159                    R
 * 150             ISR  160-191                    R
 * 160             ISR  192-223                    R
 * 170             ISR  224-255                    R
 * 180             TMR  000-031                    R
 * 190             TMR  032-063                    R
 * 1A0             TMR  064-095                    R
 * 1B0             TMR  095-128                    R
 * 1C0             TMR  128-159                    R
 * 1D0             TMR  160-191                    R
 * 1E0             TMR  192-223                    R
 * 1F0             TMR  224-255                    R
 * 200             IRR  000-031                    R
 * 210             IRR  032-063                    R
 * 220             IRR  064-095                    R
 * 230             IRR  095-128                    R
 * 240             IRR  128-159                    R
 * 250             IRR  160-191                    R
 * 260             IRR  192-223                    R
 * 270             IRR  224-255                    R
 * 280             Error Status Register           R
 * 290             Reserved
 * 2A0             Reserved
 * 2B0             Reserved
 * 2C0             Reserved
 * 2D0             Reserved
 * 2E0             Reserved
 * 2F0             Local Vector Table (CMCI)       R/W
 * 300 ICR_LOW     Interrupt Command Reg. (0-31)   R/W
 * 310 ICR_HI      Interrupt Command Reg. (32-63)  R/W
 * 320             Local Vector Table (Timer)      R/W
 * 330             Local Vector Table (Thermal)    R/W (PIV+)
 * 340             Local Vector Table (Performance) R/W (P6+)
 * 350 LVT1        Local Vector Table (LINT0)      R/W
 * 360 LVT2        Local Vector Table (LINT1)      R/W
 * 370 LVT3        Local Vector Table (ERROR)      R/W
 * 380             Initial Count Reg. for Timer    R/W
 * 390             Current Count of Timer          R
 * 3A0             Reserved
 * 3B0             Reserved
 * 3C0             Reserved
 * 3D0             Reserved
 * 3E0             Timer Divide Configuration Reg. R/W
 * 3F0             Reserved
 */


/******************************************************************************
 * global defines, etc.
 */


/******************************************************************************
 * LOCAL APIC structure
 */

#ifndef LOCORE

#define PAD3	uint32_t: 32; uint32_t: 32; uint32_t: 32
#define PAD4	uint32_t: 32; uint32_t: 32; uint32_t: 32; uint32_t: 32

struct lapic_reg {
	uint32_t val;		PAD3;
};

struct lapic_regs {
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	uint32_t id;		PAD3;
	uint32_t version;	PAD3;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	uint32_t tpr;		PAD3;
	uint32_t apr;		PAD3;
	uint32_t ppr;		PAD3;
	uint32_t eoi;		PAD3;
	/* reserved */		PAD4;
	uint32_t ldr;		PAD3;
	uint32_t dfr;		PAD3;
	uint32_t svr;		PAD3;
	struct lapic_reg	isr[8];
	struct lapic_reg	tmr[8];
	struct lapic_reg	irr[8];
	uint32_t esr;		PAD3;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	uint32_t lvt_cmci;	PAD3;
	uint32_t icr_lo;	PAD3;
	uint32_t icr_hi;	PAD3;
	struct lapic_reg	lvt[6];
	uint32_t icr_timer;	PAD3;
	uint32_t ccr_timer;	PAD3;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	/* reserved */		PAD4;
	uint32_t dcr_timer;	PAD3;
	/* reserved */		PAD4;
};

enum LAPIC_REGISTERS {
	LAPIC_ID	= 0x2,
	LAPIC_VERSION	= 0x3,
	LAPIC_TPR	= 0x8,
	LAPIC_APR	= 0x9,
	LAPIC_PPR	= 0xa,
	LAPIC_EOI	= 0xb,
	LAPIC_LDR	= 0xd,
	LAPIC_DFR	= 0xe, /* Not in x2APIC */
	LAPIC_SVR	= 0xf,
	LAPIC_ISR0	= 0x10,
	LAPIC_ISR1	= 0x11,
	LAPIC_ISR2	= 0x12,
	LAPIC_ISR3	= 0x13,
	LAPIC_ISR4	= 0x14,
	LAPIC_ISR5	= 0x15,
	LAPIC_ISR6	= 0x16,
	LAPIC_ISR7	= 0x17,
	LAPIC_TMR0	= 0x18,
	LAPIC_TMR1	= 0x19,
	LAPIC_TMR2	= 0x1a,
	LAPIC_TMR3	= 0x1b,
	LAPIC_TMR4	= 0x1c,
	LAPIC_TMR5	= 0x1d,
	LAPIC_TMR6	= 0x1e,
	LAPIC_TMR7	= 0x1f,
	LAPIC_IRR0	= 0x20,
	LAPIC_IRR1	= 0x21,
	LAPIC_IRR2	= 0x22,
	LAPIC_IRR3	= 0x23,
	LAPIC_IRR4	= 0x24,
	LAPIC_IRR5	= 0x25,
	LAPIC_IRR6	= 0x26,
	LAPIC_IRR7	= 0x27,
	LAPIC_ESR	= 0x28,
	LAPIC_LVT_CMCI	= 0x2f,
	LAPIC_ICR_LO	= 0x30,
	LAPIC_ICR_HI	= 0x31, /* Not in x2APIC */
	LAPIC_LVT_TIMER	= 0x32,
	LAPIC_LVT_THERMAL = 0x33,
	LAPIC_LVT_PCINT	= 0x34,
	LAPIC_LVT_LINT0	= 0x35,
	LAPIC_LVT_LINT1	= 0x36,
	LAPIC_LVT_ERROR	= 0x37,
	LAPIC_ICR_TIMER	= 0x38,
	LAPIC_CCR_TIMER	= 0x39,
	LAPIC_DCR_TIMER	= 0x3e,
	LAPIC_SELF_IPI	= 0x3f, /* Only in x2APIC */
	LAPIC_EXT_FEATURES = 0x40, /* AMD */
	LAPIC_EXT_CTRL	= 0x41, /* AMD */
	LAPIC_EXT_SEOI	= 0x42, /* AMD */
	LAPIC_EXT_IER0	= 0x48, /* AMD */
	LAPIC_EXT_IER1	= 0x49, /* AMD */
	LAPIC_EXT_IER2	= 0x4a, /* AMD */
	LAPIC_EXT_IER3	= 0x4b, /* AMD */
	LAPIC_EXT_IER4	= 0x4c, /* AMD */
	LAPIC_EXT_IER5	= 0x4d, /* AMD */
	LAPIC_EXT_IER6	= 0x4e, /* AMD */
	LAPIC_EXT_IER7	= 0x4f, /* AMD */
	LAPIC_EXT_LVT0	= 0x50, /* AMD */
	LAPIC_EXT_LVT1	= 0x51, /* AMD */
	LAPIC_EXT_LVT2	= 0x52, /* AMD */
	LAPIC_EXT_LVT3	= 0x53, /* AMD */
};

#define	LAPIC_MEM_MUL	0x10

/*
 * Although some registers are available on AMD processors only,
 * it's not a big waste to reserve them on all platforms.
 * However, we need to watch out for this space being assigned for
 * non-APIC purposes in the future processor models.
 */
#define	LAPIC_MEM_REGION ((LAPIC_EXT_LVT3 + 1) * LAPIC_MEM_MUL)

/******************************************************************************
 * I/O APIC structure
 */

struct ioapic {
	uint32_t ioregsel;	PAD3;
	uint32_t iowin;	PAD3;
};

/* IOAPIC Redirection Table (RTE) Entry structure */
union ioapic_rte {
	uint64_t full;
	struct {
		uint32_t lo_32;
		uint32_t hi_32;
	} u;
};

#undef PAD4
#undef PAD3

#endif  /* !LOCORE */


/******************************************************************************
 * various code 'logical' values
 */

/******************************************************************************
 * LOCAL APIC defines
 */

/* default physical locations of LOCAL (CPU) APICs */
#define DEFAULT_APIC_BASE	0xfee00000U

/* constants relating to APIC ID registers */
#define APIC_ID_MASK		0xff000000U
#define	APIC_ID_SHIFT		24U
#define	APIC_ID_CLUSTER		0xf0U
#define	APIC_ID_CLUSTER_ID	0x0fU
#define	APIC_MAX_CLUSTER	0xeU
#define	APIC_MAX_INTRACLUSTER_ID 3
#define	APIC_ID_CLUSTER_SHIFT	4

/* fields in VER */
#define APIC_VER_VERSION	0x000000ffU
#define APIC_VER_MAXLVT		0x00ff0000U
#define MAXLVTSHIFT		16U
#define APIC_VER_EOI_SUPPRESSION 0x01000000U
#define APIC_VER_AMD_EXT_SPACE	0x80000000U

/* fields in LDR */
#define	APIC_LDR_RESERVED	0x00ffffffU

/* fields in DFR */
#define	APIC_DFR_RESERVED	0x0fffffffU
#define	APIC_DFR_MODEL_MASK	0xf0000000U
#define	APIC_DFR_MODEL_FLAT	0xf0000000U
#define	APIC_DFR_MODEL_CLUSTER	0x00000000U

/* fields in SVR */
#define APIC_SVR_VECTOR		0x000000ffU
#define APIC_SVR_VEC_PROG	0x000000f0U
#define APIC_SVR_VEC_FIX	0x0000000fU
#define APIC_SVR_ENABLE		0x00000100U
#define APIC_SVR_SWDIS		0x00000000U
#define APIC_SVR_SWEN		0x00000100U
#define APIC_SVR_FOCUS		0x00000200U
#define APIC_SVR_FEN		0x00000000U
#define APIC_SVR_FDIS		0x00000200U
#define APIC_SVR_EOI_SUPPRESSION 0x00001000U

/* fields in TPR */
#define APIC_TPR_PRIO		0x000000ffU
#define APIC_TPR_INT		0x000000f0U
#define APIC_TPR_SUB		0x0000000fU

/* fields in ESR */
#define	APIC_ESR_SEND_CS_ERROR		0x00000001U
#define	APIC_ESR_RECEIVE_CS_ERROR	0x00000002U
#define	APIC_ESR_SEND_ACCEPT		0x00000004U
#define	APIC_ESR_RECEIVE_ACCEPT		0x00000008U
#define	APIC_ESR_SEND_ILLEGAL_VECTOR	0x00000020U
#define	APIC_ESR_RECEIVE_ILLEGAL_VECTOR	0x00000040U
#define	APIC_ESR_ILLEGAL_REGISTER	0x00000080U

/* fields in ICR_LOW */
#define APIC_VECTOR_MASK	0x000000ffU

#define APIC_DELMODE_MASK	0x00000700U
#define APIC_DELMODE_FIXED	0x00000000U
#define APIC_DELMODE_LOWPRIO	0x00000100U
#define APIC_DELMODE_SMI	0x00000200U
#define APIC_DELMODE_RR	0x00000300U
#define APIC_DELMODE_NMI	0x00000400U
#define APIC_DELMODE_INIT	0x00000500U
#define APIC_DELMODE_STARTUP	0x00000600U
#define APIC_DELMODE_RESV	0x00000700U

#define APIC_DESTMODE_MASK	0x00000800U
#define APIC_DESTMODE_PHY	0x00000000U
#define APIC_DESTMODE_LOG	0x00000800U

#define APIC_DELSTAT_MASK	0x00001000U
#define APIC_DELSTAT_IDLE	0x00000000U
#define APIC_DELSTAT_PEND	0x00001000U

#define APIC_RESV1_MASK		0x00002000U

#define APIC_LEVEL_MASK		0x00004000U
#define APIC_LEVEL_DEASSERT	0x00000000U
#define APIC_LEVEL_ASSERT	0x00004000U

#define APIC_TRIGMOD_MASK	0x00008000U
#define APIC_TRIGMOD_EDGE	0x00000000U
#define APIC_TRIGMOD_LEVEL	0x00008000U

#define APIC_RRSTAT_MASK	0x00030000U
#define APIC_RRSTAT_INVALID	0x00000000U
#define APIC_RRSTAT_INPROG	0x00010000U
#define APIC_RRSTAT_VALID	0x00020000U
#define APIC_RRSTAT_RESV	0x00030000U

#define APIC_DEST_MASK		0x000c0000U
#define APIC_DEST_DESTFLD	0x00000000U
#define APIC_DEST_SELF		0x00040000U
#define APIC_DEST_ALLISELF	0x00080000U
#define APIC_DEST_ALLESELF	0x000c0000U

#define APIC_RESV2_MASK		0xfff00000U

#define	APIC_ICRLO_RESV_MASK	(APIC_RESV1_MASK | APIC_RESV2_MASK)

/* fields in LVT1/2 */
#define APIC_LVT_VECTOR		0x000000ffU
#define APIC_LVT_DM		0x00000700U
#define APIC_LVT_DM_FIXED	0x00000000U
#define APIC_LVT_DM_SMI	0x00000200U
#define APIC_LVT_DM_NMI	0x00000400U
#define APIC_LVT_DM_INIT	0x00000500U
#define APIC_LVT_DM_EXTINT	0x00000700U
#define APIC_LVT_DS		0x00001000U
#define APIC_LVT_IIPP		0x00002000U
#define APIC_LVT_IIPP_INTALO	0x00002000U
#define APIC_LVT_IIPP_INTAHI	0x00000000U
#define APIC_LVT_RIRR		0x00004000U
#define APIC_LVT_TM		0x00008000U
#define APIC_LVT_M		0x00010000U


/* fields in LVT Timer */
#define APIC_LVTT_VECTOR	0x000000ffU
#define APIC_LVTT_DS		0x00001000U
#define APIC_LVTT_M		0x00010000U
#define APIC_LVTT_TM		0x00060000U
#define APIC_LVTT_TM_ONE_SHOT	0x00000000U
#define APIC_LVTT_TM_PERIODIC	0x00020000U
#define APIC_LVTT_TM_TSCDLT	0x00040000U
#define APIC_LVTT_TM_RSRV	0x00060000U

/* APIC timer current count */
#define	APIC_TIMER_MAX_COUNT	0xffffffffU

/* fields in TDCR */
#define APIC_TDCR_2		0x00U
#define APIC_TDCR_4		0x01U
#define APIC_TDCR_8		0x02U
#define APIC_TDCR_16		0x03U
#define APIC_TDCR_32		0x08U
#define APIC_TDCR_64		0x09U
#define APIC_TDCR_128		0x0aU
#define APIC_TDCR_1		0x0bU

/* Constants related to AMD Extended APIC Features Register */
#define	APIC_EXTF_ELVT_MASK	0x00ff0000U
#define	APIC_EXTF_ELVT_SHIFT	16
#define	APIC_EXTF_EXTID_CAP	0x00000004U
#define	APIC_EXTF_SEIO_CAP	0x00000002U
#define	APIC_EXTF_IER_CAP	0x00000001U

/* LVT table indices */
#define	APIC_LVT_TIMER		0U
#define	APIC_LVT_THERMAL	1U
#define	APIC_LVT_PMC		2U
#define	APIC_LVT_LINT0		3U
#define	APIC_LVT_LINT1		4U
#define	APIC_LVT_ERROR		5U
#define	APIC_LVT_CMCI		6U
#define	APIC_LVT_MAX		APIC_LVT_CMCI

/* AMD extended LVT constants, seem to be assigned by fiat */
#define	APIC_ELVT_IBS		0 /* Instruction based sampling */
#define	APIC_ELVT_MCA		1 /* MCE thresholding */
#define	APIC_ELVT_DEI		2 /* Deferred error interrupt */
#define	APIC_ELVT_SBI		3 /* Sideband interface */
#define	APIC_ELVT_MAX		APIC_ELVT_SBI

/******************************************************************************
 * I/O APIC defines
 */

/* default physical locations of an IO APIC */
#define DEFAULT_IO_APIC_BASE	0xfec00000UL

/* window register offset */
#define IOAPIC_REGSEL		0x00U
#define IOAPIC_WINDOW		0x10U

/* indexes into IO APIC */
#define IOAPIC_ID		0x00U
#define IOAPIC_VER		0x01U
#define IOAPIC_ARB		0x02U
#define IOAPIC_REDTBL		0x10U
#define IOAPIC_REDTBL0		IOAPIC_REDTBL
#define IOAPIC_REDTBL1		(IOAPIC_REDTBL+0x02U)
#define IOAPIC_REDTBL2		(IOAPIC_REDTBL+0x04U)
#define IOAPIC_REDTBL3		(IOAPIC_REDTBL+0x06U)
#define IOAPIC_REDTBL4		(IOAPIC_REDTBL+0x08U)
#define IOAPIC_REDTBL5		(IOAPIC_REDTBL+0x0aU)
#define IOAPIC_REDTBL6		(IOAPIC_REDTBL+0x0cU)
#define IOAPIC_REDTBL7		(IOAPIC_REDTBL+0x0eU)
#define IOAPIC_REDTBL8		(IOAPIC_REDTBL+0x10U)
#define IOAPIC_REDTBL9		(IOAPIC_REDTBL+0x12U)
#define IOAPIC_REDTBL10		(IOAPIC_REDTBL+0x14U)
#define IOAPIC_REDTBL11		(IOAPIC_REDTBL+0x16U)
#define IOAPIC_REDTBL12		(IOAPIC_REDTBL+0x18U)
#define IOAPIC_REDTBL13		(IOAPIC_REDTBL+0x1aU)
#define IOAPIC_REDTBL14		(IOAPIC_REDTBL+0x1cU)
#define IOAPIC_REDTBL15		(IOAPIC_REDTBL+0x1eU)
#define IOAPIC_REDTBL16		(IOAPIC_REDTBL+0x20U)
#define IOAPIC_REDTBL17		(IOAPIC_REDTBL+0x22U)
#define IOAPIC_REDTBL18		(IOAPIC_REDTBL+0x24U)
#define IOAPIC_REDTBL19		(IOAPIC_REDTBL+0x26U)
#define IOAPIC_REDTBL20		(IOAPIC_REDTBL+0x28U)
#define IOAPIC_REDTBL21		(IOAPIC_REDTBL+0x2aU)
#define IOAPIC_REDTBL22		(IOAPIC_REDTBL+0x2cU)
#define IOAPIC_REDTBL23		(IOAPIC_REDTBL+0x2eU)

/* fields in VER, for redirection entry */
#define IOAPIC_MAX_RTE_MASK	0x00ff0000U
#define MAX_RTE_SHIFT		16U

/*
 * fields in the IO APIC's redirection table entries
 */
#define IOAPIC_RTE_DEST_SHIFT  	56U
/* broadcast addr: all APICs */
#define IOAPIC_RTE_DEST_MASK	0xff00000000000000UL

#define IOAPIC_RTE_RESV	0x00fe0000UL	/* reserved */

#define IOAPIC_RTE_INTMASK	0x00010000UL	/* R/W: INTerrupt mask */
#define IOAPIC_RTE_INTMCLR	0x00000000UL	/*       clear, allow INTs */
#define IOAPIC_RTE_INTMSET	0x00010000UL	/*       set, inhibit INTs */

#define IOAPIC_RTE_TRGRMOD	0x00008000UL	/* R/W: trigger mode */
#define IOAPIC_RTE_TRGREDG	0x00000000UL	/*       edge */
#define IOAPIC_RTE_TRGRLVL	0x00008000UL	/*       level */

#define IOAPIC_RTE_REM_IRR	0x00004000UL	/* RO: remote IRR */

#define IOAPIC_RTE_INTPOL	0x00002000UL /*R/W:INT input pin polarity*/
#define IOAPIC_RTE_INTAHI	0x00000000UL	/*      active high */
#define IOAPIC_RTE_INTALO	0x00002000UL	/*      active low */

#define IOAPIC_RTE_DELIVS	0x00001000UL	/* RO: delivery status */

#define IOAPIC_RTE_DESTMOD	0x00000800UL	/*R/W:destination mode*/
#define IOAPIC_RTE_DESTPHY	0x00000000UL	/*      physical */
#define IOAPIC_RTE_DESTLOG	0x00000800UL	/*      logical */

#define IOAPIC_RTE_DELMOD	0x00000700UL	/* R/W: delivery mode */
#define IOAPIC_RTE_DELFIXED	0x00000000UL	/* fixed */
#define IOAPIC_RTE_DELLOPRI	0x00000100UL	/* lowest priority */
#define IOAPIC_RTE_DELSMI	0x00000200UL  /*System Management INT*/
#define IOAPIC_RTE_DELRSV1	0x00000300UL	/*  reserved */
#define IOAPIC_RTE_DELNMI	0x00000400UL	/*  NMI signal */
#define IOAPIC_RTE_DELINIT	0x00000500UL	/* INIT signal */
#define IOAPIC_RTE_DELRSV2	0x00000600UL	/* reserved */
#define IOAPIC_RTE_DELEXINT	0x00000700UL	/* External INTerrupt */

#define IOAPIC_RTE_INTVEC	0x000000ffUL /*R/W: INT vector field*/

#endif /* _APICREG_H_ */
