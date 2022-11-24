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
#include "types.h"
#include "macros.h"
#include "pm.h"
#include "hsm_ioctl_defs.h"

/*
 * API version for out-of-tree consumers for making compile time decisions.
 */
#define	VMMAPI_VERSION	0103	/* 2 digit major followed by 2 digit minor */

#define ALIGN_UP(x, align)	(((x) + ((align)-1)) & ~((align)-1))
#define ALIGN_DOWN(x, align)	((x) & ~((align)-1))

#define CMOS_BUF_SIZE		256

struct vmctx {
	int     fd;
	int     vmid;
	int     ioreq_client;
	uint32_t lowmem_limit;
	uint64_t highmem_gpa_base;
	size_t  lowmem;
	size_t  fbmem;
	size_t  biosmem;
	size_t  highmem;
	char    *baseaddr;
	char    *name;

	/* fields to track virtual devices */
	void *atkbdc_base;
	void *vrtc;
	void *vpit;
	void *ioc_dev;
	void *tpm_dev;
	void *fb_base;

	/* BSP state. guest loader needs to fill it */
	struct acrn_vcpu_regs bsp_regs;

	/* if gvt-g is enabled for current VM */
	bool gvt_enabled;

	void (*update_gvt_bar)(struct vmctx *ctx);
};

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

struct vm_mem_region {
	uint64_t fd_offset;
	int fd;
};
bool	vm_find_memfd_region(struct vmctx *ctx, vm_paddr_t gpa,
			     struct vm_mem_region *ret_region);
bool    vm_allow_dmabuf(struct vmctx *ctx);
/*
 * Create a device memory segment identified by 'segid'.
 *
 * Returns a pointer to the memory segment on success and MAP_FAILED otherwise.
 */
struct	vmctx *vm_create(const char *name, uint64_t req_buf, int* vcpu_num);
void	vm_pause(struct vmctx *ctx);
void	vm_reset(struct vmctx *ctx);
int	vm_create_ioreq_client(struct vmctx *ctx);
int	vm_destroy_ioreq_client(struct vmctx *ctx);
int	vm_attach_ioreq_client(struct vmctx *ctx);
int	vm_notify_request_done(struct vmctx *ctx, int vcpu);
int	vm_setup_asyncio(struct vmctx *ctx, uint64_t base);
void	vm_clear_ioreq(struct vmctx *ctx);
const char *vm_state_to_str(enum vm_suspend_how idx);
void	vm_set_suspend_mode(enum vm_suspend_how how);
#ifdef DM_DEBUG
void	notify_vmloop_thread(void);
#endif
int	vm_get_suspend_mode(void);
void	vm_destroy(struct vmctx *ctx);
int	vm_parse_memsize(const char *optarg, size_t *memsize);
int	vm_map_memseg_vma(struct vmctx *ctx, size_t len, vm_paddr_t gpa,
	uint64_t vma, int prot);
int	vm_setup_memory(struct vmctx *ctx, size_t len);
void	vm_unsetup_memory(struct vmctx *ctx);
bool	init_hugetlb(void);
void	uninit_hugetlb(void);
int	hugetlb_setup_memory(struct vmctx *ctx);
void	hugetlb_unsetup_memory(struct vmctx *ctx);
void	*vm_map_gpa(struct vmctx *ctx, vm_paddr_t gaddr, size_t len);
uint32_t vm_get_lowmem_limit(struct vmctx *ctx);
size_t	vm_get_lowmem_size(struct vmctx *ctx);
size_t	vm_get_highmem_size(struct vmctx *ctx);
int	vm_run(struct vmctx *ctx);
int	vm_suspend(struct vmctx *ctx, enum vm_suspend_how how);
int	vm_lapic_msi(struct vmctx *ctx, uint64_t addr, uint64_t msg);
int	vm_set_gsi_irq(struct vmctx *ctx, int gsi, uint32_t operation);
int	vm_assign_pcidev(struct vmctx *ctx, struct acrn_pcidev *pcidev);
int	vm_deassign_pcidev(struct vmctx *ctx, struct acrn_pcidev *pcidev);
int	vm_assign_mmiodev(struct vmctx *ctx, struct acrn_mmiodev *mmiodev);
int	vm_deassign_mmiodev(struct vmctx *ctx, struct acrn_mmiodev *mmiodev);
int	vm_map_ptdev_mmio(struct vmctx *ctx, int bus, int slot, int func,
			  vm_paddr_t gpa, size_t len, vm_paddr_t hpa);
int	vm_unmap_ptdev_mmio(struct vmctx *ctx, int bus, int slot, int func,
			  vm_paddr_t gpa, size_t len, vm_paddr_t hpa);
int	vm_set_ptdev_intx_info(struct vmctx *ctx, uint16_t virt_bdf,
	uint16_t phys_bdf, int virt_pin, int phys_pin, bool pic_pin);
int	vm_reset_ptdev_intx_info(struct vmctx *ctx, uint16_t virt_bdf,
	uint16_t phys_bdf, int virt_pin, bool pic_pin);
int	vm_add_hv_vdev(struct vmctx *ctx, struct acrn_vdev *dev);
int	vm_remove_hv_vdev(struct vmctx *ctx, struct acrn_vdev *dev);

int	acrn_parse_cpu_affinity(char *arg);
uint64_t vm_get_cpu_affinity_dm(void);
int	vm_set_vcpu_regs(struct vmctx *ctx, struct acrn_vcpu_regs *cpu_regs);

int	vm_get_cpu_state(struct vmctx *ctx, void *state_buf);
int	vm_intr_monitor(struct vmctx *ctx, void *intr_buf);
void	vm_stop_watchdog(struct vmctx *ctx);
void	vm_reset_watchdog(struct vmctx *ctx);

int	vm_ioeventfd(struct vmctx *ctx, struct acrn_ioeventfd *args);
int	vm_irqfd(struct vmctx *ctx, struct acrn_irqfd *args);

/*
 * Return a string describing the meaning of the `error' code.
 */
char* errormsg(int error);

#endif	/* _VMMAPI_H_ */
