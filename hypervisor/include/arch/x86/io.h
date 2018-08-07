/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IO_H
#define IO_H

#include <types.h>

/* Write 1 byte to specified I/O port */
static inline void pio_write8(uint8_t value, uint16_t port)
{
	asm volatile ("outb %0,%1"::"a" (value), "dN"(port));
}

/* Read 1 byte from specified I/O port */
static inline uint8_t pio_read8(uint16_t port)
{
	uint8_t value;

	asm volatile ("inb %1,%0":"=a" (value):"dN"(port));
	return value;
}

/* Write 2 bytes to specified I/O port */
static inline void pio_write16(uint16_t value, uint16_t port)
{
	asm volatile ("outw %0,%1"::"a" (value), "dN"(port));
}

/* Read 2 bytes from specified I/O port */
static inline uint16_t pio_read16(uint16_t port)
{
	uint16_t value;

	asm volatile ("inw %1,%0":"=a" (value):"dN"(port));
	return value;
}

/* Write 4 bytes to specified I/O port */
static inline void pio_write32(uint32_t value, uint16_t port)
{
	asm volatile ("outl %0,%1"::"a" (value), "dN"(port));
}

/* Read 4 bytes from specified I/O port */
static inline uint32_t pio_read32(uint16_t port)
{
	uint32_t value;

	asm volatile ("inl %1,%0":"=a" (value):"dN"(port));
	return value;
}

static inline void pio_write(uint32_t v, uint16_t addr, size_t sz)
{
	if (sz == 1U) {
		pio_write8((uint8_t)v, addr);
	} else if (sz == 2U) {
		pio_write16((uint16_t)v, addr);
	} else {
		pio_write32(v, addr);
	}
}

static inline uint32_t pio_read(uint16_t addr, size_t sz)
{
	if (sz == 1U) {
		return pio_read8(addr);
	}
	if (sz == 2U) {
		return pio_read16(addr);
	}
	return pio_read32(addr);
}

/** Writes a 32 bit value to a memory mapped IO device.
 *
 *  @param value The 32 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void mmio_write32(uint32_t value, void *addr)
{
	volatile uint32_t *addr32 = (volatile uint32_t *)addr;
	*addr32 = value;
}

/** Writes a 16 bit value to a memory mapped IO device.
 *
 *  @param value The 16 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void mmio_write16(uint16_t value, void *addr)
{
	volatile uint16_t *addr16 = (volatile uint16_t *)addr;
	*addr16 = value;
}

/** Writes an 8 bit value to a memory mapped IO device.
 *
 *  @param value The 8 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void mmio_write8(uint8_t value, void *addr)
{
	volatile uint8_t *addr8 = (volatile uint8_t *)addr;
	*addr8 = value;
}

/** Reads a 32 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 32 bit value read from the given address.
 */
static inline uint32_t mmio_read32(void *addr)
{
	return *((volatile uint32_t *)addr);
}

/** Reads a 16 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 16 bit value read from the given address.
 */
static inline uint16_t mmio_read16(void *addr)
{
	return *((volatile uint16_t *)addr);
}

/** Reads an 8 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 8 bit  value read from the given address.
 */
static inline uint8_t mmio_read8(void *addr)
{
	return *((volatile uint8_t *)addr);
}


/** Reads a 32 Bit memory mapped IO register, mask it and write it back into
 *  memory mapped IO register.
 *
 * @param addr    The address of the memory mapped IO register.
 * @param mask    The mask to apply to the value read.
 * @param value   The 32 bit value to write.
 */
static inline void set32(void *addr, uint32_t mask, uint32_t value)
{
	uint32_t temp_val;

	temp_val = mmio_read32(addr);
	mmio_write32((temp_val & ~mask) | value, addr);
}

/** Reads a 16 Bit memory mapped IO register, mask it and write it back into
 *  memory mapped IO register.
 *
 * @param addr    The address of the memory mapped IO register.
 * @param mask    The mask to apply to the value read.
 * @param value   The 16 bit value to write.
 */
static inline void set16(void *addr, uint16_t mask, uint16_t value)
{
	uint16_t temp_val;

	temp_val = mmio_read16(addr);
	mmio_write16((temp_val & ~mask) | value, addr);
}

/** Reads a 8 Bit memory mapped IO register, mask it and write it back into
 *  memory mapped IO register.
 *
 * @param addr    The address of the memory mapped IO register.
 * @param mask    The mask to apply to the value read.
 * @param value   The 8 bit value to write.
 */
static inline void set8(void *addr, uint8_t mask, uint8_t value)
{
	uint8_t temp_val;

	temp_val = mmio_read8(addr);
	mmio_write8((temp_val & ~mask) | value, addr);
}

#endif /* _IO_H defined */
