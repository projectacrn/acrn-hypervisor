/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dm.h"
#include "pci_core.h"
#include "vmmapi.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

static int pci_gvt_debug;

static struct pci_vdev *gvt_dev;

#define DPRINTF(params) do { if (pci_gvt_debug) printf params; } while (0)

#define WPRINTF(params) (printf params)

struct PCIHostDeviceAddress {
	uint32_t domain;
	uint32_t bus;
	uint32_t slot;
	uint32_t function;
};

struct pci_gvt {
	struct pci_vdev *gvt_pi;
	struct PCIHostDeviceAddress addr;
	int host_config_fd;
	FILE *gvt_file;
	/* PCI config space */
	uint8_t host_config[PCI_REGMAX+1];
	int instance_created;
};

struct gvt_interval {
	uint64_t start;
	uint64_t end;
	int idx;
};

static struct gvt_interval bar_interval[2];

/* These are the default values */
int gvt_low_gm_sz = 64; /* in MB */
int gvt_high_gm_sz = 448; /* in MB */
int gvt_fence_sz = 8;
int guest_domid = 1;

int
acrn_parse_gvtargs(char *arg)
{
	if (dm_strtoi(arg, &arg, 10, &gvt_low_gm_sz) != 0 ||
		dm_strtoi(arg, &arg, 10, &gvt_high_gm_sz) != 0 ||
		dm_strtoi(arg, &arg, 10, &gvt_fence_sz) != 0)
		return -1;

	printf("passed gvt-g optargs low_gm %d, high_gm %d, fence %d\n",
		gvt_low_gm_sz, gvt_high_gm_sz, gvt_fence_sz);

	return 0;
}

static void
pci_gvt_write(struct vmctx *ctx, int vcpu, struct pci_vdev *pi,
	      int baridx, uint64_t offset, int size, uint64_t value)
{
	/* null function, pci config space write should be trapped */
	DPRINTF(("%s: write vcpu %d, baridx %d, offset %"PRIu64", size %d, "
			"value %"PRIu64"\n",
			__func__, vcpu, baridx, offset, size, value));
}

static uint64_t
pci_gvt_read(struct vmctx *ctx, int vcpu, struct pci_vdev *pi,
	     int baridx, uint64_t offset, int size)
{
	/* null function, pci config space read should be trapped */
	DPRINTF(("%s: read vcpu %d, baridx %d, offset %"PRIu64", size %d\n",
				__func__, vcpu, baridx, offset, size));

	return 0;
}

static bool
is_two_region_overlap(uint64_t x1, uint64_t x2, uint64_t y1, uint64_t y2)
{
	if(x1 <= y2 && y1 <= x2)
		return true;
	return false;
}

void
adjust_bar_region_by_gvt_bars(struct vmctx *ctx, uint64_t *base, uint64_t size)
{
	/*
	 * for gvt, we reserve two region
	 * 1. bar0 region:[bar0_start_addr, bar0_end_addr]
	 * 2. bar2 region:[bar2_start_addr, bar2_end_addr]
	 * these two regions have sorted and stored in array bar_interval
	 */
	int i;
	
	for(i = 0; i < 2; i++){
		if(is_two_region_overlap(bar_interval[i].start,
						bar_interval[i].end, *base, *base + size -1)){
			*base = roundup2(bar_interval[i].end + 1, size);
		}
	}
}

static struct gvt_interval *
get_interval_by_idx(int idx)
{
	int i;
	
	for(i = 0; i < 2; i++)
		if(bar_interval[i].idx == idx)
			return &bar_interval[i];
	
	return NULL;
}

static int
gvt_reserve_resource(int idx, enum pcibar_type type, uint64_t size)
{
	uint64_t addr, mask, lobits, bar;
	struct gvt_interval *interval;
	int ret;

	mask = PCIM_BAR_MEM_BASE;
	lobits = PCIM_BAR_MEM_SPACE | PCIM_BAR_MEM_32;

	interval = get_interval_by_idx(idx);

	if(!interval){
		perror("gvt failed to find interval\n");
		return -1;
	}

	addr = interval->start;
	
	gvt_dev->bar[idx].type = type;
	gvt_dev->bar[idx].addr = addr;
	gvt_dev->bar[idx].size = size;

	/* Initialize the BAR register in config space */
	bar = (addr & mask) | lobits;
	pci_set_cfgdata32(gvt_dev, PCIR_BAR(idx), bar);

	ret = register_bar(gvt_dev, idx);

	if(ret != 0){
		/* because gvt isn't firstly initialized, previous pci
		 * devices' bars may conflict with gvt bars.
		 * Use register_bar to detect this case,
		 * but this case rarely happen.
		 * If this case always happens, we need to
		 * change core.c code to ensure gvt firstly initialzed
		 */
		perror("gvt failed to register_bar\n");
		return ret;
	}

	return 0;
}

static int
gvt_init_config(struct pci_gvt *gvt)
{
	int ret;
	char name[PATH_MAX];
	uint8_t cap_ptr = 0;
	uint8_t aperture_size_reg;
	uint16_t aperture_size = 256;
	char res_name[PATH_MAX];
	char resource[512];
	int res_fd;
	uint64_t bar0_start_addr;
	uint64_t bar0_end_addr;
	uint64_t bar2_start_addr;
	uint64_t bar2_end_addr;
	char *next;
	struct vmctx *ctx;
	struct gvt_interval tem;

	snprintf(res_name, sizeof(res_name),
		"/sys/bus/pci/devices/%04x:%02x:%02x.%x/resource",
		gvt->addr.domain, gvt->addr.bus, gvt->addr.slot,
		gvt->addr.function);
	res_fd = open(res_name, O_RDONLY);
	if (res_fd == -1) {
		perror("gvt:open host pci resource failed\n");
		return -1;
	}

	ret = pread(res_fd, resource, 512, 0);

	close(res_fd);

	if (ret < 512) {
		perror("failed to read host device resource space\n");
		return -1;
	}

	next = resource;

    	bar0_start_addr = strtoull(next, &next, 16);
    	bar0_end_addr = strtoull(next, &next, 16);

    	/* bar0 and bar2 have some distance, need pass the distance */
    	next = next + 80;

    	bar2_start_addr = strtoull(next, &next, 16);
    	bar2_end_addr = strtoull(next, &next, 16);

	ctx = gvt->gvt_pi->vmctx;

	if(bar0_start_addr < ctx->lowmem_limit 
		|| bar2_start_addr < ctx->lowmem_limit
		|| bar0_end_addr > PCI_EMUL_ECFG_BASE
		|| bar2_end_addr > PCI_EMUL_ECFG_BASE){
		perror("gvt pci bases are out of range\n");
		return -1;
	}

	ctx->enable_gvt = true;

	ctx->adjust_bar_region = adjust_bar_region_by_gvt_bars;

	bar_interval[0].start = bar0_start_addr;
	bar_interval[0].end = bar0_end_addr;
	bar_interval[0].idx = 0;
	bar_interval[1].start = bar2_start_addr;
	bar_interval[1].end = bar2_end_addr;
	bar_interval[1].idx = 2;

	if(bar_interval[0].start > bar_interval[1].start){
		tem = bar_interval[0];
		bar_interval[0] = bar_interval[1];
		bar_interval[1] = tem;
	}

	snprintf(name, sizeof(name),
		"/sys/bus/pci/devices/%04x:%02x:%02x.%x/config",
		gvt->addr.domain, gvt->addr.bus, gvt->addr.slot,
		gvt->addr.function);
	gvt->host_config_fd = open(name, O_RDONLY);
	if (gvt->host_config_fd == -1) {
		perror("gvt:open host pci config failed\n");
		return -1;
	}

	ret = pread(gvt->host_config_fd, gvt->host_config, PCI_REGMAX+1, 0);

	close(gvt->host_config_fd);

	if (ret <= PCI_REGMAX) {
		perror("failed to read host device config space\n");
		return -1;
	}

	/* initialize config space */
	pci_set_cfgdata16(gvt->gvt_pi, PCIR_VENDOR, gvt->host_config[0]);
	pci_set_cfgdata16(gvt->gvt_pi, PCIR_DEVICE, gvt->host_config[0x02]);
	/* status */
	pci_set_cfgdata16(gvt->gvt_pi, PCIR_STATUS, gvt->host_config[0x06]);
	/* revision id */
	pci_set_cfgdata16(gvt->gvt_pi, PCIR_REVID, gvt->host_config[0x08]);
	/* class and sub class */
	pci_set_cfgdata8(gvt->gvt_pi, PCIR_CLASS, PCIC_DISPLAY);
	pci_set_cfgdata8(gvt->gvt_pi, PCIR_SUBCLASS, PCIS_DISPLAY_VGA);
	/* capability */
	pci_set_cfgdata8(gvt->gvt_pi, PCIR_CAP_PTR, gvt->host_config[0x34]);
	cap_ptr = gvt->host_config[0x34];
	while (cap_ptr != 0) {
		pci_set_cfgdata32(gvt->gvt_pi, cap_ptr,
			gvt->host_config[cap_ptr]);
		pci_set_cfgdata32(gvt->gvt_pi, cap_ptr + 4,
			gvt->host_config[cap_ptr + 4]);
		pci_set_cfgdata32(gvt->gvt_pi, cap_ptr + 8,
			gvt->host_config[cap_ptr + 8]);
		pci_set_cfgdata32(gvt->gvt_pi, cap_ptr + 12,
			gvt->host_config[cap_ptr + 12]);
		cap_ptr = gvt->host_config[cap_ptr + 1];
	}

	/* SNB: processor graphics control register */
	pci_set_cfgdata16(gvt->gvt_pi, 0x50, gvt->host_config[0x50]);
	/* processor graphics control register */
	pci_set_cfgdata16(gvt->gvt_pi, 0x52, gvt->host_config[0x52]);

	/* Alloc resource only and no need to register bar for gvt */
	ret = gvt_reserve_resource(0, PCIBAR_MEM32,
		16 * 1024 * 1024);
	if (ret != 0) {
		pr_err("allocate gvt pci bar[0] failed\n");
		return -1;
	}

	/* same as host, but guest only use partition of it by ballon */
	aperture_size_reg = gvt->host_config[0x62];
	switch(aperture_size_reg & 0b1111){
		case 0b00000:
			aperture_size = 128;
			break;
		case 0b00001:
			aperture_size = 256;
			break;
		case 0b00011:
			aperture_size = 512;
			break;
		case 0b00111:
			aperture_size = 1024;
			break;
		case 0b01111:
			aperture_size = 2048;
			break;
		case 0b11111:
			aperture_size = 4096;
			break;
		default:
			aperture_size = 256;
			break;
	}

	ret = gvt_reserve_resource(2, PCIBAR_MEM32,
		aperture_size * 1024 * 1024);
	if (ret != 0) {
		pr_err("allocate gvt pci bar[2] failed\n");
		return -1;
	}

	/* same as host, lagecy vga usage */
	ret = pci_emul_alloc_bar(gvt->gvt_pi, 4, PCIBAR_IO, 64);
	if (ret != 0) {
		pr_err("allocate gvt pci bar[4] failed\n");
		return -1;
	}

	return 0;
}

static int
gvt_create_instance(struct pci_gvt *gvt)
{
	const char *path = "/sys/kernel/gvt/control/create_gvt_instance";
	int ret = 0;

	gvt->gvt_file = fopen(path, "w");
	if (gvt->gvt_file == NULL) {
		WPRINTF(("GVT: open %s failed\n", path));
		return -errno;
	}

	DPRINTF(("GVT: %s: domid=%d, low_gm_sz=%dMB, high_gm_sz=%dMB, "
			"fence_sz=%d\n", __func__, guest_domid,
			gvt_low_gm_sz, gvt_high_gm_sz, gvt_fence_sz));

	if (gvt_low_gm_sz <= 0 || gvt_high_gm_sz <= 0 || gvt_fence_sz <= 0) {
		WPRINTF(("GVT: %s failed: invalid parameters!\n", __func__));
		fclose(gvt->gvt_file);
		return -EINVAL;
	}

	/* The format of the string is:
	 * domid,aperture_size,gm_size,fence_size. This means we want the gvt
	 * driver to create a gvt instanc for Domain domid with the required
	 * parameters. NOTE: aperture_size and gm_size are in MB.
	 */
	if (fprintf(gvt->gvt_file, "%d,%u,%u,%u\n", guest_domid,
			gvt_low_gm_sz, gvt_high_gm_sz, gvt_fence_sz) < 0)
		ret = -errno;

	if (fclose(gvt->gvt_file) != 0)
		return -errno;

	if (!ret)
		gvt->instance_created = 1;

	return ret;
}

/* did not find deinit function caller, leave it here only */
static int
gvt_destroy_instance(struct pci_gvt *gvt)
{
	const char *path = "/sys/kernel/gvt/control/create_gvt_instance";
	int ret = 0;

	gvt->gvt_file = fopen(path, "w");
	if (gvt->gvt_file == NULL) {
		WPRINTF(("gvt: error: open %s failed", path));
		return -errno;
	}

	/* -domid means we want the gvt driver to free the gvt instance
	 * of Domain domid.
	 **/
	if (fprintf(gvt->gvt_file, "%d\n", -guest_domid) < 0)
		ret = -errno;

	if (fclose(gvt->gvt_file) != 0)
		return -errno;

	if (!ret)
		gvt->instance_created = 0;

	return ret;
}

static int
pci_gvt_init(struct vmctx *ctx, struct pci_vdev *pi, char *opts)
{
	int ret;
	struct pci_gvt *gvt = NULL;

	gvt = calloc(1, sizeof(struct pci_gvt));
	if (!gvt) {
		perror("gvt:calloc gvt failed\n");
		return -1;
	}

	gvt->instance_created = 0;
	gvt->addr.domain = 0;
	gvt->addr.bus = pi->bus;
	gvt->addr.slot = pi->slot;
	gvt->addr.function = pi->func;

	pi->arg = gvt;
	gvt->gvt_pi = pi;
	guest_domid = ctx->vmid;

	gvt_dev = pi;

	ret = gvt_init_config(gvt);

	if (ret)
		goto fail;

	ret = gvt_create_instance(gvt);

	if(!ret)
		return ret;
fail:
	gvt_dev = NULL;
	ctx->enable_gvt = false;
	perror("GVT: init failed\n");
	free(gvt);
	return -1;
}

void
pci_gvt_deinit(struct vmctx *ctx, struct pci_vdev *pi, char *opts)
{
	struct pci_gvt *gvt = pi->arg;
	int ret = 0;

	if (gvt) {
		if (gvt->instance_created)
			ret = gvt_destroy_instance(gvt);
		if (ret)
			WPRINTF(("GVT: %s: failed: errno=%d\n", __func__, ret));

		free(gvt);
		pi->arg = NULL;
		gvt_dev = NULL;
	}
}

struct pci_vdev_ops pci_ops_gvt = {
	.class_name	= "pci-gvt",
	.vdev_init	= pci_gvt_init,
	.vdev_deinit	= pci_gvt_deinit,
	.vdev_barwrite	= pci_gvt_write,
	.vdev_barread	= pci_gvt_read,
};

DEFINE_PCI_DEVTYPE(pci_ops_gvt);
