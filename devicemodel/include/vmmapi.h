/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#ifndef _VMMAPI_H_
#define	_VMMAPI_H_

#include <sys/param.h>
#include <uuid/uuid.h>
#include "types.h"
#include "vmm.h"

/*
 * API version for out-of-tree consumers for making compile time decisions.
 */
#define	VMMAPI_VERSION	0103	/* 2 digit major followed by 2 digit minor */

#define	MB	(1024 * 1024UL)
#define	GB	(1024 * 1024 * 1024UL)

#define ALIGN_UP(x, align)	(((x) + ((align)-1)) & ~((align)-1))
#define ALIGN_DOWN(x, align)	((x) & ~((align)-1))

struct vmctx {
	int     fd;
	int     vmid;
	int     ioreq_client;
	uint32_t lowmem_limit;
	int     memflags;
	size_t  lowmem;
	size_t  highmem;
	char    *mmap_lowmem;
	char    *mmap_highmem;
	char    *baseaddr;
	char    *name;
	uuid_t	vm_uuid;

	/* fields to track virtual devices */
	void *atkbdc_base;
	void *vrtc;
	void *ioc_dev;
};

/*
 * Different styles of mapping the memory assigned to a VM into the address
 * space of the controlling process.
 */
enum vm_mmap_style {
	VM_MMAP_NONE,		/* no mapping */
	VM_MMAP_ALL,		/* fully and statically mapped */
	VM_MMAP_SPARSE,		/* mappings created on-demand */
};

/*
 * 'flags' value passed to 'vm_set_memflags()'.
 */
#define	VM_MEM_F_INCORE	0x01	/* include guest memory in core file */
#define	VM_MEM_F_WIRED	0x02	/* guest memory is wired */

#define	VM_MEMMAP_F_WIRED	0x01
#define	VM_MEMMAP_F_IOMMU	0x02

#define	VM_MEMSEG_NAME(m)	((m)->name[0] != '\0' ? (m)->name : NULL)

#define	PROT_RW		(PROT_READ | PROT_WRITE)
#define	PROT_ALL	(PROT_READ | PROT_WRITE | PROT_EXEC)

struct vm_lapic_msi {
	uint64_t	msg;
	uint64_t	addr;
};

struct vm_isa_irq {
	int		atpic_irq;
	int		ioapic_irq;
};

/*
 * Create a device memory segment identified by 'segid'.
 *
 * Returns a pointer to the memory segment on success and MAP_FAILED otherwise.
 */
void	*vm_create_devmem(struct vmctx *ctx, int segid, const char *name,
			  size_t len);
int	vm_create(const char *name);
int	vm_get_device_fd(struct vmctx *ctx);
struct	vmctx *vm_open(const char *name);
void	vm_close(struct vmctx *ctx);
void	vm_pause(struct vmctx *ctx);
int	vm_set_shared_io_page(struct vmctx *ctx, uint64_t page_vma);
int	vm_create_ioreq_client(struct vmctx *ctx);
int	vm_destroy_ioreq_client(struct vmctx *ctx);
int	vm_attach_ioreq_client(struct vmctx *ctx);
int	vm_notify_request_done(struct vmctx *ctx, int vcpu);
void	vm_set_suspend_mode(enum vm_suspend_how how);
int	vm_get_suspend_mode(void);
void	vm_destroy(struct vmctx *ctx);
int	vm_parse_memsize(const char *optarg, size_t *memsize);
int	vm_map_memseg_vma(struct vmctx *ctx, size_t len, vm_paddr_t gpa,
	uint64_t vma, int prot);
int	vm_setup_memory(struct vmctx *ctx, size_t len, enum vm_mmap_style s);
void	vm_unsetup_memory(struct vmctx *ctx);
bool	check_hugetlb_support(void);
int	hugetlb_setup_memory(struct vmctx *ctx);
void	hugetlb_unsetup_memory(struct vmctx *ctx);
void	*vm_map_gpa(struct vmctx *ctx, vm_paddr_t gaddr, size_t len);
uint32_t vm_get_lowmem_limit(struct vmctx *ctx);
void	vm_set_lowmem_limit(struct vmctx *ctx, uint32_t limit);
void	vm_set_memflags(struct vmctx *ctx, int flags);
int	vm_get_memflags(struct vmctx *ctx);
size_t	vm_get_lowmem_size(struct vmctx *ctx);
size_t	vm_get_highmem_size(struct vmctx *ctx);
int	vm_run(struct vmctx *ctx);
int	vm_suspend(struct vmctx *ctx, enum vm_suspend_how how);
int	vm_apicid2vcpu(struct vmctx *ctx, int apicid);
int	vm_lapic_msi(struct vmctx *ctx, uint64_t addr, uint64_t msg);
int	vm_ioapic_assert_irq(struct vmctx *ctx, int irq);
int	vm_ioapic_deassert_irq(struct vmctx *ctx, int irq);
int	vm_isa_assert_irq(struct vmctx *ctx, int atpic_irq, int ioapic_irq);
int	vm_isa_deassert_irq(struct vmctx *ctx, int atpic_irq, int ioapic_irq);
int	vm_isa_pulse_irq(struct vmctx *ctx, int atpic_irq, int ioapic_irq);
int	vm_assign_ptdev(struct vmctx *ctx, int bus, int slot, int func);
int	vm_unassign_ptdev(struct vmctx *ctx, int bus, int slot, int func);
int	vm_map_ptdev_mmio(struct vmctx *ctx, int bus, int slot, int func,
			  vm_paddr_t gpa, size_t len, vm_paddr_t hpa);
int	vm_setup_ptdev_msi(struct vmctx *ctx,
			   struct acrn_vm_pci_msix_remap *msi_remap);
int	vm_set_ptdev_msix_info(struct vmctx *ctx, struct ic_ptdev_irq *ptirq);
int	vm_reset_ptdev_msix_info(struct vmctx *ctx, uint16_t virt_bdf,
	int vector_count);
int	vm_set_ptdev_intx_info(struct vmctx *ctx, uint16_t virt_bdf,
	uint16_t phys_bdf, int virt_pin, int phys_pin, bool pic_pin);
int	vm_reset_ptdev_intx_info(struct vmctx *ctx, int virt_pin, bool pic_pin);

int	vm_create_vcpu(struct vmctx *ctx, int vcpu_id);

int	vm_get_cpu_state(struct vmctx *ctx, void *state_buf);

extern bool hugetlb;
#endif	/* _VMMAPI_H_ */
