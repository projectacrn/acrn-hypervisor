/*
 * Copyright (C) 2023-2025 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#ifndef RISCV_IO_H
#define RISCV_IO_H

#include <barrier.h>

#define HAS_ARCH_MMIO
/*
 * Generic I/O memory access primitives.
 */
static inline void writeb(uint8_t val, volatile void *addr)
{
        asm volatile("sb %0, 0(%1)" : : "r" (val), "r" (addr) : "memory");
}

static inline void writew(uint16_t val, volatile void *addr)
{
        asm volatile("sh %w0, 0(%1)" : : "r" (val), "r" (addr) : "memory");
}

static inline void writel(uint32_t val, volatile void *addr)
{
        asm volatile("sw %0, 0(%1)" : : "r" (val), "r" (addr) : "memory");
}

static inline void writeq(uint64_t val, volatile void *addr)
{
        asm volatile("sd %0, 0(%1)" : : "r" (val), "r" (addr) : "memory");
}

static inline uint8_t readb(const volatile void *addr)
{
        uint8_t val;

        asm volatile("lb %0, 0(%1)": "=r" (val) : "r" (addr) : "memory");
        return val;
}

static inline uint16_t readw(const volatile void *addr)
{
        uint16_t val;

        asm volatile("lh %0, 0(%1)": "=r" (val) : "r" (addr) : "memory");
        return val;
}

static inline uint32_t readl(const volatile void *addr)
{
        uint32_t val;

        asm volatile("lw %0, 0(%1)": "=r" (val) : "r" (addr) : "memory");
        return val;
}

static inline uint64_t readq(const volatile void *addr)
{
        uint64_t val;

        asm volatile("ld %0, 0(%1)": "=r" (val) : "r" (addr) : "memory");
        return val;
}

/*
 * Strictly ordered I/O memory access primitives.
 */
static inline uint8_t arch_mmio_read8(const volatile void *addr)
{
        uint8_t val = readb(addr);
        cpu_read_memory_barrier();
        return val;
}

static inline uint16_t arch_mmio_read16(const volatile void *addr)
{
        uint16_t val = readw(addr);
        cpu_read_memory_barrier();
        return val;
}

static inline uint32_t arch_mmio_read32(const volatile void *addr)
{
        uint32_t val = readl(addr);
        cpu_read_memory_barrier();
        return val;
}

static inline uint64_t arch_mmio_read64(const volatile void *addr)
{
        uint64_t val = readq(addr);
        cpu_read_memory_barrier();
        return val;
}

static inline void arch_mmio_write8(uint8_t val, volatile void *addr)
{
        cpu_write_memory_barrier();
        writeb(val, addr);
}

static inline void arch_mmio_write16(uint16_t val, volatile void *addr)
{
        cpu_write_memory_barrier();
        writew(val, addr);
}

static inline void arch_mmio_write32(uint32_t val, volatile void *addr)
{
        cpu_write_memory_barrier();
        writel(val, addr);
}

static inline void arch_mmio_write64(uint64_t val, volatile void *addr)
{
        cpu_write_memory_barrier();
        writeq(val, addr);
}

#endif /* RISCV_IO_H */
