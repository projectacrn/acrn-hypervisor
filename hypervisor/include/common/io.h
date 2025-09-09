/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IO_H
#define IO_H

#include <types.h>
#include <asm/io.h>

#ifndef HAS_ARCH_PIO
/* Write 1 byte to specified I/O port */
static inline void arch_pio_write8(__unused uint8_t value, __unused uint16_t port){}

/* Read 1 byte from specified I/O port */
static inline uint8_t arch_pio_read8(__unused uint16_t port) { return 0xffU;}

/* Write 2 bytes to specified I/O port */
static inline void arch_pio_write16(__unused uint16_t value, __unused uint16_t port) {}

/* Read 2 bytes from specified I/O port */
static inline uint16_t arch_pio_read16(__unused uint16_t port) { return 0xffffU;}

/* Write 4 bytes to specified I/O port */
static inline void arch_pio_write32(__unused uint32_t value, __unused uint16_t port) {}

/* Read 4 bytes to specified I/O port */
static inline uint32_t arch_pio_read32(__unused uint16_t port) { return 0xffffffffU;}

/* Write 8 bytes to specified I/O port */
static inline void arch_pio_write(__unused uint64_t v, __unused uint16_t addr, __unused size_t sz) {}

/* Read 8 bytes to specified I/O port */
static inline uint64_t arch_pio_read(__unused uint16_t addr, __unused size_t sz) { return 0xffffffffU;}
#endif /* HAS_ARCH_PIO */

#ifndef HAS_ARCH_MMIO
/** Writes a 64 bit value to a memory mapped IO device.
 *
 *  @param value The 64 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void arch_mmio_write64(uint64_t value, void *addr)
{
	volatile uint64_t *addr64 = (volatile uint64_t *)addr;
	*addr64 = value;
}

/** Writes a 32 bit value to a memory mapped IO device.
 *
 *  @param value The 32 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void arch_mmio_write32(uint32_t value, void *addr)
{
	volatile uint32_t *addr32 = (volatile uint32_t *)addr;
	*addr32 = value;
}

/** Writes a 16 bit value to a memory mapped IO device.
 *
 *  @param value The 16 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void arch_mmio_write16(uint16_t value, void *addr)
{
	volatile uint16_t *addr16 = (volatile uint16_t *)addr;
	*addr16 = value;
}

/** Writes an 8 bit value to a memory mapped IO device.
 *
 *  @param value The 8 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void arch_mmio_write8(uint8_t value, void *addr)
{
	volatile uint8_t *addr8 = (volatile uint8_t *)addr;
	*addr8 = value;
}

/** Reads a 64 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 64 bit value read from the given address.
 */
static inline uint64_t arch_mmio_read64(const void *addr)
{
	return *((volatile const uint64_t *)addr);
}

/** Reads a 32 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 32 bit value read from the given address.
 */
static inline uint32_t arch_mmio_read32(const void *addr)
{
	return *((volatile const uint32_t *)addr);
}

/** Reads a 16 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 16 bit value read from the given address.
 */
static inline uint16_t arch_mmio_read16(const void *addr)
{
	return *((volatile const uint16_t *)addr);
}

/** Reads an 8 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 8 bit  value read from the given address.
 */
static inline uint8_t arch_mmio_read8(const void *addr)
{
	return *((volatile const uint8_t *)addr);
}
#endif /* HAS_ARCH_MMIO */

static inline uint8_t mmio_read8(const void *addr)
{
	return arch_mmio_read8(addr);
}

static inline uint16_t mmio_read16(const void *addr)
{
	return arch_mmio_read16(addr);
}

static inline uint32_t mmio_read32(const void *addr)
{
	return arch_mmio_read32(addr);
}

static inline uint64_t mmio_read64(const void *addr)
{
	return arch_mmio_read64(addr);
}

static inline uint64_t mmio_read(const void *addr, uint64_t sz)
{
	uint64_t val;
	switch (sz) {
	case 1U:
		val = (uint64_t)arch_mmio_read8(addr);
		break;
	case 2U:
		val = (uint64_t)arch_mmio_read16(addr);
		break;
	case 4U:
		val = (uint64_t)arch_mmio_read32(addr);
		break;
	default:
		val = arch_mmio_read64(addr);
		break;
	}
	return val;
}

static inline void mmio_write8(uint8_t v, void *addr)
{
	arch_mmio_write8(v, addr);
}

static inline void mmio_write16(uint16_t v, void *addr)
{
	arch_mmio_write16(v, addr);
}

static inline void mmio_write32(uint32_t v, void *addr)
{
	arch_mmio_write32(v, addr);
}

static inline void mmio_write64(uint64_t v, void *addr)
{
	arch_mmio_write64(v, addr);
}

static inline void mmio_write(void *addr, uint64_t sz, uint64_t val)
{
	switch (sz) {
	case 1U:
		mmio_write8((uint8_t)val, addr);
		break;
	case 2U:
		mmio_write16((uint16_t)val, addr);
		break;
	case 4U:
		mmio_write32((uint32_t)val, addr);
		break;
	default:
		mmio_write64(val, addr);
		break;
	}
}

static inline void pio_write8(uint8_t v, uint16_t addr)
{
	arch_pio_write8(v, addr);
}

static inline void pio_write16(uint16_t v, uint16_t addr)
{
	arch_pio_write16(v, addr);
}

static inline void pio_write32(uint32_t v, uint16_t addr)
{
	arch_pio_write32(v, addr);
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

static inline uint8_t pio_read8(uint16_t addr)
{
	return arch_pio_read8(addr);
}

static inline uint16_t pio_read16(uint16_t addr)
{
	return arch_pio_read16(addr);
}

static inline uint32_t pio_read32(uint16_t addr)
{
	return arch_pio_read32(addr);
}

static inline uint32_t pio_read(uint16_t addr, size_t sz)
{
	uint32_t ret;
	if (sz == 1U) {
		ret = pio_read8(addr);
	} else if (sz == 2U) {
		ret = pio_read16(addr);
	} else {
		ret = pio_read32(addr);
	}
	return ret;
}

#endif /* IO_H defined */
