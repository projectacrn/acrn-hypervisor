/*-
 * Copyright (c) 2013  Chris Torek <torek @ torek net>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

/**
 * @file virtio.h
 *
 * @brief Virtio Backend Service (VBS) APIs for ACRN Project
 */

#ifndef	_VIRTIO_H_
#define	_VIRTIO_H_

/*
 * These are derived from several virtio specifications.
 *
 * Some useful links:
 *    https://github.com/rustyrussell/virtio-spec
 *    http://people.redhat.com/pbonzini/virtio-spec.pdf
 */

/*
 * A virtual device has zero or more "virtual queues" (virtqueue).
 * Each virtqueue uses at least two 4096-byte pages, laid out thus:
 *
 *      +-----------------------------------------------+
 *      |    "desc":  <N> descriptors, 16 bytes each    |
 *      |   -----------------------------------------   |
 *      |   "avail":   2 uint16; <N> uint16; 1 uint16   |
 *      |   -----------------------------------------   |
 *      |              pad to 4k boundary               |
 *      +-----------------------------------------------+
 *      |   "used": 2 x uint16; <N> elems; 1 uint16     |
 *      |   -----------------------------------------   |
 *      |              pad to 4k boundary               |
 *      +-----------------------------------------------+
 *
 * The number <N> that appears here is always a power of two and is
 * limited to no more than 32768 (as it must fit in a 16-bit field).
 * If <N> is sufficiently large, the above will occupy more than
 * two pages.  In any case, all pages must be physically contiguous
 * within the guest's physical address space.
 *
 * The <N> 16-byte "desc" descriptors consist of a 64-bit guest
 * physical address <addr>, a 32-bit length <len>, a 16-bit
 * <flags>, and a 16-bit <next> field (all in guest byte order).
 *
 * There are three flags that may be set :
 *	NEXT    descriptor is chained, so use its "next" field
 *	WRITE   descriptor is for host to write into guest RAM
 *		(else host is to read from guest RAM)
 *	INDIRECT   descriptor address field is (guest physical)
 *		address of a linear array of descriptors
 *
 * Unless INDIRECT is set, <len> is the number of bytes that may
 * be read/written from guest physical address <addr>.  If
 * INDIRECT is set, WRITE is ignored and <len> provides the length
 * of the indirect descriptors (and <len> must be a multiple of
 * 16).  Note that NEXT may still be set in the main descriptor
 * pointing to the indirect, and should be set in each indirect
 * descriptor that uses the next descriptor (these should generally
 * be numbered sequentially).  However, INDIRECT must not be set
 * in the indirect descriptors.  Upon reaching an indirect descriptor
 * without a NEXT bit, control returns to the direct descriptors.
 *
 * Except inside an indirect, each <next> value must be in the
 * range [0 .. N) (i.e., the half-open interval).  (Inside an
 * indirect, each <next> must be in the range [0 .. <len>/16).)
 *
 * The "avail" data structures reside in the same pages as the
 * "desc" structures since both together are used by the device to
 * pass information to the hypervisor's virtual driver.  These
 * begin with a 16-bit <flags> field and 16-bit index <idx>, then
 * have <N> 16-bit <ring> values, followed by one final 16-bit
 * field <used_event>.  The <N> <ring> entries are simply indices
 * indices into the descriptor ring (and thus must meet the same
 * constraints as each <next> value).  However, <idx> is counted
 * up from 0 (initially) and simply wraps around after 65535; it
 * is taken mod <N> to find the next available entry.
 *
 * The "used" ring occupies a separate page or pages, and contains
 * values written from the virtual driver back to the guest OS.
 * This begins with a 16-bit <flags> and 16-bit <idx>, then there
 * are <N> "vring_used" elements, followed by a 16-bit <avail_event>.
 * The <N> "vring_used" elements consist of a 32-bit <id> and a
 * 32-bit <len> (tlen below).  The <id> is simply the index of
 * the head of a descriptor chain the guest made available
 * earlier, and the <len> is the number of bytes actually written,
 * e.g., in the case of a network driver that provided a large
 * receive buffer but received only a small amount of data.
 *
 * The two event fields, <used_event> and <avail_event>, in the
 * avail and used rings (respectively -- note the reversal!), are
 * always provided, but are used only if the virtual device
 * negotiates the ACRN_VIRTIO_RING_F_EVENT_IDX feature during feature
 * negotiation.  Similarly, both rings provide a flag --
 * ACRN_VRING_AVAIL_F_NO_INTERRUPT and ACRN_VRING_USED_F_NO_NOTIFY -- in
 * their <flags> field, indicating that the guest does not need an
 * interrupt, or that the hypervisor driver does not need a
 * notify, when descriptors are added to the corresponding ring.
 * (These are provided only for interrupt optimization and need
 * not be implemented.)
 */

#include "types.h"

/**
 * @brief virtio API
 *
 * @defgroup acrn_virtio virtio API
 * @{
 */

#define VRING_ALIGN	4096

#define ACRN_VRING_DESC_F_NEXT		(1 << 0)
#define ACRN_VRING_DESC_F_WRITE		(1 << 1)
#define ACRN_VRING_DESC_F_INDIRECT	(1 << 2)

struct virtio_desc {		/* AKA vring_desc */
	uint64_t	addr;	/* guest physical address */
	uint32_t	len;	/* length of scatter/gather seg */
	uint16_t	flags;	/* VRING_F_DESC_* */
	uint16_t	next;	/* next desc if F_NEXT */
} __attribute__((packed));

struct virtio_used {		/* AKA vring_used_elem */
	uint32_t	idx;	/* head of used descriptor chain */
	uint32_t	tlen;	/* length written-to */
} __attribute__((packed));

#define ACRN_VRING_AVAIL_F_NO_INTERRUPT	1

struct virtio_vring_avail {
	uint16_t	flags;	/* VRING_AVAIL_F_* */
	uint16_t	idx;	/* counts to 65535, then cycles */
	uint16_t	ring[];	/* size N, reported in QNUM value */
/*	uint16_t	used_event;	-- after N ring entries */
} __attribute__((packed));

#define	ACRN_VRING_USED_F_NO_NOTIFY	1
struct virtio_vring_used {
	uint16_t	flags;	/* VRING_USED_F_* */
	uint16_t	idx;	/* counts to 65535, then cycles */
	struct virtio_used ring[];
				/* size N */
/*	uint16_t	avail_event;	-- after N ring entries */
} __attribute__((packed));

/*
 * The address of any given virtual queue is determined by a single
 * Page Frame Number register.  The guest writes the PFN into the
 * PCI config space.  However, a device that has two or more
 * virtqueues can have a different PFN, and size, for each queue.
 * The number of queues is determinable via the PCI config space
 * VTCFG_R_QSEL register.  Writes to QSEL select the queue: 0 means
 * queue #0, 1 means queue#1, etc.  Once a queue is selected, the
 * remaining PFN and QNUM registers refer to that queue.
 *
 * QNUM is a read-only register containing a nonzero power of two
 * that indicates the (hypervisor's) queue size.  Or, if reading it
 * produces zero, the hypervisor does not have a corresponding
 * queue.  (The number of possible queues depends on the virtual
 * device.  The block device has just one; the network device
 * provides either two -- 0 = receive, 1 = transmit -- or three,
 * with 2 = control.)
 *
 * PFN is a read/write register giving the physical page address of
 * the virtqueue in guest memory (the guest must allocate enough space
 * based on the hypervisor's provided QNUM).
 *
 * QNOTIFY is effectively write-only: when the guest writes a queue
 * number to the register, the hypervisor should scan the specified
 * virtqueue. (Reading QNOTIFY currently always gets 0).
 */

/*
 * PFN register shift amount
 */
#define VRING_PAGE_BITS		12

/*
 * Virtio device types
 */
#define	VIRTIO_TYPE_NET		1
#define	VIRTIO_TYPE_BLOCK	2
#define	VIRTIO_TYPE_CONSOLE	3
#define	VIRTIO_TYPE_ENTROPY	4
#define	VIRTIO_TYPE_BALLOON	5
#define	VIRTIO_TYPE_IOMEMORY	6
#define	VIRTIO_TYPE_RPMSG	7
#define	VIRTIO_TYPE_SCSI	8
#define	VIRTIO_TYPE_9P		9
#define	VIRTIO_TYPE_INPUT	18

/*
 * ACRN virtio device types
 * Experimental IDs start at 0xFFFF and work down
 */
#define	VIRTIO_TYPE_RPMB	0xFFFF
#define	VIRTIO_TYPE_HECI	0xFFFE
#define	VIRTIO_TYPE_AUDIO	0xFFFD
#define	VIRTIO_TYPE_IPU		0xFFFC
#define	VIRTIO_TYPE_TSN		0xFFFB
#define	VIRTIO_TYPE_HYPERDMABUF	0xFFFA
#define	VIRTIO_TYPE_HDCP	0xFFF9
#define	VIRTIO_TYPE_COREU	0xFFF8

/*
 * PCI vendor/device IDs
 */
#define	INTEL_VENDOR_ID		0x8086
#define	VIRTIO_VENDOR		0x1AF4
#define	VIRTIO_DEV_NET		0x1000
#define	VIRTIO_DEV_BLOCK	0x1001
#define	VIRTIO_DEV_CONSOLE	0x1003
#define	VIRTIO_DEV_RANDOM	0x1005

/*
 * ACRN virtio device IDs
 */
#define	VIRTIO_DEV_RPMB		0x8601
#define	VIRTIO_DEV_HECI		0x8602
#define	VIRTIO_DEV_AUDIO	0x8603
#define	VIRTIO_DEV_IPU		0x8604
#define	VIRTIO_DEV_TSN		0x8605
#define	VIRTIO_DEV_HYPERDMABUF	0x8606
#define	VIRTIO_DEV_HDCP		0x8607
#define	VIRTIO_DEV_COREU	0x8608

/*
 * PCI config space constants.
 *
 * If MSI-X is enabled, the ISR register is generally not used,
 * and the configuration vector and queue vector appear at offsets
 * 20 and 22 with the remaining configuration registers at 24.
 * If MSI-X is not enabled, those two registers disappear and
 * the remaining configuration registers start at offset 20.
 */
#define VIRTIO_CR_HOSTCAP	0
#define VIRTIO_CR_GUESTCAP	4
#define VIRTIO_CR_PFN		8
#define VIRTIO_CR_QNUM		12
#define VIRTIO_CR_QSEL		14
#define VIRTIO_CR_QNOTIFY	16
#define VIRTIO_CR_STATUS	18
#define VIRTIO_CR_ISR		19
#define VIRTIO_CR_CFGVEC	20
#define VIRTIO_CR_QVEC		22
#define VIRTIO_CR_CFG0		20	/* No MSI-X */
#define VIRTIO_CR_CFG1		24	/* With MSI-X */
#define VIRTIO_CR_MSIX		20

/*
 * Bits in VIRTIO_CR_STATUS.  Guests need not actually set any of these,
 * but a guest writing 0 to this register means "please reset".
 */
#define	VIRTIO_CR_STATUS_ACK		0x01
				/* guest OS has acknowledged dev */
#define	VIRTIO_CR_STATUS_DRIVER		0x02
				/* guest OS driver is loaded */
#define	VIRTIO_CR_STATUS_DRIVER_OK	0x04
				/* guest OS driver ready */
#define	VIRTIO_CR_STATUS_FEATURES_OK	0x08
				/* features negotiation complete */
#define	VIRTIO_CR_STATUS_NEEDS_RESET	0x40
				/* device experienced an error and cannot
				 * recover, guest driver must reset it
				 */
#define	VIRTIO_CR_STATUS_FAILED		0x80
				/* guest has given up on this dev */

/*
 * Bits in VIRTIO_CR_ISR.  These apply only if not using MSI-X.
 *
 * (We don't [yet?] ever use CONF_CHANGED.)
 */
#define	VIRTIO_CR_ISR_QUEUES		0x01
				/* re-scan queues */
#define	VIRTIO_CR_ISR_CONF_CHANGED	0x02
				/* configuration changed */

#define VIRTIO_MSI_NO_VECTOR	0xFFFF

/*
 * Feature flags.
 * Note: bits 0 through 23 are reserved to each device type.
 */
#define	ACRN_VIRTIO_F_NOTIFY_ON_EMPTY		(1 << 24)
#define	ACRN_VIRTIO_RING_F_INDIRECT_DESC	(1 << 28)
#define	ACRN_VIRTIO_RING_F_EVENT_IDX		(1 << 29)

/* v1.0 compliant. */
#define ACRN_VIRTIO_F_VERSION_1		(1UL << 32)

/* From section 2.3, "Virtqueue Configuration", of the virtio specification */
/**
 * @brief Calculate size of a virtual ring, this interface is only valid for
 * legacy virtio.
 *
 * @param qsz Size of raw data in a certain virtqueue.
 *
 * @return size of a certain virtqueue, in bytes.
 */
static inline size_t
virtio_vring_size(u_int qsz)
{
	size_t size;

	/* constant 3 below = flags, va_idx, va_used_event */
	size = sizeof(struct virtio_desc) * qsz + sizeof(uint16_t) * (3 + qsz);
	size = roundup2(size, VRING_ALIGN);

	/* constant 3 below = flags, idx, avail_event */
	size += sizeof(uint16_t) * 3 + sizeof(struct virtio_used) * qsz;
	size = roundup2(size, VRING_ALIGN);

	return size;
}

struct vmctx;
struct pci_vdev;
struct virtio_vq_info;

/*
 * A virtual device, with some number (possibly 0) of virtual
 * queues and some size (possibly 0) of configuration-space
 * registers private to the device.  The virtio_base should come
 * at the front of each "derived class", so that a pointer to the
 * virtio_base is also a pointer to the more specific, derived-
 * from-virtio driver's virtio_base struct.
 *
 * Note: inside each hypervisor virtio driver, changes to these
 * data structures must be locked against other threads, if any.
 * Except for PCI config space register read/write, we assume each
 * driver does the required locking, but we need a pointer to the
 * lock (if there is one) for PCI config space read/write ops.
 *
 * When the guest reads or writes the device's config space, the
 * generic layer checks for operations on the special registers
 * described above.  If the offset of the register(s) being read
 * or written is past the CFG area (CFG0 or CFG1), the request is
 * passed on to the virtual device, after subtracting off the
 * generic-layer size.  (So, drivers can just use the offset as
 * an offset into "struct config", for instance.)
 *
 * (The virtio layer also makes sure that the read or write is to/
 * from a "good" config offset, hence cfgsize, and on BAR #0.
 * However, the driver must verify the read or write size and offset
 * and that no one is writing a readonly register.)
 *
 * The BROKED flag ("this thing done gone and broked") is for future
 * use.
 */
#define	VIRTIO_USE_MSIX		0x01
#define	VIRTIO_EVENT_IDX	0x02	/* use the event-index values */
#define	VIRTIO_BROKED		0x08	/* ??? */

/*
 * virtio pci device bar layout
 * 0	: legacy PIO bar
 * 1	: MSIX bar
 * 2	: modern PIO bar, used as notify
 * 4+5	: modern 64-bit MMIO bar
 *
 * pci bar layout for legacy/modern/transitional devices
 * legacy				: (0) + (1)
 * modern (no pio notify)		: (1) + (4+5)
 * modern (with pio notify)		: (1) + (2) + (4+5)
 * transitional (no pio notify)		: (0) + (1) + (4+5)
 * transitional (with pio notify)	: (0) + (1) + (2) + (4+5)
 */
#define VIRTIO_LEGACY_PIO_BAR_IDX	0
#define VIRTIO_MODERN_PIO_BAR_IDX	2
#define VIRTIO_MODERN_MMIO_BAR_IDX	4

/*
 * region layout in modern mmio bar
 * one 4KB region for one capability
 */
#define VIRTIO_CAP_COMMON_OFFSET	0x0000
#define VIRTIO_CAP_COMMON_SIZE		0x1000
#define VIRTIO_CAP_ISR_OFFSET		0x1000
#define VIRTIO_CAP_ISR_SIZE		0x1000
#define VIRTIO_CAP_DEVICE_OFFSET	0x2000
#define VIRTIO_CAP_DEVICE_SIZE		0x1000
#define VIRTIO_CAP_NOTIFY_OFFSET	0x3000
#define VIRTIO_CAP_NOTIFY_SIZE		0x1000

#define VIRTIO_MODERN_MEM_BAR_SIZE	(VIRTIO_CAP_NOTIFY_OFFSET + \
					VIRTIO_CAP_NOTIFY_SIZE)

/* 4-byte notify register for one virtqueue */
#define VIRTIO_MODERN_NOTIFY_OFF_MULT	4

/* Common configuration */
#define VIRTIO_PCI_CAP_COMMON_CFG	1
/* Notifications */
#define VIRTIO_PCI_CAP_NOTIFY_CFG	2
/* ISR access */
#define VIRTIO_PCI_CAP_ISR_CFG		3
/* Device specific configuration */
#define VIRTIO_PCI_CAP_DEVICE_CFG	4
/* PCI configuration access */
#define VIRTIO_PCI_CAP_PCI_CFG		5

#define VIRTIO_COMMON_DFSELECT		0
#define VIRTIO_COMMON_DF		4
#define VIRTIO_COMMON_GFSELECT		8
#define VIRTIO_COMMON_GF		12
#define VIRTIO_COMMON_MSIX		16
#define VIRTIO_COMMON_NUMQ		18
#define VIRTIO_COMMON_STATUS		20
#define VIRTIO_COMMON_CFGGENERATION	21
#define VIRTIO_COMMON_Q_SELECT		22
#define VIRTIO_COMMON_Q_SIZE		24
#define VIRTIO_COMMON_Q_MSIX		26
#define VIRTIO_COMMON_Q_ENABLE		28
#define VIRTIO_COMMON_Q_NOFF		30
#define VIRTIO_COMMON_Q_DESCLO		32
#define VIRTIO_COMMON_Q_DESCHI		36
#define VIRTIO_COMMON_Q_AVAILLO		40
#define VIRTIO_COMMON_Q_AVAILHI		44
#define VIRTIO_COMMON_Q_USEDLO		48
#define VIRTIO_COMMON_Q_USEDHI		52

/* Fields in VIRTIO_PCI_CAP_COMMON_CFG: */
struct virtio_pci_common_cfg {
	/* About the whole device. */
	uint32_t device_feature_select;	/* read-write */
	uint32_t device_feature;	/* read-only */
	uint32_t guest_feature_select;	/* read-write */
	uint32_t guest_feature;		/* read-write */
	uint16_t msix_config;		/* read-write */
	uint16_t num_queues;		/* read-only */
	uint8_t device_status;		/* read-write */
	uint8_t config_generation;	/* read-only */

	/* About a specific virtqueue. */
	uint16_t queue_select;		/* read-write */
	uint16_t queue_size;		/* read-write, power of 2. */
	uint16_t queue_msix_vector;	/* read-write */
	uint16_t queue_enable;		/* read-write */
	uint16_t queue_notify_off;	/* read-only */
	uint32_t queue_desc_lo;		/* read-write */
	uint32_t queue_desc_hi;		/* read-write */
	uint32_t queue_avail_lo;	/* read-write */
	uint32_t queue_avail_hi;	/* read-write */
	uint32_t queue_used_lo;		/* read-write */
	uint32_t queue_used_hi;		/* read-write */
};

/* PCI capability header: */
struct virtio_pci_cap {
	uint8_t cap_vndr;	/* Generic PCI field: PCI_CAP_ID_VNDR */
	uint8_t cap_next;	/* Generic PCI field: next ptr. */
	uint8_t cap_len;	/* Generic PCI field: capability length */
	uint8_t cfg_type;	/* Identifies the structure. */
	uint8_t bar;		/* Where to find it. */
	uint8_t padding[3];	/* Pad to full dword. */
	uint32_t offset;	/* Offset within bar. */
	uint32_t length;	/* Length of the structure, in bytes. */
};

/* Fields in VIRTIO_PCI_CAP_NOTIFY_CFG: */
struct virtio_pci_notify_cap {
	struct virtio_pci_cap cap;
	uint32_t notify_off_multiplier;	/* Multiplier for queue_notify_off. */
};

/* Fields in VIRTIO_PCI_CAP_PCI_CFG: */
struct virtio_pci_cfg_cap {
	struct virtio_pci_cap cap;
	uint8_t pci_cfg_data[4]; /* Data for BAR access. */
};

/**
 * @brief Base component to any virtio device
 */
struct virtio_base {
	struct virtio_ops *vops;	/**< virtio operations */
	int	flags;			/**< VIRTIO_* flags from above */
	pthread_mutex_t *mtx;		/**< POSIX mutex, if any */
	struct pci_vdev *dev;		/**< PCI device instance */
	uint64_t negotiated_caps;	/**< negotiated capabilities */
	uint64_t device_caps;		/**< device capabilities */
	struct virtio_vq_info *queues;	/**< one per nvq */
	int	curq;			/**< current queue */
	uint8_t	status;			/**< value from last status write */
	uint8_t	isr;			/**< ISR flags, if not MSI-X */
	uint16_t msix_cfg_idx;		/**< MSI-X vector for config event */
	uint32_t legacy_pio_bar_idx;	/**< index of legacy pio bar */
	uint32_t modern_pio_bar_idx;	/**< index of modern pio bar */
	uint32_t modern_mmio_bar_idx;	/**< index of modern mmio bar */
	uint8_t config_generation;	/**< configuration generation */
	uint32_t device_feature_select;	/**< current selected device feature */
	uint32_t driver_feature_select;	/**< current selected guest feature */
	int cfg_coff;			/**< PCI cfg access capability offset */
};

#define	VIRTIO_BASE_LOCK(vb)					\
do {								\
	if (vb->mtx)						\
		pthread_mutex_lock(vb->mtx);			\
} while (0)

#define	VIRTIO_BASE_UNLOCK(vb)					\
do {								\
	if (vb->mtx)						\
		pthread_mutex_unlock(vb->mtx);			\
} while (0)

/**
 * @brief Virtio specific operation functions for this type of virtio device
 */
struct virtio_ops {
	const char *name;	/**< name of driver (for diagnostics) */
	int	nvq;		/**< number of virtual queues */
	size_t	cfgsize;	/**< size of dev-specific config regs */
	void	(*reset)(void *);
				/**< called on virtual device reset */
	void	(*qnotify)(void *, struct virtio_vq_info *);
				/**< called on QNOTIFY if no VQ notify */
	int	(*cfgread)(void *, int, int, uint32_t *);
				/**< to read config regs */
	int	(*cfgwrite)(void *, int, int, uint32_t);
				/**< to write config regs */
	void    (*apply_features)(void *, uint64_t);
				/**< to apply negotiated features */
	void    (*set_status)(void *, uint64_t);
				/**< called to set device status */
};

#define	VQ_ALLOC	0x01	/* set once we have a pfn */
#define	VQ_BROKED	0x02	/* ??? */
/**
 * @brief Virtqueue data structure
 *
 * Data structure allocated (statically) per virtual queue.
 *
 * Drivers may change qsize after a reset.  When the guest OS
 * requests a device reset, the hypervisor first calls
 * vb->vo->reset(); then the data structure below is
 * reinitialized (for each virtqueue: vb->vo->nvq).
 *
 * The remaining fields should only be fussed-with by the generic
 * code.
 *
 * Note: the addresses of desc, avail, and vq_used are all
 * computable from each other, but it's a lot simpler if we just
 * keep a pointer to each one.  The event indices are similarly
 * (but more easily) computable, and this time we'll compute them:
 * they're just XX_ring[N].
 */
struct virtio_vq_info {
	uint16_t qsize;		/**< size of this queue (a power of 2) */
	void	(*notify)(void *, struct virtio_vq_info *);
				/**< called instead of notify, if not NULL */

	struct virtio_base *base;
				/**< backpointer to virtio_base */
	uint16_t num;		/**< the num'th queue in the virtio_base */

	uint16_t flags;		/**< flags (see above) */
	uint16_t last_avail;	/**< a recent value of avail->idx */
	uint16_t save_used;	/**< saved used->idx; see vq_endchains */
	uint16_t msix_idx;	/**< MSI-X index, or VIRTIO_MSI_NO_VECTOR */

	uint32_t pfn;		/**< PFN of virt queue (not shifted!) */

	volatile struct virtio_desc *desc;
				/**< descriptor array */
	volatile struct virtio_vring_avail *avail;
				/**< the "avail" ring */
	volatile struct virtio_vring_used *used;
				/**< the "used" ring */

	uint32_t gpa_desc[2];	/**< gpa of descriptors */
	uint32_t gpa_avail[2];	/**< gpa of avail_ring */
	uint32_t gpa_used[2];	/**< gpa of used_ring */
	bool enabled;		/**< whether the virtqueue is enabled */
};

/* as noted above, these are sort of backwards, name-wise */
#define VQ_AVAIL_EVENT_IDX(vq) \
	(*(volatile uint16_t *)&(vq)->used->ring[(vq)->qsize])
#define VQ_USED_EVENT_IDX(vq) \
	((vq)->avail->ring[(vq)->qsize])

/**
 * @brief Is this ring ready for I/O?
 *
 * @param vq Pointer to struct virtio_vq_info.
 *
 * @return 0 on not ready and 1 on ready.
 */
static inline int
vq_ring_ready(struct virtio_vq_info *vq)
{
	return (vq->flags & VQ_ALLOC);
}

/**
 * @brief Are there "available" descriptors?
 *
 * This does not count how many, just returns 1 if there is any.
 *
 * @param vq Pointer to struct virtio_vq_info.
 *
 * @return 0 on no available and 1 on available.
 */
static inline int
vq_has_descs(struct virtio_vq_info *vq)
{
	return (vq_ring_ready(vq) && vq->last_avail !=
	    vq->avail->idx);
}

/**
 * @brief Deliver an interrupt to guest on the given virtqueue.
 *
 * The interrupt could be MSI-X or a generic MSI interrupt.
 *
 * @param vb Pointer to struct virtio_base.
 * @param vq Pointer to struct virtio_vq_info.
 *
 * @return NULL
 */
static inline void
vq_interrupt(struct virtio_base *vb, struct virtio_vq_info *vq)
{
	if (pci_msix_enabled(vb->dev))
		pci_generate_msix(vb->dev, vq->msix_idx);
	else {
		VIRTIO_BASE_LOCK(vb);
		vb->isr |= VIRTIO_CR_ISR_QUEUES;
		pci_generate_msi(vb->dev, 0);
		pci_lintr_assert(vb->dev);
		VIRTIO_BASE_UNLOCK(vb);
	}
}

/**
 * @brief Deliver an config changed interrupt to guest.
 *
 * MSI-X or a generic MSI interrupt with config changed event.
 *
 * @param vb Pointer to struct virtio_base.
 *
 * @return NULL.
 */
static inline void
virtio_config_changed(struct virtio_base *vb)
{
	if (!(vb->status & VIRTIO_CR_STATUS_DRIVER_OK))
		return;

	vb->config_generation++;

	if (pci_msix_enabled(vb->dev))
		pci_generate_msix(vb->dev, vb->msix_cfg_idx);
	else {
		VIRTIO_BASE_LOCK(vb);
		vb->isr |= VIRTIO_CR_ISR_CONF_CHANGED;
		pci_generate_msi(vb->dev, 0);
		pci_lintr_assert(vb->dev);
		VIRTIO_BASE_UNLOCK(vb);
	}
}

struct iovec;

/**
 * @brief Link a virtio_base to its constants, the virtio device,
 * and the PCI emulation.
 *
 * @param base Pointer to struct virtio_base.
 * @param vops Pointer to struct virtio_ops.
 * @param pci_virtio_dev Pointer to instance of certain virtio device.
 * @param dev Pointer to struct pci_vdev which emulates a PCI device.
 * @param queues Pointer to struct virtio_vq_info, normally an array.
 *
 * @return NULL
 */
void virtio_linkup(struct virtio_base *base, struct virtio_ops *vops,
		   void *pci_virtio_dev, struct pci_vdev *dev,
		   struct virtio_vq_info *queues);

/**
 * @brief Initialize MSI-X vector capabilities if we're to use MSI-X,
 * or MSI capabilities if not.
 *
 * Wrapper function for virtio_intr_init() for cases we directly use
 * BAR 1 for MSI-X capabilities.
 *
 * @param base Pointer to struct virtio_base.
 * @param use_msix If using MSI-X.
 *
 * @return 0 on success and non-zero on fail.
 */
int virtio_interrupt_init(struct virtio_base *base, int use_msix);

/**
 * @brief Initialize MSI-X vector capabilities if we're to use MSI-X,
 * or MSI capabilities if not.
 *
 * We assume we want one MSI-X vector per queue, here, plus one
 * for the config vec.
 *
 * @param base Pointer to struct virtio_base.
 * @param barnum Which BAR[0..5] to use.
 * @param use_msix If using MSI-X.
 *
 * @return 0 on success and non-zero on fail.
 */
int virtio_intr_init(struct virtio_base *base, int barnum, int use_msix);

/**
 * @brief Reset device (device-wide).
 *
 * This erases all queues, i.e., all the queues become invalid.
 * But we don't wipe out the internal pointers, by just clearing
 * the VQ_ALLOC flag.
 *
 * It resets negotiated features to "none".
 * If MSI-X is enabled, this also resets all the vectors to NO_VECTOR.
 *
 * @param base Pointer to struct virtio_base.
 *
 * @return N/A
 */
void virtio_reset_dev(struct virtio_base *base);

/**
 * @brief Set I/O BAR (usually 0) to map PCI config registers.
 *
 * @param base Pointer to struct virtio_base.
 * @param barnum Which BAR[0..5] to use.
 *
 * @return N/A
 */
void virtio_set_io_bar(struct virtio_base *base, int barnum);

/**
 * @brief Walk through the chain of descriptors involved in a request
 * and put them into a given iov[] array.
 *
 * @param vq Pointer to struct virtio_vq_info.
 * @param pidx Pointer to available ring position.
 * @param iov Pointer to iov[] array prepared by caller.
 * @param n_iov Size of iov[] array.
 * @param flags Pointer to a uint16_t array which will contain flag of
 * each descriptor.
 *
 * @return number of descriptors.
 */
int vq_getchain(struct virtio_vq_info *vq, uint16_t *pidx,
		struct iovec *iov, int n_iov, uint16_t *flags);

/**
 * @brief Return the currently-first request chain back to the
 * available ring.
 *
 * @param vq Pointer to struct virtio_vq_info.
 *
 * @return N/A
 */
void vq_retchain(struct virtio_vq_info *vq);

/**
 * @brief Return specified request chain to the guest,
 * setting its I/O length to the provided value.
 *
 * @param vq Pointer to struct virtio_vq_info.
 * @param idx Pointer to available ring position, returned by vq_getchain().
 * @param iolen Number of data bytes to be returned to frontend.
 *
 * @return N/A
 */
void vq_relchain(struct virtio_vq_info *vq, uint16_t idx, uint32_t iolen);

/**
 * @brief Driver has finished processing "available" chains and calling
 * vq_relchain on each one.
 *
 * If driver used all the available chains, used_all_avail need to be set to 1.
 *
 * @param vq Pointer to struct virtio_vq_info.
 * @param used_all_avail Flag indicating if driver used all available chains.
 *
 * @return N/A
 */
void vq_endchains(struct virtio_vq_info *vq, int used_all_avail);

/**
 * @brief Handle PCI configuration space reads.
 *
 * Handle virtio standard register reads, and dispatch other reads to
 * actual virtio device driver.
 *
 * @param ctx Pointer to struct vmctx representing VM context.
 * @param vcpu VCPU ID.
 * @param dev Pointer to struct pci_vdev which emulates a PCI device.
 * @param baridx Which BAR[0..5] to use.
 * @param offset Register offset in bytes within a BAR region.
 * @param size Access range in bytes.
 *
 * @return register value.
 */
uint64_t virtio_pci_read(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
			 int baridx, uint64_t offset, int size);

/**
 * @brief Handle PCI configuration space writes.
 *
 * Handle virtio standard register writes, and dispatch other writes to
 * actual virtio device driver.
 *
 * @param ctx Pointer to struct vmctx representing VM context.
 * @param vcpu VCPU ID.
 * @param dev Pointer to struct pci_vdev which emulates a PCI device.
 * @param baridx Which BAR[0..5] to use.
 * @param offset Register offset in bytes within a BAR region.
 * @param size Access range in bytes.
 * @param value Data value to be written into register.
 *
 * @return N/A
 */
void virtio_pci_write(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		      int baridx, uint64_t offset, int size, uint64_t value);

/**
 * @brief Indicate the device has experienced an error.
 *
 * This is called when the device has experienced an error from which it
 * cannot re-cover. DEVICE_NEEDS_RESET is set to the device status register
 * and a config change intr is sent to the guest driver.
 *
 * @param base Pointer to struct virtio_base.
 *
 * @return N/A
 */
void virtio_dev_error(struct virtio_base *base);

/**
 * @brief Set modern BAR (usually 4) to map PCI config registers.
 *
 * Set modern MMIO BAR (usually 4) to map virtio 1.0 capabilities and optional
 * set modern PIO BAR (usually 2) to map notify capability. This interface is
 * only valid for modern virtio.
 *
 * @param base Pointer to struct virtio_base.
 * @param use_notify_pio Whether use pio for notify capability.
 *
 * @return 0 on success and non-zero on fail.
 */
int virtio_set_modern_bar(struct virtio_base *base, bool use_notify_pio);

/**
 * @brief Handle PCI configuration space reads.
 *
 * Handle virtio PCI configuration space reads. Only the specific registers
 * that need speical operation are handled in this callback. For others just
 * fallback to pci core. This interface is only valid for virtio modern.
 *
 * @param ctx Pointer to struct vmctx representing VM context.
 * @param vcpu VCPU ID.
 * @param dev Pointer to struct pci_vdev which emulates a PCI device.
 * @param coff Register offset in bytes within PCI configuration space.
 * @param bytes Access range in bytes.
 * @param rv The value returned as read.
 *
 * @return 0 on handled and non-zero on non-handled.
 */
int virtio_pci_modern_cfgread(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
			      int coff, int bytes, uint32_t *rv);
/**
 * @brief Handle PCI configuration space writes.
 *
 * Handle virtio PCI configuration space writes. Only the specific registers
 * that need speical operation are handled in this callback. For others just
 * fallback to pci core. This interface is only valid for virtio modern.
 *
 * @param ctx Pointer to struct vmctx representing VM context.
 * @param vcpu VCPU ID.
 * @param dev Pointer to struct pci_vdev which emulates a PCI device.
 * @param coff Register offset in bytes within PCI configuration space.
 * @param bytes Access range in bytes.
 * @param val The value to write.
 *
 * @return 0 on handled and non-zero on non-handled.
 */
int virtio_pci_modern_cfgwrite(struct vmctx *ctx, int vcpu,
			       struct pci_vdev *dev, int coff, int bytes,
			       uint32_t val);

/**
 * @}
 */

#endif	/* _VIRTIO_H_ */
