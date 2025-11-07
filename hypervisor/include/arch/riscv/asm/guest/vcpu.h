/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RISCV_VCPU_H
#define RISCV_VCPU_H

#include <types.h>
#include <errno.h>

#ifndef ASSEMBLER

#define RISCV_VCPU_REQUEST_EXCEPTION		0U
#define RISCV_VCPU_REQUEST_EVENT		1U

#define RISCV_VCPU_EVENT_VIRTUAL_INTERRUPT	0U

#define RISCV_VSTIMECMP_INVALID			0xffffffffffffffffULL

struct riscv_vcpu_guest_ctx {
	uint64_t vsstatus;
	uint64_t vsie;
	uint64_t vstvec;
	uint64_t vsscratch;
	uint64_t vsepc;
	uint64_t vscause;
	uint64_t vstval;
	/*
	 * vstimecmp is implemented in SSTC extension, which
	 * is mandatory for RVA23.
	 */
	uint64_t vstimecmp;
	uint64_t hvip;
	uint64_t vsatp;

	/* Risc-v spec privileged ch 21.2:
	 * senvcfg, scounteren, scontext have
	 * no matching VS CSR, they function
	 * even when V=1. HV needs to manually
	 * swap them.
	 */
	uint64_t scounteren;
	uint64_t senvcfg;
};

struct riscv_vcpu_host_ctx {
	uint64_t hcounteren;
	uint64_t henvcfg;
	uint64_t hedeleg;
	uint64_t hideleg;
	uint64_t hgatp;
	uint64_t htinst;
	uint64_t htval;
	uint64_t hie;
	uint64_t hip;
	uint64_t htimedelta;
};

struct riscv_vcpu_trap_info {
	uint64_t epc;
	uint64_t cause;
	uint64_t tval;
};

struct acrn_vcpu_arch {
	/* This have to be the first member of acrn_vcpu_arch */
	struct cpu_regs regs;

	struct riscv_vcpu_guest_ctx gctx;
	struct riscv_vcpu_host_ctx hctx;

	struct riscv_vcpu_trap_info trap;
	uint64_t irqs_pending;
	uint64_t irqs_pending_mask;
} __aligned(PAGE_SIZE);

struct acrn_vcpu;
struct intr_excp_ctx;

int32_t riscv_process_vcpu_requests(struct acrn_vcpu *vcpu);

#endif /* ASSEMBLER */

#endif /* RISCV_VCPU_H */
