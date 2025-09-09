/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef X86_IO_H
#define X86_IO_H

#include <types.h>

#define HAS_ARCH_PIO
/* X86 architecture only supports 16 bits IO space */
#define IO_SPACE_BITMASK 0xffffU

/* Write 1 byte to specified I/O port */
static inline void arch_pio_write8(uint8_t value, uint16_t port)
{
	asm volatile ("outb %0,%1"::"a" (value), "dN"(port));
}

/* Read 1 byte from specified I/O port */
static inline uint8_t arch_pio_read8(uint16_t port)
{
	uint8_t value;

	asm volatile ("inb %1,%0":"=a" (value):"dN"(port));
	return value;
}

/* Write 2 bytes to specified I/O port */
static inline void arch_pio_write16(uint16_t value, uint16_t port)
{
	asm volatile ("outw %0,%1"::"a" (value), "dN"(port));
}

/* Read 2 bytes from specified I/O port */
static inline uint16_t arch_pio_read16(uint16_t port)
{
	uint16_t value;

	asm volatile ("inw %1,%0":"=a" (value):"dN"(port));
	return value;
}

/* Write 4 bytes to specified I/O port */
static inline void arch_pio_write32(uint32_t value, uint16_t port)
{
	asm volatile ("outl %0,%1"::"a" (value), "dN"(port));
}

/* Read 4 bytes from specified I/O port */
static inline uint32_t arch_pio_read32(uint16_t port)
{
	uint32_t value;

	asm volatile ("inl %1,%0":"=a" (value):"dN"(port));
	return value;
}

#endif /* X86_IO_H defined */
