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


#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "vmmapi.h"
#include "mevent.h"

#include "dm.h"
#include "pci_core.h"
#include "log.h"

#define MAP_NOCORE 0
#define MAP_ALIGNED_SUPER 0

/*
 * Size of the guard region before and after the virtual address space
 * mapping the guest physical memory. This must be a multiple of the
 * superpage size for performance reasons.
 */
#define	VM_MMAP_GUARD_SIZE	(4 * MB)

#define SUPPORT_VHM_API_VERSION_MAJOR	1
#define SUPPORT_VHM_API_VERSION_MINOR	0

static int
check_api(int fd)
{
	struct api_version api_version;
	int error;

	error = ioctl(fd, IC_GET_API_VERSION, &api_version);
	if (error) {
		pr_err("failed to get vhm api version\n");
		return -1;
	}

	if (api_version.major_version != SUPPORT_VHM_API_VERSION_MAJOR ||
		api_version.minor_version != SUPPORT_VHM_API_VERSION_MINOR) {
		pr_err("not support vhm api version\n");
		return -1;
	}

	pr_info("VHM api version %d.%d\n", api_version.major_version,
			api_version.minor_version);

	return 0;
}

static int devfd = -1;

struct vmctx *
vm_create(const char *name, uint64_t req_buf)
{
	struct vmctx *ctx;
	struct acrn_create_vm create_vm;
	int error, retry = 10;
	uuid_t vm_uuid;
	struct stat tmp_st;

	memset(&create_vm, 0, sizeof(struct acrn_create_vm));
	ctx = calloc(1, sizeof(struct vmctx) + strnlen(name, PATH_MAX) + 1);
	if ((ctx == NULL) || (devfd != -1))
		goto err;

	if (stat("/dev/acrn_vhm", &tmp_st) == 0) {
		devfd = open("/dev/acrn_vhm", O_RDWR|O_CLOEXEC);
	} else if (stat("/dev/acrn_hsm", &tmp_st) == 0) {
		devfd = open("/dev/acrn_hsm", O_RDWR|O_CLOEXEC);
	} else {
		devfd = -1;
	}
	if (devfd == -1) {
		pr_err("Could not open /dev/acrn_vhm\n");
		goto err;
	}

	if (check_api(devfd) < 0)
		goto err;

	if (guest_uuid_str == NULL)
		guest_uuid_str = "d2795438-25d6-11e8-864e-cb7a18b34643";

	error = uuid_parse(guest_uuid_str, vm_uuid);
	if (error != 0)
		goto err;

	/* save vm uuid to ctx */
	uuid_copy(ctx->vm_uuid, vm_uuid);

	/* Pass uuid as parameter of create vm*/
	uuid_copy(create_vm.uuid, vm_uuid);

	ctx->fd = devfd;
	ctx->lowmem_limit = 2 * GB;
	ctx->highmem_gpa_base = PCI_EMUL_MEMLIMIT64;
	ctx->name = (char *)(ctx + 1);
	strncpy(ctx->name, name, strnlen(name, PATH_MAX) + 1);

	/* Set trusty enable flag */
	if (trusty_enabled)
		create_vm.vm_flag |= GUEST_FLAG_SECURE_WORLD_ENABLED;
	else
		create_vm.vm_flag &= (~GUEST_FLAG_SECURE_WORLD_ENABLED);

	if (lapic_pt) {
		create_vm.vm_flag |= GUEST_FLAG_LAPIC_PASSTHROUGH;
		create_vm.vm_flag |= GUEST_FLAG_RT;
		create_vm.vm_flag |= GUEST_FLAG_IO_COMPLETION_POLLING;
	} else {
		create_vm.vm_flag &= (~GUEST_FLAG_LAPIC_PASSTHROUGH);
		create_vm.vm_flag &= (~GUEST_FLAG_IO_COMPLETION_POLLING);
	}

	if (is_rtvm) {
		create_vm.vm_flag |= GUEST_FLAG_RT;
		create_vm.vm_flag |= GUEST_FLAG_IO_COMPLETION_POLLING;
	}

	create_vm.req_buf = req_buf;
	while (retry > 0) {
		error = ioctl(ctx->fd, IC_CREATE_VM, &create_vm);
		if (error == 0)
			break;
		usleep(500000);
		retry--;
	}

	if (error) {
		pr_err("failed to create VM %s\n", ctx->name);
		goto err;
	}

	ctx->vmid = create_vm.vmid;

	return ctx;

err:
	if (ctx != NULL)
		free(ctx);

	return NULL;
}

int
vm_create_ioreq_client(struct vmctx *ctx)
{
	return ioctl(ctx->fd, IC_CREATE_IOREQ_CLIENT, 0);
}

int
vm_destroy_ioreq_client(struct vmctx *ctx)
{
	return ioctl(ctx->fd, IC_DESTROY_IOREQ_CLIENT, ctx->ioreq_client);
}

int
vm_attach_ioreq_client(struct vmctx *ctx)
{
	int error;

	error = ioctl(ctx->fd, IC_ATTACH_IOREQ_CLIENT, ctx->ioreq_client);

	if (error) {
		pr_err("attach ioreq client return %d "
			"(1 = destroying, could be triggered by Power State "
				"change, others = error)\n", error);
		return error;
	}

	return 0;
}

int
vm_notify_request_done(struct vmctx *ctx, int vcpu)
{
	int error;
	struct ioreq_notify notify;

	bzero(&notify, sizeof(notify));
	notify.client_id = ctx->ioreq_client;
	notify.vcpu = vcpu;

	error = ioctl(ctx->fd, IC_NOTIFY_REQUEST_FINISH, &notify);

	if (error) {
		pr_err("failed: notify request finish\n");
		return -1;
	}

	return 0;
}

void
vm_destroy(struct vmctx *ctx)
{
	if (!ctx)
		return;

	ioctl(ctx->fd, IC_DESTROY_VM, NULL);
	close(ctx->fd);
	free(ctx);
	devfd = -1;
}

int
vm_parse_memsize(const char *optarg, size_t *ret_memsize)
{
	char *endptr;
	size_t optval;
	int shift;

	optval = strtoul(optarg, &endptr, 0);
	switch (tolower((unsigned char)*endptr)) {
	case 'g':
		shift = 30;
		break;
	case 'm':
		shift = 20;
		break;
	case 'k':
		shift = 10;
		break;
	case 'b':
	case '\0': /* No unit. */
		shift = 0;
	default:
		/* Unrecognized unit. */
		return -1;
	}

	optval = optval << shift;
	if (optval < 128 * MB)
		return -1;

	*ret_memsize = optval;

	return 0;
}

uint32_t
vm_get_lowmem_limit(struct vmctx *ctx)
{
	return ctx->lowmem_limit;
}

int
vm_map_memseg_vma(struct vmctx *ctx, size_t len, vm_paddr_t gpa,
	uint64_t vma, int prot)
{
	struct vm_memmap memmap;

	bzero(&memmap, sizeof(struct vm_memmap));
	memmap.type = VM_MEMMAP_SYSMEM;
	memmap.using_vma = 1;
	memmap.vma_base = vma;
	memmap.len = len;
	memmap.gpa = gpa;
	memmap.prot = prot;
	return ioctl(ctx->fd, IC_SET_MEMSEG, &memmap);
}

int
vm_setup_memory(struct vmctx *ctx, size_t memsize)
{
	/*
	 * If 'memsize' cannot fit entirely in the 'lowmem' segment then
	 * create another 'highmem' segment above 4GB for the remainder.
	 */
	if (memsize > ctx->lowmem_limit) {
		ctx->lowmem = ctx->lowmem_limit;
		ctx->highmem = memsize - ctx->lowmem_limit;
	} else {
		ctx->lowmem = memsize;
		ctx->highmem = 0;
	}

	ctx->biosmem = high_bios_size();

	return hugetlb_setup_memory(ctx);
}

void
vm_unsetup_memory(struct vmctx *ctx)
{
	/*
	 * For security reason, clean the VM's memory region
	 * to avoid secret information leaking in below case:
	 * After a UOS is destroyed, the memory will be reclaimed,
	 * then if the new UOS starts, that memory region may be
	 * allocated the new UOS, the previous UOS sensitive data
	 * may be leaked to the new UOS if the memory is not cleared.
	 *
	 */
	bzero((void *)ctx->baseaddr, ctx->lowmem);
	if (ctx->highmem > 0) {
		bzero((void *)(ctx->baseaddr + ctx->highmem_gpa_base),
			ctx->highmem);
	}

	hugetlb_unsetup_memory(ctx);
}

/*
 * Returns a non-NULL pointer if [gaddr, gaddr+len) is entirely contained in
 * the lowmem or highmem regions.
 *
 * In particular return NULL if [gaddr, gaddr+len) falls in guest MMIO region.
 * The instruction emulation code depends on this behavior.
 */
void *
vm_map_gpa(struct vmctx *ctx, vm_paddr_t gaddr, size_t len)
{

	if (ctx->lowmem > 0) {
		if (gaddr < ctx->lowmem && len <= ctx->lowmem &&
		    gaddr + len <= ctx->lowmem)
			return (ctx->baseaddr + gaddr);
	}

	if (ctx->highmem > 0) {
		if (gaddr >= ctx->highmem_gpa_base) {
			if (gaddr < ctx->highmem_gpa_base + ctx->highmem &&
			    len <= ctx->highmem &&
			    gaddr + len <= ctx->highmem_gpa_base + ctx->highmem)
				return (ctx->baseaddr + gaddr);
		}
	}

	return NULL;
}

size_t
vm_get_lowmem_size(struct vmctx *ctx)
{
	return ctx->lowmem;
}

size_t
vm_get_highmem_size(struct vmctx *ctx)
{
	return ctx->highmem;
}

int
vm_run(struct vmctx *ctx)
{
	int error;

	error = ioctl(ctx->fd, IC_START_VM, &ctx->vmid);

	return error;
}

void
vm_pause(struct vmctx *ctx)
{
	ioctl(ctx->fd, IC_PAUSE_VM, &ctx->vmid);
}

void
vm_reset(struct vmctx *ctx)
{
	ioctl(ctx->fd, IC_RESET_VM, &ctx->vmid);
}

void
vm_clear_ioreq(struct vmctx *ctx)
{
	ioctl(ctx->fd, IC_CLEAR_VM_IOREQ, NULL);
}

static int suspend_mode = VM_SUSPEND_NONE;

void
vm_set_suspend_mode(enum vm_suspend_how how)
{
	pr_notice("vm mode changed from %d to %d\n", suspend_mode, how);
	suspend_mode = how;
}

int
vm_get_suspend_mode(void)
{
	return suspend_mode;
}

int
vm_suspend(struct vmctx *ctx, enum vm_suspend_how how)
{
	vm_set_suspend_mode(how);
	mevent_notify();

	return 0;
}

int
vm_lapic_msi(struct vmctx *ctx, uint64_t addr, uint64_t msg)
{
	struct acrn_msi_entry msi;

	bzero(&msi, sizeof(msi));
	msi.msi_addr = addr;
	msi.msi_data = msg;

	return ioctl(ctx->fd, IC_INJECT_MSI, &msi);
}

int
vm_set_gsi_irq(struct vmctx *ctx, int gsi, uint32_t operation)
{
	struct acrn_irqline_ops op;
	uint64_t *req =  (uint64_t *)&op;

	op.op = operation;
	op.gsi = (uint32_t)gsi;

	return ioctl(ctx->fd, IC_SET_IRQLINE, *req);
}

int
vm_assign_ptdev(struct vmctx *ctx, int bus, int slot, int func)
{
	uint16_t bdf;

	bdf = ((bus & 0xff) << 8) | ((slot & 0x1f) << 3) |
			(func & 0x7);

	return ioctl(ctx->fd, IC_ASSIGN_PTDEV, &bdf);
}

int
vm_unassign_ptdev(struct vmctx *ctx, int bus, int slot, int func)
{
	uint16_t bdf;

	bdf = ((bus & 0xff) << 8) | ((slot & 0x1f) << 3) |
			(func & 0x7);

	return ioctl(ctx->fd, IC_DEASSIGN_PTDEV, &bdf);
}

int
vm_map_ptdev_mmio(struct vmctx *ctx, int bus, int slot, int func,
		   vm_paddr_t gpa, size_t len, vm_paddr_t hpa)
{
	struct vm_memmap memmap;

	bzero(&memmap, sizeof(struct vm_memmap));
	memmap.type = VM_MMIO;
	memmap.len = len;
	memmap.gpa = gpa;
	memmap.hpa = hpa;
	memmap.prot = PROT_ALL;

	return ioctl(ctx->fd, IC_SET_MEMSEG, &memmap);
}

int
vm_unmap_ptdev_mmio(struct vmctx *ctx, int bus, int slot, int func,
		   vm_paddr_t gpa, size_t len, vm_paddr_t hpa)
{
	struct vm_memmap memmap;

	bzero(&memmap, sizeof(struct vm_memmap));
	memmap.type = VM_MMIO;
	memmap.len = len;
	memmap.gpa = gpa;
	memmap.hpa = hpa;
	memmap.prot = PROT_ALL;

	return ioctl(ctx->fd, IC_UNSET_MEMSEG, &memmap);
}

int
vm_set_ptdev_msix_info(struct vmctx *ctx, struct ic_ptdev_irq *ptirq)
{
	if (!ptirq)
		return -1;

	return ioctl(ctx->fd, IC_SET_PTDEV_INTR_INFO, ptirq);
}

int
vm_reset_ptdev_msix_info(struct vmctx *ctx, uint16_t virt_bdf, uint16_t phys_bdf,
			 int vector_count)
{
	struct ic_ptdev_irq ptirq;

	bzero(&ptirq, sizeof(ptirq));
	ptirq.type = IRQ_MSIX;
	ptirq.virt_bdf = virt_bdf;
	ptirq.phys_bdf = phys_bdf;
	ptirq.msix.vector_cnt = vector_count;

	return ioctl(ctx->fd, IC_RESET_PTDEV_INTR_INFO, &ptirq);
}

int
vm_set_ptdev_intx_info(struct vmctx *ctx, uint16_t virt_bdf, uint16_t phys_bdf,
		       int virt_pin, int phys_pin, bool pic_pin)
{
	struct ic_ptdev_irq ptirq;

	bzero(&ptirq, sizeof(ptirq));
	ptirq.type = IRQ_INTX;
	ptirq.virt_bdf = virt_bdf;
	ptirq.phys_bdf = phys_bdf;
	ptirq.intx.virt_pin = virt_pin;
	ptirq.intx.phys_pin = phys_pin;
	ptirq.intx.is_pic_pin = pic_pin;

	return ioctl(ctx->fd, IC_SET_PTDEV_INTR_INFO, &ptirq);
}

int
vm_reset_ptdev_intx_info(struct vmctx *ctx, uint16_t virt_bdf, uint16_t phys_bdf,
			int virt_pin, bool pic_pin)
{
	struct ic_ptdev_irq ptirq;

	bzero(&ptirq, sizeof(ptirq));
	ptirq.type = IRQ_INTX;
	ptirq.intx.virt_pin = virt_pin;
	ptirq.intx.is_pic_pin = pic_pin;
	ptirq.virt_bdf = virt_bdf;
	ptirq.phys_bdf = phys_bdf;

	return ioctl(ctx->fd, IC_RESET_PTDEV_INTR_INFO, &ptirq);
}

int
vm_create_vcpu(struct vmctx *ctx, uint16_t vcpu_id)
{
	struct acrn_create_vcpu cv;
	int error;

	bzero(&cv, sizeof(struct acrn_create_vcpu));
	cv.vcpu_id = vcpu_id;
	error = ioctl(ctx->fd, IC_CREATE_VCPU, &cv);

	return error;
}

int
vm_set_vcpu_regs(struct vmctx *ctx, struct acrn_set_vcpu_regs *vcpu_regs)
{
	return ioctl(ctx->fd, IC_SET_VCPU_REGS, vcpu_regs);
}

int
vm_get_cpu_state(struct vmctx *ctx, void *state_buf)
{
	return ioctl(ctx->fd, IC_PM_GET_CPU_STATE, state_buf);
}

int
vm_intr_monitor(struct vmctx *ctx, void *intr_buf)
{
	return ioctl(ctx->fd, IC_VM_INTR_MONITOR, intr_buf);
}

int
vm_ioeventfd(struct vmctx *ctx, struct acrn_ioeventfd *args)
{
	return ioctl(ctx->fd, IC_EVENT_IOEVENTFD, args);
}

int
vm_irqfd(struct vmctx *ctx, struct acrn_irqfd *args)
{
	return ioctl(ctx->fd, IC_EVENT_IRQFD, args);
}
