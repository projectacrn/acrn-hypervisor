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
#include <assert.h>

#include "dm.h"
#include "pci_core.h"
#include "vmmapi.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

static int pci_gvt_debug;

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
	uint8_t *host_config;
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
	if (parse_bdf(arg, &gvt_low_gm_sz, &gvt_high_gm_sz,
				&gvt_fence_sz, 10) != 0) {
		return -1;
	}
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

static int
gvt_init_config(struct pci_gvt *gvt)
{
	int ret, len;
	char name[PATH_MAX];
	uint8_t cap_ptr = 0;

	gvt->host_config = calloc(1, 256);
	if (!gvt->host_config) {
		perror("gvt:calloc host config failed\n");
		return -1;
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

	len = 256;
	ret = pread(gvt->host_config_fd, gvt->host_config, len, 0);
	if (ret < len) {
		ret = ret < 0 ? -errno : -EFAULT;
		perror("failed to read host device config space");
		return ret;
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

	ret = pci_emul_alloc_bar(gvt->gvt_pi, 0, PCIBAR_MEM32,
		16 * 1024 * 1024);
	assert(ret == 0);
	/* same as host, but guest only use partition of it by ballon */
	ret = pci_emul_alloc_bar(gvt->gvt_pi, 2, PCIBAR_MEM32,
		256 * 1024 * 1024);
	assert(ret == 0);
	/* same as host, lagecy vga usage */
	ret = pci_emul_alloc_bar(gvt->gvt_pi, 4, PCIBAR_IO, 64);
	assert(ret == 0);

	close(gvt->host_config_fd);

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
		return errno;
	}

	DPRINTF(("GVT: %s: domid=%d, low_gm_sz=%dMB, high_gm_sz=%dMB, "
			"fence_sz=%d\n", __func__, guest_domid,
			gvt_low_gm_sz, gvt_high_gm_sz, gvt_fence_sz));

	if (gvt_low_gm_sz <= 0 || gvt_high_gm_sz <= 0 || gvt_fence_sz <= 0) {
		WPRINTF(("GVT: %s failed: invalid parameters!\n", __func__));
		abort();
	}

	/* The format of the string is:
	 * domid,aperture_size,gm_size,fence_size. This means we want the gvt
	 * driver to create a gvt instanc for Domain domid with the required
	 * parameters. NOTE: aperture_size and gm_size are in MB.
	 */
	if (fprintf(gvt->gvt_file, "%d,%u,%u,%u\n", guest_domid,
			gvt_low_gm_sz, gvt_high_gm_sz, gvt_fence_sz) < 0)
		ret = errno;

	if (fclose(gvt->gvt_file) != 0)
		return errno;

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
		return errno;
	}

	/* -domid means we want the gvt driver to free the gvt instance
	 * of Domain domid.
	 **/
	if (fprintf(gvt->gvt_file, "%d\n", -guest_domid) < 0)
		ret = errno;

	if (fclose(gvt->gvt_file) != 0)
		return errno;

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

	gvt->addr.domain = 0;
	gvt->addr.bus = pi->bus;
	gvt->addr.slot = pi->slot;
	gvt->addr.function = pi->func;

	pi->arg = gvt;
	gvt->gvt_pi = pi;
	guest_domid = ctx->vmid;

	ret = gvt_init_config(gvt);

	ret = gvt_create_instance(gvt);

	return ret;
}

void
pci_gvt_deinit(struct vmctx *ctx, struct pci_vdev *pi, char *opts)
{
	int ret;
	struct pci_gvt *gvt = pi->arg;

	ret = gvt_destroy_instance(gvt);
	if (ret)
		WPRINTF(("GVT: %s: failed: errno=%d\n", __func__, ret));
}

struct pci_vdev_ops pci_ops_gvt = {
	.class_name	= "pci-gvt",
	.vdev_init	= pci_gvt_init,
	.vdev_deinit	= pci_gvt_deinit,
	.vdev_barwrite	= pci_gvt_write,
	.vdev_barread	= pci_gvt_read,
};

DEFINE_PCI_DEVTYPE(pci_ops_gvt);
