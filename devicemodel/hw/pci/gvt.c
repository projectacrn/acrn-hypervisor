/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/*
 * The interface of gvt is obsoleted.
 * We reserve these code for future.
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

#define DPRINTF(params) do { if (pci_gvt_debug) pr_dbg params; } while (0)

#define WPRINTF(params) (pr_err params)

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

void
update_gvt_bar(struct vmctx *ctx)
{
	char bar_path[PATH_MAX];
	int bar_fd;
	int ret;
	char resource[76];
	char *next;
	uint64_t bar0_start_addr, bar0_end_addr, bar2_start_addr, bar2_end_addr;
	int i;

	/* "/sys/kernel/gvt/vmx/vgpu_bar_info" exposes vgpu bar regions. */
	snprintf(bar_path, sizeof(bar_path),
		"/sys/kernel/gvt/vm%d/vgpu_bar_info",
		ctx->vmid);

	if(access(bar_path, F_OK) == -1)
		return;

	bar_fd = open(bar_path, O_RDONLY);
	if(bar_fd == -1){
		pr_err("failed to open sys bar info\n");
		return;
	}

	ret = pread(bar_fd, resource, 76, 0);

	close(bar_fd);

	if (ret < 76) {
		pr_err("failed to read sys bar info\n");
		return;
	}

	next = resource;
	bar0_start_addr = strtoull(next, &next, 16);
	bar0_end_addr = strtoull(next, &next, 16) + bar0_start_addr -1;
	bar2_start_addr = strtoull(next, &next, 16);
	bar2_end_addr = strtoull(next, &next, 16) + bar2_start_addr -1;

	for(i = 0; i < REGION_NUMS; i++){
		if(reserved_bar_regions[i].vdev &&
			reserved_bar_regions[i].vdev == gvt_dev){
			pci_emul_free_bar(gvt_dev, reserved_bar_regions[i].idx);
		}
	}

	destory_io_rsvd_rgns(gvt_dev);

	ret = reserve_io_rgn(bar0_start_addr,
                        bar0_end_addr, 0, PCIBAR_MEM32, gvt_dev);
	if(ret != 0)
		return;
	ret = reserve_io_rgn(bar2_start_addr,
                        bar2_end_addr, 2, PCIBAR_MEM32, gvt_dev);
	if(ret != 0)
		return;

	pci_emul_alloc_bar(gvt_dev, 0, PCIBAR_MEM32,
			bar0_end_addr - bar0_start_addr + 1);
	pci_emul_alloc_bar(gvt_dev, 2, PCIBAR_MEM32,
			bar2_end_addr - bar2_start_addr + 1);
}

static int
gvt_init_config(struct pci_gvt *gvt)
{
	int ret;
	char name[PATH_MAX];
	uint8_t cap_ptr = 0;
	char res_name[PATH_MAX];
	char resource[512];
	int res_fd;
	uint64_t bar0_start_addr;
	uint64_t bar0_end_addr;
	uint64_t bar2_start_addr;
	uint64_t bar2_end_addr;
	char *next;
	struct vmctx *ctx;

	/* get physical gpu bars info from
	 * "/sys/bus/PCI/devices/0000\:00\:02.0/resource"
	 */
	snprintf(res_name, sizeof(res_name),
		"/sys/bus/pci/devices/%04x:%02x:%02x.%x/resource",
		gvt->addr.domain, gvt->addr.bus, gvt->addr.slot,
		gvt->addr.function);
	res_fd = open(res_name, O_RDONLY);
	if (res_fd == -1) {
		pr_err("gvt:open host pci resource failed\n");
		return -1;
	}

	ret = pread(res_fd, resource, 512, 0);

	close(res_fd);

	if (ret < 512) {
		pr_err("failed to read host device resource space\n");
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
	if(bar0_start_addr < PCI_EMUL_MEMBASE32
		|| bar2_start_addr < PCI_EMUL_MEMBASE32
		|| bar0_end_addr > PCI_EMUL_MEMLIMIT32
		|| bar2_end_addr > PCI_EMUL_MEMLIMIT32){
		pr_err("gvt pci bases are out of range\n");
		return -1;
	}

	ctx->gvt_enabled = true;
	ctx->update_gvt_bar = &update_gvt_bar;

	/* In GVT-g design, it only use pci bar0 and bar2,
	 * So we need reserve bar0 region and bar2 region only
	 */
	ret = reserve_io_rgn(bar0_start_addr,
				bar0_end_addr, 0, PCIBAR_MEM32, gvt->gvt_pi);
	if(ret != 0)
		return -1;
	ret = reserve_io_rgn(bar2_start_addr,
				bar2_end_addr, 2, PCIBAR_MEM32, gvt->gvt_pi);
	if(ret != 0)
		return -1;
	snprintf(name, sizeof(name),
		"/sys/bus/pci/devices/%04x:%02x:%02x.%x/config",
		gvt->addr.domain, gvt->addr.bus, gvt->addr.slot,
		gvt->addr.function);
	gvt->host_config_fd = open(name, O_RDONLY);
	if (gvt->host_config_fd == -1) {
		pr_err("gvt:open host pci config failed\n");
		return -1;
	}

	ret = pread(gvt->host_config_fd, gvt->host_config, PCI_REGMAX+1, 0);

	close(gvt->host_config_fd);

	if (ret <= PCI_REGMAX) {
		pr_err("failed to read host device config space\n");
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
	while (cap_ptr != 0 && cap_ptr <= PCI_REGMAX - 15) {
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

	ret = pci_emul_alloc_bar(gvt->gvt_pi, 0, PCIBAR_MEM32,
		bar0_end_addr - bar0_start_addr + 1);
	if (ret != 0) {
		pr_err("allocate gvt pci bar[0] failed\n");
		return -1;
	}

	ret = pci_emul_alloc_bar(gvt->gvt_pi, 2, PCIBAR_MEM32,
		bar2_end_addr - bar2_start_addr + 1);
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
		pr_err("gvt:calloc gvt failed\n");
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
	ctx->gvt_enabled = false;
	pr_err("GVT: init failed\n");
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

		destory_io_rsvd_rgns(gvt_dev);
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
