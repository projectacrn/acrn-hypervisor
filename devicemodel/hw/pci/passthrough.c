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
#include "dm.h"


/* Some audio drivers get topology data from ACPI NHLT table.
 * For such drivers, we need to copy the host NHLT table to make it
 * available to the Guest OS. Most audio drivers don't need this by
 * default, when that's the case setting this macro to 0 will avoid
 * unexpected failures.
 * The cAVS audio needs this however, so we enable this feature.
 */
#define AUDIO_NHLT_HACK 1

#define PCI_BDF_GPU			0x00000010	/* 00:02.0 */

/* Reserved [0x DF000000, 0x E0000000] 16M in e820 table for GVT
 * [0xDB000000, 0xDF000000) 64M, DSM, used by native GOP and gfx driver
 * for GVT-g use:
 * [0xDF000000, 0xDF800000)  8M, GOP FB, used OvmfPkg/GvtGopDxe for 1080p@30
 * [0xDFFFD000, 0xDFFFF000)  8K, OpRegion, used by GvtGopDxe and GVT-g
 * [0xDFFFF000, 0XE0000000)  4K, Reserved, not used
 * for GVT-d use:
 * [0xDFFFC000, 0xDFFFE000)  8K, OpRegion, used by native GOP and gfx driver
 * [0xDFFFE000, 0XE0000000]  8K, Extended OpRegion, store raw VBT
 * 
 * OpRegion: 8KB(0x2000)
 * [ OpRegion Header      ] Offset: 0x0
 * [ Mailbox #1: ACPI     ] Offset: 0x100
 * [ Mailbox #2: SWSCI    ] Offset: 0x200
 * [ Mailbox #3: ASLE     ] Offset: 0x300
 * [ Mailbox #4: VBT      ] Offset: 0x400
 * [ Mailbox #5: ASLE EXT ] Offset: 0x1C00
 * Extended OpRegion: 8KB(0x2000)
 * [ Raw VBT              ] Offset: 0x0
 * If VBT <= 6KB, stores in Mailbox #4
 * If VBT > 6KB, stores in Extended OpRegion
 * ASLE.rvda stores the location of VBT.
 * For OpRegion 2.1+: ASLE.rvda = offset to OpRegion base address
 * For OpRegion 2.0:  ASLE.rvda = physical address, not support currently
 */
#define GPU_DSM_GPA  			0xDB000000
#define GPU_DSM_SIZE			0x4000000
#define GPU_OPREGION_GPA  		0xDFFFC000
#define GPU_OPREGION_SIZE		0x4000

extern uint64_t audio_nhlt_len;

/* reference count for libpciaccess init/deinit */
static int pciaccess_ref_cnt;
static pthread_mutex_t ref_cnt_mtx = PTHREAD_MUTEX_INITIALIZER;

uint32_t dsm_start_hpa = 0;
uint32_t opregion_start_hpa = 0;

struct passthru_dev {
	struct pci_vdev *dev;
	struct pcibar bar[PCI_BARMAX + 1];
	struct {
		int		capoff;
	} msi;
	struct {
		int		capoff;
	} msix;
	struct {
		int 		capoff;
	} pmcap;
	bool pcie_cap;
	struct pcisel sel;
	int phys_pin;
	uint16_t phys_bdf;
	struct pci_device *phys_dev;
	/* Options for passthrough device:
	 *   need_reset - reset dev before passthrough
	 */
	bool need_reset;
	bool d3hot_reset;
	bool (*has_virt_pcicfg_regs)(int offset);
};

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

static int
cfginit_cap(struct vmctx *ctx, struct passthru_dev *ptdev)
{
	int ptr, cap, sts;
	struct pci_device *phys_dev = ptdev->phys_dev;

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
				ptdev->msi.capoff = ptr;
			} else if (cap == PCIY_MSIX) {
				ptdev->msix.capoff = ptr;
			} else if (cap == PCIY_EXPRESS) {
				ptdev->pcie_cap = true;
			} else if (cap == PCIY_PMG)
				ptdev->pmcap.capoff = ptr;

			ptr = read_config(phys_dev, ptr + PCICAP_NEXTPTR, 1);
		}
	}

	return 0;
}

static int
passthru_set_power_state(struct passthru_dev *ptdev, uint16_t dpsts) {
	int ret = -1;
	uint32_t val;

	dpsts &= PCIM_PSTAT_DMASK;
	if (ptdev->pmcap.capoff != 0) {
		val = read_config(ptdev->phys_dev,
				ptdev->pmcap.capoff + PCIR_POWER_STATUS, 2);
		val = (val & ~PCIM_PSTAT_DMASK) | dpsts;

		write_config(ptdev->phys_dev,
				ptdev->pmcap.capoff + PCIR_POWER_STATUS, 2, val);

		ret = 0;
	}
	return ret;
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
cfginitbar(struct vmctx *ctx, struct passthru_dev *ptdev)
{
	int i, error;
	struct pci_vdev *dev;
	struct pci_bar_io bar;
	enum pcibar_type bartype;
	uint64_t base, size;
	uint32_t vbar_lo32;

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
			if ((base & ~PAGE_MASK) != 0) {
				warnx("passthru device %x/%x/%x BAR %d: "
				    "base %#lx not page aligned\n",
				    ptdev->sel.bus, ptdev->sel.dev,
				    ptdev->sel.func, i, base);
				return -1;
			}
			/* roundup to PAGE_SIZE for bar size */
			if ((size & ~PAGE_MASK) != 0) {
				warnx("passthru device %x/%x/%x BAR %d: "
					"size[%lx] is expanded to page aligned [%lx]\n",
				    ptdev->sel.bus, ptdev->sel.dev,
				    ptdev->sel.func, i, size, roundup2(size, PAGE_SIZE));
				size = roundup2(size, PAGE_SIZE);
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

		/*
		 * For pass-thru devices,
		 * set the bar prefetchable property the same as physical bar.
		 *
		 * the pci bar prefetchable property has set by pci_emul_alloc_pbar,
		 * here, override the prefetchable property according to the physical bar.
		 */
		if (bartype == PCIBAR_MEM32 ||  bartype == PCIBAR_MEM64) {
			vbar_lo32 = pci_get_cfgdata32(dev, PCIR_BAR(i));

			if (bar.base & PCIM_BAR_MEM_PREFETCH)
				vbar_lo32 |= PCIM_BAR_MEM_PREFETCH;
			else
				vbar_lo32 &= ~PCIM_BAR_MEM_PREFETCH;

			pci_set_cfgdata32(dev, PCIR_BAR(i), vbar_lo32);
		}

		/*
		 * 64-bit BAR takes up two slots so skip the next one.
		 */
		if (bartype == PCIBAR_MEM64) {
			i++;
			if (i > PCI_BARMAX) {
				warnx("BAR count out of range\n");
				return -1;
			}

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
	if (ptdev->need_reset) {
		snprintf(reset_path, 40,
			"/sys/bus/pci/devices/0000:%02x:%02x.%x/reset",
			bus, slot, func);

		fd = open(reset_path, O_WRONLY);
		if (fd >= 0) {
			if (write(fd, "1", 1) < 0)
				warnx("reset dev %x/%x/%x failed!\n",
				      bus, slot, func);
			close(fd);
		}
	}

	if (ptdev->d3hot_reset) {
		if ((passthru_set_power_state(ptdev, PCIM_PSTAT_D3) != 0) ||
				passthru_set_power_state(ptdev, PCIM_PSTAT_D0) != 0)
			warnx("ptdev %x/%x/%x do d3hot_reset failed!\n", bus, slot, func);
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

static bool
has_virt_pcicfg_regs_on_ehl_gpu(int offset)
{
	return ((offset == PCIR_GEN11_BDSM_DW0) || (offset == PCIR_GEN11_BDSM_DW1) ||
		(offset == PCIR_ASLS_CTL));
}

static bool
has_virt_pcicfg_regs_on_def_gpu(int offset)
{
	return ((offset == PCIR_BDSM) || (offset == PCIR_ASLS_CTL));
}

/*
 * passthrough GPU DSM(Data Stolen Memory) and Opregion to guest
 */
void
passthru_gpu_dsm_opregion(struct vmctx *ctx, struct passthru_dev *ptdev,
			struct acrn_assign_pcidev *pcidev, uint16_t device)
{
	uint32_t opregion_phys, dsm_mask_val;

	switch (device) {
	case INTEL_ELKHARTLAKE:
	case INTEL_TIGERLAKE:
		/* BDSM register has 64 bits.
		 * bits 63:20 contains the base address of stolen memory
		 */
		dsm_start_hpa = read_config(ptdev->phys_dev, PCIR_GEN11_BDSM_DW0, 4);
		dsm_mask_val = dsm_start_hpa & ~PCIM_BDSM_MASK;
		dsm_start_hpa &= PCIM_BDSM_MASK;
		dsm_start_hpa |= (uint64_t)read_config(ptdev->phys_dev, PCIR_GEN11_BDSM_DW1, 4) << 32;

		pci_set_cfgdata32(ptdev->dev, PCIR_GEN11_BDSM_DW0, GPU_DSM_GPA | dsm_mask_val);
		/* write 0 to high 32-bits of BDSM on EHL platform */
		pci_set_cfgdata32(ptdev->dev, PCIR_GEN11_BDSM_DW1, 0);

		ptdev->has_virt_pcicfg_regs = &has_virt_pcicfg_regs_on_ehl_gpu;
		break;
	/* If on default platforms, such as KBL,WHL  */
	default:
		/* bits 31:20 contains the base address of stolen memory */
		dsm_start_hpa = read_config(ptdev->phys_dev, PCIR_BDSM, 4);
		dsm_mask_val = dsm_start_hpa & ~PCIM_BDSM_MASK;
		dsm_start_hpa &= PCIM_BDSM_MASK;

		pci_set_cfgdata32(ptdev->dev, PCIR_BDSM, GPU_DSM_GPA | dsm_mask_val);

		ptdev->has_virt_pcicfg_regs = &has_virt_pcicfg_regs_on_def_gpu;
		break;
	}

	/* initialize the EPT mapping for passthrough GPU dsm region */
	vm_map_ptdev_mmio(ctx, 0, 2, 0, GPU_DSM_GPA, GPU_DSM_SIZE, dsm_start_hpa);

	/* get opregion hpa */
	opregion_phys = read_config(ptdev->phys_dev, PCIR_ASLS_CTL, 4);
	opregion_start_hpa = opregion_phys & PCIM_ASLS_OPREGION_MASK;
	/* initialize the EPT mapping for passthrough GPU opregion */
	vm_map_ptdev_mmio(ctx, 0, 2, 0, GPU_OPREGION_GPA, GPU_OPREGION_SIZE, opregion_start_hpa);

	pci_set_cfgdata32(ptdev->dev, PCIR_ASLS_CTL, GPU_OPREGION_GPA | (opregion_phys & ~PCIM_ASLS_OPREGION_MASK));

	pcidev->type = QUIRK_PTDEV;
}

static int
parse_vmsix_on_msi_bar_id(char *s, int *id, int base)
{
	char *str, *cp;
	int ret = 0;

	if (s == NULL)
		return -EINVAL;

	str = cp = strdup(s);
	ret = dm_strtoi(cp, &cp, base, id);
	free(str);

	return ret;
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
	int bus, slot, func, idx, error;
	struct passthru_dev *ptdev;
	struct pci_device_iterator *iter;
	struct pci_device *phys_dev;
	char *opt;
	bool keep_gsi = false;
	bool need_reset = true;
	bool d3hot_reset = false;
	int vmsix_on_msi_bar_id = -1;
	struct acrn_assign_pcidev pcidev = {};
	uint16_t vendor = 0, device = 0;

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

	if (is_rtvm && (PCI_BDF(bus, slot, func) == PCI_BDF_GPU)) {
		warnx("%s RTVM doesn't support GVT-D.", __func__);
		return -EINVAL;
	}

	while ((opt = strsep(&opts, ",")) != NULL) {
		if (!strncmp(opt, "keep_gsi", 8))
			keep_gsi = true;
		else if (!strncmp(opt, "no_reset", 8))
			need_reset = false;
		else if (!strncmp(opt, "d3hot_reset", 11))
			d3hot_reset = true;
		else if (!strncmp(opt, "gpu", 3)) {
			/* Create the dedicated "igd-lpc" on 00:1f.0 for IGD passthrough */
			if (pci_parse_slot("31,igd-lpc") != 0)
				warnx("faild to create igd-lpc");
		} else if (!strncmp(opt, "vmsix_on_msi", 12)) {
			opt = strsep(&opts, ",");
			if (parse_vmsix_on_msi_bar_id(opt, &vmsix_on_msi_bar_id, 10) != 0) {
				warnx("faild to parse msix emulation bar id");
				return -EINVAL;
			}

		} else
			warnx("Invalid passthru options:%s", opt);
	}

	ptdev = calloc(1, sizeof(struct passthru_dev));
	if (ptdev == NULL) {
		warnx("%s: calloc FAIL!", __func__);
		error = -ENOMEM;
		goto done;
	}

	ptdev->phys_bdf = PCI_BDF(bus, slot, func);
	ptdev->need_reset = need_reset;
	ptdev->d3hot_reset = d3hot_reset;
	update_pt_info(ptdev->phys_bdf);

	error = pciaccess_init();
	if (error < 0)
		goto done;

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
	pci_iterator_destroy(iter);

	if (error < 0) {
		warnx("No physical PCI device %x:%x.%x!", bus, slot, func);
		goto done;
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


	/* Once this device is assigned to other guest, the original guest can't access it again
	 * So we need to cache verdor and device for filling DSDT.
	 */
	vendor = read_config(ptdev->phys_dev, PCIR_VENDOR, 2);
	device = read_config(ptdev->phys_dev, PCIR_DEVICE, 2);
	pci_set_cfgdata16(dev, PCIR_VENDOR, vendor);
	pci_set_cfgdata16(dev, PCIR_DEVICE, device);

#if AUDIO_NHLT_HACK
	/* device specific handling:
	 * audio: enable NHLT ACPI table
	 */
	if (vendor == 0x8086 && device == 0x5a98)
		acpi_table_enable(NHLT_ENTRY_NO);
#endif

	/* initialize config space */
	error = cfginit(ctx, ptdev, bus, slot, func);
	if (error < 0)
		goto done;

	if (vmsix_on_msi_bar_id != -1) {
		error = pci_emul_alloc_pbar(dev, vmsix_on_msi_bar_id, 0, PCIBAR_MEM32, 4096);
		if (error < 0)
			goto done;
		error = IRQ_MSI;
	}

	if (ptdev->phys_bdf == PCI_BDF_GPU)
		passthru_gpu_dsm_opregion(ctx, ptdev, &pcidev, device);

	pcidev.virt_bdf = PCI_BDF(dev->bus, dev->slot, dev->func);
	pcidev.phys_bdf = ptdev->phys_bdf;
	for (idx = 0; idx <= PCI_BARMAX; idx++) {
		pcidev.bar[idx] = pci_get_cfgdata32(dev, PCIR_BAR(idx));
	}

	/* If ptdev support MSI/MSIX, stop here to skip virtual INTx setup.
	 * Forge Guest to use MSI/MSIX in this case to mitigate IRQ sharing
	 * issue
	 */
	if (error != IRQ_MSI || keep_gsi) {
		/* Allocates the virq if ptdev only support INTx */
		pci_lintr_request(dev);

		ptdev->phys_pin = read_config(ptdev->phys_dev, PCIR_INTLINE, 1);

		if (ptdev->phys_pin == -1 || ptdev->phys_pin > 256) {
			warnx("ptdev %x/%x/%x has wrong phys_pin %d, likely fail!",
				bus, slot, func, ptdev->phys_pin);
			error = -1;
			goto done;
		}
	}

	pcidev.intr_line = pci_get_cfgdata8(dev, PCIR_INTLINE);
	pcidev.intr_pin = pci_get_cfgdata8(dev, PCIR_INTPIN);
	error = vm_assign_pcidev(ctx, &pcidev);
done:
	if (error && (ptdev != NULL)) {
			free(ptdev);
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
	uint16_t virt_bdf = PCI_BDF(dev->bus, dev->slot, dev->func);
	struct acrn_assign_pcidev pcidev = {};
	uint16_t phys_bdf = 0;
	char reset_path[60];
	int fd;

	if (!dev->arg) {
		warnx("%s: passthru_dev is NULL", __func__);
		return;
	}

	ptdev = (struct passthru_dev *) dev->arg;

	pr_info("vm_reset_ptdev_intx:0x%x-%x, ioapic virpin=%d.\n",
			virt_bdf, ptdev->phys_bdf, dev->lintr.ioapic_irq);
	if (dev->lintr.pin != 0) {
		vm_reset_ptdev_intx_info(ctx, virt_bdf, ptdev->phys_bdf, dev->lintr.ioapic_irq, false);
	}

	if (ptdev)
		phys_bdf = ptdev->phys_bdf;

	if (ptdev->phys_bdf == PCI_BDF_GPU) {
		vm_unmap_ptdev_mmio(ctx, 0, 2, 0, GPU_DSM_GPA, GPU_DSM_SIZE, dsm_start_hpa);
		vm_unmap_ptdev_mmio(ctx, 0, 2, 0, GPU_OPREGION_GPA, GPU_OPREGION_SIZE, opregion_start_hpa);
	}

	pcidev.virt_bdf = PCI_BDF(dev->bus, dev->slot, dev->func);
	pcidev.phys_bdf = ptdev->phys_bdf;
	pciaccess_cleanup();
	free(ptdev);

	if (!is_rtvm) {
		/* Let the HV to deassign the pt device for RTVM, In this case, the RTVM
		 * could still be alive if DM died.
		 */
		vm_deassign_pcidev(ctx, &pcidev);
	}
	if (!is_rtvm && phys_bdf) {
		memset(reset_path, 0, sizeof(reset_path));
		snprintf(reset_path, 40,
			"/sys/bus/pci/devices/0000:%02x:%02x.%x/reset",
			(phys_bdf >> 8) & 0xFF,
			(phys_bdf >> 3) & 0x1F,
			(phys_bdf & 0x7));

		fd = open(reset_path, O_WRONLY);
		if (fd >= 0) {
			if (write(fd, "1", 1) < 0)
				warnx("reset dev %x failed!\n",
				      phys_bdf);
			close(fd);
		}
	}
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

	pr_info("vm_set_ptdev_intx for %d:%d.%d, ",
		dev->bus, dev->slot, dev->func);
	pr_info("virt_pin=%d, phys_pin=%d, virt_bdf=0x%x, phys_bdf=0x%x.\n",
		dev->lintr.ioapic_irq, ptdev->phys_pin,
		virt_bdf, ptdev->phys_bdf);

	vm_set_ptdev_intx_info(ctx, virt_bdf, ptdev->phys_bdf,
			       dev->lintr.ioapic_irq, ptdev->phys_pin, false);
}

static int
passthru_cfgread(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		 int coff, int bytes, uint32_t *rv)
{
	struct passthru_dev *ptdev = dev->arg;

	if (ptdev->has_virt_pcicfg_regs && ptdev->has_virt_pcicfg_regs(coff))
		*rv = pci_get_cfgdata32(dev, coff);
	else
		*rv = read_config(ptdev->phys_dev, coff, bytes);

	return 0;
}

static int
passthru_cfgwrite(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		  int coff, int bytes, uint32_t val)
{
	struct passthru_dev *ptdev = dev->arg;

	if (!(ptdev->has_virt_pcicfg_regs && ptdev->has_virt_pcicfg_regs(coff)))
		write_config(ptdev->phys_dev, coff, bytes, val);

	return 0;
}

static void
passthru_write(struct vmctx *ctx, int vcpu, struct pci_vdev *dev, int baridx,
	       uint64_t offset, int size, uint64_t value)
{
}

static uint64_t
passthru_read(struct vmctx *ctx, int vcpu, struct pci_vdev *dev, int baridx,
	      uint64_t offset, int size)
{
	return ~0UL;
}

static void
write_dsdt_xdci(struct pci_vdev *dev)
{
	pr_info("write virt-%x:%x.%x in dsdt for XDCI @ 00:15.1\n",
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
	pr_info("write virt-%x:%x.%x in dsdt for HDAC @ 00:17.0\n",
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
	pr_info("write virt-%x:%x.%x in dsdt for HDAS @ 00:e.0\n",
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
	pr_info("write virt-%x:%x.%x in dsdt for ipu's i2c-bus @ 00:16.0\n",
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
	pr_info("write virt-%x:%x.%x in dsdt for URT1 @ 00:18.0\n",
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
write_dsdt_sdc(struct pci_vdev *dev)
{
	pr_info("write SDC-%x:%x.%x in dsdt for SDC @ 00:1b.0\n",
	       dev->bus,
	       dev->slot,
	       dev->func);
	dsdt_line("Device (SDC)");
	dsdt_line("{");
	dsdt_line("    Name (_ADR, 0x%04X%04X)", dev->slot, dev->func);
	dsdt_line("    Name (_DDN, \"Intel(R) SD Card Controller\")");
	dsdt_line("    Name (_UID, One)");
	dsdt_line("    Method (_CRS, 0, NotSerialized)");
	dsdt_line("    {");
	dsdt_line("        Name (RBUF, ResourceTemplate ()");
	dsdt_line("        {");
	dsdt_line("            GpioInt (Edge, ActiveBoth, SharedAndWake, "
					"PullNone, 0, ");
	dsdt_line("			\"\\\\_SB_.PCI0.AGPI\", 0, ResourceConsumer, ,");
	dsdt_line("                    )");
	dsdt_line("                    {   // Pin list");
	dsdt_line("                        0");
	dsdt_line("                    }");
	dsdt_line("            GpioIo (Exclusive, PullDefault, 0x0000, "
					"0x0000, IoRestrictionInputOnly,");
	dsdt_line("                    \"\\\\_SB._PCI0.AGPI\", 0x00, "
					"ResourceConsumer, ,");
	dsdt_line("                    )");
	dsdt_line("                    {   // Pin list");
	dsdt_line("                        0");
	dsdt_line("                    }");
	dsdt_line("        })");
	dsdt_line("        Return (RBUF)");
	dsdt_line("    }");
	dsdt_line("}");
}

static void
write_dsdt_tsn(struct pci_vdev *dev, uint16_t device)
{
	char device_name[4];
	uint16_t pcs_id;

	if (device == 0x4b32) {
		strncpy(device_name, "GTSN", 4);
		pcs_id = 0;
	} else if (device == 0x4ba0) {
		strncpy(device_name, "OTN0", 4);
		pcs_id = 1;
	} else if (device == 0x4bb0) {
		strncpy(device_name, "OTN1", 4);
		pcs_id = 2;
	} else {
		return;
	}

	pr_info("write TSN-%x:%x.%x in dsdt for TSN-%d\n", dev->bus, dev->slot, dev->func, pcs_id);

	dsdt_line("");
	dsdt_line("Device (%.4s)", device_name);
	dsdt_line("{");
	dsdt_line("    Name (_ADR, 0x%04X%04X)", dev->slot, dev->func);  // _ADR: Address
	dsdt_line("    OperationRegion (TSRT, PCI_Config, Zero, 0x0100)");
	dsdt_line("    Field (TSRT, AnyAcc, NoLock, Preserve)");
	dsdt_line("    {");
	dsdt_line("        DVID,   16,");
	dsdt_line("        Offset (0x10),");
	dsdt_line("        TADL,   32,");
	dsdt_line("        TADH,   32");
	dsdt_line("    }");
	dsdt_line("}");
	dsdt_line("");
	dsdt_line("Scope (_SB)");
	dsdt_line("{");
	dsdt_line("    Device (PCS%01X)", pcs_id);
	dsdt_line("    {");
	dsdt_line("        Name (_HID, \"INTC1033\")");  // _HID: Hardware ID
	dsdt_line("        Name (_UID, Zero)");  // _UID: Unique ID
	dsdt_line("        Method (_STA, 0, NotSerialized)");  // _STA: Status
	dsdt_line("        {");
	dsdt_line("");
	dsdt_line("            Return (0x0F)");
	dsdt_line("        }");
	dsdt_line("");
	dsdt_line("        Method (_CRS, 0, Serialized)");  // _CRS: Current Resource Settings
	dsdt_line("        {");
	dsdt_line("            Name (PCSR, ResourceTemplate ()");
	dsdt_line("            {");
	dsdt_line("                Memory32Fixed (ReadWrite,");
	dsdt_line("                    0x00000000,");         // Address Base
	dsdt_line("                    0x00000004,");         // Address Length
	dsdt_line("                    _Y55)");
	dsdt_line("                Memory32Fixed (ReadWrite,");
	dsdt_line("                    0x00000000,");         // Address Base
	dsdt_line("                    0x00000004,");         // Address Length
	dsdt_line("                    _Y56)");
	dsdt_line("            })");
	dsdt_line("            CreateDWordField (PCSR, \\_SB.PCS%01X._CRS._Y55._BAS, MAL0)", pcs_id);  // _BAS: Base Address
	dsdt_line("            MAL0 = ((^^PCI0.%.4s.TADL & 0xFFFFF000) + 0x0200)", device_name);
	dsdt_line("            CreateDWordField (PCSR, \\_SB.PCS%01X._CRS._Y56._BAS, MDL0)", pcs_id);  // _BAS: Base Address
	dsdt_line("            MDL0 = ((^^PCI0.%.4s.TADL & 0xFFFFF000) + 0x0204)", device_name);
	dsdt_line("            Return (PCSR)"); /* \_SB_.PCS0._CRS.PCSR */
	dsdt_line("        }");
	dsdt_line("    }");
	dsdt_line("}");
	dsdt_line("");
}

static void
passthru_write_dsdt(struct pci_vdev *dev)
{
	uint16_t vendor = 0, device = 0;

	vendor = pci_get_cfgdata16(dev, PCIR_VENDOR);

	if (vendor != 0x8086)
		return;

	device = pci_get_cfgdata16(dev, PCIR_DEVICE);

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
	else if (device == 0x5aca)
		/* SDC @ 00:1b.0 */
		write_dsdt_sdc(dev);
	else if ((device == 0x4b32) || (device == 0x4ba0) || (device == 0x4bb0))
		write_dsdt_tsn(dev, device);
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
