/*
 * Copyright (C) 2019-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 *  ACRN Inter-VM Virtualizaiton based on ivshmem-v1 device
 *
 *  +----------+    +-----------------------------------------+    +----------+
 *  |Postlaunch|    |               Service OS                |    |Postlaunch|
 *  |    VM    |    |                                         |    |    VM    |
 *  |          |    |                Interrupt                |    |          |
 *  |+--------+|    |+----------+     Foward      +----------+|    |+--------+|
 *  ||  App   ||    || acrn-dm  |    +-------+    | acrn-dm  ||    ||  App   ||
 *  ||        ||    ||+--------+|    |ivshmem|    |+--------+||    ||        ||
 *  |+---+----+|    |||ivshmem ||<---+server +--->||ivshmem |||    |+---+----+|
 *  |    |     |  +-+++   dm   ||    +-------+    ||   dm   +++-+  |    |     |
 *  |    |     |  | ||+---+----+|                 |+----+---+|| |  |    |     |
 *  |+---+----+|  | |+----^-----+                 +-----^----+| |  |+---+----+|
 *  ||UIO     ||  | |     +---------------+-------------+     | |  ||UIO     ||
 *  ||driver  ||  | |                     v                   | |  ||driver  ||
 *  |+---+----+|  | |            +--------+-------+           | |  |+---+----+|
 *  |    |     |  | |            |    /dev/shm    |           | |  |    |     |
 *  |    |     |  | |            +--------+-------+           | |  |    |     |
 *  |+---+----+|  | |                     |                   | |  |+---+----+|
 *  ||ivshmem ||  | |            +--------+-------+           | |  ||ivshmem ||
 *  ||device  ||  | |            | Shared Memory  |           | |  ||device  ||
 *  |+---+----+|  | |            +----------------+           | |  |+---+----+|
 *  +----+-----+  | +-----------------------------------------+ |  +----+-----+
 *       +--------+                                             +-------+
 *
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#include "pci_core.h"
#include "vmmapi.h"
#include "dm_string.h"
#include "log.h"

#define	IVSHMEM_MMIO_BAR	0
#define	IVSHMEM_MSIX_BAR	1
#define	IVSHMEM_MEM_BAR		2

#define	IVSHMEM_VENDOR_ID	0x1af4
#define	IVSHMEM_DEVICE_ID	0x1110
#define	IVSHMEM_CLASS		0x05
#define	IVSHMEM_REV		0x01


/* IVSHMEM MMIO Registers */
#define	IVSHMEM_REG_SIZE	0x100
#define	IVSHMEM_IRQ_MASK_REG	0x00
#define	IVSHMEM_IRQ_STA_REG	0x04
#define	IVSHMEM_IV_POS_REG	0x08
#define	IVSHMEM_DOORBELL_REG	0x0c
#define	IVSHMEM_RESERVED_REG	0x0f

/*Size of MSIX BAR of ivshmem device  should be 4KB-aligned.*/
#define	IVSHMEM_MSIX_PBA_SIZE	0x1000

#define hv_land_prefix	"hv:/"
#define dm_land_prefix	"dm:/"

struct pci_ivshmem_vdev {
	struct pci_vdev	*dev;
	char		*name;
	int		fd;
	void		*addr;
	uint32_t	size;
	bool		is_hv_land;
};

static int
create_ivshmem_from_dm(struct vmctx *ctx, struct pci_vdev *vdev,
		const char *name, uint32_t size)
{
	struct stat st;
	int fd = -1;
	void *addr = NULL;
	bool is_shm_creator = false;
	struct pci_ivshmem_vdev *ivshmem_vdev = (struct pci_ivshmem_vdev *) vdev->arg;
	uint64_t bar_addr;

	fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
	if (fd > 0)
		is_shm_creator = true;
	else if (fd < 0 && errno == EEXIST)
		fd = shm_open(name, O_RDWR, 0600);

	if (fd < 0) {
		pr_warn("failed to get %s status, error %s\n",
				name, strerror(errno));
		goto err;
	}
	if (is_shm_creator) {
		if (ftruncate(fd, size) < 0) {
			pr_warn("can't resize the shm size %u\n", size);
			goto err;
		}
	} else {
		if ((fstat(fd, &st) < 0) || st.st_size != size) {
			pr_warn("shm size is different, cur %u, creator %ld\n",
				size, st.st_size);
			goto err;
		}
	}

	addr = (void *)mmap(NULL, size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0);
	bar_addr = pci_get_cfgdata32(vdev, PCIR_BAR(IVSHMEM_MEM_BAR));
	bar_addr |= ((uint64_t)pci_get_cfgdata32(vdev, PCIR_BAR(IVSHMEM_MEM_BAR + 1)) << 32);
	bar_addr &= PCIM_BAR_MEM_BASE;
	pr_dbg("shm configuration, vma 0x%lx, ivshmem bar 0x%lx, size 0x%x\n",
			(uint64_t)addr, bar_addr, size);
	if (!addr || vm_map_memseg_vma(ctx, size, bar_addr,
				(uint64_t)addr, PROT_RW) < 0) {
		pr_warn("failed to map shared memory\n");
		goto err;
	}

	ivshmem_vdev->name = strdup(name);
	if (!ivshmem_vdev->name) {
		pr_warn("No memory for shm_name\n");
		goto err;
	}
	ivshmem_vdev->fd = fd;
	ivshmem_vdev->addr = addr;
	ivshmem_vdev->size = size;
	return 0;
err:
	if (addr)
		munmap(addr, size);
	if (fd > 0)
		close(fd);
	return -1;
}

static int
create_ivshmem_from_hv(struct vmctx *ctx, struct pci_vdev *vdev,
		const char *shm_name, uint32_t shm_size)
{
	struct acrn_vdev dev = {};
	uint64_t addr = 0;

	dev.id.fields.vendor = IVSHMEM_VENDOR_ID;
	dev.id.fields.device = IVSHMEM_DEVICE_ID;
	dev.slot = PCI_BDF(vdev->bus, vdev->slot, vdev->func);
	dev.io_addr[IVSHMEM_MMIO_BAR] = pci_get_cfgdata32(vdev,
			PCIR_BAR(IVSHMEM_MMIO_BAR));

	/*MSI-x entry table BAR(BAR1)*/
	addr = pci_get_cfgdata32(vdev, PCIR_BAR(IVSHMEM_MSIX_BAR));
	dev.io_addr[IVSHMEM_MSIX_BAR] = addr;

	addr = pci_get_cfgdata32(vdev, PCIR_BAR(IVSHMEM_MEM_BAR));
	addr |= 0x0c; /* 64bit, prefetchable */
	dev.io_addr[IVSHMEM_MEM_BAR] = addr;
	addr = pci_get_cfgdata32(vdev, PCIR_BAR(IVSHMEM_MEM_BAR + 1));
	dev.io_addr[IVSHMEM_MEM_BAR + 1] = addr;
	dev.io_size[IVSHMEM_MEM_BAR] = shm_size;
	strncpy((char*)dev.args, shm_name, 32);
	return vm_add_hv_vdev(ctx, &dev);
}

static void
pci_ivshmem_write(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
	       int baridx, uint64_t offset, int size, uint64_t value)
{
	pr_dbg("%s: baridx %d, offset = %lx, value = 0x%lx\n",
			__func__, baridx, offset, value);

	if (baridx == IVSHMEM_MMIO_BAR) {
		switch (offset) {
		/*
		 * Following registers are used to support
		 * notification/interrupt in future.
		 */
		case IVSHMEM_IRQ_MASK_REG:
		case IVSHMEM_IRQ_STA_REG:
			break;
		case IVSHMEM_DOORBELL_REG:
			pr_warn("Doorbell capability doesn't support for now, ignore vectors 0x%lx, peer id %lu\n",
					value & 0xff, ((value >> 16) & 0xff));
			break;
		default:
			pr_dbg("%s: invalid device register 0x%lx\n",
					__func__, offset);
			break;
		}
	}
}

uint64_t
pci_ivshmem_read(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
	      int baridx, uint64_t offset, int size)
{
	uint64_t val = ~0;

	pr_dbg("%s: baridx %d, offset = 0x%lx, size = 0x%x\n",
			__func__, baridx, offset, size);

	if (baridx == IVSHMEM_MMIO_BAR) {
		switch (offset) {
		/*
		 * Following registers are used to support
		 * notification/interrupt in future.
		 */
		case IVSHMEM_IRQ_MASK_REG:
		case IVSHMEM_IRQ_STA_REG:
			val = 0;
			break;
		/*
		 * If ivshmem device doesn't support interrupt,
		 * The IVPosition is zero. otherwise, it is Peer ID.
		 */
		case IVSHMEM_IV_POS_REG:
			val = 0;
			break;
		default:
			pr_dbg("%s: invalid device register 0x%lx\n",
					__func__, offset);
			break;
		}
	}

	switch (size) {
	case 1:
		val &= 0xFF;
		break;
	case 2:
		val &= 0xFFFF;
		break;
	case 4:
		val &= 0xFFFFFFFF;
		break;
	}

	return val;
}

static int
pci_ivshmem_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	uint32_t size;
	char *tmp, *name, *orig;
	struct pci_ivshmem_vdev *ivshmem_vdev = NULL;
	bool is_hv_land;
	int rc;

	/* ivshmem device usage: "-s N,ivshmem,shm_name,shm_size" */
	tmp = orig = strdup(opts);
	if (!orig) {
		pr_warn("No memory for strdup\n");
		goto err;
	}
	name = strsep(&tmp, ",");
	if (!name) {
		pr_warn("the shared memory size is not set\n");
		goto err;
	}

	if (!strncmp(name, hv_land_prefix, strlen(hv_land_prefix))) {
		is_hv_land = true;
	} else if (!strncmp(name, dm_land_prefix, strlen(dm_land_prefix))) {
		is_hv_land = false;
		name += strlen(dm_land_prefix);
	} else {
		pr_warn("the ivshmem memory prefix name is incorrect\n");
		goto err;
	}

	if (dm_strtoui(tmp, &tmp, 10, &size) != 0) {
		pr_warn("the shared memory size is incorrect, %s\n", tmp);
		goto err;
	}
	size *= 0x100000; /* convert to megabytes */
	if (size < 0x200000 || size >= 0x40000000 ||
			(size & (size - 1)) != 0) {
		pr_warn("Invalid shared memory size %u, the size range is [2MB,512MB], the unit is megabyte and the value must be a power of 2\n",
			size/0x100000);
		goto err;
	}

	ivshmem_vdev = calloc(1, sizeof(struct pci_ivshmem_vdev));
	if (!ivshmem_vdev) {
		pr_warn("failed to allocate ivshmem device\n");
		goto err;
	}

	ivshmem_vdev->dev = dev;
	ivshmem_vdev->is_hv_land = is_hv_land;
	dev->arg = ivshmem_vdev;

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_VENDOR, IVSHMEM_VENDOR_ID);
	pci_set_cfgdata16(dev, PCIR_DEVICE, IVSHMEM_DEVICE_ID);
	pci_set_cfgdata16(dev, PCIR_REVID, IVSHMEM_REV);
	pci_set_cfgdata8(dev, PCIR_CLASS, IVSHMEM_CLASS);

	pci_emul_alloc_bar(dev, IVSHMEM_MMIO_BAR, PCIBAR_MEM32, IVSHMEM_REG_SIZE);
	pci_emul_alloc_bar(dev, IVSHMEM_MSIX_BAR, PCIBAR_MEM32, IVSHMEM_MSIX_PBA_SIZE);
	pci_emul_alloc_bar(dev, IVSHMEM_MEM_BAR, PCIBAR_MEM64, size);

	if (is_hv_land) {
		rc = create_ivshmem_from_hv(ctx, dev, name, size);
	} else {
		/*
		 * TODO: If User VM reprograms ivshmem BAR2, the shared memory will be
		 * unavailable for User VM, so we need to remap GPA and HPA of shared
		 * memory in this case.
		 */
		rc = create_ivshmem_from_dm(ctx, dev, name, size);
	}
	if (rc < 0)
		goto err;

	free(orig);
	return 0;
err:
	if (orig)
		free(orig);
	if (ivshmem_vdev) {
		free(ivshmem_vdev);
		dev->arg = NULL;
	}
	return -1;
}

static void
destroy_ivshmem_from_dm(struct pci_ivshmem_vdev *vdev)
{
	if (vdev->addr && vdev->size)
		munmap(vdev->addr, vdev->size);
	if (vdev->fd > 0)
		close(vdev->fd);
}

static void
destroy_ivshmem_from_hv(struct vmctx *ctx, struct pci_vdev *vdev)
{

	struct acrn_vdev emul_dev = {};

	emul_dev.id.fields.vendor = IVSHMEM_VENDOR_ID;
	emul_dev.id.fields.device = IVSHMEM_DEVICE_ID;
	emul_dev.slot = PCI_BDF(vdev->bus, vdev->slot, vdev->func);
	vm_remove_hv_vdev(ctx, &emul_dev);
}

static void
pci_ivshmem_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct pci_ivshmem_vdev *vdev;

	vdev = (struct pci_ivshmem_vdev *)dev->arg;
	if (!vdev) {
		pr_warn("%s, invalid ivshmem instance\n", __func__);
		return;
	}
	if (vdev->is_hv_land)
		destroy_ivshmem_from_hv(ctx, dev);
	else
		destroy_ivshmem_from_dm(vdev);

	if (vdev->name) {
		/*
		 * shm_unlink will only remove the shared memory file object,
		 * the shared memory will be released until all processes
		 * which opened the shared memory close the file.
		 *
		 * Don't invoke shm_unlink(vdev->name) to remove file object now,
		 * so that the acrn-dm can communicate with the peer again after
		 * rebooting/shutdown, the side effect is that the shared memory
		 * will not be released even if all peers exit.
		 */
		free(vdev->name);
	}

	free(vdev);
	dev->arg = NULL;
}

struct pci_vdev_ops pci_ops_ivshmem = {
	.class_name	= "ivshmem",
	.vdev_init	= pci_ivshmem_init,
	.vdev_deinit	= pci_ivshmem_deinit,
	.vdev_barwrite	= pci_ivshmem_write,
	.vdev_barread	= pci_ivshmem_read
};
DEFINE_PCI_DEVTYPE(pci_ops_ivshmem);
