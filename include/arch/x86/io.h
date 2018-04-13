/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IO_H
#define IO_H

/* Definition of a IO port range */
struct vm_io_range {
	uint16_t base;		/* IO port base */
	uint16_t len;		/* IO port range */
	int flags;		/* IO port attributes */
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
	if (sz == 1)
		io_write_byte(v, addr);
	else if (sz == 2)
		io_write_word(v, addr);
	else
		io_write_long(v, addr);
}

static inline uint32_t io_read(uint16_t addr, size_t sz)
{
	if (sz == 1)
		return io_read_byte(addr);
	if (sz == 2)
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

#define IO_ATTR_R               0
#define IO_ATTR_RW              1
#define IO_ATTR_NO_ACCESS       2

/* External Interfaces */
int io_instr_handler(struct vcpu *vcpu);
void   setup_io_bitmap(struct vm *vm);
void   free_io_emulation_resource(struct vm *vm);
void   register_io_emulation_handler(struct vm *vm, struct vm_io_range *range,
		io_read_fn_t io_read_fn_ptr,
		io_write_fn_t io_write_fn_ptr);
int dm_emulate_pio_post(struct vcpu *vcpu);

/** Writes a 32 bit value to a memory mapped IO device.
 *
 *  @param value The 32 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void mmio_write_long(uint32_t value, uint64_t addr)
{
	*((uint32_t *)addr) = value;
}

/** Writes a 16 bit value to a memory mapped IO device.
 *
 *  @param value The 16 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void mmio_write_word(uint32_t value, uint64_t addr)
{
	*((uint16_t *)addr) = value;
}

/** Writes an 8 bit value to a memory mapped IO device.
 *
 *  @param value The 8 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void mmio_write_byte(uint32_t value, uint64_t addr)
{
	*((uint8_t *)addr) = value;
}

/** Reads a 32 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 32 bit value read from the given address.
 */
static inline uint32_t mmio_read_long(uint64_t addr)
{
	return *((uint32_t *)addr);
}

/** Reads a 16 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 16 bit value read from the given address.
 */
static inline uint16_t mmio_read_word(uint64_t addr)
{
	return *((uint16_t *)addr);
}

/** Reads an 8 bit value from a memory mapped IO device.
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 8 bit  value read from the given address.
 */
static inline uint8_t mmio_read_byte(uint64_t addr)
{
	return *((uint8_t *)addr);
}

/** Sets bits in a 32 bit value from a memory mapped IO device.
 *
 *  @param mask Contains the bits to set at the memory address.
 *              Bits set in this mask are set in the memory
 *              location.
 *  @param addr The memory address to read from/write to.
 */
static inline void mmio_or_long(uint32_t mask, uint64_t addr)
{
	*((uint32_t *)addr) |= mask;
}

/** Sets bits in a 16 bit value from a memory mapped IO device.
 *
 *  @param mask Contains the bits to set at the memory address.
 *              Bits set in this mask are set in the memory
 *              location.
 *  @param addr The memory address to read from/write to.
 */
static inline void mmio_or_word(uint32_t mask, uint64_t addr)
{
	*((uint16_t *)addr) |= mask;
}

/** Sets bits in an 8 bit value from a memory mapped IO device.
 *
 *  @param mask Contains the bits to set at the memory address.
 *              Bits set in this mask are set in the memory
 *              location.
 *  @param addr The memory address to read from/write to.
 */
static inline void mmio_or_byte(uint32_t mask, uint64_t addr)
{
	*((uint8_t *)addr) |= mask;
}

/** Clears bits in a 32 bit value from a memory mapped IO device.
 *
 *  @param mask Contains the bits to clear at the memory address.
 *              Bits set in this mask are cleared in the memory
 *              location.
 *  @param addr The memory address to read from/write to.
 */
static inline void mmio_and_long(uint32_t mask, uint64_t addr)
{
	*((uint32_t *)addr) &= ~mask;
}

/** Clears bits in a 16 bit value from a memory mapped IO device.
 *
 *  @param mask Contains the bits to clear at the memory address.
 *              Bits set in this mask are cleared in the memory
 *              location.
 *  @param addr The memory address to read from/write to.
 */
static inline void mmio_and_word(uint32_t mask, uint64_t addr)
{
	*((uint16_t *)addr) &= ~mask;
}

/** Clears bits in an 8 bit value from a memory mapped IO device.
 *
 *  @param mask Contains the bits to clear at the memory address.
 *              Bits set in this mask are cleared in the memory
 *              location.
 *  @param addr The memory address to read from/write to.
 */
static inline void mmio_and_byte(uint32_t mask, uint64_t addr)
{
	*((uint8_t *)addr) &= ~mask;
}

/** Performs a read-modify-write cycle for a 32 bit value from a MMIO device.
 *
 *  Reads a 32 bit value from a memory mapped IO device, sets and clears
 *  bits and writes the value back. If a bit is specified in both, the 'set'
 *  and in the 'clear' mask, it is undefined whether the resulting bit is set
 *  or cleared.
 *
 *  @param set  Contains the bits to set. Bits set in this mask
 *              are set at the memory address.
 *  @param clear Contains the bits to clear. Bits set in this
 *               mask are cleared at the memory address.
 *  @param addr The memory address to read from/write to.
 */
static inline void mmio_rmw_long(uint32_t set, uint32_t clear, uint64_t addr)
{
	*((uint32_t *)addr) =
	    (*((uint32_t *)addr) & ~clear) | set;
}

/** Performs a read-modify-write cycle for a 16 bit value from a MMIO device.
 *
 *  Reads a 16 bit value from a memory mapped IO device, sets and clears
 *  bits and writes the value back. If a bit is specified in both, the 'set'
 *  and in the 'clear' mask, it is undefined whether the resulting bit is set
 *  or cleared.
 *
 *  @param set  Contains the bits to set. Bits set in this mask
 *              are set at the memory address.
 *  @param clear Contains the bits to clear. Bits set in this
 *               mask are cleared at the memory address.
 *  @param addr The memory address to read from/write to.
 */
static inline void mmio_rmw_word(uint32_t set, uint32_t clear, uint64_t addr)
{
	*((uint16_t *)addr) =
	    (*((uint16_t *)addr) & ~clear) | set;
}

/** Performs a read-modify-write cycle for an 8 bit value from a MMIO device.
 *
 *  Reads an 8 bit value from a memory mapped IO device, sets and clears
 *  bits and writes the value back. If a bit is specified in both, the 'set'
 *  and in the 'clear' mask, it is undefined whether the resulting bit is set
 *  or cleared.
 *
 *  @param set  Contains the bits to set. Bits set in this mask
 *              are set at the memory address.
 *  @param clear Contains the bits to clear. Bits set in this
 *               mask are cleared at the memory address.
 *  @param addr The memory address to read from/write to.
 */
static inline void mmio_rmw_byte(uint32_t set, uint32_t clear, uint64_t addr)
{
	*((uint8_t *)addr) = (*((uint8_t *)addr) & ~clear) | set;
}

/** Writes a 32 bit value to a memory mapped IO device (ROM code version).
 *
 *  @param value The 32 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void __mmio_write_long(uint32_t value, uint64_t addr)
{
	*((uint32_t *)addr) = value;
}

/** Writes a 16 bit value to a memory mapped IO device (ROM code version).
 *
 *  @param value The 16 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void __mmio_write_word(uint32_t value, uint64_t addr)
{
	*((uint16_t *)addr) = value;
}

/** Writes an 8 bit value to a memory mapped IO device (ROM code version).
 *
 *  @param value The 8 bit value to write.
 *  @param addr The memory address to write to.
 */
static inline void __mmio_write_byte(uint32_t value, uint64_t addr)
{
	*((uint8_t *)addr) = value;
}

/** Reads a 32 bit value from a memory mapped IO device (ROM code version).
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 32 bit value read from the given address.
 */
static inline uint32_t __mmio_read_long(uint64_t addr)
{
	return *((uint32_t *)addr);
}

/** Reads a 16 bit value from a memory mapped IO device (ROM code version).
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 16 bit value read from the given address.
 */
static inline uint16_t __mmio_read_word(uint64_t addr)
{
	return *((uint16_t *)addr);
}

/** Reads an 8 bit value from a memory mapped IO device (ROM code version).
 *
 *  @param addr The memory address to read from.
 *
 *  @return The 32 16 value read from the given address.
 */
static inline uint8_t __mmio_read_byte(uint64_t addr)
{
	return *((uint8_t *)addr);
}

/** Sets bits in a 32 bit value from a MMIO device (ROM code version).
 *
 *  @param mask Contains the bits to set at the memory address.
 *              Bits set in this mask are set in the memory
 *              location.
 *  @param addr The memory address to read from/write to.
 */
static inline void __mmio_or_long(uint32_t mask, uint64_t addr)
{
	*((uint32_t *)addr) |= mask;
}

/** Sets bits in a 16 bit value from a MMIO device (ROM code version).
 *
 *  @param mask Contains the bits to set at the memory address.
 *              Bits set in this mask are set in the memory
 *              location.
 *  @param addr The memory address to read from/write to.
 */
static inline void __mmio_or_word(uint32_t mask, uint64_t addr)
{
	*((uint16_t *)addr) |= mask;
}

/** Sets bits in an 8 bit value from a MMIO device (ROM code version).
 *
 *  @param mask Contains the bits to set at the memory address.
 *              Bits set in this mask are set in the memory
 *              location.
 *  @param addr The memory address to read from/write to.
 */
static inline void __mmio_or_byte(uint32_t mask, uint64_t addr)
{
	*((uint8_t *)addr) |= mask;
}

/** Clears bits in a 32 bit value from a MMIO device (ROM code version).
 *
 *  @param mask Contains the bits to clear at the memory address.
 *              Bits set in this mask are cleared in the memory
 *              location.
 *  @param addr The memory address to read from/write to.
 */
static inline void __mmio_and_long(uint32_t mask, uint64_t addr)
{
	*((uint32_t *)addr) &= ~mask;
}

/** Clears bits in a 16 bit value from a MMIO device (ROM code version).
 *
 *  @param mask Contains the bits to clear at the memory address.
 *              Bits set in this mask are cleared in the memory
 *              location.
 *  @param addr The memory address to read from/write to.
 */
static inline void __mmio_and_word(uint32_t mask, uint64_t addr)
{
	*((uint16_t *)addr) &= ~mask;
}

/** Clears bits in an 8 bit value from a MMIO device (ROM code version).
 *
 *  @param mask Contains the bits to clear at the memory address.
 *              Bits set in this mask are cleared in the memory
 *              location.
 *  @param addr The memory address to read from/write to.
 */
static inline void __mmio_and_byte(uint32_t mask, uint64_t addr)
{
	*((uint8_t *)addr) &= ~mask;
}

/** Performs a read-modify-write cycle for a 32 bit value from a MMIO device
 * (ROM code version).
 *
 *  Reads a 32 bit value from a memory mapped IO device, sets and clears
 *  bits and writes the value back. If a bit is specified in both, the 'set'
 *  and in the 'clear' mask, it is undefined whether the resulting bit is set
 *  or cleared.
 *
 *  @param set  Contains the bits to set. Bits set in this mask
 *              are set at the memory address.
 *  @param clear Contains the bits to clear. Bits set in this
 *               mask are cleared at the memory address.
 *  @param addr The memory address to read from/write to.
 */
static inline void
__mmio_rmw_long(uint32_t set, uint32_t clear, uint64_t addr)
{
	*((uint32_t *)addr) =
	    (*((uint32_t *)addr) & ~clear) | set;
}

/** Performs a read-modify-write cycle for a 16 bit value from a MMIO device
 * (ROM code version).
 *
 *  Reads a 16 bit value from a memory mapped IO device, sets and clears
 *  bits and writes the value back. If a bit is specified in both, the 'set'
 *  and in the 'clear' mask, it is undefined whether the resulting bit is set
 *  or cleared.
 *
 *  @param set  Contains the bits to set. Bits set in this mask
 *              are set at the memory address.
 *  @param clear Contains the bits to clear. Bits set in this
 *               mask are cleared at the memory address.
 *  @param addr The memory address to read from/write to.
 */
static inline void
__mmio_rmw_word(uint32_t set, uint32_t clear, uint64_t addr)
{
	*((uint16_t *)addr) =
	    (*((uint16_t *)addr) & ~clear) | set;
}

/** Performs a read-modify-write cycle for an 8 bit value from a MMIO device
 * (ROM code version).
 *
 *  Reads an 8 bit value from a memory mapped IO device, sets and clears
 *  bits and writes the value back. If a bit is specified in both, the 'set'
 *  and in the 'clear' mask, it is undefined whether the resulting bit is set
 *  or cleared.
 *
 *  @param set  Contains the bits to set. Bits set in this mask
 *              are set at the memory address.
 *  @param clear Contains the bits to clear. Bits set in this
 *               mask are cleared at the memory address.
 *  @param addr The memory address to read from/write to.
 */
static inline void
__mmio_rmw_byte(uint32_t set, uint32_t clear, uint64_t addr)
{
	*((uint8_t *)addr) = (*((uint8_t *)addr) & ~clear) | set;
}

/** Reads a 32 Bit memory mapped IO register, mask it and write it back into
 *  memory mapped IO register.
 *
 * @param addr    The address of the memory mapped IO register.
 * @param mask    The mask to apply to the value read.
 * @param value   The 32 bit value to write.
 */
static inline void setl(uint64_t addr, uint32_t mask, uint32_t value)
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
static inline void setw(uint64_t addr, uint32_t mask, uint32_t value)
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
static inline void setb(uint64_t addr, uint32_t mask, uint32_t value)
{
	mmio_write_byte((mmio_read_byte(addr) & ~mask) | value, addr);
}

/* MMIO memory access types */
enum mem_io_type {
	HV_MEM_IO_READ = 0,
	HV_MEM_IO_WRITE,
};

/* MMIO emulation related structures */
#define MMIO_TRANS_VALID        1
#define MMIO_TRANS_INVALID      0
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
