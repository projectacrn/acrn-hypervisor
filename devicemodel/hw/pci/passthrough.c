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

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/user.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <pciaccess.h>
#include <fcntl.h>
#include <unistd.h>

#include "iodev.h"
#include "vmmapi.h"
#include "pciio.h"
#include "pci_core.h"
#include "acpi.h"

#ifndef PCI_COMMAND_INTX_DISABLE
#define PCI_COMMAND_INTX_DISABLE ((uint16_t)0x400)
#endif

/* Used to temporarily set mmc & mme to support only one vector for MSI,
 * remove it when multiple vectors for MSI is ready.
 */
#define FORCE_MSI_SINGLE_VECTOR 1

#define MSIX_TABLE_COUNT(ctrl) (((ctrl) & PCIM_MSIXCTRL_TABLE_SIZE) + 1)
#define MSIX_CAPLEN 12

#define	PCI_BDF_GPU		0x00000010	/* 00:02.0 */

/* Some audio drivers get topology data from ACPI NHLT table.
 * For such drivers, we need to copy the host NHLT table to make it
 * available to the Guest OS. Most audio drivers don't need this by
 * default, when that's the case setting this macro to 0 will avoid
 * unexpected failures.
 * The cAVS audio needs this however, so we enable this feature.
 */
#define AUDIO_NHLT_HACK 1

extern uint64_t audio_nhlt_len;

/* TODO: Add support for IO BAR of PTDev */
static int iofd = -1;

/* reference count for libpciaccess init/deinit */
static int pciaccess_ref_cnt;
static pthread_mutex_t ref_cnt_mtx = PTHREAD_MUTEX_INITIALIZER;

/* Not check reset capability before assign ptdev.
 * Set false by default, that is, always check.
 */
static bool no_reset = false;

struct mmio_map {
	uint64_t gpa;
	uint64_t hpa;
	size_t size;
};

struct passthru_dev {
	struct pci_vdev *dev;
	struct pcibar bar[PCI_BARMAX + 1];
	struct {
		int		capoff;
		int		msgctrl;
		int		emulated;
	} msi;
	struct {
		int		capoff;
		int		table_size;		/* page aligned size */
		void		*table_pages;
		int		table_offset;		/* page aligned */
	} msix;
	bool pcie_cap;
	struct pcisel sel;
	int phys_pin;
	uint16_t phys_bdf;
	struct pci_device *phys_dev;
	/* Options for passthrough device:
	 *   need_reset - reset dev before passthrough
	 */
	bool need_reset;
	/* The memory pages, which not overlap with MSI-X table will be
	 * passed-through to guest, two potential ranges, before and after MSI-X
	 * Table if any.
	 */
	struct mmio_map	msix_bar_mmio[2];
};

void ptdev_no_reset(bool enable)
{
	no_reset = enable;
}

static int
msi_caplen(int msgctrl)
{
	int len;

	len = 10;		/* minimum length of msi capability */

	if (msgctrl & PCIM_MSICTRL_64BIT)
		len += 4;

	/*
	 * Ignore the 'mask' and 'pending' bits in the MSI capability
	 * (msgctrl & PCIM_MSICTRL_VECTOR).
	 * Ignore 10 bytes in total (2-byte reserved, 4-byte mask bits,
	 * 4-byte pending bits).
	 * We'll let the guest manipulate them directly.
	 */

	return len;
}

static uint32_t
read_config(struct pci_device *phys_dev, long reg, int width)
{
	uint32_t temp = 0;

	switch (width) {
	case 1:
		pci_device_cfg_read_u8(phys_dev, (uint8_t *)&temp, reg);
		break;
	case 2:
		pci_device_cfg_read_u16(phys_dev, (uint16_t *)&temp, reg);
		break;
	case 4:
		pci_device_cfg_read_u32(phys_dev, &temp, reg);
		break;
	default:
		warnx("%s: invalid reg width", __func__);
		return -1;
	}

	return temp;
}

static int
write_config(struct pci_device *phys_dev, long reg, int width, uint32_t data)
{
	int temp = -1;

	switch (width) {
	case 1:
		temp = pci_device_cfg_write_u8(phys_dev, data, reg);
		break;
	case 2:
		temp = pci_device_cfg_write_u16(phys_dev, data, reg);
		break;
	case 4:
		temp = pci_device_cfg_write_u32(phys_dev, data, reg);
		break;
	default:
		warnx("%s: invalid reg width", __func__);
	}

	return temp;
}


#ifdef FORCE_MSI_SINGLE_VECTOR
/* Temporarily set mmc & mme to 0.
 * Remove it when multiple vectors for MSI ready.
 */
static inline void
clear_mmc_mme(uint32_t *val)
{
	*val &= ~((uint32_t)PCIM_MSICTRL_MMC_MASK << 16);
	*val &= ~((uint32_t)PCIM_MSICTRL_MME_MASK << 16);
}
#endif

static int
cfginit_cap(struct vmctx *ctx, struct passthru_dev *ptdev)
{
	int ptr, capptr, cap, sts, caplen;
	uint32_t u32;
	struct pci_vdev *dev;
	struct pci_device *phys_dev = ptdev->phys_dev;
	uint16_t virt_bdf = PCI_BDF(ptdev->dev->bus,
				    ptdev->dev->slot,
				    ptdev->dev->func);
	uint32_t pba_info;
	uint32_t table_info;
	uint16_t msgctrl;

	dev = ptdev->dev;

	/*
	 * Parse the capabilities and cache the location of the MSI
	 * and MSI-X capabilities.
	 */
	sts = read_config(phys_dev, PCIR_STATUS, 2);
	if (sts & PCIM_STATUS_CAPPRESENT) {
		ptr = read_config(phys_dev, PCIR_CAP_PTR, 1);
		while (ptr != 0 && ptr != 0xff) {
			cap = read_config(phys_dev, ptr + PCICAP_ID, 1);
			if (cap == PCIY_MSI) {
				/*
				 * Copy the MSI capability into the config
				 * space of the emulated pci device
				 */
				ptdev->msi.capoff = ptr;
				ptdev->msi.msgctrl = read_config(phys_dev,
					ptr + 2, 2);

#ifdef FORCE_MSI_SINGLE_VECTOR
				/* Temporarily set mmc & mme to 0,
				 * which means supporting 1 vector. So that
				 * guest will not enable more than 1 vector.
				 * Remove it when multiple vectors ready.
				 */
				ptdev->msi.msgctrl &= ~PCIM_MSICTRL_MMC_MASK;
				ptdev->msi.msgctrl &= ~PCIM_MSICTRL_MME_MASK;
#endif

				ptdev->msi.emulated = 0;
				caplen = msi_caplen(ptdev->msi.msgctrl);
				capptr = ptr;
				while (caplen > 0) {
					u32 = read_config(phys_dev, capptr, 4);

#ifdef FORCE_MSI_SINGLE_VECTOR
					/* Temporarily set mmc & mme to 0.
					 * which means supporting 1 vector.
					 * Remove it when multiple vectors ready
					 */
					if (capptr == ptdev->msi.capoff)
						clear_mmc_mme(&u32);
#endif

					pci_set_cfgdata32(dev, capptr, u32);
					caplen -= 4;
					capptr += 4;
				}
			} else if (cap == PCIY_MSIX) {
				/*
				 * Copy the MSI-X capability
				 */
				ptdev->msix.capoff = ptr;
				caplen = 12;
				capptr = ptr;
				while (caplen > 0) {
					u32 = read_config(phys_dev, capptr, 4);
					pci_set_cfgdata32(dev, capptr, u32);
					caplen -= 4;
					capptr += 4;
				}
			} else if (cap == PCIY_EXPRESS)
				ptdev->pcie_cap = true;

			ptr = read_config(phys_dev, ptr + PCICAP_NEXTPTR, 1);
		}
	}

	dev->msix.table_bar = -1;
	dev->msix.pba_bar = -1;
	if (ptdev->msix.capoff != 0) {
		capptr = ptdev->msix.capoff;

		pba_info = pci_get_cfgdata32(dev, capptr + 8);
		dev->msix.pba_bar = pba_info & PCIM_MSIX_BIR_MASK;
		dev->msix.pba_offset = pba_info & ~PCIM_MSIX_BIR_MASK;

		table_info = pci_get_cfgdata32(dev, capptr + 4);
		dev->msix.table_bar = table_info & PCIM_MSIX_BIR_MASK;
		dev->msix.table_offset = table_info & ~PCIM_MSIX_BIR_MASK;

		msgctrl = pci_get_cfgdata16(dev, capptr + 2);
		dev->msix.table_count = MSIX_TABLE_COUNT(msgctrl);
		dev->msix.pba_size = PBA_SIZE(dev->msix.table_count);
	} else if (ptdev->msi.capoff != 0) {
		struct ic_ptdev_irq ptirq;

		ptirq.type = IRQ_MSI;
		ptirq.virt_bdf = virt_bdf;
		ptirq.phys_bdf = ptdev->phys_bdf;
		/* currently, only support one vector for MSI */
		ptirq.msix.vector_cnt = 1;
		ptirq.msix.table_paddr = 0;
		ptirq.msix.table_size = 0;
		vm_set_ptdev_msix_info(ctx, &ptirq);
	}

	return 0;
}

static uint64_t
msix_table_read(struct passthru_dev *ptdev, uint64_t offset, int size)
{
	uint8_t *src8;
	uint16_t *src16;
	uint32_t *src32;
	uint64_t *src64;
	uint64_t data;

	switch (size) {
	case 1:
		src8 = (uint8_t *)(ptdev->msix.table_pages + offset - ptdev->msix.table_offset);
		data = *src8;
		break;
	case 2:
		src16 = (uint16_t *)(ptdev->msix.table_pages + offset - ptdev->msix.table_offset);
		data = *src16;
		break;
	case 4:
		src32 = (uint32_t *)(ptdev->msix.table_pages + offset - ptdev->msix.table_offset);
		data = *src32;
		break;
	case 8:
		src64 = (uint64_t *)(ptdev->msix.table_pages + offset - ptdev->msix.table_offset);
		data = *src64;
		break;
	default:
		return -1;

	}
	return data;
}

static void
msix_table_write(struct passthru_dev *ptdev, uint64_t offset, int size, uint64_t data)
{
	uint8_t *dest8;
	uint16_t *dest16;
	uint32_t *dest32;
	uint64_t *dest64;

	switch (size) {
	case 1:
		dest8 = (uint8_t *)(ptdev->msix.table_pages + offset - ptdev->msix.table_offset);
		*dest8 = data;
		break;
	case 2:
		dest16 = (uint16_t *)(ptdev->msix.table_pages + offset - ptdev->msix.table_offset);
		*dest16 = data;
		break;
	case 4:
		dest32 = (uint32_t *)(ptdev->msix.table_pages + offset - ptdev->msix.table_offset);
		*dest32 = data;
		break;
	case 8:
		dest64 = (uint64_t *)(ptdev->msix.table_pages + offset - ptdev->msix.table_offset);
		*dest64 = data;
		break;
	default:
		break;
	}
}

static inline int ptdev_msix_table_bar(struct passthru_dev *ptdev)
{
	return ptdev->dev->msix.table_bar;
}

static inline int ptdev_msix_pba_bar(struct passthru_dev *ptdev)
{
	return ptdev->dev->msix.pba_bar;
}

static int
init_msix_table(struct vmctx *ctx, struct passthru_dev *ptdev, uint64_t base)
{
	int b, s, f;
	int error, idx;
	size_t len, remaining;
	uint32_t table_size, table_offset;
	vm_paddr_t start;
	struct pci_vdev *dev = ptdev->dev;
	uint16_t virt_bdf = PCI_BDF(dev->bus, dev->slot, dev->func);
	struct ic_ptdev_irq ptirq;

	assert(ptdev_msix_table_bar(ptdev) >= 0 && ptdev_msix_pba_bar(ptdev) >= 0);

	b = ptdev->sel.bus;
	s = ptdev->sel.dev;
	f = ptdev->sel.func;

	/*
	 * If the MSI-X table BAR maps memory intended for
	 * other uses, it is at least assured that the table
	 * either resides in its own page within the region,
	 * or it resides in a page shared with only the PBA.
	 */
	table_offset = rounddown2(dev->msix.table_offset, 4096);

	table_size = dev->msix.table_offset - table_offset;
	table_size += dev->msix.table_count * MSIX_TABLE_ENTRY_SIZE;
	table_size = roundup2(table_size, 4096);

	idx = dev->msix.table_bar;
	start = dev->bar[idx].addr;
	remaining = dev->bar[idx].size;

	/* Map everything before the MSI-X table */
	if (table_offset > 0) {
		len = table_offset;
		error = vm_map_ptdev_mmio(ctx, b, s, f, start, len, base);
		if (error) {
			warnx(
			"Failed to map MSI-X BAR passthru pages on %x/%x/%x",
				b,s,f);
			return error;
		}
		/* save mapping info, which need to be unmapped when deinit */
		ptdev->msix_bar_mmio[0].gpa = start;
		ptdev->msix_bar_mmio[0].hpa = base;
		ptdev->msix_bar_mmio[0].size = len;

		base += len;
		start += len;
		remaining -= len;
	}

	/* remap real msix table (page-aligned) to user space */
	error = pci_device_map_range(ptdev->phys_dev, base, table_size,
		PCI_DEV_MAP_FLAG_WRITABLE, &ptdev->msix.table_pages);
	if (error) {
		warnx("Failed to map MSI-X table pages on %x/%x/%x", b,s,f);
		return error;
	}
	ptdev->msix.table_offset = table_offset;
	ptdev->msix.table_size = table_size;

	/* Handle MSI-X vectors:
	 * request to alloc vector entries of MSI-X.
	 * Set table_paddr/table_size to 0 to skip ioremap in sos kernel.
	 */
	ptirq.type = IRQ_MSIX;
	ptirq.virt_bdf = virt_bdf;
	ptirq.phys_bdf = ptdev->phys_bdf;
	ptirq.msix.vector_cnt = dev->msix.table_count;
	ptirq.msix.table_paddr = 0;
	ptirq.msix.table_size = 0;
	vm_set_ptdev_msix_info(ctx, &ptirq);

	/* Skip the MSI-X table */
	base += table_size;
	start += table_size;
	remaining -= table_size;

	/* Map everything beyond the end of the MSI-X table */
	if (remaining > 0) {
		len = remaining;
		error = vm_map_ptdev_mmio(ctx, b, s, f, start, len, base);
		if (error) {
			warnx(
			"Failed to map MSI-X BAR passthru pages on %x/%x/%x",
				b,s,f);
			return error;
		}
		/* save mapping info, which need to be unmapped when deinit */
		ptdev->msix_bar_mmio[1].gpa = start;
		ptdev->msix_bar_mmio[1].hpa = base;
		ptdev->msix_bar_mmio[1].size = len;
	}

	return 0;
}

static void
deinit_msix_table(struct vmctx *ctx, struct pci_vdev *dev)
{
	struct passthru_dev *ptdev = (struct passthru_dev *) dev->arg;;
	uint16_t virt_bdf = PCI_BDF(dev->bus, dev->slot, dev->func);
	int vector_cnt = dev->msix.table_count;

	printf("ptdev reset msix: 0x%x-%x, vector_cnt=%d.\n",
			virt_bdf, ptdev->phys_bdf, vector_cnt);
	vm_reset_ptdev_msix_info(ctx, virt_bdf, vector_cnt);

	if (ptdev->msix.table_pages) {
		pci_device_unmap_range(ptdev->phys_dev, ptdev->msix.table_pages, ptdev->msix.table_size);
		ptdev->msix.table_pages = NULL;
	}

	/* We passthrough the pages not overlap with MSI-X table to guest,
	 * need to unmap them  when deinit.
	 */
	for (int i = 0; i < 2; i++) {
		if(ptdev->msix_bar_mmio[i].size != 0) {
			if (vm_unmap_ptdev_mmio(ctx, ptdev->sel.bus,
					ptdev->sel.dev, ptdev->sel.func,
					ptdev->msix_bar_mmio[i].gpa,
					ptdev->msix_bar_mmio[i].size,
					ptdev->msix_bar_mmio[i].hpa)) {
				warnx("Failed to  unmap MSI-X BAR pt pages.");
			}
			ptdev->msix_bar_mmio[i].size = 0;
		}
	}
}

static int
cfginitbar(struct vmctx *ctx, struct passthru_dev *ptdev)
{
	int i, error;
	struct pci_vdev *dev;
	struct pci_bar_io bar;
	enum pcibar_type bartype;
	uint64_t base, size;

	dev = ptdev->dev;

	/*
	 * Initialize BAR registers
	 */
	for (i = 0; i <= PCI_BARMAX; i++) {
		bzero(&bar, sizeof(bar));
		bar.sel = ptdev->sel;
		bar.reg = PCIR_BAR(i);

		bar.base = read_config(ptdev->phys_dev, bar.reg, 4);
		bar.length = ptdev->phys_dev->regions[i].size;

		if (PCI_BAR_IO(bar.base)) {
			bartype = PCIBAR_IO;
			base = bar.base & PCIM_BAR_IO_BASE;
		} else {
			switch (bar.base & PCIM_BAR_MEM_TYPE) {
			case PCIM_BAR_MEM_64:
				bartype = PCIBAR_MEM64;
				break;
			default:
				bartype = PCIBAR_MEM32;
				break;
			}
			base = bar.base & PCIM_BAR_MEM_BASE;
		}
		size = bar.length;

		if (bartype != PCIBAR_IO) {
			/* note here PAGE_MASK is 0xFFFFF000 */
			if (((base | size) & ~PAGE_MASK) != 0) {
				warnx("passthru device %x/%x/%x BAR %d: "
				    "base %#lx or size %#lx not page aligned\n",
				    ptdev->sel.bus, ptdev->sel.dev,
				    ptdev->sel.func, i, base, size);
				return -1;
			}
		}

		/* Cache information about the "real" BAR */
		ptdev->bar[i].type = bartype;
		ptdev->bar[i].size = size;
		ptdev->bar[i].addr = base;

		if (size == 0)
			continue;

		/* Allocate the BAR in the guest I/O or MMIO space */
		error = pci_emul_alloc_pbar(dev, i, base, bartype, size);
		if (error)
			return -1;

		/* The MSI-X table needs special handling */
		if (i == ptdev_msix_table_bar(ptdev)) {
			error = init_msix_table(ctx, ptdev, base);
			if (error)
				return -1;
		} else if (bartype != PCIBAR_IO) {
			/* Map the physical BAR in the guest MMIO space */
			error = vm_map_ptdev_mmio(ctx, ptdev->sel.bus,
				ptdev->sel.dev, ptdev->sel.func,
				dev->bar[i].addr, dev->bar[i].size, base);
			if (error)
				return -1;
		}

		/*
		 * 64-bit BAR takes up two slots so skip the next one.
		 */
		if (bartype == PCIBAR_MEM64) {
			i++;
			assert(i <= PCI_BARMAX);
			ptdev->bar[i].type = PCIBAR_MEMHI64;
		}
	}
	return 0;
}

/*
 * return value:
 * -1 : fail
 * >=0: succeed
 *     IRQ_INTX(0): phy dev has no MSI support
 *     IRQ_MSI(1):  phy dev has MSI support
 */
static int
cfginit(struct vmctx *ctx, struct passthru_dev *ptdev, int bus,
	int slot, int func)
{
	int irq_type = IRQ_MSI;
	char reset_path[60];
	int fd;

	bzero(&ptdev->sel, sizeof(struct pcisel));
	ptdev->sel.bus = bus;
	ptdev->sel.dev = slot;
	ptdev->sel.func = func;

	if (cfginit_cap(ctx, ptdev) != 0) {
		warnx("Capability check fails for PCI %x/%x/%x",
		    bus, slot, func);
		return -1;
	}

	/* Check MSI or MSIX capabilities */
	if (ptdev->msi.capoff == 0 && ptdev->msix.capoff == 0) {
		warnx("MSI not supported for PCI %x/%x/%x",
		    bus, slot, func);
		irq_type = IRQ_INTX;
	}

	/* If SOS kernel provides 'reset' entry in sysfs, related dev has some
	 * reset capability, e.g. FLR, or secondary bus reset. We do 2 things:
	 * - reset each dev before passthrough to achieve valid dev state after
	 *   UOS reboot
	 * - refuse to passthrough PCIe dev without any reset capability
	 */
	snprintf(reset_path, 40,
		"/sys/bus/pci/devices/0000:%02x:%02x.%x/reset",
		bus, slot, func);

	fd = open(reset_path, O_WRONLY);
	if (fd >= 0) {
		if (ptdev->need_reset && write(fd, "1", 1) < 0)
			warnx("reset dev %x/%x/%x failed!\n",
			      bus, slot, func);
		close(fd);
	} else if (errno == ENOENT && ptdev->pcie_cap) {
		warnx("No reset capability for PCIe %x/%x/%x, "
				"remove it from ptdev list!!\n",
				bus, slot, func);
		if (!no_reset)
			return -1;
	}

	if (cfginitbar(ctx, ptdev) != 0) {
		warnx("failed to initialize BARs for PCI %x/%x/%x",
		    bus, slot, func);
		return -1;
	} else
		return irq_type;
}

/*
 * convert the error code of pci_system_init in libpciaccess to DM standard
 *
 * pci_system_init in libpciaccess:
 * return zero -> success
 * return positive value -> failure
 *
 * DM standard:
 * return zero -> success
 * return negative value -> failure
 */
static int
native_pci_system_init()
{
	int error;

	error = pci_system_init();

	/*
	 * convert the returned error value to negative since DM only handles
	 * the negative error code
	 */
	return -error;
}

/*
 * return zero on success or non-zero on failure
 */
int
pciaccess_init(void)
{
	int error;

	pthread_mutex_lock(&ref_cnt_mtx);

	if (!pciaccess_ref_cnt) {
		error = native_pci_system_init();
		if (error < 0) {
			warnx("libpciaccess couldn't access PCI system");
			pthread_mutex_unlock(&ref_cnt_mtx);
			return error;
		}
	}
	pciaccess_ref_cnt++;

	pthread_mutex_unlock(&ref_cnt_mtx);

	return 0;	/* success */
}

/*
 * Passthrough device initialization function:
 * - initialize virtual config space
 * - read physical info via libpciaccess
 * - issue related hypercall for passthrough
 * - Do some specific actions:
 *     - enable NHLT for audio pt dev
 *     - emulate INTPIN/INTLINE
 *     - hide INTx link if ptdev support both MSI and INTx to force guest using
 *       MSI, so that mitigate ptdev GSI sharing issue.
 */
static int
passthru_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	int bus, slot, func, error;
	struct passthru_dev *ptdev;
	struct pci_device_iterator *iter;
	struct pci_device *phys_dev;
	char *opt;
	bool keep_gsi = false;
	bool need_reset = false;

	ptdev = NULL;
	error = -EINVAL;

	if (opts == NULL) {
		warnx("Empty passthru options\n");
		return -EINVAL;
	}

	opt = strsep(&opts, ",");
	if (parse_bdf(opt, &bus, &slot, &func, 16) != 0) {
		warnx("Invalid passthru BDF options:%s", opt);
		return -EINVAL;
	}

	while ((opt = strsep(&opts, ",")) != NULL) {
		if (!strncmp(opt, "keep_gsi", 8))
			keep_gsi = true;
		else if (!strncmp(opt, "reset", 5))
			need_reset = true;
		else
			warnx("Invalid passthru options:%s", opt);
	}

	if (vm_assign_ptdev(ctx, bus, slot, func) != 0) {
		warnx("PCI device at %x/%x/%x is not using the pt(4) driver",
			bus, slot, func);
		goto done;
	}

	ptdev = calloc(1, sizeof(struct passthru_dev));
	if (ptdev == NULL) {
		warnx("%s: calloc FAIL!", __func__);
		return -ENOMEM;
	}

	ptdev->phys_bdf = PCI_BDF(bus, slot, func);
	ptdev->need_reset = need_reset;
	update_pt_info(ptdev->phys_bdf);

	error = pciaccess_init();
	if (error < 0)
		return error;

	error = -ENODEV;
	iter = pci_slot_match_iterator_create(NULL);
	while ((phys_dev = pci_device_next(iter)) != NULL) {
		if (phys_dev->bus == bus && phys_dev->dev == slot &&
			phys_dev->func == func) {
			ptdev->phys_dev = phys_dev;
			error = 0;
			break;
		}
	}

	if (error < 0) {
		warnx("No physical PCI device %x:%x.%x!", bus, slot, func);
		return -ENODEV;
	}

	pci_device_probe(ptdev->phys_dev);

	dev->arg = ptdev;
	ptdev->dev = dev;

	/* handle 0x3c~0x3f config space
	 * INTLINE/INTPIN: from emulated configuration space
	 * MINGNT/MAXLAT: from physical configuration space
	 */
	pci_set_cfgdata16(dev, PCIR_MINGNT,
			  read_config(ptdev->phys_dev, PCIR_MINGNT, 2));

#if AUDIO_NHLT_HACK
	/* device specific handling:
	 * audio: enable NHLT ACPI table
	 */
	if (read_config(ptdev->phys_dev, PCIR_VENDOR, 2) == 0x8086 &&
		read_config(ptdev->phys_dev, PCIR_DEVICE, 2) == 0x5a98)
		acpi_table_enable(NHLT_ENTRY_NO);
#endif

	/* initialize config space */
	error = cfginit(ctx, ptdev, bus, slot, func);
	if (error < 0)
		goto done;

	/* If ptdev support MSI/MSIX, stop here to skip virtual INTx setup.
	 * Forge Guest to use MSI/MSIX in this case to mitigate IRQ sharing
	 * issue
	 */
	if (error == IRQ_MSI && !keep_gsi)
		return 0;

	/* Allocates the virq if ptdev only support INTx */
	pci_lintr_request(dev);

	ptdev->phys_pin = read_config(ptdev->phys_dev, PCIR_INTLINE, 1);

	if (ptdev->phys_pin == -1 || ptdev->phys_pin > 256) {
		warnx("ptdev %x/%x/%x has wrong phys_pin %d, likely fail!",
		    bus, slot, func, ptdev->phys_pin);
		goto done;
	}

	error = 0;		/* success */
done:
	if (error) {
		free(ptdev);
		vm_unassign_ptdev(ctx, bus, slot, func);
	}
	return error;
}

void
pciaccess_cleanup(void)
{
	pthread_mutex_lock(&ref_cnt_mtx);
	pciaccess_ref_cnt--;
	if (!pciaccess_ref_cnt)
		pci_system_cleanup();
	pthread_mutex_unlock(&ref_cnt_mtx);
}

static void
passthru_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct passthru_dev *ptdev;
	uint8_t bus, slot, func;
	uint16_t virt_bdf = PCI_BDF(dev->bus, dev->slot, dev->func);
	int i;

	if (!dev->arg) {
		warnx("%s: passthru_dev is NULL", __func__);
		return;
	}

	ptdev = (struct passthru_dev *) dev->arg;
	bus = (ptdev->phys_bdf >> 8) & 0xff;
	slot = (ptdev->phys_bdf & 0xff) >> 3;
	func = ptdev->phys_bdf & 0x7;

	if (ptdev->msix.capoff != 0)
		deinit_msix_table(ctx, dev);
	else if(ptdev->msi.capoff != 0) {
		/* Currently only support 1 vector */
		printf("ptdev reset msi: 0x%x-%x\n", virt_bdf, ptdev->phys_bdf);
		vm_reset_ptdev_msix_info(ctx, virt_bdf, 1);
	}

	printf("vm_reset_ptdev_intx:0x%x-%x, ioapic virpin=%d.\n",
			virt_bdf, ptdev->phys_bdf, dev->lintr.ioapic_irq);
	vm_reset_ptdev_intx_info(ctx, dev->lintr.ioapic_irq, false);

	/* unmap the physical BAR in guest MMIO space */
	for (i = 0; i <= PCI_BARMAX; i++) {

		if (ptdev->bar[i].size == 0 ||
			i == ptdev_msix_table_bar(ptdev) ||
			ptdev->bar[i].type == PCIBAR_IO)
			continue;

		vm_unmap_ptdev_mmio(ctx, ptdev->sel.bus,
				ptdev->sel.dev, ptdev->sel.func,
				dev->bar[i].addr, ptdev->bar[i].size,
				ptdev->bar[i].addr);
	}

	pciaccess_cleanup();
	free(ptdev);
	vm_unassign_ptdev(ctx, bus, slot, func);
}

/* bind pin info for pass-through device */
static void
passthru_bind_irq(struct vmctx *ctx, struct pci_vdev *dev)
{
	struct passthru_dev *ptdev = dev->arg;
	uint16_t virt_bdf = PCI_BDF(dev->bus, dev->slot, dev->func);

	/* No allocated virq indicates ptdev with MSI support, so no need to
	 * setup intx info as MSI is preferred over ioapic intr
	 */
	if (dev->lintr.pin == 0)
		return;

	printf("vm_set_ptdev_intx for %d:%d.%d, ",
		dev->bus, dev->slot, dev->func);
	printf("virt_pin=%d, phys_pin=%d, virt_bdf=0x%x, phys_bdf=0x%x.\n",
		dev->lintr.ioapic_irq, ptdev->phys_pin,
		virt_bdf, ptdev->phys_bdf);

	vm_set_ptdev_intx_info(ctx, virt_bdf, ptdev->phys_bdf,
			       dev->lintr.ioapic_irq, ptdev->phys_pin, false);
}

static int
bar_access(int coff)
{
	if (coff >= PCIR_BAR(0) && coff < PCIR_BAR(PCI_BARMAX + 1))
		return 1;
	else
		return 0;
}

static int
passthru_cfgread(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		 int coff, int bytes, uint32_t *rv)
{
	struct passthru_dev *ptdev;

	ptdev = dev->arg;

	/*
	 * PCI BARs and MSI capability is emulated.
	 */
	if (bar_access(coff))
		return -1;

	/* INTLINE/INTPIN/MINGNT/MAXLAT need to be hacked */
	if (coff >= PCIR_INTLINE && coff <= PCIR_MAXLAT)
		return -1;

	/* Everything else just read from the device's config space */
	*rv = read_config(ptdev->phys_dev, coff, bytes);

	/*
	 * return zero for graphics stolen memory since acrn does not have
	 * support for RMRR
	 */
	if ((PCI_BDF(dev->bus, dev->slot, dev->func) == PCI_BDF_GPU)
		&& (coff == PCIR_GMCH_CTL)) {
		*rv &= ~PCIM_GMCH_CTL_GMS;
	}

	return 0;
}

static int
passthru_cfgwrite(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		  int coff, int bytes, uint32_t val)
{
	struct passthru_dev *ptdev;

	ptdev = dev->arg;

	/*
	 * PCI BARs are emulated
	 */
	if (bar_access(coff))
		return -1;

	/* INTLINE/INTPIN/MINGNT/MAXLAT need to be hacked */
	if (coff >= PCIR_INTLINE && coff <= PCIR_MAXLAT)
		return -1;

	write_config(ptdev->phys_dev, coff, bytes, val);

	return 0;
}

static void
passthru_write(struct vmctx *ctx, int vcpu, struct pci_vdev *dev, int baridx,
	       uint64_t offset, int size, uint64_t value)
{
	struct passthru_dev *ptdev;
	struct iodev_pio_req pio;

	ptdev = dev->arg;

	if (baridx == ptdev_msix_table_bar(ptdev)) {
		msix_table_write(ptdev, offset, size, value);
	} else {
		assert(dev->bar[baridx].type == PCIBAR_IO);
		bzero(&pio, sizeof(struct iodev_pio_req));
		pio.access = IODEV_PIO_WRITE;
		pio.port = ptdev->bar[baridx].addr + offset;
		pio.width = size;
		pio.val = value;

		(void)ioctl(iofd, IODEV_PIO, &pio);
	}
}

static uint64_t
passthru_read(struct vmctx *ctx, int vcpu, struct pci_vdev *dev, int baridx,
	      uint64_t offset, int size)
{
	struct passthru_dev *ptdev;
	struct iodev_pio_req pio;
	uint64_t val;

	ptdev = dev->arg;

	if (baridx == ptdev_msix_table_bar(ptdev)) {
		val = msix_table_read(ptdev, offset, size);
	} else {
		assert(dev->bar[baridx].type == PCIBAR_IO);
		bzero(&pio, sizeof(struct iodev_pio_req));
		pio.access = IODEV_PIO_READ;
		pio.port = ptdev->bar[baridx].addr + offset;
		pio.width = size;
		pio.val = 0;

		(void)ioctl(iofd, IODEV_PIO, &pio);

		val = pio.val;
	}

	return val;
}

static void
write_dsdt_xdci(struct pci_vdev *dev)
{
	printf("write virt-%x:%x.%x in dsdt for XDCI @ 00:15.1\n",
	       dev->bus,
	       dev->slot,
	       dev->func);

	dsdt_line("");
	dsdt_line("Device (XDCI)");
	dsdt_line("{");
	dsdt_line("    Name (_ADR, 0x%04X%04X)", dev->slot, dev->func);
	dsdt_line("    Name (_DDN, \"Broxton XDCI controller\")");
	dsdt_line("    Name (_STR, Unicode (\"Broxton XDCI controller\"))");
	dsdt_line("}");
	dsdt_line("");
}

static void
write_dsdt_hdac(struct pci_vdev *dev)
{
	printf("write virt-%x:%x.%x in dsdt for HDAC @ 00:17.0\n",
	       dev->bus,
	       dev->slot,
	       dev->func);

	/* Need prepare I2C # carefully for all passthrough devices */
	dsdt_line("Device (I2C0)");
	dsdt_line("{");
	dsdt_line("    Name (_ADR, 0x%04X%04X)", dev->slot, dev->func);
	dsdt_line("    Name (_DDN, \"Intel(R) I2C Controller #0\")");
	dsdt_line("    Name (_UID, One)  // _UID: Unique ID");
	dsdt_line("    Name (LINK, \"\\\\_SB.PCI0.I2C0\")");

	dsdt_line("    Name (RBUF, ResourceTemplate ()");
	dsdt_line("    {");
	dsdt_line("    })");
	dsdt_line("    Name (IC4S, 0x00061A80)");
	dsdt_line("    Name (_DSD, Package (0x02)");
	dsdt_line("    {");
	dsdt_line("        ToUUID (\"daffd814-6eba-4d8c-8a91-bc9bbf4aa301\")"
				" ,");
	dsdt_line("        Package (0x01)");
	dsdt_line("        {");
	dsdt_line("            Package (0x02)");
	dsdt_line("            {");
	dsdt_line("                \"clock-frequency\", ");
	dsdt_line("                IC4S");
	dsdt_line("            }");
	dsdt_line("        }");
	dsdt_line("    })");
	dsdt_line("    Method (FMCN, 0, Serialized)");
	dsdt_line("    {");
	dsdt_line("        Name (PKG, Package (0x03)");
	dsdt_line("        {");
	dsdt_line("            0x64, ");
	dsdt_line("            0xD6, ");
	dsdt_line("            0x1C");
	dsdt_line("        })");
	dsdt_line("        Return (PKG)");
	dsdt_line("    }");
	dsdt_line("");
	dsdt_line("    Method (FPCN, 0, Serialized)");
	dsdt_line("    {");
	dsdt_line("        Name (PKG, Package (0x03)");
	dsdt_line("        {");
	dsdt_line("            0x26, ");
	dsdt_line("            0x50, ");
	dsdt_line("            0x0C");
	dsdt_line("        })");
	dsdt_line("        Return (PKG)");
	dsdt_line("    }");
	dsdt_line("");
	dsdt_line("    Method (HSCN, 0, Serialized)");
	dsdt_line("    {");
	dsdt_line("        Name (PKG, Package (0x03)");
	dsdt_line("        {");
	dsdt_line("            0x05, ");
	dsdt_line("            0x18, ");
	dsdt_line("            0x0C");
	dsdt_line("        })");
	dsdt_line("        Return (PKG)");
	dsdt_line("    }");
	dsdt_line("");
	dsdt_line("    Method (SSCN, 0, Serialized)");
	dsdt_line("    {");
	dsdt_line("        Name (PKG, Package (0x03)");
	dsdt_line("        {");
	dsdt_line("            0x0244, ");
	dsdt_line("            0x02DA, ");
	dsdt_line("            0x1C");
	dsdt_line("        })");
	dsdt_line("        Return (PKG)");
	dsdt_line("    }");
	dsdt_line("");
	dsdt_line("    Method (_CRS, 0, NotSerialized)");
	dsdt_line("    {");
	dsdt_line("        Return (RBUF)");
	dsdt_line("    }");

	dsdt_line("    Device (HDAC)");
	dsdt_line("    {");
	dsdt_line("        Name (_HID, \"INT34C3\")  // _HID: Hardware ID");
	dsdt_line("        Name (_CID, \"INT34C3\")  // _CID: Compatible ID");
	dsdt_line("        Name (_DDN, \"Intel(R) Smart Sound Technology "
			"Audio Codec\")  // _DDN: DOS Device Name");
	dsdt_line("        Name (_UID, One)  // _UID: Unique ID");
	dsdt_line("        Method (_INI, 0, NotSerialized)");
	dsdt_line("        {");
	dsdt_line("        }");
	dsdt_line("");
	dsdt_line("        Method (_CRS, 0, NotSerialized)");
	dsdt_line("        {");
	dsdt_line("            Name (SBFB, ResourceTemplate ()");
	dsdt_line("            {");
	dsdt_line("                I2cSerialBusV2 (0x006C, "
					"ControllerInitiated, 0x00061A80,");
	dsdt_line("                    AddressingMode7Bit, "
						"\"\\\\_SB.PCI0.I2C0\",");
	dsdt_line("                    0x00, ResourceConsumer, , Exclusive,");
	dsdt_line("                    )");
	dsdt_line("            })");
	dsdt_line("            Name (SBFI, ResourceTemplate ()");
	dsdt_line("            {");
	dsdt_line("            })");
	dsdt_line("            Return (ConcatenateResTemplate (SBFB, SBFI))");
	dsdt_line("        }");
	dsdt_line("");
	dsdt_line("        Method (_STA, 0, NotSerialized)  // _STA: Status");
	dsdt_line("        {");
	dsdt_line("            Return (0x0F)");
	dsdt_line("        }");
	dsdt_line("    }");
	dsdt_line("");
	dsdt_line("}");
}

static void
write_dsdt_hdas(struct pci_vdev *dev)
{
	printf("write virt-%x:%x.%x in dsdt for HDAS @ 00:e.0\n",
	       dev->bus,
	       dev->slot,
	       dev->func);

	dsdt_line("Name (ADFM, 0x2A)");
	dsdt_line("Name (ADPM, Zero)");
	dsdt_line("Name (AG1L, Zero)");
	dsdt_line("Name (AG1H, Zero)");
	dsdt_line("Name (AG2L, Zero)");
	dsdt_line("Name (AG2H, Zero)");
	dsdt_line("Name (AG3L, Zero)");
	dsdt_line("Name (AG3H, Zero)");
	dsdt_line("Method (ADBG, 1, Serialized)");
	dsdt_line("{");
	dsdt_line("    Return (Zero)");
	dsdt_line("}");
	dsdt_line("Device (HDAS)");
	dsdt_line("{");
	dsdt_line("    Name (_ADR, 0x%04X%04X)", dev->slot, dev->func);
	dsdt_line("    OperationRegion (HDAR, PCI_Config, Zero, 0x0100)");
	dsdt_line("    Field (HDAR, ByteAcc, NoLock, Preserve)");
	dsdt_line("    {");
	dsdt_line("	VDID,   32, ");
	dsdt_line("	Offset (0x48), ");
	dsdt_line("	    ,   6, ");
	dsdt_line("	MBCG,   1, ");
	dsdt_line("	Offset (0x54), ");
	dsdt_line("	Offset (0x55), ");
	dsdt_line("	PMEE,   1, ");
	dsdt_line("	    ,   6, ");
	dsdt_line("	PMES,   1");
	dsdt_line("    }");
	dsdt_line("");
	dsdt_line("    Name (NBUF, ResourceTemplate ()");
	dsdt_line("    {");
	dsdt_line("	QWordMemory (ResourceConsumer, PosDecode, MinNotFixed,"
				" MaxNotFixed, NonCacheable, ReadOnly,");
	dsdt_line("	    0x0000000000000000, // Granularity");
	dsdt_line("	    0x00000000000F2800, // Range Minimum");
	dsdt_line("	    0x%08X, 		// Range Maximum",
				0xF2800 + audio_nhlt_len -1);
	dsdt_line("	    0x0000000000000000, // Translation Offset");
	dsdt_line("	    0x%08X, 		// Length", audio_nhlt_len);
	dsdt_line("	    ,, _Y06, AddressRangeACPI, TypeStatic)");
	dsdt_line("    })");
	dsdt_line("    Name (_S0W, 0x03)  // _S0W: S0 Device Wake State");
	dsdt_line("    Method (_DSW, 3, NotSerialized)");
	dsdt_line("    {");
	dsdt_line("	PMEE = Arg0");
	dsdt_line("    }");
	dsdt_line("");
	dsdt_line("    Name (_PRW, Package (0x02)");
	dsdt_line("    {");
	dsdt_line("	0x0E, ");
	dsdt_line("	0x03");
	dsdt_line("    })");
	dsdt_line("    Method (_PS0, 0, Serialized)");
	dsdt_line("    {");
	dsdt_line("	ADBG (\"HD-A Ctrlr D0\")");
	dsdt_line("    }");
	dsdt_line("");
	dsdt_line("    Method (_PS3, 0, Serialized)");
	dsdt_line("    {");
	dsdt_line("	ADBG (\"HD-A Ctrlr D3\")");
	dsdt_line("    }");
	dsdt_line("");
	dsdt_line("    Method (_INI, 0, NotSerialized)");
	dsdt_line("    {");
	dsdt_line("	ADBG (\"HDAS _INI\")");
	dsdt_line("    }");
	dsdt_line("");
	dsdt_line("    Method (_DSM, 4, Serialized)");
	dsdt_line("    {");
	dsdt_line("	ADBG (\"HDAS _DSM\")");
	dsdt_line("	If ((Arg0 == ToUUID ("
				"\"a69f886e-6ceb-4594-a41f-7b5dce24c553\")))");
	dsdt_line("	{");
	dsdt_line("	    Switch (ToInteger (Arg2))");
	dsdt_line("	    {");
	dsdt_line("		Case (Zero)");
	dsdt_line("		{");
	dsdt_line("		    Return (Buffer (One)");
	dsdt_line("		    {");
	dsdt_line("			 0x0F");
	dsdt_line("		    })");
	dsdt_line("		}");
	dsdt_line("		Case (One)");
	dsdt_line("		{");
	dsdt_line("		    ADBG (\"_DSM Fun 1 NHLT\")");
	dsdt_line("		    Return (NBUF)");
	dsdt_line("		}");
	dsdt_line("		Case (0x02)");
	dsdt_line("		{");
	dsdt_line("		    ADBG (\"_DSM Fun 2 FMSK\")");
	dsdt_line("		    Return (ADFM)");
	dsdt_line("		}");
	dsdt_line("		Case (0x03)");
	dsdt_line("		{");
	dsdt_line("		    ADBG (\"_DSM Fun 3 PPMS\")");
	dsdt_line("		    If ((Arg3 == ToUUID ("
				"\"b489c2de-0f96-42e1-8a2d-c25b5091ee49\")))");
	dsdt_line("		    {");
	dsdt_line("			Return ((ADPM & One))");
	dsdt_line("		    }");
	dsdt_line("");
	dsdt_line("		    If ((Arg3 == ToUUID ("
				"\"e1284052-8664-4fe4-a353-3878f72704c3\")))");
	dsdt_line("		    {");
	dsdt_line("			Return ((ADPM & 0x02))");
	dsdt_line("		    }");
	dsdt_line("");
	dsdt_line("		    If ((Arg3 == ToUUID ("
				"\"7c708106-3aff-40fe-88be-8c999b3f7445\")))");
	dsdt_line("		    {");
	dsdt_line("			Return ((ADPM & 0x04))");
	dsdt_line("		    }");
	dsdt_line("");
	dsdt_line("		    If ((Arg3 == ToUUID ("
				"\"e0e018a8-3550-4b54-a8d0-a8e05d0fcba2\")))");
	dsdt_line("		    {");
	dsdt_line("			Return ((ADPM & 0x08))");
	dsdt_line("		    }");
	dsdt_line("");
	dsdt_line("		    If ((Arg3 == ToUUID ("
				"\"202badb5-8870-4290-b536-f2380c63f55d\")))");
	dsdt_line("		    {");
	dsdt_line("			Return ((ADPM & 0x10))");
	dsdt_line("		    }");
	dsdt_line("");
	dsdt_line("		    If ((Arg3 == ToUUID ("
				"\"eb3fea76-394b-495d-a14d-8425092d5cb7\")))");
	dsdt_line("		    {");
	dsdt_line("			Return ((ADPM & 0x20))");
	dsdt_line("		    }");
	dsdt_line("");
	dsdt_line("		    If ((Arg3 == ToUUID ("
				"\"f1c69181-329a-45f0-8eef-d8bddf81e036\")))");
	dsdt_line("		    {");
	dsdt_line("			Return ((ADPM & 0x40))");
	dsdt_line("		    }");
	dsdt_line("");
	dsdt_line("		    If ((Arg3 == ToUUID ("
				"\"b3573eff-6441-4a75-91f7-4281eec4597d\")))");
	dsdt_line("		    {");
	dsdt_line("			Return ((ADPM & 0x80))");
	dsdt_line("		    }");
	dsdt_line("");
	dsdt_line("		    If ((Arg3 == ToUUID ("
				"\"ec774fa9-28d3-424a-90e4-69f984f1eeb7\")))");
	dsdt_line("		    {");
	dsdt_line("			Return ((ADPM & 0x0100))");
	dsdt_line("		    }");
	dsdt_line("");
	dsdt_line("		    If ((Arg3 == ToUUID ("
				"\"f101fef0-ff5a-4ad4-8710-43592a6f7948\")))");
	dsdt_line("		    {");
	dsdt_line("			Return ((ADPM & 0x0200))");
	dsdt_line("		    }");
	dsdt_line("");
	dsdt_line("		    If ((Arg3 == ToUUID ("
				"\"f3578986-4400-4adf-ae7e-cd433cd3f26e\")))");
	dsdt_line("		    {");
	dsdt_line("			Return ((ADPM & 0x0400))");
	dsdt_line("		    }");
	dsdt_line("");
	dsdt_line("		    If ((Arg3 == ToUUID ("
				"\"13b5e4d7-a91a-4059-8290-605b01ccb650\")))");
	dsdt_line("		    {");
	dsdt_line("			Return ((ADPM & 0x0800))");
	dsdt_line("		    }");
	dsdt_line("");
	dsdt_line("		    If ((Arg3 == ACCG (AG1L, AG1H)))");
	dsdt_line("		    {");
	dsdt_line("			Return ((ADPM & 0x20000000))");
	dsdt_line("		    }");
	dsdt_line("");
	dsdt_line("		    If ((Arg3 == ACCG (AG2L, AG2H)))");
	dsdt_line("		    {");
	dsdt_line("			Return ((ADPM & 0x40000000))");
	dsdt_line("		    }");
	dsdt_line("");
	dsdt_line("		    If ((Arg3 == ACCG (AG3L, AG3H)))");
	dsdt_line("		    {");
	dsdt_line("			Return ((ADPM & 0x80000000))");
	dsdt_line("		    }");
	dsdt_line("");
	dsdt_line("		    Return (Zero)");
	dsdt_line("		}");
	dsdt_line("		Default");
	dsdt_line("		{");
	dsdt_line("		    ADBG (\"_DSM Fun NOK\")");
	dsdt_line("		    Return (Buffer (One)");
	dsdt_line("		    {");
	dsdt_line("			 0x00");
	dsdt_line("		    })");
	dsdt_line("		}");
	dsdt_line("");
	dsdt_line("	    }");
	dsdt_line("	}");
	dsdt_line("");
	dsdt_line("	ADBG (\"_DSM UUID NOK\")");
	dsdt_line("	Return (Buffer (One)");
	dsdt_line("	{");
	dsdt_line("	     0x00");
	dsdt_line("	})");
	dsdt_line("    }");
	dsdt_line("");
	dsdt_line("    Method (ACCG, 2, Serialized)");
	dsdt_line("    {");
	dsdt_line("	Name (GBUF, Buffer (0x10){})");
	dsdt_line("	Concatenate (Arg0, Arg1, GBUF)");
	dsdt_line("	Return (GBUF) /* \\_SB_.PCI0.HDAS.ACCG.GBUF */");
	dsdt_line("    }");
	dsdt_line("}");
}

static void
write_dsdt_ipu_i2c(struct pci_vdev *dev)
{
	printf("write virt-%x:%x.%x in dsdt for ipu's i2c-bus @ 00:16.0\n",
			dev->bus, dev->slot, dev->func);

	/* physical I2C 0:16.0 */
	dsdt_line("Device (I2C1)");
	dsdt_line("{");
	dsdt_line("    Name (_ADR, 0x%04X%04X)", dev->slot, dev->func);
	dsdt_line("    Name (_DDN, \"Intel(R) I2C Controller #1\")");
	dsdt_line("    Name (_UID, One)");
	dsdt_line("    Name (LINK, \"\\\\_SB.PCI0.I2C1\")");
	dsdt_line("    Name (RBUF, ResourceTemplate ()");
	dsdt_line("    {");
	dsdt_line("    })");
	dsdt_line("    Name (IC0S, 0x00061A80)");
	dsdt_line("    Name (_DSD, Package (0x02)");
	dsdt_line("    {");
	dsdt_line("        ToUUID (\"daffd814-6eba-4d8c-8a91-bc9bbf4aa301\")"
				" ,");
	dsdt_line("        Package (0x01)");
	dsdt_line("        {");
	dsdt_line("            Package (0x02)");
	dsdt_line("            {");
	dsdt_line("                \"clock-frequency\", ");
	dsdt_line("                IC0S");
	dsdt_line("            }");
	dsdt_line("        }");
	dsdt_line("    })");

	dsdt_line("    Method (FMCN, 0, Serialized)");
	dsdt_line("    {");
	dsdt_line("        Name (PKG, Package (0x03)");
	dsdt_line("        {");
	dsdt_line("            0x64, ");
	dsdt_line("            0xD6, ");
	dsdt_line("            0x1C");
	dsdt_line("        })");
	dsdt_line("        Return (PKG)");
	dsdt_line("    }");
	dsdt_line("");

	dsdt_line("    Method (FPCN, 0, Serialized)");
	dsdt_line("    {");
	dsdt_line("        Name (PKG, Package (0x03)");
	dsdt_line("        {");
	dsdt_line("            0x26, ");
	dsdt_line("            0x50, ");
	dsdt_line("            0x0C");
	dsdt_line("        })");
	dsdt_line("        Return (PKG)");
	dsdt_line("    }");
	dsdt_line("");

	dsdt_line("    Method (HSCN, 0, Serialized)");
	dsdt_line("    {");
	dsdt_line("        Name (PKG, Package (0x03)");
	dsdt_line("        {");
	dsdt_line("            0x05, ");
	dsdt_line("            0x18, ");
	dsdt_line("            0x0C");
	dsdt_line("        })");
	dsdt_line("        Return (PKG)");
	dsdt_line("    }");
	dsdt_line("");

	dsdt_line("    Method (SSCN, 0, Serialized)");
	dsdt_line("    {");
	dsdt_line("        Name (PKG, Package (0x03)");
	dsdt_line("        {");
	dsdt_line("            0x0244, ");
	dsdt_line("            0x02DA, ");
	dsdt_line("            0x1C");
	dsdt_line("        })");
	dsdt_line("        Return (PKG)");
	dsdt_line("    }");
	dsdt_line("");

	dsdt_line("    Method (_CRS, 0, NotSerialized)");
	dsdt_line("    {");
	dsdt_line("        Return (RBUF)");
	dsdt_line("    }");
	dsdt_line("");

	/* CAM1 */
	dsdt_line("    Device (CAM1)");
	dsdt_line("    {");
	dsdt_line("        Name (_ADR, Zero)  // _ADR: Address");
	dsdt_line("        Name (_HID, \"ADV7481A\")  // _HID: Hardware ID");
	dsdt_line("        Name (_CID, \"ADV7481A\")  // _CID: Compatible ID");
	dsdt_line("        Name (_UID, One)  // _UID: Unique ID");

	dsdt_line("        Method (_CRS, 0, Serialized)");
	dsdt_line("        {");
	dsdt_line("            Name (SBUF, ResourceTemplate ()");
	dsdt_line("            {");
	dsdt_line("                GpioIo (Exclusive, PullDefault, 0x0000, "
					"0x0000, IoRestrictionInputOnly,");
	dsdt_line("                    \"\\\\_SB.GPO0\", 0x00, "
					"ResourceConsumer, ,");
	dsdt_line("                    )");
	dsdt_line("                    {   // Pin list");
	dsdt_line("                        0x001E");
	dsdt_line("                    }");
	dsdt_line("                I2cSerialBusV2 (0x0070, "
					"ControllerInitiated, 0x00061A80,");
	dsdt_line("                    AddressingMode7Bit, "
						"\"\\\\_SB.PCI0.I2C1\",");
	dsdt_line("                    0x00, ResourceConsumer, , Exclusive,");
	dsdt_line("                    )");
	dsdt_line("            })");
	dsdt_line("            Return (SBUF)");
	dsdt_line("        }");

	dsdt_line("        Method (_DSM, 4, NotSerialized)");
	dsdt_line("        {");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"377ba76a-f390-4aff-ab38-9b1bf33a3015\")))");
	dsdt_line("            {");
	dsdt_line("                Return (\"ADV7481A\")");
	dsdt_line("            }");
	dsdt_line("");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"ea3b7bd8-e09b-4239-ad6e-ed525f3f26ab\")))");
	dsdt_line("            {");
	dsdt_line("                Return (0x40)");
	dsdt_line("            }");
	dsdt_line("");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"8dbe2651-70c1-4c6f-ac87-a37cb46e4af6\")))");
	dsdt_line("            {");
	dsdt_line("                Return (0xFF)");
	dsdt_line("            }");
	dsdt_line("");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"26257549-9271-4ca4-bb43-c4899d5a4881\")))");
	dsdt_line("            {");
	dsdt_line("                If (Arg2 == One)");
	dsdt_line("                {");
	dsdt_line("                    Return (0x02)");
	dsdt_line("                }");
	dsdt_line("                If (Arg2 == 0x02)");
	dsdt_line("                {");
	dsdt_line("                    Return (0x02001000)");
	dsdt_line("                }");
	dsdt_line("                If (Arg2 == 0x03)");
	dsdt_line("                {");
	dsdt_line("                    Return (0x02000E01)");
	dsdt_line("                }");
	dsdt_line("            }");
	dsdt_line("            Return (Zero)");
	dsdt_line("        }");
	dsdt_line("    }");
	dsdt_line("");

	/* CAM2 */
	dsdt_line("    Device (CAM2)");
	dsdt_line("    {");
	dsdt_line("        Name (_ADR, Zero)  // _ADR: Address");
	dsdt_line("        Name (_HID, \"ADV7481B\")  // _HID: Hardware ID");
	dsdt_line("        Name (_CID, \"ADV7481B\")  // _CID: Compatible ID");
	dsdt_line("        Name (_UID, One)  // _UID: Unique ID");

	dsdt_line("        Method (_CRS, 0, Serialized)");
	dsdt_line("        {");
	dsdt_line("            Name (SBUF, ResourceTemplate ()");
	dsdt_line("            {");
	dsdt_line("                GpioIo (Exclusive, PullDefault, 0x0000, "
					"0x0000, IoRestrictionInputOnly,");
	dsdt_line("                    \"\\\\_SB.GPO0\", 0x00, "
					"ResourceConsumer, ,");
	dsdt_line("                    )");
	dsdt_line("                    {   // Pin list");
	dsdt_line("                        0x001E");
	dsdt_line("                    }");
	dsdt_line("                I2cSerialBusV2 (0x0071, "
					"ControllerInitiated, 0x00061A80,");
	dsdt_line("                    AddressingMode7Bit, "
						"\"\\\\_SB.PCI0.I2C1\",");
	dsdt_line("                    0x00, ResourceConsumer, , Exclusive,");
	dsdt_line("                    )");
	dsdt_line("            })");
	dsdt_line("            Return (SBUF)");
	dsdt_line("        }");

	dsdt_line("        Method (_DSM, 4, NotSerialized) ");
	dsdt_line("        {");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"377ba76a-f390-4aff-ab38-9b1bf33a3015\")))");
	dsdt_line("            {");
	dsdt_line("                Return (\"ADV7481B\")");
	dsdt_line("            }");
	dsdt_line("");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"ea3b7bd8-e09b-4239-ad6e-ed525f3f26ab\")))");
	dsdt_line("            {");
	dsdt_line("                Return (0x14)");
	dsdt_line("            }");
	dsdt_line("");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"8dbe2651-70c1-4c6f-ac87-a37cb46e4af6\")))");
	dsdt_line("            {");
	dsdt_line("                Return (0xFF)");
	dsdt_line("            }");
	dsdt_line("");
	dsdt_line("            If ((Arg0 == ToUUID ("
				"\"26257549-9271-4ca4-bb43-c4899d5a4881\")))");
	dsdt_line("            {");
	dsdt_line("                If (Arg2 == One)");
	dsdt_line("                {");
	dsdt_line("                    Return (0x02)");
	dsdt_line("                }");
	dsdt_line("                If (Arg2 == 0x02)");
	dsdt_line("                {");
	dsdt_line("                    Return (0x02001000)");
	dsdt_line("                }");
	dsdt_line("                If (Arg2 == 0x03)");
	dsdt_line("                {");
	dsdt_line("                    Return (0x02000E01)");
	dsdt_line("                }");
	dsdt_line("            }");
	dsdt_line("            Return (Zero)");
	dsdt_line("        }");
	dsdt_line("    }");
	dsdt_line("");

	dsdt_line("}");
}

static void
write_dsdt_urt1(struct pci_vdev *dev)
{
	printf("write virt-%x:%x.%x in dsdt for URT1 @ 00:18.0\n",
	       dev->bus,
	       dev->slot,
	       dev->func);
	dsdt_line("Device (URT1)");
	dsdt_line("{");
	dsdt_line("    Name (_ADR, 0x%04X%04X)", dev->slot, dev->func);
	dsdt_line("    Name (_DDN, \"Intel(R) HS-UART Controller #1\")");
	dsdt_line("    Name (_UID, One)");
	dsdt_line("    Name (RBUF, ResourceTemplate ()");
	dsdt_line("    {");
	dsdt_line("    })");
	dsdt_line("    Method (_CRS, 0, NotSerialized)");
	dsdt_line("    {");
	dsdt_line("        Return (RBUF)");
	dsdt_line("    }");
	dsdt_line("}");
}

static void
passthru_write_dsdt(struct pci_vdev *dev)
{
	struct passthru_dev *ptdev = (struct passthru_dev *) dev->arg;
	uint32_t vendor = 0, device = 0;

	vendor = read_config(ptdev->phys_dev, PCIR_VENDOR, 2);

	if (vendor != 0x8086)
		return;

	device = read_config(ptdev->phys_dev, PCIR_DEVICE, 2);

	/* Provides ACPI extra info */
	if (device == 0x5aaa)
		/* XDCI @ 00:15.1 to enable ADB */
		write_dsdt_xdci(dev);
	else if (device == 0x5ab4)
		/* HDAC @ 00:17.0 as codec */
		write_dsdt_hdac(dev);
	else if (device == 0x5a98)
		/* HDAS @ 00:e.0 */
		write_dsdt_hdas(dev);
	else if (device == 0x5aac)
		/* i2c @ 00:16.0 for ipu */
		write_dsdt_ipu_i2c(dev);
	else if (device == 0x5abc)
		/* URT1 @ 00:18.0 for bluetooth*/
		write_dsdt_urt1(dev);

}

struct pci_vdev_ops passthru = {
	.class_name		= "passthru",
	.vdev_init		= passthru_init,
	.vdev_deinit		= passthru_deinit,
	.vdev_cfgwrite		= passthru_cfgwrite,
	.vdev_cfgread		= passthru_cfgread,
	.vdev_barwrite		= passthru_write,
	.vdev_barread		= passthru_read,
	.vdev_phys_access	= passthru_bind_irq,
	.vdev_write_dsdt	= passthru_write_dsdt,
};
DEFINE_PCI_DEVTYPE(passthru);
