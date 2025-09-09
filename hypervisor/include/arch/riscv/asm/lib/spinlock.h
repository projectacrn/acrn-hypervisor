/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */
#ifndef RISCV_LIB_SPINLOCK_H
#define RISCV_LIB_SPINLOCK_H
#ifndef ASSEMBLER
/** The architecture dependent spinlock type. */
typedef struct _arch_spinlock {
	uint64_t head;
	uint64_t tail;
} arch_spinlock_t;

static inline void arch_spinlock_obtain(arch_spinlock_t *lock)
{
	asm volatile ("   li t0, 0x1\n\t"
		      "   amoadd.d.aqrl t1, t0, (%[head])\n\t"
		      "2: ld t0, (%[tail])\n\t"
		      "   beq t1, t0, 1f\n\t"
		      "   j 2b\n\t"
		      "1:\n"
		      :
		      :
		      [head] "r"(&lock->head),
		      [tail] "r"(&lock->tail)
		      : "cc", "memory", "t0", "t1");
}

static inline void arch_spinlock_release(arch_spinlock_t *lock)
{
	asm volatile ("   li t0, 0x1\n\t"
		      "   amoadd.d.aqrl t1, t0, (%[tail])\n"
		      :
		      : [tail] "r" (&lock->tail)
		      : "cc", "memory", "t0", "t1");
}

#else /* ASSEMBLER */
/** The offset of the head element. */
#define SPINLOCK_HEAD_OFFSET       0
/** The offset of the tail element. */
#define SPINLOCK_TAIL_OFFSET       8

.macro arch_spinlock_obtain lock_arg
	li t0, 0x1
	la t2, \lock_arg
	addi t3, t2, SPINLOCK_HEAD_OFFSET
	amoadd.d.aqrl t1, t0, (t3)
2:	ld t0, SPINLOCK_TAIL_OFFSET(t2)
	beq t1, t0, 1f
	j 2b
1 :
.endm
#define arch_spinlock_obtain(x) arch_spinlock_obtain lock_arg = (x)

.macro arch_spinlock_release lock_arg
	li t0, 0x1
	la t2, \lock_arg
	addi t3, t2, SPINLOCK_TAIL_OFFSET
	amoadd.d.aqrl t1, t0, (t3)
.endm

#define arch_spinlock_release(x) arch_spinlock_release lock_arg = (x)
#endif	/* ASSEMBLER */
#endif /* RISCV_LIB_SPINLOCK_H */
