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
#include "errno.h"

#include "dm.h"
#include "pci_core.h"
#include "log.h"
#include "sw_load.h"
#include "acpi.h"

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

#define VM_STATE_STR_LEN                16
static const char vm_state_str[VM_SUSPEND_LAST][VM_STATE_STR_LEN] = {
	[VM_SUSPEND_NONE]		= "RUNNING",
	[VM_SUSPEND_SYSTEM_RESET]	= "SYSTEM_RESET",
	[VM_SUSPEND_FULL_RESET]		= "FULL_RESET",
	[VM_SUSPEND_POWEROFF]		= "POWEROFF",
	[VM_SUSPEND_SUSPEND]		= "SUSPEND",
	[VM_SUSPEND_HALT]		= "HALT",
	[VM_SUSPEND_TRIPLEFAULT]	= "TRIPLEFAULT"
};

const char *vm_state_to_str(enum vm_suspend_how idx)
{
	return (idx < VM_SUSPEND_LAST) ? vm_state_str[idx] : "UNKNOWN";
}

static int devfd = -1;
static uint64_t cpu_affinity_bitmap = 0UL;

static void add_one_pcpu(int pcpu_id)
{
	if (cpu_affinity_bitmap & (1UL << pcpu_id)) {
		pr_err("%s: pcpu_id %d has been allocated to this VM.\n", __func__, pcpu_id);
		return;
	}

	cpu_affinity_bitmap |= (1UL << pcpu_id);
}

/*
 * example options:
 *   --cpu_affinity 1,2,3
 */
int acrn_parse_cpu_affinity(char *opt)
{
	char *str, *cp, *cp_opt;
	int lapic_id;

	cp_opt = cp = strdup(opt);
	if (!cp) {
		pr_err("%s: strdup returns NULL\n", __func__);
		return -1;
	}

	/* white spaces within the commane line are invalid */
	while (cp && isdigit(cp[0])) {
		str = strpbrk(cp, ",");

		/* no more entries delimited by ',' */
		if (!str) {
			if (!dm_strtoi(cp, NULL, 10, &lapic_id)) {
				add_one_pcpu(lapic_to_pcpu(lapic_id));
			}
			break;
		} else {
			if (*str == ',') {
				/* after this, 'cp' points to the character after ',' */
				str = strsep(&cp, ",");

				/* parse the entry before ',' */
				if (dm_strtoi(str, NULL, 10, &lapic_id)) {
					goto err;
				}
				add_one_pcpu(lapic_to_pcpu(lapic_id));
			}
		}
	}

	free(cp_opt);
	return 0;

err:
	free(cp_opt);
	return -1;
}

uint64_t vm_get_cpu_affinity_dm(void)
{
	return cpu_affinity_bitmap;
}

struct vmctx *
vm_create(const char *name, uint64_t req_buf, int *vcpu_num)
{
	struct vmctx *ctx;
	struct acrn_vm_creation create_vm;
	int error, retry = 10;
	struct stat tmp_st;

	memset(&create_vm, 0, sizeof(struct acrn_vm_creation));
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

	ctx->gvt_enabled = false;
	ctx->fd = devfd;
	ctx->lowmem_limit = PCI_EMUL_MEMBASE32;
	ctx->highmem_gpa_base = HIGHRAM_START_ADDR;
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
		create_vm.vm_flag |= GUEST_FLAG_PMU_PASSTHROUGH;
	} else {
		create_vm.vm_flag &= (~GUEST_FLAG_LAPIC_PASSTHROUGH);
		create_vm.vm_flag &= (~GUEST_FLAG_IO_COMPLETION_POLLING);
	}

	/* command line arguments specified CPU affinity could overwrite HV's static configuration */
	create_vm.cpu_affinity = cpu_affinity_bitmap;
	strncpy((char *)create_vm.name, name, strnlen(name, MAX_VM_NAME_LEN));

	if (is_rtvm) {
		create_vm.vm_flag |= GUEST_FLAG_RT;
		create_vm.vm_flag |= GUEST_FLAG_IO_COMPLETION_POLLING;
	}

	create_vm.ioreq_buf = req_buf;
	while (retry > 0) {
		error = ioctl(ctx->fd, ACRN_IOCTL_CREATE_VM, &create_vm);
		if (error == 0)
			break;
		usleep(500000);
		retry--;
	}

	if (error) {
		pr_err("failed to create VM %s, %s.\n", ctx->name, errormsg(errno));
		goto err;
	}

	*vcpu_num = create_vm.vcpu_num;
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
	int error;
	error = ioctl(ctx->fd, ACRN_IOCTL_CREATE_IOREQ_CLIENT, 0);
	if (error) {
		pr_err("ACRN_IOCTL_CREATE_IOREQ_CLIENT ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_destroy_ioreq_client(struct vmctx *ctx)
{
	int error;
	error = ioctl(ctx->fd, ACRN_IOCTL_DESTROY_IOREQ_CLIENT, ctx->ioreq_client);
	if (error) {
		pr_err("ACRN_IOCTL_DESTROY_IOREQ_CLIENT ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_attach_ioreq_client(struct vmctx *ctx)
{
	int error;

	error = ioctl(ctx->fd, ACRN_IOCTL_ATTACH_IOREQ_CLIENT, ctx->ioreq_client);

	if (error) {
		pr_err("ACRN_IOCTL_ATTACH_IOREQ_CLIENT ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_notify_request_done(struct vmctx *ctx, int vcpu)
{
	int error;
	struct acrn_ioreq_notify notify;

	bzero(&notify, sizeof(notify));
	notify.vmid = ctx->vmid;
	notify.vcpu = vcpu;

	error = ioctl(ctx->fd, ACRN_IOCTL_NOTIFY_REQUEST_FINISH, &notify);

	if (error) {
		pr_err("ACRN_IOCTL_NOTIFY_REQUEST_FINISH ioctl() returned an error: %s\n", errormsg(errno));
	}

	return error;
}

void
vm_destroy(struct vmctx *ctx)
{
	if (!ctx)
		return;
	if (ioctl(ctx->fd, ACRN_IOCTL_DESTROY_VM, NULL)) {
		pr_err("ACRN_IOCTL_DESTROY_VM ioctl() returned an error: %s\n", errormsg(errno));
	}
	close(ctx->fd);
	free(ctx);
	devfd = -1;
}

int
vm_setup_asyncio(struct vmctx *ctx, uint64_t base)
{
	int error;

	error = ioctl(ctx->fd, ACRN_IOCTL_SETUP_ASYNCIO, base);

	if (error) {
		pr_err("ACRN_IOCTL_SETUP_ASYNCIO ioctl() returned an error: %s\n", errormsg(errno));
	}

	return error;
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
	struct acrn_vm_memmap memmap;
	int error;
	bzero(&memmap, sizeof(struct acrn_vm_memmap));
	memmap.type = ACRN_MEMMAP_RAM;
	memmap.vma_base = vma;
	memmap.len = len;
	memmap.user_vm_pa = gpa;
	memmap.attr = prot;
	error = ioctl(ctx->fd, ACRN_IOCTL_SET_MEMSEG, &memmap);
	if (error) {
		pr_err("ACRN_IOCTL_SET_MEMSEG ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
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
	ctx->fbmem = (16 * 1024 * 1024);

	return hugetlb_setup_memory(ctx);
}

void
vm_unsetup_memory(struct vmctx *ctx)
{
	/*
	 * For security reason, clean the VM's memory region
	 * to avoid secret information leaking in below case:
	 * After a User VM is destroyed, the memory will be reclaimed,
	 * then if the new User VM starts, that memory region may be
	 * allocated the new User VM, the previous User VM sensitive data
	 * may be leaked to the new User VM if the memory is not cleared.
	 *
	 * For rtvm, we can't clean VM's memory as RTVM may still
	 * run. But we need to return the memory to Service VM here.
	 * Otherwise, VM can't be restart again.
	 */

	if (!is_rtvm) {
		bzero((void *)ctx->baseaddr, ctx->lowmem);
		bzero((void *)(ctx->baseaddr + ctx->highmem_gpa_base), ctx->highmem);
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

	pr_dbg("%s context memory is not valid!\n", __func__);
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

	error = ioctl(ctx->fd, ACRN_IOCTL_START_VM, &ctx->vmid);
	if (error) {
		pr_err("ACRN_IOCTL_START_VM ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

void
vm_pause(struct vmctx *ctx)
{
	if (ioctl(ctx->fd, ACRN_IOCTL_PAUSE_VM, &ctx->vmid)) {
		pr_err("ACRN_IOCTL_PAUSE_VM ioctl() returned an error: %s\n", errormsg(errno));
	}
}

void
vm_reset(struct vmctx *ctx)
{
	if (ioctl(ctx->fd, ACRN_IOCTL_RESET_VM, &ctx->vmid)) {
		pr_err("ACRN_IOCTL_RESET_VM ioctl() returned an error: %s\n", errormsg(errno));
	}
}

void
vm_clear_ioreq(struct vmctx *ctx)
{
	if (ioctl(ctx->fd, ACRN_IOCTL_CLEAR_VM_IOREQ, NULL)) {
		pr_err("ACRN_IOCTL_CLEAR_VM_IOREQ ioctl() returned an error: %s\n", errormsg(errno));
	}
}

static enum vm_suspend_how suspend_mode = VM_SUSPEND_NONE;

void
vm_set_suspend_mode(enum vm_suspend_how how)
{
	pr_notice("VM state changed from[ %s ] to [ %s ]\n", vm_state_to_str(suspend_mode), vm_state_to_str(how));
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
	pr_info("%s: setting VM state to %s\n", __func__, vm_state_to_str(how));
	vm_set_suspend_mode(how);
	mevent_notify();

	return 0;
}

int
vm_lapic_msi(struct vmctx *ctx, uint64_t addr, uint64_t msg)
{
	struct acrn_msi_entry msi;
	int error;
	bzero(&msi, sizeof(msi));
	msi.msi_addr = addr;
	msi.msi_data = msg;

	error = ioctl(ctx->fd, ACRN_IOCTL_INJECT_MSI, &msi);
	if (error) {
		pr_err("ACRN_IOCTL_INJECT_MSI ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_set_gsi_irq(struct vmctx *ctx, int gsi, uint32_t operation)
{
	struct acrn_irqline_ops op;
	uint64_t *req =  (uint64_t *)&op;
	int error;
	op.op = operation;
	op.gsi = (uint32_t)gsi;

	error = ioctl(ctx->fd, ACRN_IOCTL_SET_IRQLINE, *req);
	if (error) {
		pr_err("ACRN_IOCTL_SET_IRQLINE ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_assign_pcidev(struct vmctx *ctx, struct acrn_pcidev *pcidev)
{
	int error;
	error = ioctl(ctx->fd, ACRN_IOCTL_ASSIGN_PCIDEV, pcidev);
	if (error) {
		pr_err("ACRN_IOCTL_ASSIGN_PCIDEV ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_deassign_pcidev(struct vmctx *ctx, struct acrn_pcidev *pcidev)
{
	int error;
	error = ioctl(ctx->fd, ACRN_IOCTL_DEASSIGN_PCIDEV, pcidev);
	if (error) {
		pr_err("ACRN_IOCTL_DEASSIGN_PCIDEV ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_assign_mmiodev(struct vmctx *ctx, struct acrn_mmiodev *mmiodev)
{
	int error;
	error = ioctl(ctx->fd, ACRN_IOCTL_ASSIGN_MMIODEV, mmiodev);
	if (error) {
		pr_err("ACRN_IOCTL_ASSIGN_MMIODEV ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_deassign_mmiodev(struct vmctx *ctx, struct acrn_mmiodev *mmiodev)
{
	int error;
	error = ioctl(ctx->fd, ACRN_IOCTL_DEASSIGN_MMIODEV, mmiodev);
	if (error) {
		pr_err("ACRN_IOCTL_DEASSIGN_MMIODEV ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_map_ptdev_mmio(struct vmctx *ctx, int bus, int slot, int func,
		   vm_paddr_t gpa, size_t len, vm_paddr_t hpa)
{
	struct acrn_vm_memmap memmap;
	int error;
	bzero(&memmap, sizeof(struct acrn_vm_memmap));
	memmap.type = ACRN_MEMMAP_MMIO;
	memmap.len = len;
	memmap.user_vm_pa = gpa;
	memmap.service_vm_pa = hpa;
	memmap.attr = ACRN_MEM_ACCESS_RWX;
	error = ioctl(ctx->fd, ACRN_IOCTL_SET_MEMSEG, &memmap);
	if (error) {
		pr_err("ACRN_IOCTL_SET_MEMSEG ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_unmap_ptdev_mmio(struct vmctx *ctx, int bus, int slot, int func,
		   vm_paddr_t gpa, size_t len, vm_paddr_t hpa)
{
	struct acrn_vm_memmap memmap;
	int error;
	bzero(&memmap, sizeof(struct acrn_vm_memmap));
	memmap.type = ACRN_MEMMAP_MMIO;
	memmap.len = len;
	memmap.user_vm_pa = gpa;
	memmap.service_vm_pa = hpa;
	memmap.attr = ACRN_MEM_ACCESS_RWX;

	error = ioctl(ctx->fd, ACRN_IOCTL_UNSET_MEMSEG, &memmap);
	if (error) {
		pr_err("ACRN_IOCTL_UNSET_MEMSEG ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_add_hv_vdev(struct vmctx *ctx, struct acrn_vdev *dev)
{
	int error;
	error = ioctl(ctx->fd, ACRN_IOCTL_CREATE_VDEV, dev);
	if (error) {
		pr_err("ACRN_IOCTL_CREATE_VDEV ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_remove_hv_vdev(struct vmctx *ctx, struct acrn_vdev *dev)
{
	int error;
	error = ioctl(ctx->fd, ACRN_IOCTL_DESTROY_VDEV, dev);
	if (error) {
		pr_err("ACRN_IOCTL_DESTROY_VDEV ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_set_ptdev_intx_info(struct vmctx *ctx, uint16_t virt_bdf, uint16_t phys_bdf,
		       int virt_pin, int phys_pin, bool pic_pin)
{
	struct acrn_ptdev_irq ptirq;
	int error;
	bzero(&ptirq, sizeof(ptirq));
	ptirq.type = ACRN_PTDEV_IRQ_INTX;
	ptirq.virt_bdf = virt_bdf;
	ptirq.phys_bdf = phys_bdf;
	ptirq.intx.virt_pin = virt_pin;
	ptirq.intx.phys_pin = phys_pin;
	ptirq.intx.is_pic_pin = pic_pin;

	error = ioctl(ctx->fd, ACRN_IOCTL_SET_PTDEV_INTR, &ptirq);
	if (error) {
		pr_err("ACRN_IOCTL_SET_PTDEV_INTR ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_reset_ptdev_intx_info(struct vmctx *ctx, uint16_t virt_bdf, uint16_t phys_bdf,
			int virt_pin, bool pic_pin)
{
	struct acrn_ptdev_irq ptirq;
	int error;
	bzero(&ptirq, sizeof(ptirq));
	ptirq.type = ACRN_PTDEV_IRQ_INTX;
	ptirq.intx.virt_pin = virt_pin;
	ptirq.intx.is_pic_pin = pic_pin;
	ptirq.virt_bdf = virt_bdf;
	ptirq.phys_bdf = phys_bdf;

	error = ioctl(ctx->fd, ACRN_IOCTL_RESET_PTDEV_INTR, &ptirq);
	if (error) {
		pr_err("ACRN_IOCTL_RESET_PTDEV_INTR ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_set_vcpu_regs(struct vmctx *ctx, struct acrn_vcpu_regs *vcpu_regs)
{
	int error;
	error = ioctl(ctx->fd, ACRN_IOCTL_SET_VCPU_REGS, vcpu_regs);
	if (error) {
		pr_err("ACRN_IOCTL_SET_VCPU_REGS ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_get_cpu_state(struct vmctx *ctx, void *state_buf)
{
	int error;
	error = ioctl(ctx->fd, ACRN_IOCTL_PM_GET_CPU_STATE, state_buf);
	if (error) {
		pr_err("ACRN_IOCTL_PM_GET_CPU_STATE ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_intr_monitor(struct vmctx *ctx, void *intr_buf)
{
	int error;
	error = ioctl(ctx->fd, ACRN_IOCTL_VM_INTR_MONITOR, intr_buf);
	if (error) {
		pr_err("ACRN_IOCTL_VM_INTR_MONITOR ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_ioeventfd(struct vmctx *ctx, struct acrn_ioeventfd *args)
{
	int error;
	error = ioctl(ctx->fd, ACRN_IOCTL_IOEVENTFD, args);
	if (error) {
		pr_err("ACRN_IOCTL_IOEVENTFD ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

int
vm_irqfd(struct vmctx *ctx, struct acrn_irqfd *args)
{
	int error;
	error = ioctl(ctx->fd, ACRN_IOCTL_IRQFD, args);
	if (error) {
		pr_err("ACRN_IOCTL_IRQFD ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
}

char*
errormsg(int error)
{
	switch (error){
	case ENOTTY:
		return "Undefined operation";
	case ENOSYS:
		return "Obsoleted operation";
	default:
		return strerror(error);
	}
}
