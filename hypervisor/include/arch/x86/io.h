/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IO_H
#define IO_H

#include <types.h>

/* Definition of a IO port range */
struct vm_io_range {
	uint16_t base;		/* IO port base */
	uint16_t len;		/* IO port range */
	uint32_t flags;		/* IO port attributes */
};

/* Write 1 byte to specified I/O port */
static inline void io_write_byte(uint8_t value, uint16_t port)
{
	asm volatile ("outb %0,%1"::"a" (value), "dN"(port));
}

/* Read 1 byte from specified I/O port */
static inline uint8_t io_read_byte(uint16_t port)
{
	uint8_t value;

	asm volatile ("inb %1,%0":"=a" (value):"dN"(port));
	return value;
}

/* Write 2 bytes to specified I/O port */
static inline void io_write_word(uint16_t value, uint16_t port)
{
	asm volatile ("outw %0,%1"::"a" (value), "dN"(port));
}

/* Read 2 bytes from specified I/O port */
static inline uint16_t io_read_word(uint16_t port)
{
	uint16_t value;

	asm volatile ("inw %1,%0":"=a" (value):"dN"(port));
	return value;
}

/* Write 4 bytes to specified I/O port */
static inline void io_write_long(uint32_t value, uint16_t port)
{
	asm volatile ("outl %0,%1"::"a" (value), "dN"(port));
}

/* Read 4 bytes from specified I/O port */
static inline uint32_t io_read_long(uint16_t port)
{
	uint32_t value;

	asm volatile ("inl %1,%0":"=a" (value):"dN"(port));
	return value;
}

static inline void io_write(uint32_t v, uint16_t addr, size_t sz)
{
	if (sz == 1U)
		io_write_byte((uint8_t)v, addr);
	else if (sz == 2U)
		io_write_word((uint16_t)v, addr);
	else
		io_write_long(v, addr);
}

static inline uint32_t io_read(uint16_t addr, size_t sz)
{
	if (sz == 1U)
		return io_read_byte(addr);
	if (sz == 2U)
		return io_read_word(addr);
	return io_read_long(addr);
}

struct vm_io_handler;
struct vm;
struct vcpu;

typedef
uint32_t (*io_read_fn_t)(struct vm_io_handler *, struct vm *,
				uint16_t, size_t);

typedef
void (*io_write_fn_t)(struct vm_io_handler *, struct vm *,
				uint16_t, size_t, uint32_t);

/* Describes a single IO handler description entry. */
struct vm_io_handler_desc {

	/** The base address of the IO range for this description. */
	uint16_t addr;
	/** The number of bytes covered by this description. */
	size_t len;

	/** A pointer to the "read" function.
	 *
	 * The read function is called from the hypervisor whenever
	 * a read access to a range described in "ranges" occur.
	 * The arguments to the callback are:
	 *
	 *    - The address of the port to read from.
	 *    - The width of the read operation (1,2 or 4).
	 *
	 * The implementation must return the ports content as
	 * byte, word or doubleword (depending on the width).
	 *
	 * If the pointer is null, a read of 1's is assumed.
	 */

	io_read_fn_t io_read;
	/** A pointer to the "write" function.
	 *
	 * The write function is called from the hypervisor code
	 * whenever a write access to a range described in "ranges"
	 * occur. The arguments to the callback are:
	 *
	 *   - The address of the port to write to.
	 *   - The width of the write operation (1,2 or 4).
	 *   - The value to write as byte, word or doubleword
	 *     (depending on the width)
	 *
	 * The implementation must write the value to the port.
	 *
	 * If the pointer is null, the write access is ignored.
    */

	io_write_fn_t io_write;
};

struct vm_io_handler {
	struct vm_io_handler *next;
	struct vm_io_handler_desc desc;
};

#define IO_ATTR_R               0U
#define IO_ATTR_RW              1U
#define IO_ATTR_NO_ACCESS       2U

/* External Interfaces */
int io_instr_vmexit_handler(struct vcpu *vcpu);
void   setup_io_bitmap(struct vm *vm);
void   free_io_emulation_resource(struct vm *vm);
void   allow_guest_io_access(struct vm *vm, uint32_t address, uint32_t nbytes);
void   register_io_emulation_handler(struct vm *vm, struct vm_io_range *range,
		io_read_fn_t io_read_fn_ptr,
		io_write_fn_t io_write_fn_ptr);
int dm_emulate_pio_post(struct vcpu *vcpu);

/** Writes a 32 bit value to a memory mapped IO device.
 *
 *  @param value The 32 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void mmio_write_long(uint32_t value, void *addr)
{
	volatile uint32_t *addr32 = (volatile uint32_t *)addr;
	*addr32 = value;
}

/** Writes a 16 bit value to a memory mapped IO device.
 *
 *  @param value The 16 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void mmio_write_word(uint16_t value, void *addr)
{
	volatile uint16_t *addr16 = (volatile uint16_t *)addr;
	*addr16 = value;
}

/** Writes an 8 bit value to a memory mapped IO device.
 *
 *  @param value The 8 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void mmio_write_byte(uint8_t value, void *addr)
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
static inline uint32_t mmio_read_long(void *addr)
{
	return *((volatile uint32_t *)addr);
}

/** Reads a 16 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 16 bit value read from the given address.
 */
static inline uint16_t mmio_read_word(void *addr)
{
	return *((volatile uint16_t *)addr);
}

/** Reads an 8 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 8 bit  value read from the given address.
 */
static inline uint8_t mmio_read_byte(void *addr)
{
	return *((volatile uint8_t *)addr);
}


/** Writes a 32 bit value to a memory mapped IO device (ROM code version).
 *
 *  @param value The 32 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void __mmio_write_long(uint32_t value, void *addr)
{
	volatile uint32_t *addr32 = (volatile uint32_t *)addr;
	*addr32 = value;
}

/** Writes a 16 bit value to a memory mapped IO device (ROM code version).
 *
 *  @param value The 16 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void __mmio_write_word(uint16_t value, void *addr)
{
	volatile uint16_t *addr16 = (volatile uint16_t *)addr;
	*addr16 = value;
}

/** Writes an 8 bit value to a memory mapped IO device (ROM code version).
 *
 *  @param value The 8 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void __mmio_write_byte(uint8_t value, void *addr)
{
	volatile uint8_t *addr8 = (volatile uint8_t *)addr;
	*addr8 = value;
}

/** Reads a 32 bit value from a memory mapped IO device (ROM code version).
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 32 bit value read from the given address.
 */
static inline uint32_t __mmio_read_long(void *addr)
{
	return *((volatile uint32_t *)addr);
}

/** Reads a 16 bit value from a memory mapped IO device (ROM code version).
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 16 bit value read from the given address.
 */
static inline uint16_t __mmio_read_word(void *addr)
{
	return *((volatile uint16_t *)addr);
}

/** Reads an 8 bit value from a memory mapped IO device (ROM code version).
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 32 16 value read from the given address.
 */
static inline uint8_t __mmio_read_byte(void *addr)
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
static inline void setl(void *addr, uint32_t mask, uint32_t value)
{
	mmio_write_long((mmio_read_long(addr) & ~mask) | value, addr);
}

/** Reads a 16 Bit memory mapped IO register, mask it and write it back into
 *  memory mapped IO register.
 *
 * @param addr    The address of the memory mapped IO register.
 * @param mask    The mask to apply to the value read.
 * @param value   The 16 bit value to write.
 */
static inline void setw(void *addr, uint16_t mask, uint16_t value)
{
	mmio_write_word((mmio_read_word(addr) & ~mask) | value, addr);
}

/** Reads a 8 Bit memory mapped IO register, mask it and write it back into
 *  memory mapped IO register.
 *
 * @param addr    The address of the memory mapped IO register.
 * @param mask    The mask to apply to the value read.
 * @param value   The 8 bit value to write.
 */
static inline void setb(void *addr, uint8_t mask, uint8_t value)
{
	mmio_write_byte((mmio_read_byte(addr) & ~mask) | value, addr);
}

/* MMIO memory access types */
enum mem_io_type {
	HV_MEM_IO_READ = 0,
	HV_MEM_IO_WRITE,
};

/* MMIO emulation related structures */
#define MMIO_TRANS_VALID        1U
#define MMIO_TRANS_INVALID      0U
struct mem_io {
	uint64_t paddr;      /* Physical address being accessed */
	enum mem_io_type read_write;   /* 0 = read / 1 = write operation */
	uint8_t access_size; /* Access size being emulated */
	uint8_t sign_extend_read; /* 1 if sign extension required for read */
	uint64_t value;      /* Value read or value to write */
	uint8_t mmio_status; /* Indicates if this MMIO transaction is valid */
	/* Used to store emulation context for this mmio transaction */
	void *private_data;
};

#endif /* _IO_H defined */
