/*
 * Copyright (C) 2023-2025 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Haicheng Li <haicheng.li@intel.com>
 */

#ifndef RISCV_LIB_BITS_H
#define RISCV_LIB_BITS_H
#include <types.h>
#include <asm/cpu.h>
#define INVALID_BIT_INDEX  0xffffU
static inline uint16_t arch_ffs64(uint64_t x)
{
    asm volatile ("ctz %0, %1" : "=r"(x) : "r"(x));
    return x == BITS_PER_LONG ? INVALID_BIT_INDEX :(uint16_t)x;
}

static inline uint16_t arch_fls64(uint64_t x)
{
    asm volatile ("clz %0, %1" : "=r"(x) : "r"(x));
    return  x == BITS_PER_LONG ? INVALID_BIT_INDEX :(BITS_PER_LONG - 1U - (uint16_t)x);
}

static inline int16_t arch_fls32(uint32_t x)
{
    asm volatile ("clzw %0, %1" : "=r" (x) : "r" (x));
    return  x == 32 ? (int16_t)INVALID_BIT_INDEX :(int16_t)(31 - x);
}

/*
 * return !!((*addr) & (1UL<<nr));
 * Note:Input parameter nr shall be less than 64. If nr>=64, it will
 * be truncated.
 */
 static inline bool arch_bitmap_test(uint32_t nr, const volatile uint64_t *addr)
{
    uint64_t mask = 1UL << (nr & ((8U * sizeof(uint64_t)) - 1U));
    return !!(*addr & mask);
}

static inline bool arch_bitmap32_test(uint32_t nr, const volatile uint32_t *addr)
{
    uint32_t mask = 1UL << (nr & ((8U * sizeof(uint32_t)) - 1U));
    // TODO: Use RISC-V B extension instruction 'bext' to optimize this function
    return !!(*addr & mask);
}

/*
 * (*addr) |= (1UL<<nr);
 * Note:Input parameter nr shall be less than 64.
 * If nr>=64, it will be truncated.
 */
#define build_bitmap_set(name, op_len, op_type, lock)                   \
static inline void name(uint32_t nr_arg, volatile op_type *addr)        \
{                                                                       \
    op_type mask = 1UL << (nr_arg & ((8U * sizeof(op_type)) - 1U));     \
    asm volatile (                                                      \
        "amoor." op_len "." lock " zero, %1, %0"                        \
        : "+A" (*addr)                                                  \
        : "r" (mask)                                                    \
        : "memory"                                                      \
    );                                                                  \
}
build_bitmap_set(arch_bitmap_set, "d", uint64_t, "aqrl")
build_bitmap_set(arch_bitmap32_set, "w", uint32_t, "aqrl")

/*
 * (*addr) &= ~(1UL<<nr);
 * Note:Input parameter nr shall be less than 64.
 * If nr>=64, it will be truncated.
 */
#define build_bitmap_clear(name, op_len, op_type, lock)                 \
static inline void name(uint32_t nr_arg, volatile op_type *addr)        \
{                                                                       \
    op_type mask = ~(1UL << (nr_arg & ((8U * sizeof(op_type)) - 1U)));  \
    asm volatile (                                                      \
        "amoand." op_len "." lock " zero, %1, %0"                       \
        : "+A" (*addr)                                                  \
        : "r" (mask)                                                    \
        : "memory"                                                      \
    );                                                                  \
}
build_bitmap_clear(arch_bitmap_clear, "d", uint64_t, "aqrl")
build_bitmap_clear(arch_bitmap32_clear, "w", uint32_t, "aqrl")

/*
 * bool ret = (*addr) & (1UL<<nr);
 * (*addr) |= (1UL<<nr);
 * return ret;
 * Note:Input parameter nr shall be less than 64. If nr>=64, it
 * will be truncated.
 */
#define build_bitmap_testandset(name, op_len, op_type, lock)            \
static inline bool name(uint32_t nr_arg, volatile op_type *addr)        \
{                                                                       \
    op_type mask = 1UL << (nr_arg & ((8U * sizeof(op_type)) - 1U));     \
    op_type old;                                                        \
    asm volatile (                                                      \
        "amoor." op_len "." lock " %0, %2, %1"                          \
        : "=r" (old), "+A" (*addr)                                      \
        : "r" (mask)                                                    \
        : "memory"                                                      \
    );                                                                  \
    return !!(old & mask);                                              \
}
build_bitmap_testandset(arch_bitmap_test_and_set, "d", uint64_t, "aqrl")
build_bitmap_testandset(arch_bitmap32_test_and_set, "w", uint32_t, "aqrl")

/*
 * bool ret = (*addr) & (1UL<<nr);
 * (*addr) &= ~(1UL<<nr);
 * return ret;
 * Note:Input parameter nr shall be less than 64. If nr>=64,
 * it will be truncated.
 */
#define build_bitmap_testandclear(name, op_len, op_type, lock)          \
static inline bool name(uint32_t nr_arg, volatile op_type *addr)        \
{                                                                       \
    op_type mask = 1UL << (nr_arg & ((8U * sizeof(op_type)) - 1U));     \
    op_type old;                                                        \
    asm volatile (                                                      \
        "amoand." op_len "." lock " %0, %2, %1"                         \
        : "=r" (old), "+A" (*addr)                                      \
        : "r" (~mask)                                                   \
        : "memory"                                                      \
    );                                                                  \
    return !!(old & mask);                                              \
}
build_bitmap_testandclear(arch_bitmap_test_and_clear, "d", uint64_t, "aqrl")
build_bitmap_testandclear(arch_bitmap32_test_and_clear, "w", uint32_t, "aqrl")
#endif /* RISCV_LIB_BITOPS_H */
