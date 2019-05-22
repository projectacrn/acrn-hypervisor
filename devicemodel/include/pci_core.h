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

#ifndef _PCI_CORE_H_
#define _PCI_CORE_H_

#include <sys/queue.h>

#include <assert.h>
#include <stdbool.h>
#include "types.h"
#include "pcireg.h"

#define	PCI_BARMAX	PCIR_MAX_BAR_0	/* BAR registers in a Type 0 header */
#define	PCI_BDF(b, d, f) (((b & 0xFF) << 8) | ((d & 0x1F) << 3) | ((f & 0x7)))

#define	PCI_EMUL_ECFG_BASE	0xE0000000UL	/* 3.5GB */

#define	PCI_EMUL_MEMBASE64	0x100000000UL	/* 4GB */
#define	PCI_EMUL_MEMLIMIT64	0x140000000UL	/* 5GB */

struct vmctx;
struct pci_vdev;
struct memory_region;

struct pci_vdev_ops {
	char	*class_name;		/* Name of device class */

	/* instance creation */
	int	(*vdev_init)(struct vmctx *, struct pci_vdev *,
			     char *opts);

	/* instance deinit */
	void	(*vdev_deinit)(struct vmctx *, struct pci_vdev *,
			char *opts);

	/* ACPI DSDT enumeration */
	void	(*vdev_write_dsdt)(struct pci_vdev *);

	/* ops related to physical resources */
	void	(*vdev_phys_access)(struct vmctx *ctx, struct pci_vdev *dev);

	/* update BAR map callback */
	void	(*vdev_update_bar_map)(struct vmctx *ctx,
				struct pci_vdev *dev, int idx,
				uint64_t orig_addr);

	/* config space read/write callbacks */
	int	(*vdev_cfgwrite)(struct vmctx *ctx, int vcpu,
			       struct pci_vdev *pi, int offset,
			       int bytes, uint32_t val);
	int	(*vdev_cfgread)(struct vmctx *ctx, int vcpu,
			      struct pci_vdev *pi, int offset,
			      int bytes, uint32_t *retval);

	/* BAR read/write callbacks */
	void	(*vdev_barwrite)(struct vmctx *ctx, int vcpu,
				 struct pci_vdev *pi, int baridx,
				 uint64_t offset, int size, uint64_t value);
	uint64_t  (*vdev_barread)(struct vmctx *ctx, int vcpu,
				struct pci_vdev *pi, int baridx,
				uint64_t offset, int size);
};

/*
 * Put all PCI instances' addresses into one section named pci_devemu_set
 * so that DM could enumerate and initialize each of them.
 */
#define DEFINE_PCI_DEVTYPE(x)	DATA_SET(pci_vdev_ops_set, x)

enum pcibar_type {
	PCIBAR_NONE,
	PCIBAR_IO,
	PCIBAR_MEM32,
	PCIBAR_MEM64,
	PCIBAR_MEMHI64
};

struct pcibar {
	enum pcibar_type	type;		/* io or memory */
	uint64_t		size;
	uint64_t		addr;
	bool			sizing;
};

#define PI_NAMESZ	40

struct msix_table_entry {
	uint64_t	addr;
	uint32_t	msg_data;
	uint32_t	vector_control;
} __attribute__((packed));

/*
 * In case the structure is modified to hold extra information, use a define
 * for the size that should be emulated.
 */
#define	MSIX_TABLE_ENTRY_SIZE	16
#define MAX_MSIX_TABLE_ENTRIES	2048
#define	PBA_SIZE(msgnum)	(roundup2((msgnum), 64) / 8)

enum lintr_stat {
	IDLE,
	ASSERTED,
	PENDING
};

struct pci_vdev {
	struct pci_vdev_ops *dev_ops;
	struct vmctx *vmctx;
	uint8_t	bus, slot, func;
	char	name[PI_NAMESZ];
	int	bar_getsize;
	int	prevcap;
	int	capend;

	struct {
		int8_t	pin;
		enum lintr_stat	state;
		int		pirq_pin;
		int		ioapic_irq;
		pthread_mutex_t	lock;
	} lintr;

	struct {
		int		enabled;
		uint64_t	addr;
		uint64_t	msg_data;
		int		maxmsgnum;
	} msi;

	struct {
		int	enabled;
		int	table_bar;
		int	pba_bar;
		uint32_t table_offset;
		int	table_count;
		uint32_t pba_offset;
		int	pba_size;
		int	function_mask;
		struct msix_table_entry *table;	/* allocated at runtime */
		void	*pba_page;
		int	pba_page_offset;
	} msix;

	void	*arg;		/* devemu-private data */

	uint8_t	cfgdata[PCI_REGMAX + 1];
	struct pcibar bar[PCI_BARMAX + 1];
};

struct gsi_dev {
	/*
	 * For PCI devices, use a string "b:d.f" to stand for the device's BDF,
	 *  such as "00:00.0".
	 * For non-PCI devices, use the device's name to stand for the device,
	 *  such as "timer".
	 */
	char *dev_name;
	uint8_t gsi;
};

extern struct gsi_dev gsi_dev_mapping_tables[];
extern int num_gsi_dev_mapping_tables;

struct msicap {
	uint8_t		capid;
	uint8_t		nextptr;
	uint16_t	msgctrl;
	uint32_t	addrlo;
	uint32_t	addrhi;
	uint16_t	msgdata;
} __attribute__((packed));
static_assert(sizeof(struct msicap) == 14, "compile-time assertion failed");

struct msixcap {
	uint8_t		capid;
	uint8_t		nextptr;
	uint16_t	msgctrl;
	uint32_t	table_info;	/* bar index and offset within it */
	uint32_t	pba_info;	/* bar index and offset within it */
} __attribute__((packed));
static_assert(sizeof(struct msixcap) == 12, "compile-time assertion failed");

struct pciecap {
	uint8_t		capid;
	uint8_t		nextptr;
	uint16_t	pcie_capabilities;

	uint32_t	dev_capabilities;	/* all devices */
	uint16_t	dev_control;
	uint16_t	dev_status;

	uint32_t	link_capabilities;	/* devices with links */
	uint16_t	link_control;
	uint16_t	link_status;

	uint32_t	slot_capabilities;	/* ports with slots */
	uint16_t	slot_control;
	uint16_t	slot_status;

	uint16_t	root_control;		/* root ports */
	uint16_t	root_capabilities;
	uint32_t	root_status;

	uint32_t	dev_capabilities2;	/* all devices */
	uint16_t	dev_control2;
	uint16_t	dev_status2;

	uint32_t	link_capabilities2;	/* devices with links */
	uint16_t	link_control2;
	uint16_t	link_status2;

	uint32_t	slot_capabilities2;	/* ports with slots */
	uint16_t	slot_control2;
	uint16_t	slot_status2;
} __attribute__((packed));
static_assert(sizeof(struct pciecap) == 60, "compile-time assertion failed");

typedef void (*pci_lintr_cb)(int b, int s, int pin, int pirq_pin,
			     int ioapic_irq, void *arg);

int	init_pci(struct vmctx *ctx);
void	deinit_pci(struct vmctx *ctx);
void	msicap_cfgwrite(struct pci_vdev *pi, int capoff, int offset,
			int bytes, uint32_t val);
void	msixcap_cfgwrite(struct pci_vdev *pi, int capoff, int offset,
			 int bytes, uint32_t val);
void	pci_callback(void);
int	pci_emul_alloc_bar(struct pci_vdev *pdi, int idx,
			   enum pcibar_type type, uint64_t size);
int	pci_emul_alloc_pbar(struct pci_vdev *pdi, int idx,
			    uint64_t hostbase, enum pcibar_type type,
			    uint64_t size);
void	pci_emul_free_bars(struct pci_vdev *pdi);
int	pci_emul_add_capability(struct pci_vdev *dev, u_char *capdata,
				int caplen);
int	pci_emul_find_capability(struct pci_vdev *dev, uint8_t capid,
				 int *p_capoff);
int	pci_emul_add_msicap(struct pci_vdev *pi, int msgnum);
int	pci_emul_add_pciecap(struct pci_vdev *pi, int pcie_device_type);

/**
 * @brief Generate a MSI interrupt to guest
 *
 * @param dev Pointer to struct pci_vdev representing virtual PCI device.
 * @param index Message data index.
 *
 * @return None
 */
void	pci_generate_msi(struct pci_vdev *dev, int index);

/**
 * @brief Generate a MSI-X interrupt to guest
 *
 * @param dev Pointer to struct pci_vdev representing virtual PCI device.
 * @param index MSIs table entry index.
 *
 * @return None
 */
void	pci_generate_msix(struct pci_vdev *dev, int index);

/**
 * @brief Assert INTx pin of virtual PCI device
 *
 * @param dev Pointer to struct pci_vdev representing virtual PCI device.
 *
 * @return None
 */
void	pci_lintr_assert(struct pci_vdev *dev);

/**
 * @brief Deassert INTx pin of virtual PCI device
 *
 * @param dev Pointer to struct pci_vdev representing virtual PCI device.
 *
 * @return None
 */
void	pci_lintr_deassert(struct pci_vdev *dev);

void	pci_lintr_request(struct pci_vdev *pi);
void	pci_lintr_release(struct pci_vdev *pi);
int	pci_msi_enabled(struct pci_vdev *pi);
int	pci_msix_enabled(struct pci_vdev *pi);
int	pci_msix_table_bar(struct pci_vdev *pi);
int	pci_msix_pba_bar(struct pci_vdev *pi);
int	pci_msi_maxmsgnum(struct pci_vdev *pi);
int	pci_parse_slot(char *opt);
void	pci_populate_msicap(struct msicap *cap, int msgs, int nextptr);
int	pci_emul_add_msixcap(struct pci_vdev *pi, int msgnum, int barnum);
int	pci_emul_msix_twrite(struct pci_vdev *pi, uint64_t offset, int size,
			     uint64_t value);
uint64_t pci_emul_msix_tread(struct pci_vdev *pi, uint64_t offset, int size);
int	pci_count_lintr(int bus);
void	pci_walk_lintr(int bus, pci_lintr_cb cb, void *arg);
void	pci_write_dsdt(void);
int	pci_bus_configured(int bus);
int	emulate_pci_cfgrw(struct vmctx *ctx, int vcpu, int in, int bus,
			  int slot, int func, int reg, int bytes, int *value);
int	create_gsi_sharing_groups(void);
void	update_pt_info(uint16_t phys_bdf);
int	check_gsi_sharing_violation(void);
int	pciaccess_init(void);
void	pciaccess_cleanup(void);
int	parse_bdf(char *s, int *bus, int *dev, int *func, int base);
struct pci_vdev *pci_get_vdev_info(int slot);


/**
 * @brief Set virtual PCI device's configuration space in 1 byte width
 *
 * @param dev Pointer to struct pci_vdev representing virtual PCI device.
 * @param offset Offset in configuration space.
 * @param val Value in 1 byte.
 *
 * @return None
 */
static inline void
pci_set_cfgdata8(struct pci_vdev *dev, int offset, uint8_t val)
{
	assert(offset <= PCI_REGMAX);
	*(uint8_t *)(dev->cfgdata + offset) = val;
}

/**
 * @brief Set virtual PCI device's configuration space in 2 bytes width
 *
 * @param dev Pointer to struct pci_vdev representing virtual PCI device.
 * @param offset Offset in configuration space.
 * @param val Value in 2 bytes.
 *
 * @return None
 */
static inline void
pci_set_cfgdata16(struct pci_vdev *dev, int offset, uint16_t val)
{
	assert(offset <= (PCI_REGMAX - 1) && (offset & 1) == 0);
	*(uint16_t *)(dev->cfgdata + offset) = val;
}

/**
 * @brief Set virtual PCI device's configuration space in 4 bytes width
 *
 * @param dev Pointer to struct pci_vdev representing virtual PCI device.
 * @param offset Offset in configuration space.
 * @param val Value in 4 bytes.
 *
 * @return None
 */
static inline void
pci_set_cfgdata32(struct pci_vdev *dev, int offset, uint32_t val)
{
	assert(offset <= (PCI_REGMAX - 3) && (offset & 3) == 0);
	*(uint32_t *)(dev->cfgdata + offset) = val;
}

/**
 * @brief Get virtual PCI device's configuration space in 1 byte width
 *
 * @param dev Pointer to struct pci_vdev representing virtual PCI device.
 * @param offset Offset in configuration space.
 *
 * @return The configuration value in 1 byte.
 */
static inline uint8_t
pci_get_cfgdata8(struct pci_vdev *dev, int offset)
{
	assert(offset <= PCI_REGMAX);
	return (*(uint8_t *)(dev->cfgdata + offset));
}

/**
 * @brief Get virtual PCI device's configuration space in 2 byte width
 *
 * @param dev Pointer to struct pci_vdev representing virtual PCI device.
 * @param offset Offset in configuration space.
 *
 * @return The configuration value in 2 bytes.
 */
static inline uint16_t
pci_get_cfgdata16(struct pci_vdev *dev, int offset)
{
	assert(offset <= (PCI_REGMAX - 1) && (offset & 1) == 0);
	return (*(uint16_t *)(dev->cfgdata + offset));
}

/**
 * @brief Get virtual PCI device's configuration space in 4 byte width
 *
 * @param dev Pointer to struct pci_vdev representing virtual PCI device.
 * @param offset Offset in configuration space.
 *
 * @return The configuration value in 4 bytes.
 */
static inline uint32_t
pci_get_cfgdata32(struct pci_vdev *dev, int offset)
{
	assert(offset <= (PCI_REGMAX - 3) && (offset & 3) == 0);
	return (*(uint32_t *)(dev->cfgdata + offset));
}

#endif /* _PCI_CORE_H_ */
