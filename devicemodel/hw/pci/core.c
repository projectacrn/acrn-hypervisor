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

#include <sys/user.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <stdbool.h>

#include "dm.h"
#include "vmmapi.h"
#include "acpi.h"
#include "inout.h"
#include "ioapic.h"
#include "mem.h"
#include "pci_core.h"
#include "irq.h"
#include "lpc.h"
#include "sw_load.h"
#include "log.h"

#define CONF1_ADDR_PORT    0x0cf8
#define CONF1_DATA_PORT    0x0cfc

#define CONF1_ENABLE	   0x80000000ul

#define	MAXBUSES	(PCI_BUSMAX + 1)
#define MAXSLOTS	(PCI_SLOTMAX + 1)
#define	MAXFUNCS	(PCI_FUNCMAX + 1)

struct funcinfo {
	char	*fi_name;
	char	*fi_param;
	char	*fi_param_saved; /* save for reboot */
	struct pci_vdev *fi_devi;
};

struct intxinfo {
	int	ii_count;
	int	ii_pirq_pin;
	int	ii_ioapic_irq;
};

struct slotinfo {
	struct intxinfo si_intpins[4];
	struct funcinfo si_funcs[MAXFUNCS];
};

struct businfo {
	uint16_t iobase, iolimit;		/* I/O window */
	uint32_t membase32, memlimit32;		/* mmio window below 4GB */
	uint64_t membase64, memlimit64;		/* mmio window above 4GB */
	struct slotinfo slotinfo[MAXSLOTS];
};

static struct businfo *pci_businfo[MAXBUSES];

SET_DECLARE(pci_vdev_ops_set, struct pci_vdev_ops);

static uint64_t pci_emul_iobase;
static uint64_t pci_emul_membase32;
static uint64_t pci_emul_membase64;

extern bool skip_pci_mem64bar_workaround;

struct mmio_rsvd_rgn reserved_bar_regions[REGION_NUMS];

#define	PCI_EMUL_IOBASE		0x2000
#define	PCI_EMUL_IOLIMIT	0x10000

#define	PCI_EMUL_ECFG_SIZE	(MAXBUSES * 1024 * 1024)    /* 1MB per bus */
SYSRES_MEM(PCI_EMUL_ECFG_BASE, PCI_EMUL_ECFG_SIZE);

static struct pci_vdev_ops *pci_emul_finddev(char *name);
static void pci_lintr_route(struct pci_vdev *dev);
static void pci_lintr_update(struct pci_vdev *dev);
static void pci_cfgrw(struct vmctx *ctx, int vcpu, int in, int bus, int slot,
		      int func, int coff, int bytes, uint32_t *val);
static void pci_emul_free_msixcap(struct pci_vdev *pdi);

int compare_mmio_rgns(const void *data1, const void *data2)
{
    struct mmio_rsvd_rgn *rng1, *rng2;

    rng1 = (struct mmio_rsvd_rgn*)data1;
    rng2 = (struct mmio_rsvd_rgn*)data2;

    if(!rng1->vdev)
            return 1;
    if(!rng2->vdev)
            return -1;
    return (rng1->start - rng2->start);
}

/* FIXME: the new registered region may overlap with exist mmio regions
 * whatever they are registered by dm or reserved.
 * Due to we only has gvt-g to use this feature,
 * this case rarely happen.
 */
int create_mmio_rsvd_rgn(uint64_t start,
	    uint64_t end, int idx, int bar_type, struct pci_vdev *vdev)
{
	int i;

	if(bar_type == PCIBAR_IO){
		pr_err("fail to create PCIBAR_IO bar_type\n");
		return -1;
	}

	for(i = 0; i < REGION_NUMS; i++){
		if(reserved_bar_regions[i].vdev == NULL){
			reserved_bar_regions[i].start = start;
			reserved_bar_regions[i].end = end;
			reserved_bar_regions[i].idx = idx;
			reserved_bar_regions[i].bar_type = bar_type;
			reserved_bar_regions[i].vdev = vdev;

			/* sort reserved_bar_regions array by "start" member,
			 * if this mmio_rsvd_rgn is not used, put it in the last.
			 */
			qsort((void*)reserved_bar_regions, REGION_NUMS,
					sizeof(reserved_bar_regions[0]),  compare_mmio_rgns);
			return 0;
		}
	}

	pr_err("reserved_bar_regions is overflow\n");
	return -1;
}

void destory_mmio_rsvd_rgns(struct pci_vdev *vdev){
    int i;

    for(i = 0; i < REGION_NUMS; i++)
		if(reserved_bar_regions[i].vdev == vdev)
			reserved_bar_regions[i].vdev = NULL;
}

static bool
is_mmio_rgns_overlap(uint64_t x1, uint64_t x2, uint64_t y1, uint64_t y2)
{
	if(x1 <= y2 && y1 <= x2)
		return true;
	return false;
}

/* reserved_bar_regions has sorted mmio_rsvd_rgns.
 * iterate all mmio_rsvd_rgn in reserved_bar_regions,
 * if [base, base + size - 1] with any mmio_rsvd_rgn,
 * adjust base addr to ensure [base, base + size - 1]
 * won't overlap with reserved mmio_rsvd_rgn
 */
static void
adjust_bar_region(uint64_t *base, uint64_t size, int bar_type)
{
	int i;

	for(i = 0; i < REGION_NUMS; i++){
		if(!reserved_bar_regions[i].vdev ||
			reserved_bar_regions[i].bar_type != bar_type)
			continue;
		if(is_mmio_rgns_overlap(reserved_bar_regions[i].start,
					reserved_bar_regions[i].end, *base, *base + size -1)){
			*base = roundup2(reserved_bar_regions[i].end + 1, size);
		}
	}
}

static inline void
CFGWRITE(struct pci_vdev *dev, int coff, uint32_t val, int bytes)
{
	if (bytes == 1)
		pci_set_cfgdata8(dev, coff, val);
	else if (bytes == 2)
		pci_set_cfgdata16(dev, coff, val);
	else
		pci_set_cfgdata32(dev, coff, val);
}

static inline uint32_t
CFGREAD(struct pci_vdev *dev, int coff, int bytes)
{
	if (bytes == 1)
		return pci_get_cfgdata8(dev, coff);
	else if (bytes == 2)
		return pci_get_cfgdata16(dev, coff);
	else
		return pci_get_cfgdata32(dev, coff);
}

static inline int
is_pt_pci(struct pci_vdev *dev)
{
	if (dev == NULL || strncmp(dev->dev_ops->class_name, "passthru",8))
		return 0;
	else
		return 1;
}

/*
 * I/O access
 */

/*
 * Slot options are in the form:
 *
 *  <bus>:<slot>:<func>,<emul>[,<config>]
 *  <slot>[:<func>],<emul>[,<config>]
 *
 *  slot is 0..31
 *  func is 0..7
 *  emul is a string describing the type of PCI device e.g. virtio-net
 *  config is an optional string, depending on the device, that can be
 *  used for configuration.
 *   Examples are:
 *     1,virtio-net,tap0
 *     3:0,dummy
 */
static void
pci_parse_slot_usage(char *aopt)
{
	pr_err("Invalid PCI slot info field \"%s\"\n", aopt);
}

int
parse_bdf(char *s, int *bus, int *dev, int *func, int base)
{
	char *s_bus, *s_dev, *s_func;
	char *str, *cp;
	int ret = 0;

	str = cp = strdup(s);
	bus ? *bus = 0 : 0;
	dev ? *dev = 0 : 0;
	func ? *func = 0 : 0;
	s_bus = s_dev = s_func = NULL;
	s_dev = strsep(&cp, ":/.");
	if (cp) {
		s_func = strsep(&cp, ":/.");
		if (cp) {
			s_bus = s_dev;
			s_dev = s_func;
			s_func = strsep(&cp, ":/.");
		}
	}

	if (s_dev && dev)
		ret |= dm_strtoi(s_dev, &s_dev, base, dev);
	if (s_func && func)
		ret |= dm_strtoi(s_func, &s_func, base, func);
	if (s_bus && bus)
		ret |= dm_strtoi(s_bus, &s_bus, base, bus);
	free(str);

	return ret;
}

int
pci_parse_slot(char *opt)
{
	struct businfo *bi;
	struct slotinfo *si;
	char *emul, *config, *str, *cp, *b = NULL;
	int error, bnum, snum, fnum;

	error = -1;
	str = strdup(opt);
	if (!str) {
		pr_err("%s: strdup returns NULL\n", __func__);
		return -1;
	}

	emul = config = NULL;
	cp = str;
	str = strsep(&cp, ",");
	if (cp) {
		emul = strsep(&cp, ",");
		/* for boot device */
		if (cp && *cp == 'b' && *(cp+1) == ',')
			b = strsep(&cp, ",");
		config = cp;
	} else {
		pci_parse_slot_usage(opt);
		goto done;
	}

	/* <bus>:<slot>:<func> */
	if (parse_bdf(str, &bnum, &snum, &fnum, 10) != 0)
		snum = -1;

	if (bnum < 0 || bnum >= MAXBUSES || snum < 0 || snum >= MAXSLOTS ||
	    fnum < 0 || fnum >= MAXFUNCS) {
		pci_parse_slot_usage(opt);
		goto done;
	}

	if (pci_businfo[bnum] == NULL)
		pci_businfo[bnum] = calloc(1, sizeof(struct businfo));

	bi = pci_businfo[bnum];
	si = &bi->slotinfo[snum];

	if (si->si_funcs[fnum].fi_name != NULL) {
		pr_err("pci slot %d:%d already occupied!\n",
			snum, fnum);
		goto done;
	}

	if (pci_emul_finddev(emul) == NULL) {
		pr_err("pci slot %d:%d: unknown device \"%s\"\n",
			snum, fnum, emul);
		goto done;
	}

	error = 0;
	si->si_funcs[fnum].fi_name = emul;
	/* saved fi param in case reboot */
	si->si_funcs[fnum].fi_param_saved = config;

	if (b != NULL) {
		if ((strcmp("virtio-blk", emul) == 0) &&  (b != NULL) &&
			(strchr(b, 'b') != NULL)) {
			vsbl_set_bdf(bnum, snum, fnum);
		}
	}
done:
	if (error)
		free(str);

	return error;
}

static int
pci_valid_pba_offset(struct pci_vdev *dev, uint64_t offset)
{
	if (offset < dev->msix.pba_offset)
		return 0;

	if (offset >= dev->msix.pba_offset + dev->msix.pba_size)
		return 0;

	return 1;
}

int
pci_emul_msix_twrite(struct pci_vdev *dev, uint64_t offset, int size,
		     uint64_t value)
{
	int msix_entry_offset;
	int tab_index;
	char *dest;

	/* support only 4 or 8 byte writes */
	if (size != 4 && size != 8)
		return -1;

	/*
	 * Return if table index is beyond what device supports
	 */
	tab_index = offset / MSIX_TABLE_ENTRY_SIZE;
	if (tab_index >= dev->msix.table_count)
		return -1;

	msix_entry_offset = offset % MSIX_TABLE_ENTRY_SIZE;

	/* support only aligned writes */
	if ((msix_entry_offset % size) != 0)
		return -1;

	dest = (char *)(dev->msix.table + tab_index);
	dest += msix_entry_offset;

	if (size == 4)
		*((uint32_t *)dest) = value;
	else
		*((uint64_t *)dest) = value;

	return 0;
}

uint64_t
pci_emul_msix_tread(struct pci_vdev *dev, uint64_t offset, int size)
{
	char *dest;
	int msix_entry_offset;
	int tab_index;
	uint64_t retval = ~0;

	/*
	 * The PCI standard only allows 4 and 8 byte accesses to the MSI-X
	 * table but we also allow 1 byte access to accommodate reads from
	 * ddb.
	 */
	if (size != 1 && size != 4 && size != 8)
		return retval;

	msix_entry_offset = offset % MSIX_TABLE_ENTRY_SIZE;

	/* support only aligned reads */
	if ((msix_entry_offset % size) != 0)
		return retval;

	tab_index = offset / MSIX_TABLE_ENTRY_SIZE;

	if (tab_index < dev->msix.table_count) {
		/* valid MSI-X Table access */
		dest = (char *)(dev->msix.table + tab_index);
		dest += msix_entry_offset;

		if (size == 1)
			retval = *((uint8_t *)dest);
		else if (size == 4)
			retval = *((uint32_t *)dest);
		else
			retval = *((uint64_t *)dest);
	} else if (pci_valid_pba_offset(dev, offset)) {
		/* return 0 for PBA access */
		retval = 0;
	}

	return retval;
}

int
pci_msix_table_bar(struct pci_vdev *dev)
{
	if (dev->msix.table != NULL)
		return dev->msix.table_bar;
	else
		return -1;
}

int
pci_msix_pba_bar(struct pci_vdev *dev)
{
	if (dev->msix.table != NULL)
		return dev->msix.pba_bar;
	else
		return -1;
}

static inline uint64_t
bar_value(int size, uint64_t val)
{
	uint64_t mask;

	assert(size == 1 || size == 2 || size == 4 || size == 8);
	mask = (size < 8 ? 1UL << (size * 8) : 0UL) - 1;

	return val & mask;
}

static int
pci_emul_io_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		    uint32_t *eax, void *arg)
{
	struct pci_vdev *pdi = arg;
	struct pci_vdev_ops *ops = pdi->dev_ops;
	uint64_t offset;
	int i;

	for (i = 0; i <= PCI_BARMAX; i++) {
		if (pdi->bar[i].type == PCIBAR_IO &&
		    port >= pdi->bar[i].addr &&
		    port + bytes <= pdi->bar[i].addr + pdi->bar[i].size) {
			offset = port - pdi->bar[i].addr;
			if (in) {
				*eax = (*ops->vdev_barread)(ctx, vcpu, pdi, i,
				                            offset, bytes);
				*eax = bar_value(bytes, *eax);
			} else
				(*ops->vdev_barwrite)(ctx, vcpu, pdi, i, offset,
				                      bytes, bar_value(bytes, *eax));
			return 0;
		}
	}
	return -1;
}

static int
pci_emul_mem_handler(struct vmctx *ctx, int vcpu, int dir, uint64_t addr,
		     int size, uint64_t *val, void *arg1, long arg2)
{
	struct pci_vdev *pdi = arg1;
	struct pci_vdev_ops *ops = pdi->dev_ops;
	uint64_t offset;
	int bidx = (int) arg2;

	if (addr + size > pdi->bar[bidx].addr + pdi->bar[bidx].size) {
		pr_err("%s, Out of emulated memory range\n", __func__);
		return -ESRCH;
	}

	offset = addr - pdi->bar[bidx].addr;

	if (dir == MEM_F_WRITE) {
		if (size == 8) {
			(*ops->vdev_barwrite)(ctx, vcpu, pdi, bidx, offset,
					   4, *val & 0xffffffff);
			(*ops->vdev_barwrite)(ctx, vcpu, pdi, bidx, offset + 4,
					   4, *val >> 32);
		} else {
			(*ops->vdev_barwrite)(ctx, vcpu, pdi, bidx, offset,
					   size, bar_value(size, *val));
		}
	} else {
		if (size == 8) {
			uint64_t val_lo, val_hi;

			val_lo = (*ops->vdev_barread)(ctx, vcpu, pdi, bidx,
			                              offset, 4);
			val_lo = bar_value(4, val_lo);

			val_hi = (*ops->vdev_barread)(ctx, vcpu, pdi, bidx,
			                              offset + 4, 4);

			*val = val_lo | (val_hi << 32);
		} else {
			*val = (*ops->vdev_barread)(ctx, vcpu, pdi, bidx,
			                            offset, size);
			*val = bar_value(size, *val);
		}
	}

	return 0;
}

static int
pci_emul_alloc_resource(uint64_t *baseptr, uint64_t limit, uint64_t size,
                    uint64_t *addr, int bar_type)
{
	uint64_t base;

	if ((size & (size - 1)) != 0) {	/* must be a power of 2 */
		pr_err("%s: Cannot alloc invalid size %lld resource\n", __func__, size);
		return -1;
	}

	/* PCI spec said that BAR base should be naturally aligned. On ACRN
	 * if the bar size < PAGE_SIZE, BAR base should be aligned with
	 * PAGE_SIZE. This is because the minimal size that EPT can map/unmap
	 * is PAGE_SIZE.
	 */
	if (size < PAGE_SIZE)
		size = PAGE_SIZE;
	base = roundup2(*baseptr, size);

	/* TODO:Currently, we only reserve gvt mmio regions,
	 * so ignore PCIBAR_IO when adjust_bar_region.
	 * If other devices also use reserved bar regions later,
	 * need remove pcibar_type != PCIBAR_IO condition
	 */
	if(bar_type != PCIBAR_IO)
		adjust_bar_region(&base, size, bar_type);

	if (base + size <= limit) {
		*addr = base;
		*baseptr = base + size;
		return 0;
	} else
		return -1;
}

int
pci_emul_alloc_bar(struct pci_vdev *pdi, int idx, enum pcibar_type type,
		   uint64_t size)
{
	return pci_emul_alloc_pbar(pdi, idx, 0, type, size);
}

/*
 * Register (or unregister) the MMIO or I/O region associated with the BAR
 * register 'idx' of an emulated pci device.
 */
static int
modify_bar_registration(struct pci_vdev *dev, int idx, int registration)
{
	int error;
	struct inout_port iop;
	struct mem_range mr;

	if (is_pt_pci(dev)) {
		pr_dbg("%s: bypass for pci-passthru %x:%x.%x\n", __func__, dev->bus, dev->slot, dev->func);
		return 0;
	}

	switch (dev->bar[idx].type) {
	case PCIBAR_IO:
		bzero(&iop, sizeof(struct inout_port));
		iop.name = dev->name;
		iop.port = dev->bar[idx].addr;
		iop.size = dev->bar[idx].size;
		if (registration) {
			iop.flags = IOPORT_F_INOUT;
			iop.handler = pci_emul_io_handler;
			iop.arg = dev;
			error = register_inout(&iop);
		} else
			error = unregister_inout(&iop);
		break;
	case PCIBAR_MEM32:
	case PCIBAR_MEM64:
		bzero(&mr, sizeof(struct mem_range));
		mr.name = dev->name;
		mr.base = dev->bar[idx].addr;
		mr.size = dev->bar[idx].size;
		if (registration) {
			mr.flags = MEM_F_RW;
			mr.handler = pci_emul_mem_handler;
			mr.arg1 = dev;
			mr.arg2 = idx;
			error = register_mem(&mr);
		} else
			error = unregister_mem(&mr);
		break;
	default:
		error = EINVAL;
		break;
	}

	return error;
}

static void
unregister_bar(struct pci_vdev *dev, int idx)
{
	modify_bar_registration(dev, idx, 0);
}

static int
register_bar(struct pci_vdev *dev, int idx)
{
	return modify_bar_registration(dev, idx, 1);
}

/* Are we decoding i/o port accesses for the emulated pci device? */
static bool
porten(struct pci_vdev *dev)
{
	uint16_t cmd;

	cmd = pci_get_cfgdata16(dev, PCIR_COMMAND);

	return (cmd & PCIM_CMD_PORTEN) != 0;
}

/* Are we decoding memory accesses for the emulated pci device? */
static bool
memen(struct pci_vdev *dev)
{
	uint16_t cmd;

	cmd = pci_get_cfgdata16(dev, PCIR_COMMAND);

	return (cmd & PCIM_CMD_MEMEN) != 0;
}

/*
 * Update the MMIO or I/O address that is decoded by the BAR register.
 *
 * If the pci device has enabled the address space decoding then intercept
 * the address range decoded by the BAR register.
 */
static void
update_bar_address(struct vmctx *ctx, struct pci_vdev *dev, uint64_t addr,
	int idx, int type, bool ignore_reg_unreg)
{
	bool decode = false;

	if (!ignore_reg_unreg) {
		if (dev->bar[idx].type == PCIBAR_IO)
			decode = porten(dev);
		else
			decode = memen(dev);
	}

	if (decode)
		unregister_bar(dev, idx);

	/* TODO:Currently, we only reserve gvt mmio regions,
	 * so ignore PCIBAR_IO when adjust_bar_region_with_reserved_bars.
	 * If other devices also use reserved bar regions later,
	 * need remove pcibar_type != PCIBAR_IO condition
	 */
	if(type != PCIBAR_IO && ctx->gvt_enabled)
		/* uos kernel may update gvt bars' value,
		 * but ACRN-DM doesn't know this update.
		 * When other pci devices write bar address,
		 * ACRN-DM need update vgpu bars' info.
		 */
		ctx->update_gvt_bar(ctx);

	switch (type) {
	case PCIBAR_IO:
	case PCIBAR_MEM32:
		dev->bar[idx].addr = addr;
		break;
	case PCIBAR_MEM64:
		dev->bar[idx].addr &= ~0xffffffffUL;
		dev->bar[idx].addr |= addr;
		break;
	case PCIBAR_MEMHI64:
		dev->bar[idx].addr &= 0xffffffff;
		dev->bar[idx].addr |= addr;
		break;
	default:
		pr_err("%s: invalid bar type %d\n", __func__, type);
		return;
	}

	if (decode)
		register_bar(dev, idx);
}

static struct mmio_rsvd_rgn *
get_mmio_rsvd_rgn_by_vdev_idx(struct pci_vdev *pdi, int idx)
{
	int i;

	for(i = 0; i < REGION_NUMS; i++){
		if(reserved_bar_regions[i].vdev &&
			reserved_bar_regions[i].idx == idx &&
			reserved_bar_regions[i].vdev == pdi)
			return &reserved_bar_regions[i];
	}

	return NULL;
}

int
pci_emul_alloc_pbar(struct pci_vdev *pdi, int idx, uint64_t hostbase,
		    enum pcibar_type type, uint64_t size)
{
	int error;
	uint64_t *baseptr, limit, addr, mask, lobits, bar;
	struct mmio_rsvd_rgn *region;

	if ((size & (size - 1)) != 0)
		size = 1UL << flsl(size);	/* round up to a power of 2 */

	/* Enforce minimum BAR sizes required by the PCI standard */
	if (type == PCIBAR_IO) {
		if (size < 4)
			size = 4;
	} else {
		if (size < 16)
			size = 16;
	}

	switch (type) {
	case PCIBAR_NONE:
		baseptr = NULL;
		addr = mask = lobits = 0;
		break;
	case PCIBAR_IO:
		baseptr = &pci_emul_iobase;
		limit = PCI_EMUL_IOLIMIT;
		mask = PCIM_BAR_IO_BASE;
		lobits = PCIM_BAR_IO_SPACE;
		break;
	case PCIBAR_MEM64:
		if (idx + 1 > PCI_BARMAX) {
			pr_err("%s: invalid bar number %d for MEM64 type\n", __func__, idx);
			return -1;
		}
		/*
		 * FIXME
		 * Some drivers do not work well if the 64-bit BAR is allocated
		 * above 4GB. Allow for this by allocating small requests under
		 * 4GB unless then allocation size is larger than some arbitrary
		 * number (32MB currently). If guest booted by ovmf, then skip the
		 * workaround.
		 */
		if (!skip_pci_mem64bar_workaround && (size <= 32 * 1024 * 1024)) {
			baseptr = &pci_emul_membase32;
			limit = PCI_EMUL_MEMLIMIT32;
			mask = PCIM_BAR_MEM_BASE;
			lobits = PCIM_BAR_MEM_SPACE | PCIM_BAR_MEM_64;
			break;
		}

		/*
		 * XXX special case for device requiring peer-peer DMA
		 */
		if (size == 0x100000000UL)
			baseptr = &hostbase;
		else
			baseptr = &pci_emul_membase64;
		limit = PCI_EMUL_MEMLIMIT64;
		mask = PCIM_BAR_MEM_BASE;
		lobits = PCIM_BAR_MEM_SPACE | PCIM_BAR_MEM_64 |
			PCIM_BAR_MEM_PREFETCH;
		break;
	case PCIBAR_MEM32:
		baseptr = &pci_emul_membase32;
		limit = PCI_EMUL_MEMLIMIT32;
		mask = PCIM_BAR_MEM_BASE;
		lobits = PCIM_BAR_MEM_SPACE | PCIM_BAR_MEM_32;
		break;
	default:
		pr_err("%s: invalid bar type %d\n", __func__, type);
		return -1;
	}

	region = get_mmio_rsvd_rgn_by_vdev_idx(pdi, idx);
	if(region)
		addr = region->start;

	if (baseptr != NULL && !region) {
		error = pci_emul_alloc_resource(baseptr, limit, size, &addr, type);
		if (error != 0)
			return error;
	}

	pdi->bar[idx].type = type;
	pdi->bar[idx].addr = addr;
	pdi->bar[idx].size = size;

	/* Initialize the BAR register in config space */
	bar = (addr & mask) | lobits;
	pci_set_cfgdata32(pdi, PCIR_BAR(idx), bar);

	if (type == PCIBAR_MEM64) {
		pdi->bar[idx + 1].type = PCIBAR_MEMHI64;
		pci_set_cfgdata32(pdi, PCIR_BAR(idx + 1), bar >> 32);
	}

	error = register_bar(pdi, idx);

	if(error != 0){
		/* FIXME: Currently, only gvt needs reserve regions.
		 * because gvt isn't firstly initialized, previous pci
		 * devices' bars may conflict with gvt bars.
		 * Use register_bar to detect this case,
		 * but this case rarely happen.
		 * If this case always happens, we need to
		 * change core.c code to ensure gvt firstly initialzed
		 */
		printf("%s failed to register_bar\n", pdi->name);
		return error;
	}

	return 0;
}

void
pci_emul_free_bar(struct pci_vdev *pdi, int idx)
{
	bool enabled;

	if ((pdi->bar[idx].type != PCIBAR_NONE) &&
		(pdi->bar[idx].type != PCIBAR_MEMHI64)){
		/*
		 * Check whether the bar is enabled or not,
		 * if it is disabled then it should have been
		 * unregistered in pci_emul_cmdsts_write.
		 */
		if (pdi->bar[idx].type == PCIBAR_IO)
			enabled = porten(pdi);
		else
			enabled = memen(pdi);

		if (enabled)
			unregister_bar(pdi, idx);
		pdi->bar[idx].type = PCIBAR_NONE;
	}
}

void
pci_emul_free_bars(struct pci_vdev *pdi)
{
	int i;

	for (i = 0; i < PCI_BARMAX; i++)
		pci_emul_free_bar(pdi, i);
}

#define	CAP_START_OFFSET	0x40
int
pci_emul_add_capability(struct pci_vdev *dev, u_char *capdata, int caplen)
{
	int i, capoff, reallen;
	uint16_t sts;

	reallen = roundup2(caplen, 4);		/* dword aligned */

	sts = pci_get_cfgdata16(dev, PCIR_STATUS);
	if ((sts & PCIM_STATUS_CAPPRESENT) == 0)
		capoff = CAP_START_OFFSET;
	else
		capoff = dev->capend + 1;

	/* Check if we have enough space */
	if (capoff + reallen > PCI_REGMAX + 1)
		return -1;

	/* Set the previous capability pointer */
	if ((sts & PCIM_STATUS_CAPPRESENT) == 0) {
		pci_set_cfgdata8(dev, PCIR_CAP_PTR, capoff);
		pci_set_cfgdata16(dev, PCIR_STATUS, sts|PCIM_STATUS_CAPPRESENT);
	} else
		pci_set_cfgdata8(dev, dev->prevcap + 1, capoff);

	/* Copy the capability */
	for (i = 0; i < caplen; i++)
		pci_set_cfgdata8(dev, capoff + i, capdata[i]);

	/* Set the next capability pointer */
	pci_set_cfgdata8(dev, capoff + 1, 0);

	dev->prevcap = capoff;
	dev->capend = capoff + reallen - 1;
	return 0;
}

/*
 * p_capoff is used as both input and output. Set *p_capoff to 0 when this
 * function is called for the first time, it will return offset of the first
 * matched one in p_capoff. To find the next matched one, please use the
 * returned *p_capoff from last call as the input, in this case the offset of
 * the next matched one will be returned in *p_capoff.
 * Please check the returned value first before touch p_capoff.
 */
int
pci_emul_find_capability(struct pci_vdev *dev, uint8_t capid, int *p_capoff)
{
	int coff;
	uint16_t sts;

	sts = pci_get_cfgdata16(dev, PCIR_STATUS);
	if ((sts & PCIM_STATUS_CAPPRESENT) == 0)
		return -1;

	if (!p_capoff)
		return -1;

	if (*p_capoff == 0)
		coff = pci_get_cfgdata8(dev, PCIR_CAP_PTR);
	else if (*p_capoff >= CAP_START_OFFSET && *p_capoff <= dev->prevcap)
		coff = pci_get_cfgdata8(dev, *p_capoff + 1);
	else
		return -1;

	while (coff >= CAP_START_OFFSET && coff <= dev->prevcap) {
		if (pci_get_cfgdata8(dev, coff) == capid) {
			*p_capoff = coff;
			return 0;
		}
		coff = pci_get_cfgdata8(dev, coff + 1);
	}

	return -1;
}

static struct pci_vdev_ops *
pci_emul_finddev(char *name)
{
	struct pci_vdev_ops **pdpp, *pdp;

	SET_FOREACH(pdpp, pci_vdev_ops_set) {
		pdp = *pdpp;
		if (!strcmp(pdp->class_name, name))
			return pdp;
	}

	return NULL;
}

static int
pci_emul_init(struct vmctx *ctx, struct pci_vdev_ops *ops, int bus, int slot,
	      int func, struct funcinfo *fi)
{
	struct pci_vdev *pdi;
	int err;

	pdi = calloc(1, sizeof(struct pci_vdev));
	if (!pdi) {
		pr_err("%s: calloc returns NULL\n", __func__);
		return -1;
	}

	pdi->vmctx = ctx;
	pdi->bus = bus;
	pdi->slot = slot;
	pdi->func = func;
	pthread_mutex_init(&pdi->lintr.lock, NULL);
	pdi->lintr.pin = 0;
	pdi->lintr.state = IDLE;
	pdi->lintr.pirq_pin = 0;
	pdi->lintr.ioapic_irq = 0;
	pdi->dev_ops = ops;
	snprintf(pdi->name, PI_NAMESZ, "%s-pci-%d", ops->class_name, slot);

	/* Disable legacy interrupts */
	pci_set_cfgdata8(pdi, PCIR_INTLINE, 255);
	pci_set_cfgdata8(pdi, PCIR_INTPIN, 0);

	pci_set_cfgdata8(pdi, PCIR_COMMAND,
		    PCIM_CMD_PORTEN | PCIM_CMD_MEMEN | PCIM_CMD_BUSHOSTEN);

	if (fi->fi_param_saved)
		fi->fi_param = strdup(fi->fi_param_saved);
	else
		fi->fi_param = NULL;
	err = (*ops->vdev_init)(ctx, pdi, fi->fi_param);
	if (err == 0)
		fi->fi_devi = pdi;
	else
		free(pdi);

	return err;
}

static void
pci_emul_deinit(struct vmctx *ctx, struct pci_vdev_ops *ops, int bus, int slot,
		int func, struct funcinfo *fi)
{
	if (ops->vdev_deinit && fi->fi_devi)
		(*ops->vdev_deinit)(ctx, fi->fi_devi, fi->fi_param);
	if (fi->fi_param)
		free(fi->fi_param);

	if (fi->fi_devi) {
		pci_lintr_release(fi->fi_devi);
		pci_emul_free_bars(fi->fi_devi);
		pci_emul_free_msixcap(fi->fi_devi);
		free(fi->fi_devi);
	}
}

int
pci_populate_msicap(struct msicap *msicap, int msgnum, int nextptr)
{
	int mmc;

	/* Number of msi messages must be a power of 2 between 1 and 32 */
	if (((msgnum & (msgnum - 1)) != 0) || msgnum < 1 || msgnum > 32) {
		pr_err("%s: invalid number of msi messages!\n", __func__);
		return -1;
	}
	mmc = ffs(msgnum) - 1;

	bzero(msicap, sizeof(struct msicap));
	msicap->capid = PCIY_MSI;
	msicap->nextptr = nextptr;
	msicap->msgctrl = PCIM_MSICTRL_64BIT | (mmc << 1);

	return 0;
}

int
pci_emul_add_msicap(struct pci_vdev *dev, int msgnum)
{
	struct msicap msicap;

	return pci_populate_msicap(&msicap, msgnum, 0) ||
		pci_emul_add_capability(dev, (u_char *)&msicap, sizeof(msicap));
}

static void
pci_populate_msixcap(struct msixcap *msixcap, int msgnum, int barnum,
		     uint32_t msix_tab_size)
{

	bzero(msixcap, sizeof(struct msixcap));
	msixcap->capid = PCIY_MSIX;

	/*
	 * Message Control Register, all fields set to
	 * zero except for the Table Size.
	 * Note: Table size N is encoded as N-1
	 */
	msixcap->msgctrl = msgnum - 1;

	/*
	 * MSI-X BAR setup:
	 * - MSI-X table start at offset 0
	 * - PBA table starts at a 4K aligned offset after the MSI-X table
	 */
	msixcap->table_info = barnum & PCIM_MSIX_BIR_MASK;
	msixcap->pba_info = msix_tab_size | (barnum & PCIM_MSIX_BIR_MASK);
}

static int
pci_msix_table_init(struct pci_vdev *dev, int table_entries)
{
	int i, table_size;

	table_size = table_entries * MSIX_TABLE_ENTRY_SIZE;
	dev->msix.table = calloc(1, table_size);
	if (!dev->msix.table) {
		pr_err("%s: Cannot alloc memory!\n", __func__);
		return -1;
	}

	/* set mask bit of vector control register */
	for (i = 0; i < table_entries; i++)
		dev->msix.table[i].vector_control |= PCIM_MSIX_VCTRL_MASK;

	return 0;
}

int
pci_emul_add_msixcap(struct pci_vdev *dev, int msgnum, int barnum)
{
	uint32_t tab_size;
	struct msixcap msixcap;

	if (msgnum > MAX_MSIX_TABLE_ENTRIES) {
		pr_err("%s: Too many entries!\n", __func__);
		return -1;
	}

	tab_size = msgnum * MSIX_TABLE_ENTRY_SIZE;

	/* Align table size to nearest 4K */
	tab_size = roundup2(tab_size, 4096);

	dev->msix.table_bar = barnum;
	dev->msix.pba_bar   = barnum;
	dev->msix.table_offset = 0;
	dev->msix.table_count = msgnum;
	dev->msix.pba_offset = tab_size;
	dev->msix.pba_size = PBA_SIZE(msgnum);

	if (pci_msix_table_init(dev, msgnum) != 0)
		return -1;

	pci_populate_msixcap(&msixcap, msgnum, barnum, tab_size);

	/* allocate memory for MSI-X Table and PBA */
	pci_emul_alloc_bar(dev, barnum, PCIBAR_MEM32,
				tab_size + dev->msix.pba_size);

	return (pci_emul_add_capability(dev, (u_char *)&msixcap,
					sizeof(msixcap)));
}

static void
pci_emul_free_msixcap(struct pci_vdev *pdi)
{
	if (pdi->msix.table) {
		free(pdi->msix.table);
		pdi->msix.table = NULL;
	}
}


void
msixcap_cfgwrite(struct pci_vdev *dev, int capoff, int offset,
		 int bytes, uint32_t val)
{
	uint16_t msgctrl, rwmask;
	int off;

	off = offset - capoff;
	/* Message Control Register */
	if (off == 2 && bytes == 2) {
		rwmask = PCIM_MSIXCTRL_MSIX_ENABLE |
			PCIM_MSIXCTRL_FUNCTION_MASK;
		msgctrl = pci_get_cfgdata16(dev, offset);
		msgctrl &= ~rwmask;
		msgctrl |= val & rwmask;
		val = msgctrl;

		dev->msix.enabled = val & PCIM_MSIXCTRL_MSIX_ENABLE;
		dev->msix.function_mask = val & PCIM_MSIXCTRL_FUNCTION_MASK;
		pci_lintr_update(dev);
	}

	CFGWRITE(dev, offset, val, bytes);
}

void
msicap_cfgwrite(struct pci_vdev *dev, int capoff, int offset,
		int bytes, uint32_t val)
{
	uint16_t msgctrl, rwmask, msgdata, mme;
	uint32_t addrlo;

	/*
	 * If guest is writing to the message control register make sure
	 * we do not overwrite read-only fields.
	 */
	if ((offset - capoff) == 2 && bytes == 2) {
		rwmask = PCIM_MSICTRL_MME_MASK | PCIM_MSICTRL_MSI_ENABLE;
		msgctrl = pci_get_cfgdata16(dev, offset);
		msgctrl &= ~rwmask;
		msgctrl |= val & rwmask;
		val = msgctrl;

		addrlo = pci_get_cfgdata32(dev, capoff + 4);
		if (msgctrl & PCIM_MSICTRL_64BIT)
			msgdata = pci_get_cfgdata16(dev, capoff + 12);
		else
			msgdata = pci_get_cfgdata16(dev, capoff + 8);

		mme = msgctrl & PCIM_MSICTRL_MME_MASK;
		dev->msi.enabled = msgctrl & PCIM_MSICTRL_MSI_ENABLE ? 1 : 0;
		if (dev->msi.enabled) {
			dev->msi.addr = addrlo;
			dev->msi.msg_data = msgdata;
			dev->msi.maxmsgnum = 1 << (mme >> 4);
		} else {
			dev->msi.maxmsgnum = 0;
		}
		pci_lintr_update(dev);
	}

	CFGWRITE(dev, offset, val, bytes);
}

void
pciecap_cfgwrite(struct pci_vdev *dev, int capoff, int offset,
		 int bytes, uint32_t val)
{
	/* XXX don't write to the readonly parts */
	CFGWRITE(dev, offset, val, bytes);
}

#define	PCIECAP_VERSION	0x2
int
pci_emul_add_pciecap(struct pci_vdev *dev, int type)
{
	int err;
	struct pciecap pciecap;

	if (type != PCIEM_TYPE_ROOT_PORT)
		return -1;

	bzero(&pciecap, sizeof(pciecap));

	pciecap.capid = PCIY_EXPRESS;
	pciecap.pcie_capabilities = PCIECAP_VERSION | PCIEM_TYPE_ROOT_PORT;
	pciecap.link_capabilities = 0x411;	/* gen1, x1 */
	pciecap.link_status = 0x11;		/* gen1, x1 */

	err = pci_emul_add_capability(dev, (u_char *)&pciecap, sizeof(pciecap));
	return err;
}

/*
 * This function assumes that 'coff' is in the capabilities region of the
 * config space.
 */
static void
pci_emul_capwrite(struct pci_vdev *dev, int offset, int bytes, uint32_t val)
{
	int capid;
	uint8_t capoff, nextoff;

	/* Do not allow un-aligned writes */
	if ((offset & (bytes - 1)) != 0)
		return;

	/* Find the capability that we want to update */
	capoff = CAP_START_OFFSET;
	while (1) {
		nextoff = pci_get_cfgdata8(dev, capoff + 1);
		if (nextoff == 0)
			break;
		if (offset >= capoff && offset < nextoff)
			break;

		capoff = nextoff;
	}

	/*
	 * Capability ID and Next Capability Pointer are readonly.
	 * However, some o/s's do 4-byte writes that include these.
	 * For this case, trim the write back to 2 bytes and adjust
	 * the data.
	 */
	if (offset == capoff || offset == capoff + 1) {
		if (offset == capoff && bytes == 4) {
			bytes = 2;
			offset += 2;
			val >>= 16;
		} else
			return;
	}

	capid = pci_get_cfgdata8(dev, capoff);
	switch (capid) {
	case PCIY_MSI:
		msicap_cfgwrite(dev, capoff, offset, bytes, val);
		break;
	case PCIY_MSIX:
		msixcap_cfgwrite(dev, capoff, offset, bytes, val);
		break;
	case PCIY_EXPRESS:
		pciecap_cfgwrite(dev, capoff, offset, bytes, val);
		break;
	default:
		CFGWRITE(dev, offset, val, bytes);
		break;
	}
}

static int
pci_emul_iscap(struct pci_vdev *dev, int offset)
{
	uint16_t sts;

	sts = pci_get_cfgdata16(dev, PCIR_STATUS);
	if ((sts & PCIM_STATUS_CAPPRESENT) != 0) {
		if (offset >= CAP_START_OFFSET && offset <= dev->capend)
			return 1;
	}
	return 0;
}

static int
pci_emul_fallback_handler(struct vmctx *ctx, int vcpu, int dir, uint64_t addr,
			  int size, uint64_t *val, void *arg1, long arg2)
{
	/*
	 * Ignore writes; return 0xff's for reads. The mem read code
	 * will take care of truncating to the correct size.
	 */
	if (dir == MEM_F_READ)
		*val = 0xffffffffffffffff;

	return 0;
}

static int
pci_emul_ecfg_handler(struct vmctx *ctx, int vcpu, int dir, uint64_t addr,
		      int bytes, uint64_t *val, void *arg1, long arg2)
{
	int bus, slot, func, coff, in;

	coff = addr & 0xfff;
	func = (addr >> 12) & 0x7;
	slot = (addr >> 15) & 0x1f;
	bus = (addr >> 20) & 0xff;
	in = (dir == MEM_F_READ);
	if (in)
		*val = ~0UL;
	pci_cfgrw(ctx, vcpu, in, bus, slot, func, coff, bytes, (uint32_t *)val);
	return 0;
}

#define	BUSIO_ROUNDUP		32
#define	BUSMEM_ROUNDUP		(1024 * 1024)

int
init_pci(struct vmctx *ctx)
{
	struct mem_range mr;
	struct pci_vdev_ops *ops;
	struct businfo *bi;
	struct slotinfo *si;
	struct funcinfo *fi;
	int bus, slot, func, i;
	int success_cnt = 0;
	int error;
	uint64_t bus0_memlimit;

	pci_emul_iobase = PCI_EMUL_IOBASE;
	pci_emul_membase32 = vm_get_lowmem_limit(ctx);
	pci_emul_membase64 = PCI_EMUL_MEMBASE64;

	create_gsi_sharing_groups();

	for (bus = 0; bus < MAXBUSES; bus++) {
		bi = pci_businfo[bus];
		if (bi == NULL)
			continue;
		/*
		 * Keep track of the i/o and memory resources allocated to
		 * this bus.
		 */
		bi->iobase = pci_emul_iobase;
		bi->membase32 = pci_emul_membase32;
		bi->membase64 = pci_emul_membase64;

		for (slot = 0; slot < MAXSLOTS; slot++) {
			si = &bi->slotinfo[slot];
			for (func = 0; func < MAXFUNCS; func++) {
				fi = &si->si_funcs[func];
				if (fi->fi_name == NULL)
					continue;
				ops = pci_emul_finddev(fi->fi_name);
				if (!ops) {
					pr_warn("No driver for device [%s]\n", fi->fi_name);
					continue;
				}
				pr_notice("pci init %s\r\n", fi->fi_name);
				error = pci_emul_init(ctx, ops, bus, slot,
				    func, fi);
				if (error) {
					pr_err("pci %s init failed\n", fi->fi_name);
					goto pci_emul_init_fail;
				}
				success_cnt++;
			}
		}

		/*
		 * Add some slop to the I/O and memory resources decoded by
		 * this bus to give a guest some flexibility if it wants to
		 * reprogram the BARs.
		 */
		pci_emul_iobase += BUSIO_ROUNDUP;
		pci_emul_iobase = roundup2(pci_emul_iobase, BUSIO_ROUNDUP);
		bi->iolimit = pci_emul_iobase;

		pci_emul_membase32 += BUSMEM_ROUNDUP;
		pci_emul_membase32 = roundup2(pci_emul_membase32,
		    BUSMEM_ROUNDUP);
		bi->memlimit32 = pci_emul_membase32;

		pci_emul_membase64 += BUSMEM_ROUNDUP;
		pci_emul_membase64 = roundup2(pci_emul_membase64,
		    BUSMEM_ROUNDUP);
		bi->memlimit64 = pci_emul_membase64;
	}

	/* TODO: gvt PCI bar0 and bar2 aren't allocated by ACRN DM,
	 * here, need update bus0 memlimit32 value.
	 * Currently, we only deal with bus0 memlimit32.
	 * If other PCI devices also use reserved regions,
	 * need to change these code.
	 */
	bi = pci_businfo[0];
	bus0_memlimit = bi->memlimit32;
	for(i = 0; i < REGION_NUMS; i++){
		if(reserved_bar_regions[i].vdev &&
		  reserved_bar_regions[i].bar_type == PCIBAR_MEM32){
			bus0_memlimit = (bus0_memlimit > (reserved_bar_regions[i].end + 1))
				        ? bus0_memlimit : (reserved_bar_regions[i].end + 1);
		}
	}
	bi->memlimit32 = bus0_memlimit;

	error = check_gsi_sharing_violation();
	if (error < 0)
		goto pci_emul_init_fail;

	/*
	 * PCI backends are initialized before routing INTx interrupts
	 * so that LPC devices are able to reserve ISA IRQs before
	 * routing PIRQ pins.
	 */
	for (bus = 0; bus < MAXBUSES; bus++) {
		bi = pci_businfo[bus];
		if (bi == NULL)
			continue;

		for (slot = 0; slot < MAXSLOTS; slot++) {
			si = &bi->slotinfo[slot];
			for (func = 0; func < MAXFUNCS; func++) {
				fi = &si->si_funcs[func];
				if (fi->fi_devi == NULL)
					continue;
				pci_lintr_route(fi->fi_devi);
				ops = fi->fi_devi->dev_ops;
				if (ops && ops->vdev_phys_access)
					ops->vdev_phys_access(ctx,
						fi->fi_devi);
			}
		}
	}
	lpc_pirq_routed();

	/*
	 * The guest physical memory map looks like the following:
	 * [0,              lowmem)         guest system memory
	 * [lowmem,         lowmem_limit)   memory hole (may be absent)
	 * [lowmem_limit,   0xE0000000)     PCI hole (32-bit BAR allocation)
	 * [0xE0000000,     0xF0000000)     PCI extended config window
	 * [0xF0000000,     4GB)            LAPIC, IOAPIC, HPET, firmware
	 * [4GB,            5GB)            PCI hole (64-bit BAR allocation)
	 * [5GB,            5GB + highmem)  guest system memory
	 */

	/*
	 * Accesses to memory addresses that are not allocated to system
	 * memory or PCI devices return 0xff's.
	 */
	bzero(&mr, sizeof(struct mem_range));
	mr.name = "PCI hole (32-bit)";
	mr.flags = MEM_F_RW;
	mr.base = PCI_EMUL_MEMBASE32;
	mr.size = PCI_EMUL_MEMLIMIT32 - PCI_EMUL_MEMBASE32;
	mr.handler = pci_emul_fallback_handler;
	error = register_mem_fallback(&mr);
	if (error != 0)
		goto pci_emul_init_fail;

	/* ditto for the 64-bit PCI host aperture */
	bzero(&mr, sizeof(struct mem_range));
	mr.name = "PCI hole (64-bit)";
	mr.flags = MEM_F_RW;
	mr.base = PCI_EMUL_MEMBASE64;
	mr.size = PCI_EMUL_MEMLIMIT64 - PCI_EMUL_MEMBASE64;
	mr.handler = pci_emul_fallback_handler;
	error = register_mem_fallback(&mr);
	if (error != 0)
		goto pci_emul_init_fail;

	/* PCI extended config space */
	bzero(&mr, sizeof(struct mem_range));
	mr.name = "PCI ECFG";
	mr.flags = MEM_F_RW;
	mr.base = PCI_EMUL_ECFG_BASE;
	mr.size = PCI_EMUL_ECFG_SIZE;
	mr.handler = pci_emul_ecfg_handler;
	error = register_mem(&mr);
	if (error != 0)
		goto pci_emul_init_fail;

	return 0;

pci_emul_init_fail:
	for (bus = 0; bus < MAXBUSES && success_cnt > 0; bus++) {
		bi = pci_businfo[bus];
		if (bi == NULL)
			continue;
		for (slot = 0; slot < MAXSLOTS && success_cnt > 0; slot++) {
			si = &bi->slotinfo[slot];
			for (func = 0; func < MAXFUNCS; func++) {
				fi = &si->si_funcs[func];
				if (fi->fi_name == NULL)
					continue;
				if (success_cnt-- <= 0)
					break;
				ops = pci_emul_finddev(fi->fi_name);
				if (!ops) {
					pr_warn("No driver for device [%s]\n", fi->fi_name);
					continue;
				}
				pci_emul_deinit(ctx, ops, bus, slot,
				    func, fi);
			}
		}
	}

	return error;
}

void
deinit_pci(struct vmctx *ctx)
{
	struct pci_vdev_ops *ops;
	struct businfo *bi;
	struct slotinfo *si;
	struct funcinfo *fi;
	int bus, slot, func;
	struct mem_range mr;

	/* Release PCI extended config space */
	bzero(&mr, sizeof(struct mem_range));
	mr.name = "PCI ECFG";
	mr.base = PCI_EMUL_ECFG_BASE;
	mr.size = PCI_EMUL_ECFG_SIZE;
	unregister_mem(&mr);

	/* Release PCI hole space */
	bzero(&mr, sizeof(struct mem_range));
	mr.name = "PCI hole (32-bit)";
	mr.base = PCI_EMUL_MEMBASE32;
	mr.size = PCI_EMUL_MEMLIMIT32 - PCI_EMUL_MEMBASE32;
	unregister_mem_fallback(&mr);

	/* ditto for the 64-bit PCI host aperture */
	bzero(&mr, sizeof(struct mem_range));
	mr.name = "PCI hole (64-bit)";
	mr.base = PCI_EMUL_MEMBASE64;
	mr.size = PCI_EMUL_MEMLIMIT64 - PCI_EMUL_MEMBASE64;
	unregister_mem_fallback(&mr);

	for (bus = 0; bus < MAXBUSES; bus++) {
		bi = pci_businfo[bus];
		if (bi == NULL)
			continue;

		for (slot = 0; slot < MAXSLOTS; slot++) {
			si = &bi->slotinfo[slot];
			for (func = 0; func < MAXFUNCS; func++) {
				fi = &si->si_funcs[func];
				if (fi->fi_name == NULL)
					continue;
				ops = pci_emul_finddev(fi->fi_name);
				if (!ops) {
					pr_warn("No driver for device [%s]\n", fi->fi_name);
					continue;
				}
				pr_notice("pci deinit %s\n", fi->fi_name);
				pci_emul_deinit(ctx, ops, bus, slot,
				    func, fi);
			}
		}
	}
}

static void
pci_apic_prt_entry(int bus, int slot, int pin, int pirq_pin, int ioapic_irq,
		   void *arg)
{
	dsdt_line("  Package ()");
	dsdt_line("  {");
	dsdt_line("    0x%X,", slot << 16 | 0xffff);
	dsdt_line("    0x%02X,", pin - 1);
	dsdt_line("    Zero,");
	dsdt_line("    0x%X", ioapic_irq);
	dsdt_line("  },");
}

static void
pci_pirq_prt_entry(int bus, int slot, int pin, int pirq_pin, int ioapic_irq,
		   void *arg)
{
	char *name;

	name = lpc_pirq_name(pirq_pin);
	if (name == NULL)
		return;
	dsdt_line("  Package ()");
	dsdt_line("  {");
	dsdt_line("    0x%X,", slot << 16 | 0xffff);
	dsdt_line("    0x%02X,", pin - 1);
	dsdt_line("    %s,", name);
	dsdt_line("    0x00");
	dsdt_line("  },");
	free(name);
}

/*
 * A acrn-dm virtual machine has a flat PCI hierarchy with a root port
 * corresponding to each PCI bus.
 */
static void
pci_bus_write_dsdt(int bus)
{
	struct businfo *bi;
	struct slotinfo *si;
	struct pci_vdev *dev;
	int count, func, slot;

	/*
	 * If there are no devices on this 'bus' then just return.
	 */
	 bi = pci_businfo[bus];
	if (bi == NULL) {
		/*
		 * Bus 0 is special because it decodes the I/O ports used
		 * for PCI config space access even if there are no devices
		 * on it.
		 */
		if (bus != 0)
			return;
	}

	dsdt_line("  Device (PCI%01X)", bus);
	dsdt_line("  {");
	dsdt_line("    Name (_HID, EisaId (\"PNP0A03\"))");

	dsdt_line("    Method (_BBN, 0, NotSerialized)");
	dsdt_line("    {");
	dsdt_line("        Return (0x%08X)", bus);
	dsdt_line("    }");
	dsdt_line("    Name (_CRS, ResourceTemplate ()");
	dsdt_line("    {");
	dsdt_line("      WordBusNumber (ResourceProducer, MinFixed, "
	    "MaxFixed, PosDecode,");
	dsdt_line("        0x0000,             // Granularity");
	dsdt_line("        0x%04X,             // Range Minimum", bus);
	dsdt_line("        0x%04X,             // Range Maximum", bus);
	dsdt_line("        0x0000,             // Translation Offset");
	dsdt_line("        0x0001,             // Length");
	dsdt_line("        ,, )");

	if (bus == 0) {
		dsdt_indent(3);
		dsdt_fixed_ioport(0xCF8, 8);
		dsdt_unindent(3);

		dsdt_line("      WordIO (ResourceProducer, MinFixed, MaxFixed, "
		    "PosDecode, EntireRange,");
		dsdt_line("        0x0000,             // Granularity");
		dsdt_line("        0x0000,             // Range Minimum");
		dsdt_line("        0x0CF7,             // Range Maximum");
		dsdt_line("        0x0000,             // Translation Offset");
		dsdt_line("        0x0CF8,             // Length");
		dsdt_line("        ,, , TypeStatic)");

		dsdt_line("      WordIO (ResourceProducer, MinFixed, MaxFixed, "
		    "PosDecode, EntireRange,");
		dsdt_line("        0x0000,             // Granularity");
		dsdt_line("        0x0D00,             // Range Minimum");
		dsdt_line("        0x%04X,             // Range Maximum",
		    PCI_EMUL_IOBASE - 1);
		dsdt_line("        0x0000,             // Translation Offset");
		dsdt_line("        0x%04X,             // Length",
		    PCI_EMUL_IOBASE - 0x0D00);
		dsdt_line("        ,, , TypeStatic)");

		if (bi == NULL) {
			dsdt_line("    })");
			goto done;
		}
	}

	/* i/o window */
	dsdt_line("      WordIO (ResourceProducer, MinFixed, MaxFixed, "
	    "PosDecode, EntireRange,");
	dsdt_line("        0x0000,             // Granularity");
	dsdt_line("        0x%04X,             // Range Minimum", bi->iobase);
	dsdt_line("        0x%04X,             // Range Maximum",
	    bi->iolimit - 1);
	dsdt_line("        0x0000,             // Translation Offset");
	dsdt_line("        0x%04X,             // Length",
	    bi->iolimit - bi->iobase);
	dsdt_line("        ,, , TypeStatic)");

	/* mmio window (32-bit) */
	dsdt_line("      DWordMemory (ResourceProducer, PosDecode, "
	    "MinFixed, MaxFixed, NonCacheable, ReadWrite,");
	dsdt_line("        0x00000000,         // Granularity");
	dsdt_line("        0x%08X,         // Range Minimum\n", PCI_EMUL_MEMBASE32);
	dsdt_line("        0x%08X,         // Range Maximum\n",
	    PCI_EMUL_MEMLIMIT32 - 1);
	dsdt_line("        0x00000000,         // Translation Offset");
	dsdt_line("        0x%08X,         // Length\n",
	    PCI_EMUL_MEMLIMIT32 - PCI_EMUL_MEMBASE32);
	dsdt_line("        ,, , AddressRangeMemory, TypeStatic)");

	/* mmio window (64-bit) */
	dsdt_line("      QWordMemory (ResourceProducer, PosDecode, "
	    "MinFixed, MaxFixed, NonCacheable, ReadWrite,");
	dsdt_line("        0x0000000000000000, // Granularity");
	dsdt_line("        0x%016lX, // Range Minimum\n", PCI_EMUL_MEMBASE64);
	dsdt_line("        0x%016lX, // Range Maximum\n",
	    PCI_EMUL_MEMLIMIT64 - 1);
	dsdt_line("        0x0000000000000000, // Translation Offset");
	dsdt_line("        0x%016lX, // Length\n",
	    PCI_EMUL_MEMLIMIT64 - PCI_EMUL_MEMBASE64);
	dsdt_line("        ,, , AddressRangeMemory, TypeStatic)");
	dsdt_line("    })");

	if (!is_rtvm) {
		count = pci_count_lintr(bus);
		if (count != 0) {
			dsdt_indent(2);
			dsdt_line("Name (PPRT, Package ()");
			dsdt_line("{");
			pci_walk_lintr(bus, pci_pirq_prt_entry, NULL);
			dsdt_line("})");
			dsdt_line("Name (APRT, Package ()");
			dsdt_line("{");
			pci_walk_lintr(bus, pci_apic_prt_entry, NULL);
			dsdt_line("})");
			dsdt_line("Method (_PRT, 0, NotSerialized)");
			dsdt_line("{");
			dsdt_line("  If (PICM)");
			dsdt_line("  {");
			dsdt_line("    Return (APRT)");
			dsdt_line("  }");
			dsdt_line("  Else");
			dsdt_line("  {");
			dsdt_line("    Return (PPRT)");
			dsdt_line("  }");
			dsdt_line("}");
			dsdt_unindent(2);
		}
	}

	dsdt_indent(2);
	for (slot = 0; slot < MAXSLOTS; slot++) {
		si = &bi->slotinfo[slot];
		for (func = 0; func < MAXFUNCS; func++) {
			dev = si->si_funcs[func].fi_devi;
			if (dev != NULL &&
			    dev->dev_ops->vdev_write_dsdt != NULL)
				dev->dev_ops->vdev_write_dsdt(dev);
		}
	}
	dsdt_unindent(2);
done:
	dsdt_line("  }");
}

void
pci_write_dsdt(void)
{
	int bus;

	dsdt_indent(1);
	dsdt_line("Name (PICM, 0x00)");
	dsdt_line("Method (_PIC, 1, NotSerialized)");
	dsdt_line("{");
	dsdt_line("  Store (Arg0, PICM)");
	dsdt_line("}");
	dsdt_line("");
	dsdt_line("Scope (_SB)");
	dsdt_line("{");
	for (bus = 0; bus < MAXBUSES; bus++)
		pci_bus_write_dsdt(bus);
	dsdt_line("}");
	dsdt_unindent(1);
}

int
pci_bus_configured(int bus)
{
	return (pci_businfo[bus] != NULL);
}

int
pci_msi_enabled(struct pci_vdev *dev)
{
	return dev->msi.enabled;
}

int
pci_msi_maxmsgnum(struct pci_vdev *dev)
{
	if (dev->msi.enabled)
		return dev->msi.maxmsgnum;
	else
		return 0;
}

int
pci_msix_enabled(struct pci_vdev *dev)
{
	return (dev->msix.enabled && !dev->msi.enabled);
}

/**
 * @brief Generate a MSI-X interrupt to guest
 *
 * @param dev Pointer to struct pci_vdev representing virtual PCI device.
 * @param index MSIx table entry index.
 *
 * @return None
 */
void
pci_generate_msix(struct pci_vdev *dev, int index)
{
	struct msix_table_entry *mte;

	if (!pci_msix_enabled(dev))
		return;

	if (dev->msix.function_mask)
		return;

	if (index >= dev->msix.table_count)
		return;

	mte = &dev->msix.table[index];
	if ((mte->vector_control & PCIM_MSIX_VCTRL_MASK) == 0) {
		/* XXX Set PBA bit if interrupt is disabled */
		vm_lapic_msi(dev->vmctx, mte->addr, mte->msg_data);
	}
}

/**
 * @brief Generate a MSI interrupt to guest
 *
 * @param dev Pointer to struct pci_vdev representing virtual PCI device.
 * @param index Message data index.
 *
 * @return None
 */
void
pci_generate_msi(struct pci_vdev *dev, int index)
{
	if (pci_msi_enabled(dev) && index < pci_msi_maxmsgnum(dev)) {
		vm_lapic_msi(dev->vmctx, dev->msi.addr,
			     dev->msi.msg_data + index);
	}
}

static bool
pci_lintr_permitted(struct pci_vdev *dev)
{
	uint16_t cmd;

	cmd = pci_get_cfgdata16(dev, PCIR_COMMAND);
	return (!(dev->msi.enabled || dev->msix.enabled ||
		(cmd & PCIM_CMD_INTxDIS)));
}

void
pci_lintr_request(struct pci_vdev *dev)
{
	struct businfo *bi;
	struct slotinfo *si;
	int bestpin, bestcount, pin;

	bi = pci_businfo[dev->bus];
	if (bi == NULL) {
		pr_err("%s: pci [%s] has wrong bus %d info!\n", __func__, dev->name, dev->bus);
		return;
	}

	/*
	 * Just allocate a pin from our slot.  The pin will be
	 * assigned IRQs later when interrupts are routed.
	 */
	si = &bi->slotinfo[dev->slot];
	bestpin = 0;
	bestcount = si->si_intpins[0].ii_count;
	for (pin = 1; pin < 4; pin++) {
		if (si->si_intpins[pin].ii_count < bestcount) {
			bestpin = pin;
			bestcount = si->si_intpins[pin].ii_count;
		}
	}

	si->si_intpins[bestpin].ii_count++;
	dev->lintr.pin = bestpin + 1;
	pci_set_cfgdata8(dev, PCIR_INTPIN, bestpin + 1);
}

void
pci_lintr_release(struct pci_vdev *dev)
{
	struct businfo *bi;
	struct slotinfo *si;
	int pin;

	bi = pci_businfo[dev->bus];
	if (bi == NULL) {
		pr_err("%s: pci [%s] has wrong bus %d info!\n", __func__, dev->name, dev->bus);
		return;
	}

	si = &bi->slotinfo[dev->slot];

	for (pin = 1; pin < 4; pin++) {
		si->si_intpins[pin].ii_count = 0;
		si->si_intpins[pin].ii_pirq_pin = 0;
		si->si_intpins[pin].ii_ioapic_irq = 0;
	}
}

static void
pci_lintr_route(struct pci_vdev *dev)
{
	struct businfo *bi;
	struct intxinfo *ii;

	if (dev->lintr.pin == 0)
		return;

	bi = pci_businfo[dev->bus];
	if (bi == NULL) {
		pr_err("%s: pci [%s] has wrong bus %d info!\n", __func__, dev->name, dev->bus);
		return;
	}
	ii = &bi->slotinfo[dev->slot].si_intpins[dev->lintr.pin - 1];

	/*
	 * Attempt to allocate an I/O APIC pin for this intpin if one
	 * is not yet assigned.
	 */
	if (ii->ii_ioapic_irq == 0)
		ii->ii_ioapic_irq = ioapic_pci_alloc_irq(dev);

	/*
	 * Attempt to allocate a PIRQ pin for this intpin if one is
	 * not yet assigned.
	 */
	if (ii->ii_pirq_pin == 0)
		ii->ii_pirq_pin = pirq_alloc_pin(dev);

	dev->lintr.ioapic_irq = ii->ii_ioapic_irq;
	dev->lintr.pirq_pin = ii->ii_pirq_pin;
	pci_set_cfgdata8(dev, PCIR_INTLINE, pirq_irq(ii->ii_pirq_pin));
}

/**
 * @brief Assert INTx pin of virtual PCI device
 *
 * @param dev Pointer to struct pci_vdev representing virtual PCI device.
 *
 * @return None
 */
void
pci_lintr_assert(struct pci_vdev *dev)
{
	if (dev->lintr.pin <= 0) {
		pr_warn("%s: Invalid intr pin on dev [%s]\n", __func__, dev->name);
		return;
	}

	pthread_mutex_lock(&dev->lintr.lock);
	if (dev->lintr.state == IDLE) {
		if (pci_lintr_permitted(dev)) {
			dev->lintr.state = ASSERTED;
			pci_irq_assert(dev);
		} else
			dev->lintr.state = PENDING;
	}
	pthread_mutex_unlock(&dev->lintr.lock);
}

/**
 * @brief Deassert INTx pin of virtual PCI device
 *
 * @param dev Pointer to struct pci_vdev representing virtual PCI device.
 *
 * @return None
 */
void
pci_lintr_deassert(struct pci_vdev *dev)
{
	if (dev->lintr.pin <= 0) {
		pr_warn("%s: Invalid intr pin on dev [%s]\n", __func__, dev->name);
		return;
	}

	pthread_mutex_lock(&dev->lintr.lock);
	if (dev->lintr.state == ASSERTED) {
		dev->lintr.state = IDLE;
		pci_irq_deassert(dev);
	} else if (dev->lintr.state == PENDING)
		dev->lintr.state = IDLE;
	pthread_mutex_unlock(&dev->lintr.lock);
}

static void
pci_lintr_update(struct pci_vdev *dev)
{
	pthread_mutex_lock(&dev->lintr.lock);
	if (dev->lintr.state == ASSERTED && !pci_lintr_permitted(dev)) {
		pci_irq_deassert(dev);
		dev->lintr.state = PENDING;
	} else if (dev->lintr.state == PENDING && pci_lintr_permitted(dev)) {
		dev->lintr.state = ASSERTED;
		pci_irq_assert(dev);
	}
	pthread_mutex_unlock(&dev->lintr.lock);
}

int
pci_count_lintr(int bus)
{
	int count, slot, pin;
	struct slotinfo *slotinfo;

	count = 0;
	if (pci_businfo[bus] != NULL) {
		for (slot = 0; slot < MAXSLOTS; slot++) {
			slotinfo = &pci_businfo[bus]->slotinfo[slot];
			for (pin = 0; pin < 4; pin++) {
				if (slotinfo->si_intpins[pin].ii_count != 0)
					count++;
			}
		}
	}
	return count;
}

void
pci_walk_lintr(int bus, pci_lintr_cb cb, void *arg)
{
	struct businfo *bi;
	struct slotinfo *si;
	struct intxinfo *ii;
	int slot, pin;

	bi = pci_businfo[bus];
	if (bi == NULL)
		return;

	for (slot = 0; slot < MAXSLOTS; slot++) {
		si = &bi->slotinfo[slot];
		for (pin = 0; pin < 4; pin++) {
			ii = &si->si_intpins[pin];
			if (ii->ii_count != 0)
				cb(bus, slot, pin + 1, ii->ii_pirq_pin,
				    ii->ii_ioapic_irq, arg);
		}
	}
}

/*
 * Return 1 if the emulated device in 'slot' is a multi-function device.
 * Return 0 otherwise.
 */
static int
pci_emul_is_mfdev(int bus, int slot)
{
	struct businfo *bi;
	struct slotinfo *si;
	int f, numfuncs;

	numfuncs = 0;
	bi = pci_businfo[bus];
	if (bi != NULL) {
		si = &bi->slotinfo[slot];
		for (f = 0; f < MAXFUNCS; f++) {
			if (si->si_funcs[f].fi_devi != NULL)
				numfuncs++;
		}
	}
	return (numfuncs > 1);
}

/*
 * Ensure that the PCIM_MFDEV bit is properly set (or unset) depending on
 * whether or not is a multi-function being emulated in the pci 'slot'.
 */
static void
pci_emul_hdrtype_fixup(int bus, int slot, int off, int bytes, uint32_t *rv)
{
	int mfdev;

	if (off <= PCIR_HDRTYPE && off + bytes > PCIR_HDRTYPE) {
		mfdev = pci_emul_is_mfdev(bus, slot);
		switch (bytes) {
		case 1:
		case 2:
			*rv &= ~PCIM_MFDEV;
			if (mfdev)
				*rv |= PCIM_MFDEV;
			break;
		case 4:
			*rv &= ~(PCIM_MFDEV << 16);
			if (mfdev)
				*rv |= (PCIM_MFDEV << 16);
			break;
		}
	}
}

static void
pci_emul_cmdsts_write(struct pci_vdev *dev, int coff, uint32_t new, int bytes)
{
	int i, rshift;
	uint32_t cmd, cmd2, changed, old, readonly;

	cmd = pci_get_cfgdata16(dev, PCIR_COMMAND);	/* stash old value */

	/*
	 * From PCI Local Bus Specification 3.0 sections 6.2.2 and 6.2.3.
	 *
	 * XXX Bits 8, 11, 12, 13, 14 and 15 in the status register are
	 * 'write 1 to clear'. However these bits are not set to '1' by
	 * any device emulation so it is simpler to treat them as readonly.
	 */
	rshift = (coff & 0x3) * 8;
	readonly = 0xFFFFF880 >> rshift;

	old = CFGREAD(dev, coff, bytes);
	new &= ~readonly;
	new |= (old & readonly);
	CFGWRITE(dev, coff, new, bytes);		/* update config */

	cmd2 = pci_get_cfgdata16(dev, PCIR_COMMAND);	/* get updated value */
	changed = cmd ^ cmd2;

	/*
	 * If the MMIO or I/O address space decoding has changed then
	 * register/unregister all BARs that decode that address space.
	 */
	for (i = 0; i <= PCI_BARMAX; i++) {
		switch (dev->bar[i].type) {
		case PCIBAR_NONE:
		case PCIBAR_MEMHI64:
			break;
		case PCIBAR_IO:
		/* I/O address space decoding changed? */
			if (changed & PCIM_CMD_PORTEN) {
				if (porten(dev))
					register_bar(dev, i);
				else
					unregister_bar(dev, i);
			}
			break;
		case PCIBAR_MEM32:
		case PCIBAR_MEM64:
		/* MMIO address space decoding changed? */
			if (changed & PCIM_CMD_MEMEN) {
				if (memen(dev))
					register_bar(dev, i);
				else
					unregister_bar(dev, i);
			}
			break;
		default:
			pr_err("%s: invalid bar type %d\n", __func__, dev->bar[i].type);
			return;
		}
	}

	/*
	 * If INTx has been unmasked and is pending, assert the
	 * interrupt.
	 */
	pci_lintr_update(dev);
}

static void
pci_cfgrw(struct vmctx *ctx, int vcpu, int in, int bus, int slot, int func,
	  int coff, int bytes, uint32_t *eax)
{
	struct businfo *bi;
	struct slotinfo *si;
	struct pci_vdev *dev;
	struct pci_vdev_ops *ops;
	int idx, needcfg;
	uint64_t addr, bar, mask;
	bool decode, ignore_reg_unreg = false;
	uint8_t mmio_bar_prop;

	bi = pci_businfo[bus];
	if (bi != NULL) {
		si = &bi->slotinfo[slot];
		dev = si->si_funcs[func].fi_devi;
	} else
		dev = NULL;

	/*
	 * Just return if there is no device at this slot:func or if the
	 * the guest is doing an un-aligned access.
	 */
	if (dev == NULL || (bytes != 1 && bytes != 2 && bytes != 4) ||
	    (coff & (bytes - 1)) != 0) {
		if (in)
			*eax = 0xffffffff;
		return;
	}

	ops = dev->dev_ops;

	/*
	 * For non-passthru device, extended config space is NOT supported.
	 * Ignore all writes beyond the standard config space and return all
	 * ones on reads.
	 *
	 * For passthru device, extended config space is supported.
	 * Access to extended config space is implemented via libpciaccess.
	 */
	if (strcmp("passthru", ops->class_name)) {
		if (coff >= PCI_REGMAX + 1) {
			if (in) {
				*eax = 0xffffffff;
				/*
				 * Extended capabilities begin at offset 256 in
				 * config space.
				 * Absence of extended capabilities is signaled
				 * with all 0s in the extended capability header
				 * at offset 256.
				 */
				if (coff <= PCI_REGMAX + 4)
					*eax = 0x00000000;
			}
			return;
		}
	}

	/*
	 * Config read
	 */
	if (in) {
		/* Let the device emulation override the default handler */
		if (ops->vdev_cfgread != NULL) {
			needcfg = ops->vdev_cfgread(ctx, vcpu, dev, coff, bytes,
			    eax);
		} else {
			needcfg = 1;
		}

		if (needcfg)
			*eax = CFGREAD(dev, coff, bytes);

		pci_emul_hdrtype_fixup(bus, slot, coff, bytes, eax);
	} else {
		/* Let the device emulation override the default handler */
		if (ops->vdev_cfgwrite != NULL &&
		    (*ops->vdev_cfgwrite)(ctx, vcpu, dev,
					  coff, bytes, *eax) == 0)
			return;

		/*
		 * Special handling for write to BAR registers
		 */
		if (coff >= PCIR_BAR(0) && coff < PCIR_BAR(PCI_BARMAX + 1)) {
			/*
			 * Ignore writes to BAR registers that are not
			 * 4-byte aligned.
			 */
			if (bytes != 4 || (coff & 0x3) != 0)
				return;
			idx = (coff - PCIR_BAR(0)) / 4;
			mask = ~(dev->bar[idx].size - 1);

			if (dev->bar[idx].type == PCIBAR_IO)
				decode = porten(dev);
			else
				decode = memen(dev);

			/* Some driver does not disable the decode of BAR
			 * register via the command register before sizing a
			 * BAR. This will lead to a overlay of the BAR
			 * addresses when trying to register the intermediate
			 * BAR address via register_bar. A stateful variable
			 * sizing is used to keep track of such kind of BAR
			 * address changes and workaroud this violation.
			 */
			if (decode) {
				if (!dev->bar[idx].sizing && (*eax == ~0U)) {
					dev->bar[idx].sizing = true;
					ignore_reg_unreg = true;
				} else if (dev->bar[idx].sizing && (*eax != ~0U)) {
					dev->bar[idx].sizing = false;
					ignore_reg_unreg = true;
				}
			}

			/* save the bar property for MMIO pci bar. */
			mmio_bar_prop = pci_get_cfgdata32(dev, PCIR_BAR(idx)) &
						(PCIM_BAR_SPACE | PCIM_BAR_MEM_TYPE |
						PCIM_BAR_MEM_PREFETCH);

			switch (dev->bar[idx].type) {
			case PCIBAR_NONE:
				dev->bar[idx].addr = bar = 0;
				break;
			case PCIBAR_IO:
				addr = *eax & mask;
				addr &= 0xffff;
				bar = addr | PCIM_BAR_IO_SPACE;
				/*
				 * Register the new BAR value for interception
				 */
				if (addr != dev->bar[idx].addr) {
					update_bar_address(ctx, dev, addr, idx,
							   PCIBAR_IO,
							   ignore_reg_unreg);
				}
				break;
			case PCIBAR_MEM32:
				addr = bar = *eax & mask;
				/* Restore the readonly fields for mmio bar */
				bar |= mmio_bar_prop;
				if (addr != dev->bar[idx].addr) {
					update_bar_address(ctx, dev, addr, idx,
							   PCIBAR_MEM32,
							   ignore_reg_unreg);
				}
				break;
			case PCIBAR_MEM64:
				addr = bar = *eax & mask;
				/* Restore the readonly fields for mmio bar */
				bar |= mmio_bar_prop;
				if (addr != (uint32_t)dev->bar[idx].addr) {
					update_bar_address(ctx, dev, addr, idx,
							   PCIBAR_MEM64,
							   ignore_reg_unreg);
				}
				break;
			case PCIBAR_MEMHI64:
				mask = ~(dev->bar[idx - 1].size - 1);
				addr = ((uint64_t)*eax << 32) & mask;
				bar = addr >> 32;
				if (bar != dev->bar[idx - 1].addr >> 32) {
					update_bar_address(ctx, dev, addr, idx - 1,
							   PCIBAR_MEMHI64,
							   ignore_reg_unreg);
				}
				break;
			default:
				pr_err("%s: invalid bar type %d\n", __func__, dev->bar[idx].type);
				return;
			}
			pci_set_cfgdata32(dev, coff, bar);

		} else if (coff == PCIR_BIOS) {
			/* ignore ROM BAR length request */
		} else if (pci_emul_iscap(dev, coff)) {
			pci_emul_capwrite(dev, coff, bytes, *eax);
		} else if (coff >= PCIR_COMMAND && coff < PCIR_REVID) {
			pci_emul_cmdsts_write(dev, coff, *eax, bytes);
		} else {
			CFGWRITE(dev, coff, *eax, bytes);
		}
	}
}

int
emulate_pci_cfgrw(struct vmctx *ctx, int vcpu, int in, int bus, int slot,
		  int func, int reg, int bytes, int *value)
{
	pci_cfgrw(ctx, vcpu, in, bus, slot, func, reg,
			bytes, (uint32_t *)value);
	return 0;
}

#define PCI_EMUL_TEST
#ifdef PCI_EMUL_TEST
/*
 * Define a dummy test device
 */
#define DIOSZ	8
#define DMEMSZ	4096
struct pci_emul_dummy {
	uint8_t   ioregs[DIOSZ];
	uint8_t	  memregs[2][DMEMSZ];
};

#define	PCI_EMUL_MSI_MSGS	 4
#define	PCI_EMUL_MSIX_MSGS	16

static int
pci_emul_dinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct pci_emul_dummy *dummy;

	dummy = calloc(1, sizeof(struct pci_emul_dummy));

	dev->arg = dummy;

	pci_set_cfgdata16(dev, PCIR_DEVICE, 0x0001);
	pci_set_cfgdata16(dev, PCIR_VENDOR, 0x10DD);
	pci_set_cfgdata8(dev, PCIR_CLASS, 0x02);

	return pci_emul_add_msicap(dev, PCI_EMUL_MSI_MSGS) ||
		pci_emul_alloc_bar(dev, 0, PCIBAR_IO, DIOSZ) ||
		pci_emul_alloc_bar(dev, 1, PCIBAR_MEM32, DMEMSZ) ||
		pci_emul_alloc_bar(dev, 2, PCIBAR_MEM32, DMEMSZ);
}

static void
pci_emul_diow(struct vmctx *ctx, int vcpu, struct pci_vdev *dev, int baridx,
	      uint64_t offset, int size, uint64_t value)
{
	int i;
	void *offset_ptr;
	struct pci_emul_dummy *dummy = dev->arg;

	if (baridx == 0) {
		if (offset + size > DIOSZ) {
			pr_err("diow: iow too large, offset %ld size %d\n",
			       offset, size);
			return;
		}

		offset_ptr = (void *) &dummy->ioregs[offset];
		if (size == 1)
			*(uint8_t *)offset_ptr = value & 0xff;
		else if (size == 2)
			*(uint16_t *)offset_ptr = value & 0xffff;
		else if (size == 4)
			*(uint32_t *)offset = value;
		else
			pr_err("diow: iow unknown size %d\n", size);

		/*
		 * Special magic value to generate an interrupt
		 */
		if (offset == 4 && size == 4 && pci_msi_enabled(dev))
			pci_generate_msi(dev, value % pci_msi_maxmsgnum(dev));

		if (value == 0xabcdef) {
			for (i = 0; i < pci_msi_maxmsgnum(dev); i++)
				pci_generate_msi(dev, i);
		}
	}

	if (baridx == 1 || baridx == 2) {
		if (offset + size > DMEMSZ) {
			pr_err("diow: memw too large, offset %ld size %d\n",
			       offset, size);
			return;
		}

		i = baridx - 1;		/* 'memregs' index */

		offset_ptr = (void *) &dummy->memregs[i][offset];
		if (size == 1)
			*(uint8_t *)offset_ptr = value;
		else if (size == 2)
			*(uint16_t *)offset_ptr = value;
		else if (size == 4)
			*(uint32_t *)offset_ptr = value;
		else if (size == 8)
			*(uint64_t *)offset_ptr = value;
		else
			pr_err("diow: memw unknown size %d\n", size);

		/*
		 * magic interrupt ??
		 */
	}

	if (baridx > 2 || baridx < 0)
		pr_err("diow: unknown bar idx %d\n", baridx);
}

static uint64_t
pci_emul_dior(struct vmctx *ctx, int vcpu, struct pci_vdev *dev, int baridx,
	      uint64_t offset, int size)
{
	struct pci_emul_dummy *dummy = dev->arg;
	uint32_t value = 0;
	int i;
	void *offset_ptr;

	if (baridx == 0) {
		if (offset + size > DIOSZ) {
			pr_err("dior: ior too large, offset %ld size %d\n",
			       offset, size);
			return 0;
		}

		value = 0;
		offset_ptr = (void *) &dummy->ioregs[offset];
		if (size == 1)
			value = *(uint8_t *)offset_ptr;
		else if (size == 2)
			value = *(uint16_t *)offset_ptr;
		else if (size == 4)
			value = *(uint32_t *)offset_ptr;
		else
			pr_err("dior: ior unknown size %d\n", size);
	}

	if (baridx == 1 || baridx == 2) {
		if (offset + size > DMEMSZ) {
			pr_err("dior: memr too large, offset %ld size %d\n",
			       offset, size);
			return 0;
		}

		i = baridx - 1;		/* 'memregs' index */

		offset_ptr = (void *) &dummy->memregs[i][offset];
		if (size == 1)
			value = *(uint8_t *)offset_ptr;
		else if (size == 2)
			value = *(uint16_t *)offset_ptr;
		else if (size == 4)
			value = *(uint32_t *)offset_ptr;
		else if (size == 8)
			value = *(uint64_t *)offset_ptr;
		else
			pr_err("dior: ior unknown size %d\n", size);
	}


	if (baridx > 2 || baridx < 0) {
		pr_err("dior: unknown bar idx %d\n", baridx);
		return 0;
	}

	return value;
}

struct pci_vdev*
pci_get_vdev_info(int slot)
{
	struct businfo *bi;
	struct slotinfo *si;
	struct pci_vdev *dev = NULL;

	bi = pci_businfo[0];
	if (bi == NULL)
		return NULL;
	if (slot < 0 || slot >= MAXSLOTS)
		return NULL;

	si = &bi->slotinfo[slot];
	if (si != NULL)
		dev = si->si_funcs[0].fi_devi;
	else
		pr_err("slot=%d is empty!\n", slot);

	return dev;
}

struct pci_vdev_ops pci_dummy = {
	.class_name	= "dummy",
	.vdev_init	= pci_emul_dinit,
	.vdev_barwrite	= pci_emul_diow,
	.vdev_barread	= pci_emul_dior
};
DEFINE_PCI_DEVTYPE(pci_dummy);

#endif /* PCI_EMUL_TEST */
