/*
 * Copyright (C) 2018 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * The Intel Trace Hub (aka. North Peak, NPK) is a trace aggregator for
 * Software, Firmware, and Hardware. On the virtualization platform, it
 * can be used to output the traces from SOS/UOS/Hypervisor/FW together
 * with unified timestamps.
 *
 * There are 2 software visible MMIO space in the npk pci device. One is
 * the CSR which maps the configuration registers, and the other is the
 * STMR which is organized as many Masters, and used to send the traces.
 * Each Master has a fixed number of Channels, which is 128 on GP. Each
 * channel occupies 64B, so the offset of each Master is 8K (64B*128).
 * Here is the detailed layout of STMR:
 *                          M=NPK_SW_MSTR_STP (1024 on GP)
 *                        +-------------------+
 *                        |    m[M],c[C-1]    |
 *           Base(M,C-1)  +-------------------+
 *                        |        ...        |
 *                        +-------------------+
 *                        |     m[M],c[0]     |
 *             Base(M,0)  +-------------------+
 *                        |        ...        |
 *                        +-------------------+
 *                        |    m[i+1],c[1]    |
 *           Base(i+1,1)  +-------------------+
 *                        |    m[i+1],c[0]    |
 *           Base(i+1,0)  +-------------------+
 *                        |        ...        |
 *                        +-------------------+
 *                        |     m[i],c[1]     |
 * Base(i,1)=SW_BAR+0x40  +-------------------+
 *                        |     m[i],c[0]     |  64B
 *      Base(i,0)=SW_BAR  +-------------------+
 *                         i=NPK_SW_MSTR_STRT (256 on GP)
 *
 * CSR and STMR are treated differently in npk virtualization because:
 * 1. CSR configuration should come from just one OS, instead of each OS.
 * In our case, it should come from SOS.
 * 2. For performance and timing concern, the traces from each OS should
 * be written to STMR directly.
 *
 * Based on these, the npk virtualization is implemented in this way:
 * 1. The physical CSR is owned by SOS, and dm/npk emulates a software
 * one for the UOS, to keep the npk driver on UOS unchanged. Some CSR
 * initial values are configured to make the UOS npk driver think it
 * is working on a real npk. The CSR configuration from UOS is ignored
 * by dm, and it will not bring any side-effect. Because traces are the
 * only things needed from UOS, the location to send traces to and the
 * trace format are not affected by the CSR configuration.
 * 2. Part of the physical STMR will be reserved for the SOS, and the
 * others will be passed through to the UOS, so that the UOS can write
 * the traces to the MMIO space directly.
 *
 * A parameter is needed to indicate the offset and size of the Masters
 * to pass through to the UOS. For example, "-s 0:2,npk,512/256", there
 * are 256 Masters from #768 (256+512, #256 is the starting Master for
 * software tracing) passed through to the UOS.
 *
 *             CSR                       STMR
 * SOS:  +--------------+  +----------------------------------+
 *       | physical CSR |  | Reserved for SOS |               |
 *       +--------------+  +----------------------------------+
 * UOS:  +--------------+                     +---------------+
 *       | sw CSR by dm |                     | mapped to UOS |
 *       +--------------+                     +---------------+
 *
 * Here is an overall flow about how it works.
 * 1. System boots up, and the npk driver on SOS is loaded.
 * 2. The dm is launched with parameters to enable npk virtualization.
 * 3. The dm/npk sets up a bar for CSR, and some values are initialized
 * based on the parameters, for example, the total number of Masters for
 * the UOS.
 * 4. The dm/npk sets up a bar for STMR, and maps part of the physical
 * STMR to it with an offset, according to the parameters.
 * 5. The UOS boots up, and the native npk driver on the UOS is loaded.
 * 6. Enable the traces from UOS, and the traces are written directly to
 * STMR, but not output by npk for now.
 * 7. Enable the npk output on SOS, and now the traces are output by npk
 * to the selected target.
 * 8. If the memory is the selected target, the traces can be retrieved
 * from memory on SOS, after stopping the traces.
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

#include "dm.h"
#include "vmmapi.h"
#include "pci_core.h"
#include "npk.h"

static int pci_npk_debug;
#define DPRINTF(params) do { if (pci_npk_debug) printf params; } while (0)
#define WPRINTF(params) (printf params)

#define npk_gth_reg(x)     (npk_csr[NPK_CSR_GTH].data.u8[(x)])
#define npk_sth_reg(x)     (npk_csr[NPK_CSR_STH].data.u8[(x)])
#define npk_msc0_reg(x)    (npk_csr[NPK_CSR_MSC0].data.u8[(x)])
#define npk_msc1_reg(x)    (npk_csr[NPK_CSR_MSC1].data.u8[(x)])
#define npk_pti_reg(x)     (npk_csr[NPK_CSR_PTI].data.u8[(x)])
#define npk_gth_reg32(x)   (npk_csr[NPK_CSR_GTH].data.u32[(x)>>2])
#define npk_sth_reg32(x)   (npk_csr[NPK_CSR_STH].data.u32[(x)>>2])
#define npk_msc0_reg32(x)  (npk_csr[NPK_CSR_MSC0].data.u32[(x)>>2])
#define npk_msc1_reg32(x)  (npk_csr[NPK_CSR_MSC1].data.u32[(x)>>2])
#define npk_pti_reg32(x)   (npk_csr[NPK_CSR_PTI].data.u32[(x)>>2])

/* the registers in CSR */
static uint8_t _npk_gth_reg[NPK_CSR_GTH_SZ];
static uint8_t _npk_sth_reg[NPK_CSR_STH_SZ];
static uint8_t _npk_msc0_reg[NPK_CSR_MSC0_SZ];
static uint8_t _npk_msc1_reg[NPK_CSR_MSC1_SZ];
static uint8_t _npk_pti_reg[NPK_CSR_PTI_SZ];

static struct npk_regs npk_csr[NPK_CSR_LAST] = {
	/* GTH */
	{ NPK_CSR_GTH_BASE,  NPK_CSR_GTH_SZ,  { _npk_gth_reg  } },
	/* STH */
	{ NPK_CSR_STH_BASE,  NPK_CSR_STH_SZ,  { _npk_sth_reg  } },
	/* MSC0 */
	{ NPK_CSR_MSC0_BASE, NPK_CSR_MSC0_SZ, { _npk_msc0_reg } },
	/* MSC1 */
	{ NPK_CSR_MSC1_BASE, NPK_CSR_MSC1_SZ, { _npk_msc1_reg } },
	/* PTI */
	{ NPK_CSR_PTI_BASE,  NPK_CSR_PTI_SZ,  { _npk_pti_reg  } }
};

/* the default values are from intel_th developer's manual */
static struct npk_reg_default_val regs_default_val[] = {
	{ NPK_CSR_GTH,  NPK_CSR_GTHOPT0, 0x00040101},
	{ NPK_CSR_MSC0, NPK_CSR_MSCxCTL, 0x00000300},
	{ NPK_CSR_MSC1, NPK_CSR_MSCxCTL, 0x00000300}
};
#define regs_default_val_num (sizeof(regs_default_val) / \
		sizeof(struct npk_reg_default_val))

static int npk_in_use;
static uint64_t sw_bar_base;

/* get the pointer to the register based on the offset */
static inline uint32_t *offset2reg(uint64_t offset)
{
	uint32_t *reg = NULL, i;
	struct npk_regs *regs;

	/* traverse the npk_csr to find the correct one */
	for (i = NPK_CSR_FIRST; i < NPK_CSR_LAST; i++) {
		regs = &npk_csr[i];
		if (offset >= regs->base && offset < regs->base + regs->size) {
			reg = regs->data.u32 + ((offset - regs->base) >> 2);
			break;
		}
	}

	return reg;
}

static inline int valid_param(uint32_t m_off, uint32_t m_num)
{
	/* 8-aligned, no less than 8, no overflow */
	if (((m_off & 0x7U) == 0) && ((m_num & 0x7U) == 0) && (m_off > 0x7U)
			&& (m_num > 0x7U) && (m_off + m_num <= NPK_SW_MSTR_NUM))
		return 1;

	return 0;
}

/*
 * Set up a bar for CSR, and some values are initialized based on the
 * parameters, for example, the total number of Masters for the UOS.
 * Set up a bar for STMR, and map part of the physical STMR to it with
 * an offset, according to the parameters.
 */
static int pci_npk_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	int i, b, s, f, fd, ret, rc, error = -1;
	DIR *dir;
	struct dirent *dent;
	char name[PATH_MAX];
	uint8_t h_cfg[PCI_REGMAX + 1];
	uint32_t m_off, m_num;
	struct npk_reg_default_val *d;
	char *cp;

	if (npk_in_use) {
		WPRINTF(("NPK is already in use\n"));
		return error;
	}
	npk_in_use = 1;

	/*
	 * CSR  (bar#0): emulate it for guests using npk_csr
	 *
	 * STMR (bar#2): map the host MMIO space to guests with an offset
	 *
	 * +--NPK_SW_MSTR_STRT  +--m_off                NPK_SW_MSTR_STP--+
	 * |                    +-----  m_num  ------+                   |
	 * v                    v                    v                   v
	 * +--------------------+--------------------+-------------------+
	 * |                    |                    |                   |
	 * |  Reserved for SOS  |  Mapped for UOS#x  |                   |
	 * |                    |                    |                   |
	 * +--------------------+--------------------+-------------------+
	 * ^                    ^
	 * |                    |
	 * +--sw_bar for host   +--sw_bar for UOS#x
	 */

	/* get the master offset and the number for this guest */
	if ((opts == NULL) || dm_strtoui(opts, &cp, 10, &m_off) || *cp != '/' ||
			dm_strtoui(cp + 1, &cp, 10, &m_num) || !valid_param(m_off, m_num)) {
		m_off = 256;
		m_num = 256;
	}

	/* check if the intel_th_pci driver is loaded */
	dir = opendir(NPK_DRV_SYSFS_PATH);
	if (dir == NULL) {
		WPRINTF(("NPK driver not loaded\n"));
		return error;
	}

	/* traverse the driver folder, and try to find the NPK BDF# */
	while ((dent = readdir(dir)) != NULL) {
		if (strncmp(dent->d_name, "0000:", 5) != 0 ||
			parse_bdf((dent->d_name + 5), &b, &s, &f, 16) != 0)
			continue;
		else
			break;
	}
	closedir(dir);

	if (!dent) {
		WPRINTF(("Cannot find NPK device\n"));
		return error;
	}

	/* read the host NPK configuration space */
	rc = snprintf(name, PATH_MAX, "%s/%s/config", NPK_DRV_SYSFS_PATH,
			dent->d_name);
	if (rc >= PATH_MAX || rc < 0)
		WPRINTF(("NPK device name is invalid!\n"));
	fd = open(name, O_RDONLY);
	if (fd == -1) {
		WPRINTF(("Cannot open host NPK config\n"));
		return error;
	}

	ret = pread(fd, h_cfg, PCI_REGMAX + 1, 0);
	close(fd);
	if (ret < PCI_REGMAX + 1) {
		WPRINTF(("Cannot read host NPK config\n"));
		return error;
	}

	/* initialize the configuration space */
	pci_set_cfgdata16(dev, PCIR_VENDOR, *(uint16_t *)&h_cfg[PCIR_VENDOR]);
	pci_set_cfgdata16(dev, PCIR_DEVICE, *(uint16_t *)&h_cfg[PCIR_DEVICE]);
	pci_set_cfgdata16(dev, PCIR_REVID, *(uint16_t *)&h_cfg[PCIR_REVID]);
	pci_set_cfgdata8(dev, PCIR_CLASS, h_cfg[PCIR_CLASS]);

	/* get the host base of NPK bar#2, plus the offset for the guest */
	sw_bar_base = *(uint32_t *)&h_cfg[PCIR_BAR(2)] & PCIM_BAR_MEM_BASE;
	sw_bar_base += NPK_MSTR_TO_MEM_SZ(m_off);

	/* allocate the bar#0 (CSR)*/
	error = pci_emul_alloc_bar(dev, 0, PCIBAR_MEM64, NPK_CSR_MTB_BAR_SZ);
	if (error) {
		WPRINTF(("Cannot alloc bar#0 for the guest\n"));
		return error;
	}

	/* allocate the bar#2 (STMR)*/
	error = pci_emul_alloc_pbar(dev, 2, sw_bar_base, PCIBAR_MEM64,
			NPK_MSTR_TO_MEM_SZ(m_num));
	if (error) {
		WPRINTF(("Cannot alloc bar#2 for the guest\n"));
		return error;
	}

	/*
	 * map this part of STMR to the guest so that the traces from UOS are
	 * written directly to it.
	 */
	error = vm_map_ptdev_mmio(ctx, dev->bus, dev->slot, dev->func,
			dev->bar[2].addr, dev->bar[2].size, sw_bar_base);
	if (error) {
		WPRINTF(("Cannot Map the address to the guest MMIO space\n"));
		return error;
	}

	/* setup default values for some registers */
	for (i = 0; i < regs_default_val_num; i++) {
		d = &regs_default_val[i];
		npk_csr[d->csr].data.u32[d->offset >> 2] = d->default_val;
	}
	/* setup the SW Master Start/Stop and Channels per Master for UOS */
	npk_sth_reg32(NPK_CSR_STHCAP0) = NPK_SW_MSTR_STRT |
				((m_num + NPK_SW_MSTR_STRT - 1) << 16);
	npk_sth_reg32(NPK_CSR_STHCAP1) = ((NPK_SW_MSTR_STRT - 1) << 24) |
				NPK_CHANNELS_PER_MSTR;

	/* set Pipe Line Empty for GTH/MSCx State */
	npk_gth_reg(NPK_CSR_GTHSTAT) = NPK_CSR_GTHSTAT_PLE;
	npk_msc0_reg32(NPK_CSR_MSCxSTS) = NPK_CSR_MSCxSTS_PLE;
	npk_msc1_reg32(NPK_CSR_MSCxSTS) = NPK_CSR_MSCxSTS_PLE;

	DPRINTF(("NPK[%x:%x:%x] h_bar#2@0x%lx g_bar#2@0x%lx[0x%lx] m+%d[%d]\n",
				b, s, f, sw_bar_base, dev->bar[2].addr,
				dev->bar[2].size, m_off, m_num));

	return 0;
}

static void pci_npk_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	vm_unmap_ptdev_mmio(ctx, dev->bus, dev->slot, dev->func,
			dev->bar[2].addr, dev->bar[2].size, sw_bar_base);
	npk_in_use = 0;
}

/* the CSR configuration from UOS will not take effect on the physical NPK */
static void pci_npk_write(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		int baridx, uint64_t offset, int size, uint64_t value)
{
	uint32_t *reg;

	DPRINTF(("W %d +0x%lx[%d] val 0x%lx\n", baridx, offset, size, value));

	if (baridx != 0 || (offset & 0x3) || size != 4)
		return;

	/* try to set the register value in npk_csr */
	reg = offset2reg(offset);
	if (reg)
		*reg = (uint32_t)value;
}

static uint64_t pci_npk_read(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		int baridx, uint64_t offset, int size)
{
	uint32_t *reg, val = 0;

	DPRINTF(("R %d +0x%lx[%d] val 0x%x\n", baridx, offset, size, val));

	if (baridx != 0 || (offset & 0x3) || size != 4)
		return (uint64_t)val;

	/* try to get the register value from npk_csr */
	reg = offset2reg(offset);
	if (reg)
		val = *reg;

	return (uint64_t)val;
}

struct pci_vdev_ops pci_ops_npk = {
	.class_name	= "npk",
	.vdev_init	= pci_npk_init,
	.vdev_deinit	= pci_npk_deinit,
	.vdev_barwrite	= pci_npk_write,
	.vdev_barread	= pci_npk_read,
};
DEFINE_PCI_DEVTYPE(pci_ops_npk);
