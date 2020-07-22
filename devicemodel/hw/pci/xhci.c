/*-
 * Copyright (c) 2014 Leon Dang <ldang@nahannisys.com>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * XHCI options:
 *   -s <n>,xhci,{devices}
 *
 *  devices:
 *    tablet             USB tablet mouse
 */

/*
 * xHCI DRD control flow digram.
 *  +---------------------------+
 *  |         ACRN DM           |
 *  |  +---------------------+  |
 *  |  |    xhci emulator    |  |
 *  |  |                     |  |
 *  |  |  +---------------+  |  |
 *  |  |  | drd emulator  |<----------+    +----------------------+
 *  |  |  +---------------+  |  |     |    |        app           |
 *  |  +---------|-----------+  |     |    +----------------------+
 *  +------------|--------------+     | echo H or D |
 *               | SOS USER SPACE     |             |  UOS USER SPACE
 *  -------------|--------------------|-------------|-----------------
 *               v SOS KERNEL SPACE   |             v  UOS KERNEL SPACE
 *  +------------------------------+  |    +--------------------------+
 *  | native drd sysfs interface   |  |    |native drd sysfs interface|
 *  +------------------------------+  |    +--------------------------+
 *               |                    |             |
 *               v                    |             v
 *  +------------------------+        |    +----------------------+
 *  |    natvie drd driver   |        +----|   native drd driver  |
 *  +------------------------+             +----------------------+
 *               |
 *  -------------|---------------------------------------------------
 *  HARDWARE     |
 *  +------------|----------+
 *  |xHCI        v          |     +-----------+
 *  |   +----------------+  |     |   xDCI    |
 *  |   | switch control |  |     +-----------+
 *  |   +-------+--------+  |          |
 *  +-----------+-----------+          |
 *              |       |              |
 *              |       +----+---------+
 *              |            |
 *              |     +------+------+
 *              +-----|   PHY MUX   |
 *                    +---+-----+---+
 *                        |     |
 *                    +---+     +---+
 *                +---+----+   +----+---+
 *                |USB2 PHY|   |USB3 PHY|
 *                +--------+   +--------+
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <semaphore.h>
#include "usb.h"
#include "usbdi.h"
#include "xhcireg.h"
#include "dm.h"
#include "pci_core.h"
#include "xhci.h"
#include "usb_pmapper.h"
#include "vmmapi.h"
#include "dm_string.h"
#include "timer.h"

#undef LOG_TAG
#define LOG_TAG			"xHCI: "
#define	XHCI_MAX_DEVS		20	/* 10 root hub + 10 external hub */
#define	XHCI_MAX_SLOTS		64	/* min allowed by Windows drivers */

/*
 * XHCI data structures can be up to 64k, but limit paddr_guest2host mapping
 * to 4k to avoid going over the guest physical memory barrier.
 */
#define	XHCI_PADDR_SZ		4096	/* paddr_guest2host max size */
#define	XHCI_ERST_MAX		2	/* max 2^entries event ring seg tbl */
#define	XHCI_CAPLEN		(8*8)	/* offset of op register space */
#define	XHCI_HCCPRAMS2		0x1C	/* offset of HCCPARAMS2 register */
#define	XHCI_PORTREGS_START	0x400
#define	XHCI_DOORBELL_MAX	256
#define	XHCI_STREAMS_MAX	1	/* 4-15 in XHCI spec */

/* caplength and hci-version registers */
#define	XHCI_SET_CAPLEN(x)		((x) & 0xFF)
#define	XHCI_SET_HCIVERSION(x)		(((x) & 0xFFFF) << 16)
#define	XHCI_GET_HCIVERSION(x)		(((x) >> 16) & 0xFFFF)

/* hcsparams1 register */
#define	XHCI_SET_HCSP1_MAXSLOTS(x)	((x) & 0xFF)
#define	XHCI_SET_HCSP1_MAXINTR(x)	(((x) & 0x7FF) << 8)
#define	XHCI_SET_HCSP1_MAXPORTS(x)	(((x) & 0xFF) << 24)

/* hcsparams2 register */
#define	XHCI_SET_HCSP2_IST(x)		((x) & 0x0F)
#define	XHCI_SET_HCSP2_ERSTMAX(x)	(((x) & 0x0F) << 4)
#define	XHCI_SET_HCSP2_MAXSCRATCH_HI(x)	(((x) & 0x1F) << 21)
#define	XHCI_SET_HCSP2_MAXSCRATCH_LO(x)	(((x) & 0x1F) << 27)

/* hcsparams3 register */
#define	XHCI_SET_HCSP3_U1EXITLATENCY(x)	((x) & 0xFF)
#define	XHCI_SET_HCSP3_U2EXITLATENCY(x)	(((x) & 0xFFFF) << 16)

/* hccparams1 register */
#define	XHCI_SET_HCCP1_AC64(x)		((x) & 0x01)
#define	XHCI_SET_HCCP1_BNC(x)		(((x) & 0x01) << 1)
#define	XHCI_SET_HCCP1_CSZ(x)		(((x) & 0x01) << 2)
#define	XHCI_SET_HCCP1_PPC(x)		(((x) & 0x01) << 3)
#define	XHCI_SET_HCCP1_PIND(x)		(((x) & 0x01) << 4)
#define	XHCI_SET_HCCP1_LHRC(x)		(((x) & 0x01) << 5)
#define	XHCI_SET_HCCP1_LTC(x)		(((x) & 0x01) << 6)
#define	XHCI_SET_HCCP1_NSS(x)		(((x) & 0x01) << 7)
#define	XHCI_SET_HCCP1_PAE(x)		(((x) & 0x01) << 8)
#define	XHCI_SET_HCCP1_SPC(x)		(((x) & 0x01) << 9)
#define	XHCI_SET_HCCP1_SEC(x)		(((x) & 0x01) << 10)
#define	XHCI_SET_HCCP1_CFC(x)		(((x) & 0x01) << 11)
#define	XHCI_SET_HCCP1_MAXPSA(x)	(((x) & 0x0F) << 12)
#define	XHCI_SET_HCCP1_XECP(x)		(((x) & 0xFFFF) << 16)

/* hccparams2 register */
#define	XHCI_SET_HCCP2_U3C(x)		((x) & 0x01)
#define	XHCI_SET_HCCP2_CMC(x)		(((x) & 0x01) << 1)
#define	XHCI_SET_HCCP2_FSC(x)		(((x) & 0x01) << 2)
#define	XHCI_SET_HCCP2_CTC(x)		(((x) & 0x01) << 3)
#define	XHCI_SET_HCCP2_LEC(x)		(((x) & 0x01) << 4)
#define	XHCI_SET_HCCP2_CIC(x)		(((x) & 0x01) << 5)

/* other registers */
#define	XHCI_SET_DOORBELL(x)		((x) & ~0x03)
#define	XHCI_SET_RTSOFFSET(x)		((x) & ~0x0F)

/* register masks */
#define	XHCI_PS_PLS_MASK		(0xF << 5)	/* port link state */
#define	XHCI_PS_SPEED_MASK		(0xF << 10)	/* port speed */
#define	XHCI_PS_PIC_MASK		(0x3 << 14)	/* port indicator */

/* port register set */
#define	XHCI_PORTREGS_BASE		0x400		/* base offset */
#define	XHCI_PORTREGS_PORT0		0x3F0
#define	XHCI_PORTREGS_SETSZ		0x10		/* size of a set */

#define	MASK_64_HI(x)			((x) & ~0xFFFFFFFFULL)
#define	MASK_64_LO(x)			((x) & 0xFFFFFFFFULL)

#define	FIELD_REPLACE(a, b, m, s)	(((a) & ~((m) << (s))) | \
					(((b) & (m)) << (s)))
#define	FIELD_COPY(a, b, m, s)		(((a) & ~((m) << (s))) | \
					(((b) & ((m) << (s)))))

struct pci_xhci_trb_ring {
	uint64_t ringaddr;		/* current dequeue guest address */
	uint32_t ccs;			/* consumer cycle state */
};

struct xhci_ep_timer_data {
	uint32_t slot;
	uint8_t epnum;
	uint8_t dir;
	struct pci_xhci_dev_emu *dev;
};

/* device endpoint transfer/stream rings */
struct pci_xhci_dev_ep {
	union {
		struct xhci_trb		*_epu_tr;
		struct xhci_stream_ctx	*_epu_sctx;
	} _ep_trbsctx;
#define	ep_tr		_ep_trbsctx._epu_tr
#define	ep_sctx		_ep_trbsctx._epu_sctx

	union {
		struct pci_xhci_trb_ring _epu_trb;
		struct pci_xhci_trb_ring *_epu_sctx_trbs;
	} _ep_trb_rings;
#define	ep_ringaddr	_ep_trb_rings._epu_trb.ringaddr
#define	ep_ccs		_ep_trb_rings._epu_trb.ccs
#define	ep_sctx_trbs	_ep_trb_rings._epu_sctx_trbs

	struct usb_xfer *ep_xfer;	/* transfer chain */
	struct acrn_timer isoc_timer;
	struct xhci_ep_timer_data timer_data;
	pthread_mutex_t mtx;
};

/* device context base address array: maps slot->device context */
struct xhci_dcbaa {
	uint64_t dcba[USB_MAX_DEVICES+1]; /* xhci_dev_ctx ptrs */
};

/* port status registers */
struct pci_xhci_portregs {
	uint32_t	portsc;		/* port status and control */
	uint32_t	portpmsc;	/* port pwr mgmt status & control */
	uint32_t	portli;		/* port link info */
	uint32_t	porthlpmc;	/* port hardware LPM control */
} __attribute__((packed));
#define	XHCI_PS_SPEED_SET(x)	(((x) & 0xF) << 10)

/* xHC operational registers */
struct pci_xhci_opregs {
	uint32_t	usbcmd;		/* usb command */
	uint32_t	usbsts;		/* usb status */
	uint32_t	pgsz;		/* page size */
	uint32_t	dnctrl;		/* device notification control */
	uint64_t	crcr;		/* command ring control */
	uint64_t	dcbaap;		/* device ctx base addr array ptr */
	uint32_t	config;		/* configure */

	/* guest mapped addresses: */
	struct xhci_trb	*cr_p;		/* crcr dequeue */
	struct xhci_dcbaa *dcbaa_p;	/* dev ctx array ptr */
};

/* xHC runtime registers */
struct pci_xhci_rtsregs {
	uint32_t	mfindex;	/* microframe index */
	struct {			/* interrupter register set */
		uint32_t	iman;	/* interrupter management */
		uint32_t	imod;	/* interrupter moderation */
		uint32_t	erstsz;	/* event ring segment table size */
		uint32_t	rsvd;
		uint64_t	erstba;	/* event ring seg-tbl base addr */
		uint64_t	erdp;	/* event ring dequeue ptr */
	} intrreg __attribute__((packed));

	/* guest mapped addresses */
	struct xhci_erst	*erstba_p;
	int			er_deq_seg; /* event ring dequeue segment */
	int			er_enq_idx; /* event ring enqueue index */
	int			er_enq_seg; /* event ring enqueue segment */
	uint32_t		event_pcs;  /* producer cycle state flag */
};

/* this is used to describe the VBus Drop state */
enum pci_xhci_vbdp_state {
	S3_VBDP_NONE = 0,
	S3_VBDP_START,
	S3_VBDP_END
};

struct pci_xhci_excap_ptr {
	uint8_t cap_id;
	uint8_t cap_ptr;
} __attribute__((packed));

struct pci_xhci_excap_drd_apl {
	struct pci_xhci_excap_ptr excap_ptr;
	uint8_t padding[102]; /* Followed native xHCI MMIO layout */
	uint32_t drdcfg0;
	uint32_t drdcfg1;
} __attribute__((packed));

struct pci_xhci_excap_prot {
	struct pci_xhci_excap_ptr excap_ptr;
	uint8_t rev_min;
	uint8_t rev_maj;
	char string[4];
	uint8_t port_off;
	uint8_t port_cnt;
	uint16_t psic_prot_def;
	uint32_t reserve;
} __attribute__((packed));

struct pci_xhci_excap {
	uint32_t start;
	uint32_t end;
	void *data;
};

struct xhci_block {
	uint32_t ccs;
	uint64_t trb_addr;
	uint32_t streamid;
};

static DEFINE_EXCP_PROT(u2_prot,
		0x08,
		2,
		XHCI_MAX_DEVS/2 + 1,
		XHCI_MAX_DEVS/2);
static DEFINE_EXCP_PROT(u3_prot,
		0x14,
		3,
		1,
		XHCI_MAX_DEVS/2);

static DEFINE_EXCP_VENDOR_DRD(XHCI_ID_DRD_INTEL,
		0x00,
		0x00,
		0x00);

/*
 * Extended capabilities layout of APL platform.
 * excap start		excap end		register value
 * 0x8000		0x8010			0x02000802
 * 0x8020		0x8030			0x03001402
 * 0x8070		0x80E0			0x000000C0
 */
struct pci_xhci_excap excap_group_apl[] = {
	{0x8000, 0x8010, &excap_u2_prot},
	{0x8020, 0x8030, &excap_u3_prot},
	{0x8070, 0x80E0, &excap_drd_apl},
	{EXCAP_GROUP_END, EXCAP_GROUP_END, EXCAP_GROUP_NULL}
};

/*
 * default xhci extended capabilities
 * excap start  excap end  register value
 * 0x8000       0x8010     0x02000802
 * 0x8020       0x8030     0x03001402
 */
struct pci_xhci_excap excap_group_dft[] = {
	{0x8000, 0x8010, &excap_u2_prot},
	{0x8020, 0x8030, &excap_u3_prot},
	{EXCAP_GROUP_END, EXCAP_GROUP_END, EXCAP_GROUP_NULL}
};

struct pci_xhci_vdev;

/*
 * USB device emulation container.
 * This is referenced from usb_hci->dev; 1 pci_xhci_dev_emu for each
 * emulated device instance.
 */
struct pci_xhci_dev_emu {
	struct pci_xhci_vdev	*xdev;

	/* XHCI contexts */
	struct xhci_dev_ctx	*dev_ctx;
	struct pci_xhci_dev_ep	eps[XHCI_MAX_ENDPOINTS];
	int			dev_slotstate;

	struct usb_devemu	*dev_ue;	/* USB emulated dev */
	void			*dev_instance;	/* device's instance */

	struct usb_hci		hci;
};

struct pci_xhci_native_port {
	struct usb_native_devinfo info;
	uint8_t vport;
	uint8_t state;
};

/* This is used to describe the VBus Drop state */
struct pci_xhci_vbdp_dev_state {
	struct	usb_devpath path;
	uint8_t	vport;
	uint8_t	state;
};

struct pci_xhci_vdev {
	struct pci_vdev *dev;
	pthread_mutex_t mtx;

	uint32_t	caplength;	/* caplen & hciversion */
	uint32_t	hcsparams1;	/* structural parameters 1 */
	uint32_t	hcsparams2;	/* structural parameters 2 */
	uint32_t	hcsparams3;	/* structural parameters 3 */
	uint32_t	hccparams1;	/* capability parameters 1 */
	uint32_t	dboff;		/* doorbell offset */
	uint32_t	rtsoff;		/* runtime register space offset */
	uint32_t	rtsend;
	uint32_t	hccparams2;	/* capability parameters 2 */

	uint32_t	excapoff;	/* ext-capability registers offset */
	uint32_t	regsend;	/* end of configuration registers */

	struct pci_xhci_opregs  opregs;
	struct pci_xhci_rtsregs rtsregs;

	struct pci_xhci_portregs *portregs;
	struct pci_xhci_dev_emu  **devices; /* XHCI[port] = device */
	struct pci_xhci_dev_emu  **slots;   /* slots assigned from 1 */

	bool		slot_allocated[XHCI_MAX_SLOTS + 1];
	int		ndevices;
	uint16_t	pid;
	uint16_t	vid;

	void		*excap_ptr;
	int (*excap_write)(struct pci_xhci_vdev *, uint64_t, uint64_t);
	int		usb2_port_start;
	int		usb3_port_start;

	pthread_t	vbdp_thread;
	sem_t		vbdp_sem;
	bool		vbdp_polling;
	int		vbdp_dev_num;
	struct pci_xhci_vbdp_dev_state vbdp_devs[XHCI_MAX_VIRT_PORTS];

	/*
	 * native_ports uses for record the command line assigned native root
	 * hub ports and its child external hub ports.
	 */
	struct pci_xhci_native_port native_ports[XHCI_MAX_VIRT_PORTS];
	struct timespec init_time;
	uint32_t	quirks;
};

/* portregs and devices arrays are set up to start from idx=1 */
#define	XHCI_PORTREG_PTR(x, n)	(&(x)->portregs[(n)])
#define	XHCI_SLOTDEV_PTR(x, n)	((x)->slots[(n)])
#define	XHCI_HALTED(xdev)	((xdev)->opregs.usbsts & XHCI_STS_HCH)
#define	XHCI_GADDR(xdev, a)	paddr_guest2host((xdev)->dev->vmctx, (a), \
				XHCI_PADDR_SZ - ((a) & (XHCI_PADDR_SZ-1)))

/* port mapping status */
#define VPORT_FREE (0)
#define VPORT_ASSIGNED (1)
#define VPORT_CONNECTED (2)
#define VPORT_EMULATED (3)

struct pci_xhci_option_elem {
	char *parse_opt;
	int (*parse_fn)(struct pci_xhci_vdev *, char *);
};

static int xhci_in_use;
static int pci_xhci_insert_event(struct pci_xhci_vdev *xdev,
				 struct xhci_trb *evtrb, int do_intr);
static void pci_xhci_dump_trb(struct xhci_trb *trb);
static void pci_xhci_assert_interrupt(struct pci_xhci_vdev *xdev);
static void pci_xhci_reset_slot(struct pci_xhci_vdev *xdev, int slot);
static void pci_xhci_reset_port(struct pci_xhci_vdev *xdev, int portn,
				int warm);
static void pci_xhci_update_ep_ring(struct pci_xhci_vdev *xdev,
				    struct pci_xhci_dev_emu *dev,
				    struct pci_xhci_dev_ep *devep,
				    struct xhci_endp_ctx *ep_ctx,
				    uint32_t streamid, uint64_t ringaddr,
				    int ccs);
static void pci_xhci_init_port(struct pci_xhci_vdev *xdev, int portn);
static int pci_xhci_connect_port(struct pci_xhci_vdev *xdev, int port,
		int usb_speed, int need_intr);
static int pci_xhci_disconnect_port(struct pci_xhci_vdev *xdev, int port,
		int need_intr);
static struct pci_xhci_dev_emu *pci_xhci_dev_create(struct pci_xhci_vdev *
		xdev, void *dev_data);
static void pci_xhci_dev_destroy(struct pci_xhci_dev_emu *de);
static void pci_xhci_set_evtrb(struct xhci_trb *evtrb, uint64_t port,
		uint32_t errcode, uint32_t evtype);
static int pci_xhci_xfer_complete(struct pci_xhci_vdev *xdev,
		struct usb_xfer *xfer, uint32_t slot, uint32_t epid,
		int *do_intr);
static inline int pci_xhci_is_valid_portnum(int n);
static int pci_xhci_parse_tablet(struct pci_xhci_vdev *xdev, char *opts);
static int pci_xhci_parse_log_level(struct pci_xhci_vdev *xdev, char *opts);
static int pci_xhci_parse_extcap(struct pci_xhci_vdev *xdev, char *opts);
static int pci_xhci_convert_speed(int lspeed);
static void pci_xhci_free_usb_xfer(struct usb_xfer *xfer);
static void pci_xhci_isoc_handler(void *arg, uint64_t param);

#define XHCI_OPT_MAX_LEN 32
static struct pci_xhci_option_elem xhci_option_table[] = {
	{"tablet", pci_xhci_parse_tablet},
	{"log", pci_xhci_parse_log_level},
	{"cap", pci_xhci_parse_extcap}
};

static bool
pci_xhci_is_vport_free(struct pci_xhci_vdev *xdev, int portnum)
{
	int i;

	for (i = 0; i < XHCI_MAX_VIRT_PORTS; i++)
		if (xdev->native_ports[i].vport == portnum)
			return true;

	return false;
}

static int
pci_xhci_get_free_vport(struct pci_xhci_vdev *xdev,
		struct usb_native_devinfo *di)
{
	int ports, porte;
	int i, j, k;

	if (di->bcd < 0x300)
		ports = xdev->usb2_port_start;
	else
		ports = xdev->usb3_port_start;

	porte = ports + (XHCI_MAX_DEVS / 2);

	for (i = ports; i <= porte; i++) {
		for (j = 0; j < XHCI_MAX_VIRT_PORTS; j++) {
			if (xdev->native_ports[j].vport == i)
				break;

			k = xdev->vbdp_dev_num;
			if (k > 0 && xdev->vbdp_devs[j].state == S3_VBDP_START
					&& xdev->vbdp_devs[j].vport == i)
				break;
		}
		if (j >= XHCI_MAX_VIRT_PORTS)
			return i;
	}
	return -1;
}

static int
pci_xhci_set_native_port_assigned(struct pci_xhci_vdev *xdev,
		struct usb_native_devinfo *info)
{
	int i;

	for (i = 0; i < XHCI_MAX_VIRT_PORTS; i++)
		if (xdev->native_ports[i].state == VPORT_FREE)
			break;

	if (i < XHCI_MAX_VIRT_PORTS) {
		xdev->native_ports[i].info = *info;
		xdev->native_ports[i].state = VPORT_ASSIGNED;
		return i;
	}

	return -1;
}

static int
pci_xhci_get_native_port_index_by_path(struct pci_xhci_vdev *xdev,
		struct usb_devpath *path)
{
	int i;

	for (i = 0; i < XHCI_MAX_VIRT_PORTS; i++)
		if (usb_dev_path_cmp(&xdev->native_ports[i].info.path, path))
			return i;
	return -1;
}

static int
pci_xhci_get_native_port_index_by_vport(struct pci_xhci_vdev *xdev,
		uint8_t vport)
{
	int i;

	for (i = 0; i < XHCI_MAX_VIRT_PORTS; i++)
		if (xdev->native_ports[i].vport == vport)
			return i;

	return -1;
}

static void
pci_xhci_clr_native_port_assigned(struct pci_xhci_vdev *xdev,
		struct usb_native_devinfo *info)
{
	int i;

	i = pci_xhci_get_native_port_index_by_path(xdev, &info->path);
	if (i >= 0) {
		xdev->native_ports[i].state = VPORT_FREE;
		xdev->native_ports[i].vport = 0;
		memset(&xdev->native_ports[i].info, 0, sizeof(*info));
	}
}

static int
pci_xhci_assign_hub_ports(struct pci_xhci_vdev *xdev,
		struct usb_native_devinfo *info)
{
	int index;
	uint8_t i;
	struct usb_native_devinfo di;
	struct usb_devpath *path;

	if (!xdev || !info || info->type != USB_TYPE_EXTHUB)
		return -1;

	index = pci_xhci_get_native_port_index_by_path(xdev, &info->path);
	if (index < 0) {
		UPRINTF(LDBG, "cannot find hub %d-%s\r\n", info->path.bus,
				usb_dev_path(&info->path));
		return -1;
	}

	xdev->native_ports[index].info = *info;
	UPRINTF(LDBG, "Found an USB hub %d-%s with %d port(s).\r\n",
			info->path.bus, usb_dev_path(&info->path),
			info->maxchild);

	path = &di.path;
	for (i = 1; i <= info->maxchild; i++) {

		/* make a device path for hub ports */
		memcpy(path->path, info->path.path, info->path.depth);
		memcpy(path->path + info->path.depth, &i, sizeof(i));
		memset(path->path + info->path.depth + 1, 0,
				USB_MAX_TIERS - info->path.depth - 1);
		path->depth = info->path.depth + 1;
		path->bus = info->path.bus;

		/* set the device path as assigned */
		index = pci_xhci_set_native_port_assigned(xdev, &di);
		if (index < 0) {
			UPRINTF(LFTL, "too many USB devices\r\n");
			return -1;
		}
		UPRINTF(LDBG, "Add %d-%s as assigned port\r\n",
				path->bus, usb_dev_path(path));
	}
	return 0;
}

static int
pci_xhci_unassign_hub_ports(struct pci_xhci_vdev *xdev,
		struct usb_native_devinfo *info)
{
	uint8_t i, index;
	struct usb_native_devinfo di, *oldinfo;
	struct usb_devpath *path;

	if (!xdev || !info || info->type != USB_TYPE_EXTHUB)
		return -1;

	index = pci_xhci_get_native_port_index_by_path(xdev, &info->path);
	if (index < 0) {
		UPRINTF(LFTL, "cannot find USB hub %d-%s\r\n",
			info->path.bus, usb_dev_path(&info->path));
		return -1;
	}

	oldinfo = &xdev->native_ports[index].info;
	UPRINTF(LDBG, "Disconnect an USB hub %d-%s with %d port(s)\r\n",
			oldinfo->path.bus, usb_dev_path(&oldinfo->path),
			oldinfo->maxchild);

	path = &di.path;
	for (i = 1; i <= oldinfo->maxchild; i++) {

		/* make a device path for hub ports */
		memcpy(path->path, oldinfo->path.path, oldinfo->path.depth);
		memcpy(path->path + oldinfo->path.depth, &i, sizeof(i));
		memset(path->path + oldinfo->path.depth + 1, 0,
				USB_MAX_TIERS - oldinfo->path.depth - 1);
		path->depth = oldinfo->path.depth + 1;
		path->bus = oldinfo->path.bus;

		/* clear the device path as not assigned */
		pci_xhci_clr_native_port_assigned(xdev, &di);
		UPRINTF(LDBG, "Del %d-%s as assigned port\r\n",
				path->bus, usb_dev_path(path));
	}
	return 0;
}


static void *
xhci_vbdp_thread(void *data)
{
	int i, j;
	int speed;
	struct pci_xhci_vdev *xdev;
	struct pci_xhci_native_port *p;

	xdev = data;
	while (xdev->vbdp_polling) {

		sem_wait(&xdev->vbdp_sem);
		for (i = 0; i < XHCI_MAX_VIRT_PORTS; ++i)
			if (xdev->vbdp_devs[i].state == S3_VBDP_END) {
				xdev->vbdp_devs[i].state = S3_VBDP_NONE;
				break;
			}

		if (i >= XHCI_MAX_VIRT_PORTS)
			continue;

		j = pci_xhci_get_native_port_index_by_path(xdev,
				&xdev->vbdp_devs[i].path);
		if (j < 0)
			continue;

		p = &xdev->native_ports[j];
		if (p->state != VPORT_CONNECTED)
			continue;

		speed = pci_xhci_convert_speed(p->info.speed);
		pci_xhci_connect_port(xdev, p->vport, speed, 1);
		UPRINTF(LINF, "change portsc for %d-%s\r\n", p->info.path.bus,
				usb_dev_path(&p->info.path));
	}
	return NULL;
}

static int
pci_xhci_native_usb_dev_conn_cb(void *hci_data, void *dev_data)
{
	struct pci_xhci_vdev *xdev;
	struct usb_native_devinfo *di;
	int vport = -1;
	int index;
	int rc;
	int i;
	int s3_conn = 0;

	xdev = hci_data;
	di = dev_data;

	/* print physical information about new device */
	UPRINTF(LINF, "%04x:%04x %d-%s connecting.\r\n", di->vid, di->pid,
			di->path.bus, usb_dev_path(&di->path));

	index = pci_xhci_get_native_port_index_by_path(xdev, &di->path);
	if (index < 0) {
		UPRINTF(LINF, "%04x:%04x %d-%s doesn't belong to this"
				" vm, bye.\r\n", di->vid, di->pid,
				di->path.bus, usb_dev_path(&di->path));
		return 0;
	}

	if (di->type == USB_TYPE_EXTHUB) {
		rc = pci_xhci_assign_hub_ports(xdev, di);
		if (rc < 0)
			UPRINTF(LFTL, "fail to assign ports of hub %d-%s\r\n",
					di->path.bus, usb_dev_path(&di->path));
		return 0;
	}

	UPRINTF(LINF, "%04x:%04x %d-%s belong to this vm.\r\n", di->vid,
			di->pid, di->path.bus, usb_dev_path(&di->path));

	for (i = 0; xdev->vbdp_dev_num && i < XHCI_MAX_VIRT_PORTS; ++i) {
		if (xdev->vbdp_devs[i].state != S3_VBDP_START)
			continue;

		if (!usb_dev_path_cmp(&di->path, &xdev->vbdp_devs[i].path))
			continue;

		s3_conn = 1;
		vport = xdev->vbdp_devs[i].vport;
		UPRINTF(LINF, "Skip and cache connect event for %d-%s\r\n",
				di->path.bus, usb_dev_path(&di->path));
		break;
	}

	if (vport <= 0)
		vport = pci_xhci_get_free_vport(xdev, di);

	if (vport <= 0) {
		UPRINTF(LFTL, "no free virtual port for native device %d-%s"
				"\r\n", di->path.bus,
				usb_dev_path(&di->path));
		goto errout;
	}

	xdev->native_ports[index].vport = vport;
	xdev->native_ports[index].info = *di;
	xdev->native_ports[index].state = VPORT_CONNECTED;

	UPRINTF(LINF, "%04X:%04X %d-%s is attached to virtual port %d.\r\n",
			di->vid, di->pid, di->path.bus,
			usb_dev_path(&di->path), vport);

	/* we will report connecting event in xhci_vbdp_thread for
	 * device that hasn't complete the S3 process
	 */
	if (s3_conn)
		return 0;

	/* Trigger port change event for the arriving device */
	if (pci_xhci_connect_port(xdev, vport, di->speed, 1))
		UPRINTF(LFTL, "fail to report port event\n");

	return 0;
errout:
	return -1;
}

static int
pci_xhci_native_usb_dev_disconn_cb(void *hci_data, void *dev_data)
{
	struct pci_xhci_vdev *xdev;
	struct pci_xhci_dev_emu *edev;
	struct usb_native_devinfo *di;
	uint8_t vport, slot;
	uint16_t state;
	int need_intr = 1;
	int index;
	int rc;
	int i;

	xdev = hci_data;
	di = dev_data;
	if (!pci_xhci_is_valid_portnum(ROOTHUB_PORT(di->path))) {
		UPRINTF(LFTL, "invalid physical port %d\r\n",
				ROOTHUB_PORT(di->path));
		return -1;
	}

	index = pci_xhci_get_native_port_index_by_path(xdev, &di->path);
	if (index < 0) {
		UPRINTF(LFTL, "fail to find physical port %d\r\n",
				ROOTHUB_PORT(di->path));
		return -1;
	}

	if (di->type == USB_TYPE_EXTHUB) {
		rc = pci_xhci_unassign_hub_ports(xdev, di);
		if (rc < 0)
			UPRINTF(LFTL, "fail to unassign the ports of hub"
					" %d-%s\r\n", di->path.bus,
					usb_dev_path(&di->path));
		return 0;
	}

	state = xdev->native_ports[index].state;
	vport = xdev->native_ports[index].vport;

	if (state == VPORT_CONNECTED && vport > 0) {
		/*
		 * When this place is reached, it means the physical
		 * USB device is disconnected before the emulation
		 * procedure is started. The related states should be
		 * cleared for future connecting.
		 */
		UPRINTF(LFTL, "disconnect VPORT_CONNECTED device: "
				"%d-%s vport %d\r\n", di->path.bus,
				usb_dev_path(&di->path), vport);
		pci_xhci_disconnect_port(xdev, vport, 0);
		xdev->native_ports[index].state = VPORT_ASSIGNED;
		return 0;
	}

	edev = xdev->devices[vport];
	for (slot = 1; slot <= XHCI_MAX_SLOTS; ++slot)
		if (xdev->slots[slot] == edev)
			break;

	for (i = 0; xdev->vbdp_dev_num && i < XHCI_MAX_VIRT_PORTS; ++i) {
		if (xdev->vbdp_devs[i].state != S3_VBDP_START)
			continue;

		if (!usb_dev_path_cmp(&xdev->vbdp_devs[i].path, &di->path))
			continue;

		/*
		 * we do nothing here for device that is in the middle of
		 * S3 resuming process.
		 */
		UPRINTF(LINF, "disconnect device %d-%s on vport %d with "
				"state %d and return.\r\n", di->path.bus,
				usb_dev_path(&di->path), vport, state);
		return 0;
	}

	if (!(state == VPORT_EMULATED || state == VPORT_CONNECTED))
		UPRINTF(LFTL, "error: unexpected state %d\r\n", state);

	xdev->native_ports[index].state = VPORT_ASSIGNED;
	xdev->native_ports[index].vport = 0;

	UPRINTF(LINF, "disconnect device %d-%s on vport %d with state %d\r\n",
			di->path.bus, usb_dev_path(&di->path), vport, state);
	if (pci_xhci_disconnect_port(xdev, vport, need_intr)) {
		UPRINTF(LFTL, "fail to report event\r\n");
		return -1;
	}

	/*
	 * At this point, the resources allocated for virtual device
	 * should not be released, it should be released in the
	 * pci_xhci_cmd_disable_slot function.
	 */
	return 0;
}

/*
 * return value:
 * = 0: succeed without interrupt
 * > 0: succeed with interrupt
 * < 0: failure
 */
static int
pci_xhci_usb_dev_notify_cb(void *hci_data, void *udev_data)
{
	int slot, epid, intr, rc;
	struct usb_xfer *xfer;
	struct pci_xhci_dev_emu *edev;
	struct pci_xhci_vdev *xdev;

	xfer = udev_data;
	if (!xfer)
		return -1;

	epid = xfer->epid;
	edev = xfer->dev;
	if (!edev)
		return -1;

	xdev = edev->xdev;
	if (!xdev)
		return -1;

	slot = edev->hci.hci_address;
	rc = pci_xhci_xfer_complete(xdev, xfer, slot, epid, &intr);

	if (rc)
		return -1;
	else if (intr)
		return 1;
	else
		return 0;
}

static int
pci_xhci_usb_dev_intr_cb(void *hci_data, void *udev_data)
{
	struct pci_xhci_dev_emu *edev;

	edev = hci_data;
	if (edev && edev->xdev)
		pci_xhci_assert_interrupt(edev->xdev);

	return 0;
}

static int
pci_xhci_usb_dev_lock_ep_cb(void *hci_data, void *udev_data)
{
	struct pci_xhci_dev_emu *edev;
	struct pci_xhci_dev_ep	*ep;
	int			epid;

	edev = hci_data;
	epid = *((int *)udev_data);

	if (edev && edev->xdev && epid > 0 && epid < 32) {
		ep = &edev->eps[epid];
		pthread_mutex_lock(&ep->mtx);
	}

	return 0;
}

static int
pci_xhci_usb_dev_unlock_ep_cb(void *hci_data, void *udev_data)
{
	struct pci_xhci_dev_emu	*edev;
	struct pci_xhci_dev_ep	*ep;
	int			epid;

	edev = hci_data;
	epid = *((int *)udev_data);

	if (edev && edev->xdev && epid > 0 && epid < 32) {
		ep = &edev->eps[epid];
		pthread_mutex_unlock(&ep->mtx);
	}

	return 0;
}

static struct pci_xhci_dev_emu*
pci_xhci_dev_create(struct pci_xhci_vdev *xdev, void *dev_data)
{
	struct usb_devemu *ue = NULL;
	struct pci_xhci_dev_emu *de = NULL;
	void *ud = NULL;
	int rc;

	ue = calloc(1, sizeof(struct usb_devemu));
	if (!ue)
		return NULL;

	/*
	 * TODO: at present, the following functions are
	 * enough. But for the purpose to be compatible with
	 * usb_mouse.c, the high level design including the
	 * function interface should be changed and refined
	 * in future.
	 */
	ue->ue_init     = usb_dev_init;
	ue->ue_request  = usb_dev_request;
	ue->ue_data     = usb_dev_data;
	ue->ue_info	= usb_dev_info;
	ue->ue_reset    = usb_dev_reset;
	ue->ue_remove   = NULL;
	ue->ue_stop     = NULL;
	ue->ue_deinit	= usb_dev_deinit;
	ue->ue_devtype  = USB_DEV_PORT_MAPPER;

	ud = ue->ue_init(dev_data, NULL);
	if (!ud)
		goto errout;

	rc = ue->ue_info(ud, USB_INFO_VERSION, &ue->ue_usbver,
			sizeof(ue->ue_usbver));
	if (rc < 0)
		goto errout;

	rc = ue->ue_info(ud, USB_INFO_SPEED, &ue->ue_usbspeed,
			sizeof(ue->ue_usbspeed));
	if (rc < 0)
		goto errout;

	de = calloc(1, sizeof(struct pci_xhci_dev_emu));
	if (!de)
		goto errout;

	de->xdev            = xdev;
	de->dev_ue          = ue;
	de->dev_instance    = ud;
	de->hci.dev         = NULL;
	de->hci.hci_intr    = NULL;
	de->hci.hci_event   = NULL;
	de->hci.hci_address = 0;

	return de;

errout:
	if (ud)
		ue->ue_deinit(ud);

	free(ue);
	free(de);
	return NULL;
}

static void
pci_xhci_dev_destroy(struct pci_xhci_dev_emu *de)
{
	struct usb_devemu *ue;
	struct usb_dev *ud;
	struct pci_xhci_dev_ep *vdep;
	int i;

	if (de) {
		ue = de->dev_ue;
		ud = de->dev_instance;
		if (ue) {
			if (ue->ue_devtype == USB_DEV_PORT_MAPPER) {
				if (ue->ue_deinit)
					ue->ue_deinit(ud);
			}
		} else
			return;

		if (ue->ue_devtype == USB_DEV_PORT_MAPPER)
			free(ue);

		for (i = 1; i < XHCI_MAX_ENDPOINTS; i++) {
			vdep = &de->eps[i];
			if (vdep->ep_xfer) {
				pci_xhci_free_usb_xfer(vdep->ep_xfer);
				vdep->ep_xfer = NULL;
			}
		}

		free(de);
	}
}

static inline int
pci_xhci_is_valid_portnum(int n)
{
	return n > 0 && n <= XHCI_MAX_DEVS;
}

static int
pci_xhci_convert_speed(int lspeed)
{
	/* according to xhci spec, zero means undefined speed */
	int speed = 0;

	switch (lspeed) {
	case USB_SPEED_LOW:
		speed = 0x2;
		break;
	case USB_SPEED_FULL:
		speed = 0x1;
		break;
	case USB_SPEED_HIGH:
		speed = 0x3;
		break;
	case USB_SPEED_SUPER:
		speed = 0x4;
		break;
	default:
		UPRINTF(LFTL, "unkown speed %08x\r\n", lspeed);
	}
	return speed;
}

static int
pci_xhci_change_port(struct pci_xhci_vdev *xdev, int port, int usb_speed,
		int conn, int need_intr)
{
	int speed, error;
	struct xhci_trb evtrb;
	struct pci_xhci_portregs *reg;

	reg = XHCI_PORTREG_PTR(xdev, port);
	if (conn == 0) {
		reg->portsc &= ~(XHCI_PS_CCS | XHCI_PS_PED);
		reg->portsc |= (XHCI_PS_CSC |
				XHCI_PS_PLS_SET(UPS_PORT_LS_RX_DET));
	} else {
		speed = pci_xhci_convert_speed(usb_speed);
		reg->portsc = XHCI_PS_CCS | XHCI_PS_PP | XHCI_PS_CSC;
		reg->portsc |= XHCI_PS_SPEED_SET(speed);
	}

	if (!need_intr)
		return 0;

	if (!(xdev->opregs.usbcmd & XHCI_CMD_INTE))
		need_intr = 0;

	if (!(xdev->opregs.usbcmd & XHCI_CMD_RS))
		return 0;

	/* make an event for the guest OS */
	pci_xhci_set_evtrb(&evtrb,
			port,
			XHCI_TRB_ERROR_SUCCESS,
			XHCI_TRB_EVENT_PORT_STS_CHANGE);

	/* put it in the event ring */
	error = pci_xhci_insert_event(xdev, &evtrb, 1);
	if (error != XHCI_TRB_ERROR_SUCCESS)
		UPRINTF(LWRN, "fail to report port change\r\n");

	UPRINTF(LDBG, "%s: port %d:%08X\r\n", __func__, port, reg->portsc);
	return (error == XHCI_TRB_ERROR_SUCCESS) ? 0 : -1;
}

static int
pci_xhci_connect_port(struct pci_xhci_vdev *xdev, int port, int usb_speed,
		int intr)
{
	return pci_xhci_change_port(xdev, port, usb_speed, 1, intr);
}

static int
pci_xhci_disconnect_port(struct pci_xhci_vdev *xdev, int port, int intr)
{
	/* for disconnect, the speed is useless */
	return pci_xhci_change_port(xdev, port, 0, 0, intr);
}

static void
pci_xhci_set_evtrb(struct xhci_trb *evtrb, uint64_t port, uint32_t errcode,
	uint32_t evtype)
{
	evtrb->qwTrb0 = port << 24;
	evtrb->dwTrb2 = XHCI_TRB_2_ERROR_SET(errcode);
	evtrb->dwTrb3 = XHCI_TRB_3_TYPE_SET(evtype);
}

/* controller reset */
static void
pci_xhci_reset(struct pci_xhci_vdev *xdev)
{
	int i;

	xdev->rtsregs.er_enq_idx = 0;
	xdev->rtsregs.er_enq_seg = 0;
	xdev->rtsregs.event_pcs = 1;
	struct pci_xhci_dev_emu *dev;

	for (i = 1; i <= XHCI_MAX_DEVS; i++)
	{
		dev = xdev->devices[i];
		if (dev) {
			xdev->devices[i] = NULL;
			pci_xhci_dev_destroy(dev);
			/* FIXME: The ndevices hasn't
			 * aligned between ++ and --
			 * */
			xdev->ndevices--;
		}
	}

	for (i = 1; i <= XHCI_MAX_SLOTS; i++)
	{
		xdev->slots[i] = NULL;
		xdev->slot_allocated[i] = false;
	}
}

static uint32_t
pci_xhci_usbcmd_write(struct pci_xhci_vdev *xdev, uint32_t cmd)
{
	int i, j;
	struct pci_xhci_native_port *p;

	if (cmd & XHCI_CMD_RS) {
		xdev->opregs.usbcmd |= XHCI_CMD_RS;
		xdev->opregs.usbsts &= ~XHCI_STS_HCH;
		xdev->opregs.usbsts |= XHCI_STS_PCD;
	} else {
		xdev->opregs.usbcmd &= ~XHCI_CMD_RS;
		xdev->opregs.usbsts |= XHCI_STS_HCH;
		xdev->opregs.usbsts &= ~XHCI_STS_PCD;
	}

	/* start execution of schedule; stop when set to 0 */
	cmd |= xdev->opregs.usbcmd & XHCI_CMD_RS;

	if (cmd & XHCI_CMD_HCRST) {
		/* reset controller */
		pci_xhci_reset(xdev);
		cmd &= ~XHCI_CMD_HCRST;
	}

	if (cmd & XHCI_CMD_CSS) {
		/* TODO: should think about what happen if system S3 fail
		 * and under that situation, the vbdp_devs and se_dev_num
		 * should also need to be cleared
		 */
		xdev->vbdp_dev_num = 0;
		memset(xdev->vbdp_devs, 0, sizeof(xdev->vbdp_devs));

		for (i = 0; i < XHCI_MAX_VIRT_PORTS; ++i) {
			p = &xdev->native_ports[i];
			if (xdev->native_ports[i].state == VPORT_EMULATED) {
				/* save the device state before suspending */
				j = xdev->vbdp_dev_num;
				xdev->vbdp_devs[j].path = p->info.path;
				xdev->vbdp_devs[j].vport = p->vport;
				xdev->vbdp_devs[j].state = S3_VBDP_START;
				xdev->vbdp_dev_num++;

				/* clear PORTSC register */
				pci_xhci_init_port(xdev, p->vport);

				/* clear other information for this device*/
				p->vport = 0;
				p->state = VPORT_ASSIGNED;
				UPRINTF(LINF, "s3: save %d-%s state\r\n",
						p->info.path.bus,
						usb_dev_path(&p->info.path));
			}
		}
	}

	cmd &= ~(XHCI_CMD_CSS | XHCI_CMD_CRS);
	return cmd;
}

static void
pci_xhci_portregs_write(struct pci_xhci_vdev *xdev,
			uint64_t offset,
			uint64_t value)
{
	struct xhci_trb		evtrb;
	struct pci_xhci_portregs *p;
	int port;
	uint32_t oldpls, newpls;

	if (xdev->portregs == NULL)
		return;

	port = (offset - XHCI_PORTREGS_PORT0) / XHCI_PORTREGS_SETSZ;
	offset = (offset - XHCI_PORTREGS_PORT0) % XHCI_PORTREGS_SETSZ;

	UPRINTF(LDBG, "portregs wr offset 0x%lx, port %u: 0x%lx\r\n",
		offset, port, value);

	if (port > XHCI_MAX_DEVS || port < 0) {
		UPRINTF(LWRN, "portregs_write to bad port %d\r\n", port);
		return;
	}

	p = XHCI_PORTREG_PTR(xdev, port);
	switch (offset) {
	case 0:
		/* port reset or warm reset */
		if (value & (XHCI_PS_PR | XHCI_PS_WPR)) {
			pci_xhci_reset_port(xdev, port, value & XHCI_PS_WPR);
			break;
		}

		if ((p->portsc & XHCI_PS_PP) == 0) {
			UPRINTF(LWRN, "portregs_write to unpowered "
				 "port %d\r\n", port);
			break;
		}

		/* Port status and control register  */
		oldpls = XHCI_PS_PLS_GET(p->portsc);
		newpls = XHCI_PS_PLS_GET(value);

		p->portsc &= XHCI_PS_PED | XHCI_PS_PLS_MASK |
			     XHCI_PS_SPEED_MASK | XHCI_PS_PIC_MASK;

		if (pci_xhci_is_vport_free(xdev, port) == true)
			p->portsc |= XHCI_PS_CCS;

		p->portsc |= (value &
			      ~(XHCI_PS_OCA |
				XHCI_PS_PR  |
				XHCI_PS_PED |
				XHCI_PS_PLS_MASK   |	/* link state */
				XHCI_PS_SPEED_MASK |
				XHCI_PS_PIC_MASK   |	/* port indicator */
				XHCI_PS_LWS | XHCI_PS_DR | XHCI_PS_WPR));

		/* clear control bits */
		p->portsc &= ~(value &
			       (XHCI_PS_CSC |
				XHCI_PS_PEC |
				XHCI_PS_WRC |
				XHCI_PS_OCC |
				XHCI_PS_PRC |
				XHCI_PS_PLC |
				XHCI_PS_CEC |
				XHCI_PS_CAS));

		/* port disable request; for USB3, don't care */
		if (value & XHCI_PS_PED)
			UPRINTF(LDBG, "Disable port %d request\r\n", port);

		if (!(value & XHCI_PS_LWS))
			break;

		UPRINTF(LDBG, "Port new PLS: %d\r\n", newpls);

		switch (newpls) {
		case UPS_PORT_LS_U0:
		case UPS_PORT_LS_U3:
			if (oldpls != newpls) {
				p->portsc &= ~XHCI_PS_PLS_MASK;
				p->portsc |= XHCI_PS_PLS_SET(newpls);

				/*
				 * TODO:
				 * Should check if this is exactly
				 * consistent with xHCI spec.
				 */
				if (newpls == 0)
					p->portsc |= XHCI_PS_PLC;

				if (oldpls != 0 && newpls == 0) {
					pci_xhci_set_evtrb(&evtrb, port,
					    XHCI_TRB_ERROR_SUCCESS,
					    XHCI_TRB_EVENT_PORT_STS_CHANGE);
					pci_xhci_insert_event(xdev, &evtrb, 1);
				}
			}
			break;
		case UPS_PORT_LS_RESUME:
			p->portsc &= ~XHCI_PS_PLS_MASK;
			p->portsc |= XHCI_PS_PLS_SET(newpls);
			break;
		default:
			UPRINTF(LWRN, "Unhandled change port %d PLS %u\r\n",
				 port, newpls);
			break;
		}
		break;
	case 4:
		/* Port power management status and control register  */
		p->portpmsc = value;
		break;
	case 8:
		/* Port link information register */
		UPRINTF(LDBG, "attempted write to PORTLI, port %d\r\n",
			port);
		break;
	case 12:
		/*
		 * Port hardware LPM control register.
		 * For USB3, this register is reserved.
		 */
		p->porthlpmc = value;
		break;
	}
}

static int
pci_xhci_apl_drdregs_write(struct pci_xhci_vdev *xdev, uint64_t offset,
		uint64_t value)
{
	int rc = 0, fd;
	char *mstr;
	int msz = 0;
	uint32_t drdcfg1 = 0;
	struct pci_xhci_excap *excap;
	struct pci_xhci_excap_drd_apl *excap_drd;

	excap = xdev->excap_ptr;
	while (excap && excap->start != XHCI_APL_DRDCAP_BASE)
		excap++;

	if (!excap || !excap->data || excap->start != XHCI_APL_DRDCAP_BASE) {
		UPRINTF(LWRN, "drd extended capability can't be found\r\n");
		return -1;
	}

	excap_drd = excap->data;

	offset -= XHCI_APL_DRDREGS_BASE;
	if (offset != XHCI_DRD_MUX_CFG0) {
		UPRINTF(LWRN, "drd configuration register access failed.\r\n");
		return -1;
	}

	if (excap_drd->drdcfg0 == value) {
		UPRINTF(LDBG, "No mode switch action. Current drd: %s mode\r\n",
			excap_drd->drdcfg1 & XHCI_DRD_CFG1_HOST_MODE ?
			"host" : "device");
		return 0;
	}

	excap_drd->drdcfg0 = value;

	if (value & XHCI_DRD_CFG0_IDPIN_EN) {
		if ((value & XHCI_DRD_CFG0_IDPIN) == 0) {
			mstr = XHCI_NATIVE_DRD_HOST_MODE;
			msz = strlen(XHCI_NATIVE_DRD_HOST_MODE);
			drdcfg1 |= XHCI_DRD_CFG1_HOST_MODE;
		} else {
			mstr = XHCI_NATIVE_DRD_DEV_MODE;
			msz = strlen(XHCI_NATIVE_DRD_DEV_MODE);
			drdcfg1 &= ~XHCI_DRD_CFG1_HOST_MODE;
		}
	} else
		return 0;

	fd = open(XHCI_NATIVE_DRD_SWITCH_PATH, O_WRONLY);
	if (fd < 0) {
		UPRINTF(LWRN, "drd native interface open failed\r\n");
		return -1;
	}

	rc = write(fd, mstr, msz);
	close(fd);
	if (rc == msz)
		excap_drd->drdcfg1 = drdcfg1;
	else {
		UPRINTF(LWRN, "drd native interface write "
			"%s mode failed, drdcfg0: 0x%x, "
			"drdcfg1: 0x%x.\r\n",
			value & XHCI_DRD_CFG0_IDPIN ? "device" : "host",
			excap_drd->drdcfg0, excap_drd->drdcfg1);
		return -1;
	}
	return 0;
}

static void
pci_xhci_excap_write(struct pci_xhci_vdev *xdev, uint64_t offset,
			uint64_t value)
{
	int rc = 0;

	if (xdev->excap_ptr && xdev->excap_write)
		rc = xdev->excap_write(xdev, offset, value);
	else
		UPRINTF(LWRN, "write invalid offset 0x%lx\r\n", offset);

	if (rc)
		UPRINTF(LWRN, "something wrong for xhci excap offset "
				"0x%lx write \r\n", offset);
}

struct xhci_dev_ctx *
pci_xhci_get_dev_ctx(struct pci_xhci_vdev *xdev, uint32_t slot)
{
	uint64_t devctx_addr;
	struct xhci_dev_ctx *devctx;

	if (slot <= 0 || slot > XHCI_MAX_SLOTS || !xdev->slot_allocated[slot] ||
			!xdev->opregs.dcbaa_p) {
		UPRINTF(LFTL, "invalid ctx: slot %d, alloc %d dcbaa %p\r\n",
				slot, xdev->slot_allocated[slot],
				xdev->opregs.dcbaa_p);
		return NULL;
	}

	devctx_addr = xdev->opregs.dcbaa_p->dcba[slot];

	if (devctx_addr == 0) {
		UPRINTF(LDBG, "get_dev_ctx devctx_addr == 0 slot %d\r\n", slot);
		return NULL;
	}

	UPRINTF(LDBG, "get dev ctx, slot %u devctx addr %016lx\r\n",
		slot, devctx_addr);
	devctx = XHCI_GADDR(xdev, devctx_addr & ~0x3FUL);

	return devctx;
}

struct xhci_trb *
pci_xhci_trb_next(struct pci_xhci_vdev *xdev,
		  struct xhci_trb *curtrb,
		  uint64_t *guestaddr)
{
	struct xhci_trb *next;

	if (XHCI_TRB_3_TYPE_GET(curtrb->dwTrb3) == XHCI_TRB_TYPE_LINK) {
		if (guestaddr)
			*guestaddr = curtrb->qwTrb0 & ~0xFUL;

		next = XHCI_GADDR(xdev, curtrb->qwTrb0 & ~0xFUL);
	} else {
		if (guestaddr)
			*guestaddr += sizeof(struct xhci_trb) & ~0xFUL;

		next = curtrb + 1;
	}

	return next;
}

static void
pci_xhci_assert_interrupt(struct pci_xhci_vdev *xdev)
{

	xdev->rtsregs.intrreg.erdp |= XHCI_ERDP_LO_BUSY;
	xdev->rtsregs.intrreg.iman |= XHCI_IMAN_INTR_PEND;
	xdev->opregs.usbsts |= XHCI_STS_EINT;

	/* only trigger interrupt if permitted */
	if ((xdev->opregs.usbcmd & XHCI_CMD_INTE) &&
	    (xdev->rtsregs.intrreg.iman & XHCI_IMAN_INTR_ENA)) {
		if (pci_msi_enabled(xdev->dev))
			pci_generate_msi(xdev->dev, 0);
		else
			pci_lintr_assert(xdev->dev);
	}
}

static void
pci_xhci_deassert_interrupt(struct pci_xhci_vdev *xdev)
{
	if (!pci_msi_enabled(xdev->dev))
		pci_lintr_assert(xdev->dev);
}

static struct usb_xfer *
pci_xhci_alloc_usb_xfer(struct pci_xhci_dev_emu *dev, int epid)
{
	struct usb_xfer *xfer;
	struct xhci_dev_ctx *dev_ctx;
	struct xhci_endp_ctx *ep_ctx;
	int max_blk_cnt, i = 0;
	uint8_t type;

	if (!dev)
		return NULL;

	dev_ctx = dev->dev_ctx;
	ep_ctx = &dev_ctx->ctx_ep[epid];
	type = XHCI_EPCTX_1_EPTYPE_GET(ep_ctx->dwEpCtx1);

	/* TODO:
	 * The following code is still not perfect, due to fixed values are
	 * not flexible and the overflow risk is still existed. Will try to
	 * find a dynamic way could work both for Linux and Windows.
	 */
	switch (type) {
	case XHCI_EPTYPE_CTRL:
	case XHCI_EPTYPE_INT_IN:
	case XHCI_EPTYPE_INT_OUT:
		max_blk_cnt = 128;
		break;
	case XHCI_EPTYPE_BULK_IN:
	case XHCI_EPTYPE_BULK_OUT:
		max_blk_cnt = 1024;
		break;
	case XHCI_EPTYPE_ISOC_IN:
	case XHCI_EPTYPE_ISOC_OUT:
		max_blk_cnt = 2048;
		break;
	default:
		UPRINTF(LFTL, "err: unexpected epid %d type %d\r\n",
				epid, type);
		return NULL;
	}

	xfer = calloc(1, sizeof(struct usb_xfer));
	if (!xfer)
		return NULL;

	xfer->reqs = calloc(max_blk_cnt, sizeof(struct usb_dev_req *));
	if (!xfer->reqs)
		goto fail;

	xfer->data = calloc(max_blk_cnt, sizeof(struct usb_block));
	if (!xfer->data)
		goto fail;

	for (i = 0; i < max_blk_cnt; ++i) {
		xfer->data[i].hcb = calloc(1, sizeof(struct xhci_block));
		if (!xfer->data[i].hcb)
			goto fail;
	}

	UPRINTF(LINF, "allocate %d blocks for epid %d type %d\r\n",
			max_blk_cnt, epid, type);

	xfer->max_blk_cnt = max_blk_cnt;
	xfer->dev = (void *)dev;
	xfer->epid = epid;
	return xfer;

fail:
	if (xfer->data) {
		for (; i >= 0; i--)
			if (xfer->data[i].hcb)
				free(xfer->data[i].hcb);
		free(xfer->data);
	}
	if (xfer->reqs)
		free(xfer->reqs);
	free(xfer);
	return NULL;
}

static void
pci_xhci_free_usb_xfer(struct usb_xfer *xfer)
{
	int i;

	if (!xfer)
		return;

	for (i = 0; i < xfer->max_blk_cnt; i++)
		free(xfer->data[i].hcb);

	free(xfer->data);
	free(xfer->reqs);
	free(xfer->ureq);
	free(xfer);
}

static int
pci_xhci_init_ep(struct pci_xhci_dev_emu *dev, int epid, uint32_t slot)
{
	struct xhci_dev_ctx	*dev_ctx;
	struct pci_xhci_dev_ep	*devep;
	struct xhci_endp_ctx	*ep_ctx;
	pthread_mutexattr_t	attr;
	uint32_t		pstreams;
	int			i, rc;

	dev_ctx = dev->dev_ctx;
	ep_ctx = &dev_ctx->ctx_ep[epid];
	devep = &dev->eps[epid];

	rc = pthread_mutexattr_init(&attr);
	if (rc) {
		UPRINTF(LFTL, "%s: mutexattr init failed %d\r\n", __func__, rc);
		return -1;
	}

	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (rc) {
		UPRINTF(LFTL, "%s: mutexattr set failed %d\r\n", __func__, rc);
		return -1;
	}

	rc = pthread_mutex_init(&devep->mtx, &attr);
	if (rc) {
		UPRINTF(LFTL, "%s: mutexlock init failed %d\r\n", __func__, rc);
		return -1;
	}

	pstreams = XHCI_EPCTX_0_MAXP_STREAMS_GET(ep_ctx->dwEpCtx0);
	if (pstreams > 0) {
		UPRINTF(LDBG, "init_ep %d with pstreams %d\r\n",
				epid, pstreams);

		devep->ep_sctx = XHCI_GADDR(dev->xdev, ep_ctx->qwEpCtx2 &
					    XHCI_EPCTX_2_TR_DQ_PTR_MASK);
		devep->ep_sctx_trbs = calloc(pstreams,
				      sizeof(struct pci_xhci_trb_ring));
		for (i = 0; i < pstreams; i++) {
			devep->ep_sctx_trbs[i].ringaddr =
						 devep->ep_sctx[i].qwSctx0 &
						 XHCI_SCTX_0_TR_DQ_PTR_MASK;
			devep->ep_sctx_trbs[i].ccs =
			     XHCI_SCTX_0_DCS_GET(devep->ep_sctx[i].qwSctx0);
		}
	} else {
		UPRINTF(LDBG, "init_ep %d with no pstreams\r\n", epid);
		devep->ep_ringaddr = ep_ctx->qwEpCtx2 &
				     XHCI_EPCTX_2_TR_DQ_PTR_MASK;
		devep->ep_ccs = XHCI_EPCTX_2_DCS_GET(ep_ctx->qwEpCtx2);
		devep->ep_tr = XHCI_GADDR(dev->xdev, devep->ep_ringaddr);
		UPRINTF(LDBG, "init_ep tr DCS %x\r\n", devep->ep_ccs);
	}

	if (devep->ep_xfer == NULL) {
		devep->ep_xfer = pci_xhci_alloc_usb_xfer(dev, epid);
		if (!devep->ep_xfer)
			goto errout;
	}

	devep->timer_data.dev = dev;
	devep->timer_data.slot = slot;
	devep->timer_data.epnum = epid;
	devep->timer_data.dir = (epid & 0x1) ? TOKEN_IN : TOKEN_OUT;
	devep->isoc_timer.clockid = CLOCK_MONOTONIC;
	rc = acrn_timer_init(&devep->isoc_timer, pci_xhci_isoc_handler,
			&devep->timer_data);
	if (rc < 0) {
		UPRINTF(LFTL, "ep%d: failed to create isoc timer\r\n", epid);
		goto errout;
	}

	return 0;

errout:
	pci_xhci_free_usb_xfer(devep->ep_xfer);
	devep->ep_xfer = NULL;
	devep->timer_data.dev = NULL;
	devep->timer_data.slot = 0;
	devep->timer_data.epnum = 0;
	pthread_mutex_destroy(&devep->mtx);
	return -1;
}

static void
pci_xhci_disable_ep(struct pci_xhci_dev_emu *dev, int epid)
{
	struct xhci_dev_ctx    *dev_ctx;
	struct pci_xhci_dev_ep *devep;
	struct xhci_endp_ctx   *ep_ctx;

	UPRINTF(LDBG, "pci_xhci disable_ep %d\r\n", epid);
	dev_ctx = dev->dev_ctx;
	ep_ctx = &dev_ctx->ctx_ep[epid];
	ep_ctx->dwEpCtx0 = (ep_ctx->dwEpCtx0 & ~0x7) | XHCI_ST_EPCTX_DISABLED;

	devep = &dev->eps[epid];
	pthread_mutex_lock(&devep->mtx);

	if (XHCI_EPCTX_0_MAXP_STREAMS_GET(ep_ctx->dwEpCtx0) > 0 &&
		devep->ep_sctx_trbs != NULL)
		free(devep->ep_sctx_trbs);

	if (devep->ep_xfer != NULL) {
		pci_xhci_free_usb_xfer(devep->ep_xfer);
		devep->ep_xfer = NULL;
	}

	acrn_timer_deinit(&devep->isoc_timer);
	devep->timer_data.dev = NULL;
	devep->timer_data.slot = 0;
	devep->timer_data.epnum = 0;

	pthread_mutex_unlock(&devep->mtx);
	pthread_mutex_destroy(&devep->mtx);
}

/* reset device at slot and data structures related to it */
static void
pci_xhci_reset_slot(struct pci_xhci_vdev *xdev, int slot)
{
	struct pci_xhci_dev_emu *dev;

	dev = XHCI_SLOTDEV_PTR(xdev, slot);
	if (!dev)
		UPRINTF(LDBG, "reset unassigned slot (%d)?\r\n", slot);
	else
		dev->dev_slotstate = XHCI_ST_DISABLED;

	/* TODO: reset ring buffer pointers */
}

static int
pci_xhci_insert_event(struct pci_xhci_vdev *xdev,
		      struct xhci_trb *evtrb,
		      int do_intr)
{
	struct pci_xhci_rtsregs *rts;
	struct xhci_erst *erst;
	struct xhci_trb *evts;
	uint64_t erdp;
	int erdp_idx, err;

	err = XHCI_TRB_ERROR_SUCCESS;

	rts = &xdev->rtsregs;
	erdp = rts->intrreg.erdp & ~0xF;
	erst = &rts->erstba_p[rts->er_enq_seg];
	erdp_idx = (erdp - erst->qwRingSegBase) / sizeof(struct xhci_trb);

	UPRINTF(LDBG, "insert event 0[%lx] 2[%x] 3[%x]\r\n"
			"\terdp idx %d/seg %d, enq idx %d/seg %d, pcs %u\r\n"
			"\t(erdp=0x%lx, erst=0x%lx, tblsz=%u, do_intr %d)\r\n",
			evtrb->qwTrb0, evtrb->dwTrb2, evtrb->dwTrb3,
			erdp_idx, rts->er_deq_seg,
			rts->er_enq_idx, rts->er_enq_seg,
			rts->event_pcs, erdp,
			rts->erstba_p->qwRingSegBase,
			rts->erstba_p->dwRingSegSize, do_intr);

	evtrb->dwTrb3 &= ~XHCI_TRB_3_CYCLE_BIT;
	evtrb->dwTrb3 |= rts->event_pcs;

	erst = &rts->erstba_p[rts->er_enq_seg];
	evts = XHCI_GADDR(xdev, erst->qwRingSegBase);
	memcpy(&evts[rts->er_enq_idx], evtrb, sizeof(struct xhci_trb));

	if (rts->er_enq_idx == erst->dwRingSegSize - 1) {
		rts->er_enq_idx = 0;
		rts->er_enq_seg = (rts->er_enq_seg + 1) % rts->intrreg.erstsz;
	} else {
		rts->er_enq_idx = (rts->er_enq_idx + 1) % erst->dwRingSegSize;
	}

	if (rts->er_enq_idx == 0 && rts->er_enq_seg == 0)
		rts->event_pcs ^= 1;

	if (do_intr)
		pci_xhci_assert_interrupt(xdev);

	return err;
}

static uint32_t
pci_xhci_cmd_enable_slot(struct pci_xhci_vdev *xdev, uint32_t *slot)
{
	uint32_t cmderr;
	int i;

	cmderr = XHCI_TRB_ERROR_SUCCESS;
	for (i = 1; i <= XHCI_MAX_SLOTS; i++)
		if (xdev->slot_allocated[i] == false)
			break;

	if (i > XHCI_MAX_SLOTS)
		cmderr = XHCI_TRB_ERROR_NO_SLOTS;
	else {
		xdev->slot_allocated[i] = true;
		*slot = i;
	}

	UPRINTF(LINF, "enable slot (error=%d) return slot %u\r\n",
			cmderr != XHCI_TRB_ERROR_SUCCESS, *slot);
	return cmderr;
}

static uint32_t
pci_xhci_cmd_disable_slot(struct pci_xhci_vdev *xdev, uint32_t slot)
{
	struct pci_xhci_dev_emu *dev;
	struct usb_dev *udev;
	struct usb_native_devinfo *di = NULL;
	struct usb_devpath *path;
	uint32_t cmderr;
	int i, j, index;

	UPRINTF(LINF, "pci_xhci disable slot %u\r\n", slot);

	cmderr = XHCI_TRB_ERROR_NO_SLOTS;
	if (xdev->portregs == NULL)
		goto done;

	if (slot > xdev->ndevices) {
		cmderr = XHCI_TRB_ERROR_SLOT_NOT_ON;
		goto done;
	}

	dev = XHCI_SLOTDEV_PTR(xdev, slot);
	if (dev) {
		if (dev->dev_slotstate == XHCI_ST_DISABLED) {
			cmderr = XHCI_TRB_ERROR_SLOT_NOT_ON;
		} else {
			dev->dev_slotstate = XHCI_ST_DISABLED;
			cmderr = XHCI_TRB_ERROR_SUCCESS;
			/* TODO: reset events and endpoints */
		}
	} else {
		UPRINTF(LINF, "disable NULL device, slot %d\r\n", slot);
		goto done;
	}

	for (i = 1; i <= XHCI_MAX_DEVS; ++i)
		if (dev == xdev->devices[i])
			break;

	if (i <= XHCI_MAX_DEVS && XHCI_PORTREG_PTR(xdev, i)) {
		XHCI_PORTREG_PTR(xdev, i)->portsc &= ~(XHCI_PS_CSC |
				XHCI_PS_CCS | XHCI_PS_PED);

		udev = dev->dev_instance;

		xdev->devices[i] = NULL;
		xdev->slots[slot] = NULL;
		xdev->slot_allocated[slot] = false;

		di = &udev->info;
		index = pci_xhci_get_native_port_index_by_path(xdev, &di->path);
		if (index < 0) {
			/*
			 * one possible reason for failing to find the device is
			 * it is plugged out during the resuming process. we
			 * should give the xhci_vbdp_thread an opportunity to
			 * try.
			 */
			sem_post(&xdev->vbdp_sem);
			cmderr = XHCI_TRB_ERROR_SLOT_NOT_ON;
			goto done;
		}

		for (j = 0; j < XHCI_MAX_VIRT_PORTS; ++j) {
			path = &xdev->vbdp_devs[j].path;

			if (!usb_dev_path_cmp(path, &di->path))
				continue;

			xdev->vbdp_devs[j].state = S3_VBDP_END;
			xdev->vbdp_dev_num--;
			sem_post(&xdev->vbdp_sem);
			UPRINTF(LINF, "signal device %d-%s to connect\r\n",
					di->path.bus, usb_dev_path(&di->path));
		}
		UPRINTF(LINF, "disable slot %d for native device %d-%s\r\n",
				slot, di->path.bus, usb_dev_path(&di->path));

		/* release all the resource allocated for virtual device */
		pci_xhci_dev_destroy(dev);
	} else
		UPRINTF(LWRN, "invalid slot %d\r\n", slot);

done:
	return cmderr;
}

static uint32_t
pci_xhci_cmd_reset_device(struct pci_xhci_vdev *xdev, uint32_t slot)
{
	struct pci_xhci_dev_emu *dev;
	struct xhci_dev_ctx     *dev_ctx;
	struct xhci_endp_ctx    *ep_ctx;
	uint32_t	cmderr;
	int		i;

	cmderr = XHCI_TRB_ERROR_NO_SLOTS;
	if (xdev->portregs == NULL)
		goto done;

	UPRINTF(LDBG, "pci_xhci reset device slot %u\r\n", slot);

	dev = XHCI_SLOTDEV_PTR(xdev, slot);
	if (!dev || dev->dev_slotstate == XHCI_ST_DISABLED)
		cmderr = XHCI_TRB_ERROR_SLOT_NOT_ON;
	else {
		dev->dev_slotstate = XHCI_ST_DEFAULT;

		dev_ctx = pci_xhci_get_dev_ctx(xdev, slot);
		if (!dev_ctx) {
			cmderr = XHCI_TRB_ERROR_SLOT_NOT_ON;
			goto done;
		}

		/* slot state */
		dev_ctx->ctx_slot.dwSctx3 =
			FIELD_REPLACE(dev_ctx->ctx_slot.dwSctx3,
				      XHCI_ST_SLCTX_DEFAULT, 0x1F, 27);

		/* number of contexts */
		dev_ctx->ctx_slot.dwSctx0 =
			FIELD_REPLACE(dev_ctx->ctx_slot.dwSctx0, 1, 0x1F, 27);

		/* reset all eps other than ep-0 */
		for (i = 2; i <= 31; i++) {
			ep_ctx = &dev_ctx->ctx_ep[i];
			ep_ctx->dwEpCtx0 =
				FIELD_REPLACE(ep_ctx->dwEpCtx0,
					      XHCI_ST_EPCTX_DISABLED, 0x7, 0);
		}

		cmderr = XHCI_TRB_ERROR_SUCCESS;
	}

	pci_xhci_reset_slot(xdev, slot);

done:
	return cmderr;
}

static uint32_t
pci_xhci_cmd_address_device(struct pci_xhci_vdev *xdev,
			    uint32_t slot,
			    struct xhci_trb *trb)
{
	struct pci_xhci_dev_emu	*dev;
	struct xhci_input_dev_ctx *input_ctx;
	struct xhci_slot_ctx *islot_ctx;
	struct xhci_dev_ctx *dev_ctx;
	struct xhci_endp_ctx *ep0_ctx;
	struct usb_native_devinfo *di;
	uint32_t cmderr;
	uint8_t rh_port;

	input_ctx = XHCI_GADDR(xdev, trb->qwTrb0 & ~0xFUL);
	islot_ctx = &input_ctx->ctx_slot;
	ep0_ctx = &input_ctx->ctx_ep[1];

	cmderr = XHCI_TRB_ERROR_SUCCESS;

	UPRINTF(LINF, "address device, input ctl: D 0x%08x A 0x%08x,\r\n"
		 "          slot %08x %08x %08x %08x\r\n"
		 "          ep0  %08x %08x %016lx %08x\r\n",
		input_ctx->ctx_input.dwInCtx0, input_ctx->ctx_input.dwInCtx1,
		islot_ctx->dwSctx0, islot_ctx->dwSctx1,
		islot_ctx->dwSctx2, islot_ctx->dwSctx3,
		ep0_ctx->dwEpCtx0, ep0_ctx->dwEpCtx1, ep0_ctx->qwEpCtx2,
		ep0_ctx->dwEpCtx4);

	/* when setting address: drop-ctx=0, add-ctx=slot+ep0 */
	if ((input_ctx->ctx_input.dwInCtx0 != 0) ||
	    (input_ctx->ctx_input.dwInCtx1 & 0x03) != 0x03) {
		UPRINTF(LWRN, "address device, input ctl invalid\r\n");
		cmderr = XHCI_TRB_ERROR_TRB;
		goto done;
	}

	if (slot <= 0 || slot > XHCI_MAX_SLOTS ||
			xdev->slot_allocated[slot] == false) {
		UPRINTF(LWRN, "address device, invalid slot %d\r\n", slot);
		cmderr = XHCI_TRB_ERROR_SLOT_NOT_ON;
		goto done;
	}

	dev = xdev->slots[slot];
	if (!dev) {
		int index;

		rh_port = XHCI_SCTX_1_RH_PORT_GET(islot_ctx->dwSctx1);
		index = pci_xhci_get_native_port_index_by_vport(xdev, rh_port);
		if (index < 0) {
			cmderr = XHCI_TRB_ERROR_TRB;
			UPRINTF(LFTL, "invalid root hub port %d\r\n", rh_port);
			goto done;
		}

		di = &xdev->native_ports[index].info;
		UPRINTF(LINF, "create virtual device for %d-%s on virtual "
				"port %d\r\n", di->path.bus,
				usb_dev_path(&di->path), rh_port);

		dev = pci_xhci_dev_create(xdev, di);
		if (!dev) {
			UPRINTF(LFTL, "fail to create device for %d-%s\r\n",
					di->path.bus,
					usb_dev_path(&di->path));
			goto done;
		}

		xdev->native_ports[index].state = VPORT_EMULATED;
		xdev->devices[rh_port] = dev;
		xdev->ndevices++;
		xdev->slots[slot] = dev;
		dev->hci.hci_address = slot;
	}

	/* assign address to slot */
	dev_ctx = pci_xhci_get_dev_ctx(xdev, slot);
	if (!dev_ctx) {
		cmderr = XHCI_TRB_ERROR_CONTEXT_STATE;
		goto done;
	}
	UPRINTF(LDBG, "address device, dev ctx\r\n"
		 "      slot %08x %08x %08x %08x\r\n",
		dev_ctx->ctx_slot.dwSctx0, dev_ctx->ctx_slot.dwSctx1,
		dev_ctx->ctx_slot.dwSctx2, dev_ctx->ctx_slot.dwSctx3);

	dev = XHCI_SLOTDEV_PTR(xdev, slot);

	dev->hci.hci_address = slot;
	dev->dev_ctx = dev_ctx;

	if (dev->dev_ue->ue_reset == NULL ||
	    dev->dev_ue->ue_reset(dev->dev_instance) < 0) {
		cmderr = XHCI_TRB_ERROR_ENDP_NOT_ON;
		goto done;
	}

	memcpy(&dev_ctx->ctx_slot, islot_ctx, sizeof(struct xhci_slot_ctx));

	dev_ctx->ctx_slot.dwSctx3 =
		XHCI_SCTX_3_SLOT_STATE_SET(XHCI_ST_SLCTX_ADDRESSED) |
		XHCI_SCTX_3_DEV_ADDR_SET(slot);

	memcpy(&dev_ctx->ctx_ep[1], ep0_ctx, sizeof(struct xhci_endp_ctx));
	ep0_ctx = &dev_ctx->ctx_ep[1];
	ep0_ctx->dwEpCtx0 = (ep0_ctx->dwEpCtx0 & ~0x7) |
		XHCI_EPCTX_0_EPSTATE_SET(XHCI_ST_EPCTX_RUNNING);

	if (pci_xhci_init_ep(dev, 1, slot)) {
		cmderr = XHCI_TRB_ERROR_INCOMPAT_DEV;
		goto done;
	}

	dev->dev_slotstate = XHCI_ST_ADDRESSED;

	UPRINTF(LINF, "address device, output ctx\r\n"
		 "      slot %08x %08x %08x %08x\r\n"
		 "      ep0  %08x %08x %016lx %08x\r\n",
		dev_ctx->ctx_slot.dwSctx0, dev_ctx->ctx_slot.dwSctx1,
		dev_ctx->ctx_slot.dwSctx2, dev_ctx->ctx_slot.dwSctx3,
		ep0_ctx->dwEpCtx0, ep0_ctx->dwEpCtx1, ep0_ctx->qwEpCtx2,
		ep0_ctx->dwEpCtx4);

done:
	return cmderr;
}

static uint32_t
pci_xhci_cmd_config_ep(struct pci_xhci_vdev *xdev,
		       uint32_t slot,
		       struct xhci_trb *trb)
{
	struct xhci_input_dev_ctx *input_ctx;
	struct pci_xhci_dev_emu	*dev;
	struct xhci_dev_ctx	*dev_ctx;
	struct xhci_endp_ctx	*ep_ctx, *iep_ctx;
	uint32_t	cmderr;
	int		i;

	cmderr = XHCI_TRB_ERROR_SUCCESS;

	UPRINTF(LDBG, "config_ep slot %u\r\n", slot);

	dev = XHCI_SLOTDEV_PTR(xdev, slot);
	if (dev == NULL) {
		cmderr = XHCI_TRB_ERROR_SLOT_NOT_ON;
		goto done;
	}

	if ((trb->dwTrb3 & XHCI_TRB_3_DCEP_BIT) != 0) {
		UPRINTF(LDBG, "config_ep - deconfigure ep slot %u\r\n", slot);
		if (dev->dev_ue->ue_stop != NULL)
			dev->dev_ue->ue_stop(dev->dev_instance);

		dev->dev_slotstate = XHCI_ST_ADDRESSED;

		dev->hci.hci_address = 0;
		dev_ctx = pci_xhci_get_dev_ctx(xdev, slot);
		if (!dev_ctx) {
			cmderr = XHCI_TRB_ERROR_TRB;
			goto done;
		}

		/* number of contexts */
		dev_ctx->ctx_slot.dwSctx0 =
			FIELD_REPLACE(dev_ctx->ctx_slot.dwSctx0, 1, 0x1F, 27);

		/* slot state */
		dev_ctx->ctx_slot.dwSctx3 =
			FIELD_REPLACE(dev_ctx->ctx_slot.dwSctx3,
				      XHCI_ST_SLCTX_ADDRESSED, 0x1F, 27);

		/* disable endpoints */
		for (i = 2; i < 32; i++)
			pci_xhci_disable_ep(dev, i);

		cmderr = XHCI_TRB_ERROR_SUCCESS;
		goto done;
	}

	if (dev->dev_slotstate < XHCI_ST_ADDRESSED) {
		UPRINTF(LWRN, "config_ep slotstate x%x != addressed\r\n",
			dev->dev_slotstate);
		cmderr = XHCI_TRB_ERROR_SLOT_NOT_ON;
		goto done;
	}

	/* In addressed/configured state;
	 * for each drop endpoint ctx flag:
	 *   ep->state = DISABLED
	 * for each add endpoint ctx flag:
	 *   cp(ep-in, ep-out)
	 *   ep->state = RUNNING
	 * for each drop+add endpoint flag:
	 *   reset ep resources
	 *   cp(ep-in, ep-out)
	 *   ep->state = RUNNING
	 * if input->DisabledCtx[2-31] < 30: (at least 1 ep not disabled)
	 *   slot->state = configured
	 */

	input_ctx = XHCI_GADDR(xdev, trb->qwTrb0 & ~0xFUL);
	dev_ctx = dev->dev_ctx;
	UPRINTF(LDBG, "config_ep inputctx: D:x%08x A:x%08x 7:x%08x\r\n",
		input_ctx->ctx_input.dwInCtx0, input_ctx->ctx_input.dwInCtx1,
		input_ctx->ctx_input.dwInCtx7);

	for (i = 2; i <= 31; i++) {
		ep_ctx = &dev_ctx->ctx_ep[i];

		if (input_ctx->ctx_input.dwInCtx0 &
		    XHCI_INCTX_0_DROP_MASK(i)) {
			UPRINTF(LDBG, " config ep - dropping ep %d\r\n", i);
			pci_xhci_disable_ep(dev, i);
		}

		if (input_ctx->ctx_input.dwInCtx1 &
		    XHCI_INCTX_1_ADD_MASK(i)) {
			iep_ctx = &input_ctx->ctx_ep[i];

			UPRINTF(LDBG, " enable ep%d %08x %08x %016lx %08x\r\n",
				i, iep_ctx->dwEpCtx0, iep_ctx->dwEpCtx1,
				iep_ctx->qwEpCtx2, iep_ctx->dwEpCtx4);

			memcpy(ep_ctx, iep_ctx, sizeof(struct xhci_endp_ctx));

			if (pci_xhci_init_ep(dev, i, slot)) {
				cmderr = XHCI_TRB_ERROR_RESOURCE;
				goto error;
			}

			/* ep state */
			ep_ctx->dwEpCtx0 =
				FIELD_REPLACE(ep_ctx->dwEpCtx0,
					      XHCI_ST_EPCTX_RUNNING, 0x7, 0);
		}
	}

	/* slot state to configured */
	dev_ctx->ctx_slot.dwSctx3 =
		FIELD_REPLACE(dev_ctx->ctx_slot.dwSctx3,
			      XHCI_ST_SLCTX_CONFIGURED, 0x1F, 27);
	dev_ctx->ctx_slot.dwSctx0 =
		FIELD_COPY(dev_ctx->ctx_slot.dwSctx0,
			   input_ctx->ctx_slot.dwSctx0, 0x1F, 27);
	dev->dev_slotstate = XHCI_ST_CONFIGURED;

	UPRINTF(LDBG, "EP configured; slot %u [0]=0x%08x [1]=0x%08x"
			" [2]=0x%08x [3]=0x%08x\r\n", slot,
			dev_ctx->ctx_slot.dwSctx0,
			dev_ctx->ctx_slot.dwSctx1,
			dev_ctx->ctx_slot.dwSctx2,
			dev_ctx->ctx_slot.dwSctx3);

done:
	return cmderr;
error:
	for (; i >= 2; --i)
		pci_xhci_disable_ep(dev, i);
	return cmderr;
}

static uint32_t
pci_xhci_cmd_reset_ep(struct pci_xhci_vdev *xdev,
		      uint32_t slot,
		      struct xhci_trb *trb)
{
	struct pci_xhci_dev_emu	*dev;
	struct pci_xhci_dev_ep	*devep;
	struct xhci_dev_ctx	*dev_ctx;
	struct xhci_endp_ctx	*ep_ctx;
	struct usb_xfer	*xfer;
	struct usb_dev_req	*r;
	uint32_t cmderr, epid;
	uint32_t type;
	int i;

	epid = XHCI_TRB_3_EP_GET(trb->dwTrb3);

	UPRINTF(LDBG, "reset ep %u: slot %u cmd_type: %02X\r\n", epid, slot,
			XHCI_TRB_3_TYPE_GET(trb->dwTrb3));

	cmderr = XHCI_TRB_ERROR_SUCCESS;

	type = XHCI_TRB_3_TYPE_GET(trb->dwTrb3);

	dev = XHCI_SLOTDEV_PTR(xdev, slot);
	/* There have three scenarios the pointer will be NULL.
	 * 1.Enable slot command not received.
	 * 2.Enable slot command have been received but not receive
	 * address device command.
	 * 3.After received disable slot command.
	 * In scenario 1 and 3 the slot state should be Disabled.
	 * In scenario 2, the slot state should be Enabled.
	 * Fllow xHCI spec 4.6.8 and 4.6.9, for stop and reset endpoint
	 * command, when the slot state is Disabled the error code should
	 * be Slot Not Enabled Error, when the slot have been enabled by
	 * an Enable Slot Command the error code should be Context State Error.
	 * TODO: The dev_slotstate should move from pci_xhci_dev_emu to
	 * pci_xhci_vdev, and slot_allocated need to removed.
	 */

	if (!dev)
	{
		if (xdev->slot_allocated[slot] == true)
			cmderr = XHCI_TRB_ERROR_CONTEXT_STATE;
		else
			cmderr = XHCI_TRB_ERROR_SLOT_NOT_ON;
		goto done;
	}


	if (type == XHCI_TRB_TYPE_STOP_EP &&
	   (trb->dwTrb3 & XHCI_TRB_3_SUSP_EP_BIT) != 0) {
		/* XXX suspend endpoint for 10ms */
	}

	if (epid < 1 || epid > 31) {
		UPRINTF(LDBG, "reset ep: invalid epid %u\r\n", epid);
		cmderr = XHCI_TRB_ERROR_TRB;
		goto done;
	}

	dev_ctx = dev->dev_ctx;
	ep_ctx = &dev_ctx->ctx_ep[epid];

	if (type == XHCI_TRB_TYPE_RESET_EP &&
			(ep_ctx->dwEpCtx0 & 0x7) != XHCI_ST_EPCTX_HALTED) {
		cmderr = XHCI_TRB_ERROR_CONTEXT_STATE;
		goto done;
	}

	devep = &dev->eps[epid];
	pthread_mutex_lock(&devep->mtx);

	xfer = devep->ep_xfer;
	for (i = 0; i < xfer->max_blk_cnt; ++i) {
		r = xfer->reqs[i];
		if (r && r->trn)
			/* let usb_dev_comp_req to free the memory */
			libusb_cancel_transfer(r->trn);
	}

	xfer->ndata = 0;
	xfer->head = 0;
	xfer->tail = 0;

	ep_ctx->dwEpCtx0 = (ep_ctx->dwEpCtx0 & ~0x7) | XHCI_ST_EPCTX_STOPPED;

	if (XHCI_EPCTX_0_MAXP_STREAMS_GET(ep_ctx->dwEpCtx0) == 0)
		ep_ctx->qwEpCtx2 = devep->ep_ringaddr | devep->ep_ccs;

	UPRINTF(LDBG, "reset ep[%u] %08x %08x %016lx %08x\r\n",
		epid, ep_ctx->dwEpCtx0, ep_ctx->dwEpCtx1, ep_ctx->qwEpCtx2,
		ep_ctx->dwEpCtx4);
	pthread_mutex_unlock(&devep->mtx);

done:
	return cmderr;
}

static uint32_t
pci_xhci_find_stream(struct pci_xhci_vdev *xdev,
		     struct xhci_endp_ctx *ep,
		     uint32_t streamid,
		     struct xhci_stream_ctx **osctx)
{
	struct xhci_stream_ctx *sctx;
	uint32_t	maxpstreams;

	maxpstreams = XHCI_EPCTX_0_MAXP_STREAMS_GET(ep->dwEpCtx0);
	if (maxpstreams == 0)
		return XHCI_TRB_ERROR_TRB;

	if (maxpstreams > XHCI_STREAMS_MAX)
		return XHCI_TRB_ERROR_INVALID_SID;

	if (XHCI_EPCTX_0_LSA_GET(ep->dwEpCtx0) == 0) {
		UPRINTF(LWRN, "find_stream; LSA bit not set\r\n");
		return XHCI_TRB_ERROR_INVALID_SID;
	}

	/* only support primary stream */
	if (streamid > maxpstreams)
		return XHCI_TRB_ERROR_STREAM_TYPE;

	sctx = XHCI_GADDR(xdev, ep->qwEpCtx2 & ~0xFUL) + streamid;
	if (!XHCI_SCTX_0_SCT_GET(sctx->qwSctx0))
		return XHCI_TRB_ERROR_STREAM_TYPE;

	*osctx = sctx;

	return XHCI_TRB_ERROR_SUCCESS;
}

static uint32_t
pci_xhci_cmd_set_tr(struct pci_xhci_vdev *xdev,
		    uint32_t slot,
		    struct xhci_trb *trb)
{
	struct pci_xhci_dev_emu	*dev;
	struct pci_xhci_dev_ep	*devep;
	struct xhci_dev_ctx	*dev_ctx;
	struct xhci_endp_ctx	*ep_ctx;
	uint32_t	cmderr, epid;
	uint32_t	streamid;

	cmderr = XHCI_TRB_ERROR_SUCCESS;

	dev = XHCI_SLOTDEV_PTR(xdev, slot);
	UPRINTF(LDBG, "set_tr: new-tr x%016lx, SCT %u DCS %u\r\n"
		 "      stream-id %u, slot %u, epid %u, C %u\r\n",
		 (trb->qwTrb0 & ~0xF),  (uint32_t)((trb->qwTrb0 >> 1) & 0x7),
		 (uint32_t)(trb->qwTrb0 & 0x1), (trb->dwTrb2 >> 16) & 0xFFFF,
		 XHCI_TRB_3_SLOT_GET(trb->dwTrb3),
		 XHCI_TRB_3_EP_GET(trb->dwTrb3), trb->dwTrb3 & 0x1);

	epid = XHCI_TRB_3_EP_GET(trb->dwTrb3);
	if (epid < 1 || epid > 31) {
		UPRINTF(LDBG, "set_tr_deq: invalid epid %u\r\n", epid);
		cmderr = XHCI_TRB_ERROR_TRB;
		goto done;
	}

	dev_ctx = dev->dev_ctx;
	ep_ctx = &dev_ctx->ctx_ep[epid];
	devep = &dev->eps[epid];

	switch (XHCI_EPCTX_0_EPSTATE_GET(ep_ctx->dwEpCtx0)) {
	case XHCI_ST_EPCTX_STOPPED:
	case XHCI_ST_EPCTX_ERROR:
		break;
	default:
		UPRINTF(LDBG, "cmd set_tr invalid state %x\r\n",
			XHCI_EPCTX_0_EPSTATE_GET(ep_ctx->dwEpCtx0));
		cmderr = XHCI_TRB_ERROR_CONTEXT_STATE;
		goto done;
	}

	streamid = XHCI_TRB_2_STREAM_GET(trb->dwTrb2);
	if (XHCI_EPCTX_0_MAXP_STREAMS_GET(ep_ctx->dwEpCtx0) > 0) {
		struct xhci_stream_ctx *sctx;

		sctx = NULL;
		cmderr = pci_xhci_find_stream(xdev, ep_ctx, streamid, &sctx);
		if (sctx != NULL) {

			devep->ep_sctx[streamid].qwSctx0 = trb->qwTrb0;
			devep->ep_sctx_trbs[streamid].ringaddr =
			    trb->qwTrb0 & ~0xF;
			devep->ep_sctx_trbs[streamid].ccs =
			    XHCI_EPCTX_2_DCS_GET(trb->qwTrb0);
		}
	} else {
		if (streamid != 0) {
			UPRINTF(LDBG, "cmd set_tr streamid %x != 0\r\n",
				streamid);
		}
		ep_ctx->qwEpCtx2 = trb->qwTrb0 & ~0xFUL;
		devep->ep_ringaddr = ep_ctx->qwEpCtx2 & ~0xFUL;
		devep->ep_ccs = trb->qwTrb0 & 0x1;
		devep->ep_tr = XHCI_GADDR(xdev, devep->ep_ringaddr);

		UPRINTF(LDBG, "set_tr first TRB:\r\n");
		pci_xhci_dump_trb(devep->ep_tr);
	}
	ep_ctx->dwEpCtx0 = (ep_ctx->dwEpCtx0 & ~0x7) | XHCI_ST_EPCTX_STOPPED;

done:
	return cmderr;
}

static uint32_t
pci_xhci_cmd_eval_ctx(struct pci_xhci_vdev *xdev,
		      uint32_t slot,
		      struct xhci_trb *trb)
{
	struct xhci_input_dev_ctx *input_ctx;
	struct xhci_slot_ctx      *islot_ctx;
	struct xhci_dev_ctx       *dev_ctx;
	struct xhci_endp_ctx      *ep0_ctx;
	uint32_t cmderr;

	input_ctx = XHCI_GADDR(xdev, trb->qwTrb0 & ~0xFUL);
	islot_ctx = &input_ctx->ctx_slot;
	ep0_ctx = &input_ctx->ctx_ep[1];

	cmderr = XHCI_TRB_ERROR_SUCCESS;
	UPRINTF(LDBG, "eval ctx, input ctl: D 0x%08x A 0x%08x,\r\n"
		 "      slot %08x %08x %08x %08x\r\n"
		 "      ep0  %08x %08x %016lx %08x\r\n",
		input_ctx->ctx_input.dwInCtx0, input_ctx->ctx_input.dwInCtx1,
		islot_ctx->dwSctx0, islot_ctx->dwSctx1,
		islot_ctx->dwSctx2, islot_ctx->dwSctx3,
		ep0_ctx->dwEpCtx0, ep0_ctx->dwEpCtx1, ep0_ctx->qwEpCtx2,
		ep0_ctx->dwEpCtx4);

	/* this command expects drop-ctx=0 & add-ctx=slot+ep0 */
	if ((input_ctx->ctx_input.dwInCtx0 != 0) ||
	    (input_ctx->ctx_input.dwInCtx1 & 0x03) == 0) {
		UPRINTF(LWRN, "eval ctx, input ctl invalid\r\n");
		cmderr = XHCI_TRB_ERROR_TRB;
		goto done;
	}

	/* assign address to slot; in this emulation, slot_id = address */
	dev_ctx = pci_xhci_get_dev_ctx(xdev, slot);
	if (dev_ctx == NULL) {
		cmderr = XHCI_TRB_ERROR_CMD_ABORTED;
		goto done;
	}

	UPRINTF(LDBG, "eval ctx, dev ctx\r\n"
		 "      slot %08x %08x %08x %08x\r\n",
		dev_ctx->ctx_slot.dwSctx0, dev_ctx->ctx_slot.dwSctx1,
		dev_ctx->ctx_slot.dwSctx2, dev_ctx->ctx_slot.dwSctx3);

	if (input_ctx->ctx_input.dwInCtx1 & 0x01) {	/* slot ctx */
		/* set max exit latency */
		dev_ctx->ctx_slot.dwSctx1 =
			FIELD_COPY(dev_ctx->ctx_slot.dwSctx1,
				   input_ctx->ctx_slot.dwSctx1, 0xFFFF, 0);

		/* set interrupter target */
		dev_ctx->ctx_slot.dwSctx2 =
			FIELD_COPY(dev_ctx->ctx_slot.dwSctx2,
				   input_ctx->ctx_slot.dwSctx2, 0x3FF, 22);
	}
	if (input_ctx->ctx_input.dwInCtx1 & 0x02) {	/* control ctx */
		/* set max packet size */
		dev_ctx->ctx_ep[1].dwEpCtx1 =
			FIELD_COPY(dev_ctx->ctx_ep[1].dwEpCtx1,
				   ep0_ctx->dwEpCtx1, 0xFFFF, 16);

		ep0_ctx = &dev_ctx->ctx_ep[1];
	}

	UPRINTF(LDBG, "eval ctx, output ctx\r\n"
		 "      slot %08x %08x %08x %08x\r\n"
		 "      ep0  %08x %08x %016lx %08x\r\n",
		dev_ctx->ctx_slot.dwSctx0, dev_ctx->ctx_slot.dwSctx1,
		dev_ctx->ctx_slot.dwSctx2, dev_ctx->ctx_slot.dwSctx3,
		ep0_ctx->dwEpCtx0, ep0_ctx->dwEpCtx1, ep0_ctx->qwEpCtx2,
		ep0_ctx->dwEpCtx4);

done:
	return cmderr;
}

#define XHCI_GET_SLOT(xdev, trb, slot, cmderr)			\
	do {								\
		slot = (XHCI_TRB_3_SLOT_GET(trb->dwTrb3) >              \
			XHCI_MAX_SLOTS) ? 0 :   			\
			XHCI_TRB_3_SLOT_GET(trb->dwTrb3);		\
		if (!slot)						\
			cmderr = XHCI_TRB_ERROR_INVALID;		\
		else if (!xdev->slot_allocated[slot])			\
			cmderr = XHCI_TRB_ERROR_SLOT_NOT_ON;		\
	} while (0)							\

static int
pci_xhci_complete_commands(struct pci_xhci_vdev *xdev)
{
	struct xhci_trb	evtrb;
	struct xhci_trb	*trb;
	uint64_t	crcr;
	uint32_t	ccs;		/* cycle state (XHCI 4.9.2) */
	uint32_t	type;
	uint32_t	slot;
	uint32_t	cmderr;
	int		error;

	error = 0;
	xdev->opregs.crcr |= XHCI_CRCR_LO_CRR;

	trb = xdev->opregs.cr_p;
	ccs = xdev->opregs.crcr & XHCI_CRCR_LO_RCS;
	crcr = xdev->opregs.crcr & ~0xF;

	while (1) {
		xdev->opregs.cr_p = trb;

		type = XHCI_TRB_3_TYPE_GET(trb->dwTrb3);

		if ((trb->dwTrb3 & XHCI_TRB_3_CYCLE_BIT) !=
		    (ccs & XHCI_TRB_3_CYCLE_BIT))
			break;

		UPRINTF(LDBG, "cmd type 0x%x, Trb0 x%016lx dwTrb2 x%08x"
			" dwTrb3 x%08x, TRB_CYCLE %u/ccs %u\r\n",
			type, trb->qwTrb0, trb->dwTrb2, trb->dwTrb3,
			trb->dwTrb3 & XHCI_TRB_3_CYCLE_BIT, ccs);

		cmderr = XHCI_TRB_ERROR_SUCCESS;
		evtrb.dwTrb2 = 0;
		evtrb.dwTrb3 = (ccs & XHCI_TRB_3_CYCLE_BIT) |
		      XHCI_TRB_3_TYPE_SET(XHCI_TRB_EVENT_CMD_COMPLETE);
		slot = 0;

		switch (type) {
		case XHCI_TRB_TYPE_LINK:			/* 0x06 */
			if (trb->dwTrb3 & XHCI_TRB_3_TC_BIT)
				ccs ^= XHCI_CRCR_LO_RCS;
			break;

		case XHCI_TRB_TYPE_ENABLE_SLOT:			/* 0x09 */
			/*
			 *From xHCI spec 4.5.3.2, the only command that
			 *software is allowed to issue for the slot in
			 *disabled state is the Enable Slot Command.
			 * */
			cmderr = pci_xhci_cmd_enable_slot(xdev, &slot);
			break;

		case XHCI_TRB_TYPE_DISABLE_SLOT:		/* 0x0A */
			XHCI_GET_SLOT(xdev, trb, slot, cmderr);
			if (slot)
				cmderr = pci_xhci_cmd_disable_slot(xdev, slot);
			break;

		case XHCI_TRB_TYPE_ADDRESS_DEVICE:		/* 0x0B */
			XHCI_GET_SLOT(xdev, trb, slot, cmderr);
			if (slot)
				cmderr = pci_xhci_cmd_address_device(xdev, slot, trb);
			break;

		case XHCI_TRB_TYPE_CONFIGURE_EP:		/* 0x0C */
			XHCI_GET_SLOT(xdev, trb, slot, cmderr);
			if (slot)
				cmderr = pci_xhci_cmd_config_ep(xdev, slot, trb);
			break;

		case XHCI_TRB_TYPE_EVALUATE_CTX:		/* 0x0D */
			XHCI_GET_SLOT(xdev, trb, slot, cmderr);
			if (slot)
				cmderr = pci_xhci_cmd_eval_ctx(xdev, slot, trb);
			break;

		case XHCI_TRB_TYPE_RESET_EP:			/* 0x0E */
			UPRINTF(LDBG, "Reset Endpoint on slot %d\r\n", slot);
			XHCI_GET_SLOT(xdev, trb, slot, cmderr);
			if (slot)
				cmderr = pci_xhci_cmd_reset_ep(xdev, slot, trb);
			break;

		case XHCI_TRB_TYPE_STOP_EP:			/* 0x0F */
			UPRINTF(LDBG, "Stop Endpoint on slot %d\r\n", slot);
			XHCI_GET_SLOT(xdev, trb, slot, cmderr);
			if (slot)
				cmderr = pci_xhci_cmd_reset_ep(xdev, slot, trb);
			break;

		case XHCI_TRB_TYPE_SET_TR_DEQUEUE:		/* 0x10 */
			XHCI_GET_SLOT(xdev, trb, slot, cmderr);
			if (slot)
				cmderr = pci_xhci_cmd_set_tr(xdev, slot, trb);
			break;

		case XHCI_TRB_TYPE_RESET_DEVICE:		/* 0x11 */
			XHCI_GET_SLOT(xdev, trb, slot, cmderr);
			if (slot)
				cmderr = pci_xhci_cmd_reset_device(xdev, slot);
			break;

		case XHCI_TRB_TYPE_FORCE_EVENT:			/* 0x12 */
			/* TODO: */
			break;

		case XHCI_TRB_TYPE_NEGOTIATE_BW:		/* 0x13 */
			break;

		case XHCI_TRB_TYPE_SET_LATENCY_TOL:		/* 0x14 */
			break;

		case XHCI_TRB_TYPE_GET_PORT_BW:			/* 0x15 */
			break;

		case XHCI_TRB_TYPE_FORCE_HEADER:		/* 0x16 */
			break;

		case XHCI_TRB_TYPE_NOOP_CMD:			/* 0x17 */
			break;

		default:
			UPRINTF(LDBG, "unsupported cmd %x\r\n", type);
			break;
		}

		if (type != XHCI_TRB_TYPE_LINK) {
			/*
			 * insert command completion event and assert intr
			 */
			evtrb.qwTrb0 = crcr;
			evtrb.dwTrb2 |= XHCI_TRB_2_ERROR_SET(cmderr);
			evtrb.dwTrb3 |= XHCI_TRB_3_SLOT_SET(slot);
			UPRINTF(LDBG, "command 0x%x result: 0x%x\r\n",
				type, cmderr);
			pci_xhci_insert_event(xdev, &evtrb, 1);
		}

		trb = pci_xhci_trb_next(xdev, trb, &crcr);
	}

	xdev->opregs.crcr = crcr | (xdev->opregs.crcr & XHCI_CRCR_LO_CA) | ccs;
	xdev->opregs.crcr &= ~XHCI_CRCR_LO_CRR;
	return error;
}

static void
pci_xhci_dump_trb(struct xhci_trb *trb)
{
	static const char *const trbtypes[] = {
		"RESERVED",
		"NORMAL",
		"SETUP_STAGE",
		"DATA_STAGE",
		"STATUS_STAGE",
		"ISOCH",
		"LINK",
		"EVENT_DATA",
		"NOOP",
		"ENABLE_SLOT",
		"DISABLE_SLOT",
		"ADDRESS_DEVICE",
		"CONFIGURE_EP",
		"EVALUATE_CTX",
		"RESET_EP",
		"STOP_EP",
		"SET_TR_DEQUEUE",
		"RESET_DEVICE",
		"FORCE_EVENT",
		"NEGOTIATE_BW",
		"SET_LATENCY_TOL",
		"GET_PORT_BW",
		"FORCE_HEADER",
		"NOOP_CMD"
	};
	uint32_t type;

	type = XHCI_TRB_3_TYPE_GET(trb->dwTrb3);
	UPRINTF(LDBG, "trb[@%p] type x%02x %s 0:x%016lx 2:x%08x "
		"3:x%08x\r\n", trb, type,
		 type <= XHCI_TRB_TYPE_NOOP_CMD ? trbtypes[type] : "INVALID",
		 trb->qwTrb0, trb->dwTrb2, trb->dwTrb3);
}

static int
pci_xhci_xfer_complete(struct pci_xhci_vdev *xdev, struct usb_xfer *xfer,
		uint32_t slot, uint32_t epid, int *do_intr)
{
	struct xhci_dev_ctx	*dev_ctx;
	struct xhci_endp_ctx	*ep_ctx;
	struct xhci_trb		*trb;
	struct xhci_block	*hcb;
	struct xhci_trb		evtrb;
	uint32_t trbflags;
	uint32_t edtla;
	uint32_t i;
	int  err = XHCI_TRB_ERROR_SUCCESS;
	int rem_len = 0;

	dev_ctx = pci_xhci_get_dev_ctx(xdev, slot);

	ep_ctx = &dev_ctx->ctx_ep[epid];

	/* err is used as completion code and sent to guest driver */
	switch (xfer->status) {
	case USB_ERR_STALLED:
		ep_ctx->dwEpCtx0 = (ep_ctx->dwEpCtx0 & ~0x7) |
			XHCI_ST_EPCTX_HALTED;
		err = XHCI_TRB_ERROR_STALL;
		break;
	case USB_ERR_SHORT_XFER:
		err = XHCI_TRB_ERROR_SHORT_PKT;
		break;
	case USB_ERR_TIMEOUT:
	case USB_ERR_IOERROR:
		err = XHCI_TRB_ERROR_XACT;
		break;
	case USB_ERR_BAD_BUFSIZE:
		err = XHCI_TRB_ERROR_BABBLE;
		break;
	case USB_ERR_NORMAL_COMPLETION:
		break;
	default:
		UPRINTF(LFTL, "unknown error %d\r\n", xfer->status);
	}

	*do_intr = 0;
	edtla = 0;

	/* go through list of TRBs and insert event(s) */
	for (i = (uint32_t)xfer->head; xfer->ndata > 0; ) {
		hcb = xfer->data[i].hcb;
		evtrb.qwTrb0 = hcb->trb_addr;
		trb = XHCI_GADDR(xdev, evtrb.qwTrb0);
		trbflags = trb->dwTrb3;

		UPRINTF(LDBG, "xfer[%d] done?%u:%d trb %x %016lx %x "
			 "(err %d) IOC?%d, type %d\r\n",
			 i, xfer->data[i].stat, xfer->data[i].blen,
			 XHCI_TRB_3_TYPE_GET(trbflags), evtrb.qwTrb0, trbflags,
			 err, trb->dwTrb3 & XHCI_TRB_3_IOC_BIT ? 1 : 0,
			 xfer->data[i].type);

		if (xfer->data[i].stat < USB_BLOCK_HANDLED) {
			xfer->head = (int)i;
			break;
		}

		xfer->data[i].stat = USB_BLOCK_FREE;
		xfer->ndata--;
		xfer->head = index_inc(xfer->head, xfer->max_blk_cnt);
		edtla += xfer->data[i].bdone;

		trb->dwTrb3 = (trb->dwTrb3 & ~0x1) | (hcb->ccs);
		if (xfer->data[i].type == USB_DATA_PART) {
			rem_len += xfer->data[i].blen;
			i = index_inc(i, xfer->max_blk_cnt);

			/* This 'continue' will delay the IOC behavior which
			 * could decrease the number of virtual interrupts.
			 * This could GREATLY improve the performance especially
			 * under ISOCH scenario.
			 */
			continue;
		} else
			rem_len += xfer->data[i].blen;

		if (err == XHCI_TRB_ERROR_SUCCESS && rem_len > 0)
			err = XHCI_TRB_ERROR_SHORT_PKT;

		/* Only interrupt if IOC or short packet */
		if (!(trb->dwTrb3 & XHCI_TRB_3_IOC_BIT) &&
		    !((err == XHCI_TRB_ERROR_SHORT_PKT) &&
		      (trb->dwTrb3 & XHCI_TRB_3_ISP_BIT))) {

			i = index_inc(i, xfer->max_blk_cnt);
			continue;
		}

		evtrb.dwTrb2 = XHCI_TRB_2_ERROR_SET(err) |
			XHCI_TRB_2_REM_SET(rem_len);

		evtrb.dwTrb3 = XHCI_TRB_3_TYPE_SET(XHCI_TRB_EVENT_TRANSFER) |
			XHCI_TRB_3_SLOT_SET(slot) | XHCI_TRB_3_EP_SET(epid);

		if (XHCI_TRB_3_TYPE_GET(trbflags) == XHCI_TRB_TYPE_EVENT_DATA) {
			UPRINTF(LDBG, "EVENT_DATA edtla %u\r\n", edtla);
			evtrb.qwTrb0 = trb->qwTrb0;
			evtrb.dwTrb2 = (edtla & 0xFFFFF) |
				 XHCI_TRB_2_ERROR_SET(err);
			evtrb.dwTrb3 |= XHCI_TRB_3_ED_BIT;
			edtla = 0;
		}

		*do_intr = 1;

		err = pci_xhci_insert_event(xdev, &evtrb, 0);
		if (err != XHCI_TRB_ERROR_SUCCESS)
			break;

		i = index_inc(i, xfer->max_blk_cnt);
		rem_len = 0;
	}

	return err;
}

static void
pci_xhci_update_ep_ring(struct pci_xhci_vdev *xdev,
			struct pci_xhci_dev_emu *dev,
			struct pci_xhci_dev_ep *devep,
			struct xhci_endp_ctx *ep_ctx,
			uint32_t streamid,
			uint64_t ringaddr,
			int ccs)
{
	if (XHCI_EPCTX_0_MAXP_STREAMS_GET(ep_ctx->dwEpCtx0) != 0) {
		devep->ep_sctx[streamid].qwSctx0 = (ringaddr & ~0xFUL) |
						   (ccs & 0x1);

		devep->ep_sctx_trbs[streamid].ringaddr = ringaddr & ~0xFUL;
		devep->ep_sctx_trbs[streamid].ccs = ccs & 0x1;
		ep_ctx->qwEpCtx2 = (ep_ctx->qwEpCtx2 & ~0x1) | (ccs & 0x1);

		UPRINTF(LDBG, "update ep-ring stream %d, addr %lx\r\n",
			 streamid, devep->ep_sctx[streamid].qwSctx0);
	} else {
		devep->ep_ringaddr = ringaddr & ~0xFUL;
		devep->ep_ccs = ccs & 0x1;
		devep->ep_tr = XHCI_GADDR(xdev, ringaddr & ~0xFUL);
		ep_ctx->qwEpCtx2 = (ringaddr & ~0xFUL) | (ccs & 0x1);

		UPRINTF(LDBG, "update ep-ring, addr %lx\r\n",
			(devep->ep_ringaddr | devep->ep_ccs));
	}
}

/*
 * Outstanding transfer still in progress (device NAK'd earlier) so retry
 * the transfer again to see if it succeeds.
 */
static int
pci_xhci_try_usb_xfer(struct pci_xhci_vdev *xdev,
		      struct pci_xhci_dev_emu *dev,
		      struct pci_xhci_dev_ep *devep,
		      struct xhci_endp_ctx *ep_ctx,
		      uint32_t slot,
		      uint32_t epid)
{
	struct usb_xfer *xfer;
	int		err;
	int		do_intr;

	ep_ctx->dwEpCtx0 =
		FIELD_REPLACE(ep_ctx->dwEpCtx0, XHCI_ST_EPCTX_RUNNING, 0x7, 0);

	err = 0;
	do_intr = 0;

	xfer = devep->ep_xfer;
	pthread_mutex_lock(&devep->mtx);

	/* outstanding requests queued up */
	if (dev->dev_ue->ue_data != NULL) {
		err = dev->dev_ue->ue_data(dev->dev_instance, xfer, epid & 0x1 ?
					   USB_XFER_IN : USB_XFER_OUT, epid/2);
		if (err == USB_ERR_CANCELLED) {
			if (USB_DATA_GET_ERRCODE(&xfer->data[xfer->head]) ==
			    USB_NAK)
				err = XHCI_TRB_ERROR_SUCCESS;
		}
		/*
		 * Only for usb_mouse.c, emulation with port mapping will do it
		 * by the libusb callback function.
		 */
		else if (dev->dev_ue->ue_devtype == USB_DEV_STATIC) {
			err = pci_xhci_xfer_complete(xdev, xfer, slot, epid,
						     &do_intr);
			if (err == XHCI_TRB_ERROR_SUCCESS && do_intr)
				pci_xhci_assert_interrupt(xdev);

			pci_xhci_free_usb_xfer(devep->ep_xfer);
			devep->ep_xfer = NULL;
		}
	}

	pthread_mutex_unlock(&devep->mtx);
	return err;
}

static int
pci_xhci_handle_transfer(struct pci_xhci_vdev *xdev,
			 struct pci_xhci_dev_emu *dev,
			 struct pci_xhci_dev_ep *devep,
			 struct xhci_endp_ctx *ep_ctx,
			 struct xhci_trb *trb,
			 uint32_t slot,
			 uint32_t epid,
			 uint64_t addr,
			 uint32_t ccs,
			 uint32_t streamid)
{
	struct xhci_trb		*setup_trb;
	struct usb_xfer		*xfer;
	struct xhci_block	hcb;
	struct usb_block	*xfer_block;
	struct usb_block	*prev_block;
	struct itimerspec	delay;
	uint64_t		val;
	uint32_t		trbflags;
	int			do_intr, err;
	int			do_retry;

	ep_ctx->dwEpCtx0 = FIELD_REPLACE(ep_ctx->dwEpCtx0,
					 XHCI_ST_EPCTX_RUNNING, 0x7, 0);

	xfer = devep->ep_xfer;
	pthread_mutex_lock(&devep->mtx);

	UPRINTF(LDBG, "handle_transfer slot %u\r\n", slot);

retry:
	err = 0;
	do_retry = 0;
	do_intr = 0;
	setup_trb = NULL;
	prev_block = NULL;

	while (1) {
		pci_xhci_dump_trb(trb);

		trbflags = trb->dwTrb3;

		if (XHCI_TRB_3_TYPE_GET(trbflags) != XHCI_TRB_TYPE_LINK &&
		    (trbflags & XHCI_TRB_3_CYCLE_BIT) !=
		    (ccs & XHCI_TRB_3_CYCLE_BIT)) {
			UPRINTF(LDBG, "Cycle-bit changed trbflags %x,"
					" ccs %x\r\n",
					trbflags & XHCI_TRB_3_CYCLE_BIT, ccs);
			break;
		}

		xfer_block = NULL;

		hcb.ccs = ccs;
		hcb.trb_addr = addr;
		hcb.streamid = streamid;

		switch (XHCI_TRB_3_TYPE_GET(trbflags)) {
		case XHCI_TRB_TYPE_LINK:
			if (trb->dwTrb3 & XHCI_TRB_3_TC_BIT) {
				ccs ^= 0x1;
				hcb.ccs = ccs;
			}

			xfer_block = usb_block_append(xfer, NULL, 0, &hcb,
					sizeof(hcb));
			if (!xfer_block) {
				err = XHCI_TRB_ERROR_STALL;
				goto errout;
			}
			xfer_block->stat = USB_BLOCK_FREE;
			break;

		case XHCI_TRB_TYPE_SETUP_STAGE:
			if ((trbflags & XHCI_TRB_3_IDT_BIT) == 0 ||
			    XHCI_TRB_2_BYTES_GET(trb->dwTrb2) != 8) {
				UPRINTF(LDBG, "invalid setup trb\r\n");
				err = XHCI_TRB_ERROR_TRB;
				goto errout;
			}
			setup_trb = trb;

			val = trb->qwTrb0;
			if (!xfer->ureq)
				xfer->ureq = malloc(
					sizeof(struct usb_device_request));
			if (!xfer->ureq) {
				err = XHCI_TRB_ERROR_STALL;
				goto errout;
			}
			memcpy(xfer->ureq, &val,
			       sizeof(struct usb_device_request));

			xfer_block = usb_block_append(xfer, NULL, 0, &hcb,
					sizeof(hcb));
			if (!xfer_block) {
				free(xfer->ureq);
				xfer->ureq = NULL;
				err = XHCI_TRB_ERROR_STALL;
				goto errout;
			}
			xfer_block->stat = USB_BLOCK_HANDLED;
			break;

		case XHCI_TRB_TYPE_ISOCH:
			/* According to xHCI spec 4.10.3.1 and 4.14.2.1, the
			 * condition for judging {under,over}run event is
			 * 'empty ring'. But it didn't define how long the
			 * 'empty' state takes to identify this scenario. As
			 * an experience value, 100 ms (100 ESIT) is used to
			 * decide whether the {under,over}run event should be
			 * reported to the Guest OS.
			 */
			delay.it_interval.tv_sec = 0;
			delay.it_interval.tv_nsec = 0;
			delay.it_value.tv_sec = 0;
			delay.it_value.tv_nsec = 100000000;
			if (acrn_timer_settime(&devep->isoc_timer, &delay)) {
				UPRINTF(LFTL, "isoc timer set time failed\n");
				goto errout;
			}
			/* fall through */

		case XHCI_TRB_TYPE_NORMAL:
			if (setup_trb != NULL) {
				UPRINTF(LWRN, "trb not supposed to be in "
					 "ctl scope\r\n");
				err = XHCI_TRB_ERROR_TRB;
				goto errout;
			}
			/* fall through */

		case XHCI_TRB_TYPE_DATA_STAGE:
			xfer_block = usb_block_append(xfer,
					(void *)(trbflags & XHCI_TRB_3_IDT_BIT ?
					&trb->qwTrb0 :
					XHCI_GADDR(xdev, trb->qwTrb0)),
					trb->dwTrb2 & 0x1FFFF, &hcb,
					sizeof(hcb));

			if (!xfer_block) {
				err = XHCI_TRB_ERROR_STALL;
				goto errout;
			}

			if (trb->dwTrb3 & XHCI_TRB_3_CHAIN_BIT)
				xfer_block->type = USB_DATA_PART;
			else
				xfer_block->type = USB_DATA_FULL;

			prev_block = xfer_block;
			break;

		case XHCI_TRB_TYPE_STATUS_STAGE:
			xfer_block = usb_block_append(xfer, NULL, 0, &hcb,
					sizeof(hcb));
			break;

		case XHCI_TRB_TYPE_NOOP:
			xfer_block = usb_block_append(xfer, NULL, 0, &hcb,
					sizeof(hcb));
			if (!xfer_block) {
				err = XHCI_TRB_ERROR_STALL;
				goto errout;
			}
			xfer_block->stat = USB_BLOCK_HANDLED;
			break;

		case XHCI_TRB_TYPE_EVENT_DATA:
			xfer_block = usb_block_append(xfer, NULL, 0, &hcb,
					sizeof(hcb));
			if (!xfer_block) {
				err = XHCI_TRB_ERROR_TRB;
				goto errout;
			}
			if ((epid > 1) && (trbflags & XHCI_TRB_3_IOC_BIT))
				xfer_block->stat = USB_BLOCK_HANDLED;

			if (prev_block && prev_block->type == USB_DATA_PART) {
				prev_block->type = USB_DATA_FULL;
				prev_block = NULL;
			}

			break;

		default:
			UPRINTF(LWRN, "handle xfer unexpected trb type "
				 "0x%x\r\n",
				 XHCI_TRB_3_TYPE_GET(trbflags));
			err = XHCI_TRB_ERROR_TRB;
			goto errout;
		}

		trb = pci_xhci_trb_next(xdev, trb, &addr);

		UPRINTF(LDBG, "next trb: 0x%lx\r\n", (uint64_t)trb);

		if (xfer_block) {
			/* FIXME:
			 * should add some code to process the scenario in
			 * which endpoint stop command is comming in the
			 * middle of many data transfers.
			 */
			pci_xhci_update_ep_ring(xdev, dev, devep, ep_ctx,
					streamid, addr, ccs);
		}

		if (trbflags & XHCI_TRB_3_BEI_BIT)
			continue;

		if (xdev->quirks & XHCI_QUIRK_INTEL_ISOCH_NO_BEI)
			continue;

		/* win10 needs it */
		if (XHCI_TRB_3_TYPE_GET(trbflags) == XHCI_TRB_TYPE_EVENT_DATA)
			continue;

		/* handle current batch that requires interrupt on complete */
		if (trbflags & XHCI_TRB_3_IOC_BIT) {
			UPRINTF(LDBG, "trb IOC bit set\r\n");
			do_retry = 1;
			break;
		}
	}

	UPRINTF(LDBG, "[%d]: xfer->ndata %u\r\n", __LINE__, xfer->ndata);

	if (xfer->ndata <= 0)
		goto errout;

	if (epid == 1) {
		err = USB_ERR_NOT_STARTED;
		if (dev->dev_ue->ue_request != NULL)
			err = dev->dev_ue->ue_request(dev->dev_instance, xfer);
		setup_trb = NULL;
	} else {
		/* handle data transfer */
		pci_xhci_try_usb_xfer(xdev, dev, devep, ep_ctx, slot, epid);
		err = XHCI_TRB_ERROR_SUCCESS;
		goto errout;
	}

	err = pci_xhci_xfer_complete(xdev, xfer, slot, epid, &do_intr);
	if (err != XHCI_TRB_ERROR_SUCCESS)
		do_retry = 0;

errout:
	if (err == XHCI_TRB_ERROR_EV_RING_FULL)
		UPRINTF(LDBG, "[%d]: event ring full\r\n", __LINE__);

	if (!do_retry)
		pthread_mutex_unlock(&devep->mtx);

	if (do_intr)
		pci_xhci_assert_interrupt(xdev);

	if (do_retry) {
		UPRINTF(LDBG, "[%d]: retry next TRBs\r\n", __LINE__);
		goto retry;
	}
	return err;
}

static void
pci_xhci_device_doorbell(struct pci_xhci_vdev *xdev,
			 uint32_t slot,
			 uint32_t epid,
			 uint32_t streamid)
{
	struct pci_xhci_dev_emu *dev;
	struct pci_xhci_dev_ep	*devep;
	struct xhci_dev_ctx	*dev_ctx;
	struct xhci_endp_ctx	*ep_ctx;
	struct pci_xhci_trb_ring *sctx_tr;
	struct xhci_trb	*trb;
	uint64_t	ringaddr;
	uint32_t	ccs;

	UPRINTF(LDBG, "doorbell slot %u epid %u stream %u\r\n",
		slot, epid, streamid);

	if (slot <= 0 || slot > XHCI_MAX_SLOTS || !xdev->slot_allocated[slot]) {
		UPRINTF(LWRN, "invalid doorbell slot %u\r\n", slot);
		return;
	}

	dev = XHCI_SLOTDEV_PTR(xdev, slot);
	if (!dev)
		return;

	devep = &dev->eps[epid];
	dev_ctx = pci_xhci_get_dev_ctx(xdev, slot);
	if (!dev_ctx)
		return;
	ep_ctx = &dev_ctx->ctx_ep[epid];

	sctx_tr = NULL;

	UPRINTF(LDBG, "device doorbell ep[%u] %08x %08x %016lx %08x\r\n",
		epid, ep_ctx->dwEpCtx0, ep_ctx->dwEpCtx1, ep_ctx->qwEpCtx2,
		ep_ctx->dwEpCtx4);

	if (ep_ctx->qwEpCtx2 == 0)
		return;

	/*
	 * In USB emulation with port mapping, the following transfer should
	 * NOT be called, or else the interrupt transfer will result
	 * of invalid and infinite loop. It is used by usb_mouse.c only.
	 */
	/* handle pending transfers */
	if (dev->dev_ue && dev->dev_ue->ue_devtype == USB_DEV_STATIC &&
			devep->ep_xfer->ndata > 0) {
		pci_xhci_try_usb_xfer(xdev, dev, devep, ep_ctx, slot, epid);
		return;
	}

	/* get next trb work item */
	if (XHCI_EPCTX_0_MAXP_STREAMS_GET(ep_ctx->dwEpCtx0) != 0) {
		sctx_tr = &devep->ep_sctx_trbs[streamid];
		ringaddr = sctx_tr->ringaddr;
		ccs = sctx_tr->ccs;
		trb = XHCI_GADDR(xdev, sctx_tr->ringaddr & ~0xFUL);
		UPRINTF(LDBG, "doorbell, stream %u, ccs %lx, trb ccs %x\r\n",
			streamid, ep_ctx->qwEpCtx2 & XHCI_TRB_3_CYCLE_BIT,
			trb->dwTrb3 & XHCI_TRB_3_CYCLE_BIT);
	} else {
		ringaddr = devep->ep_ringaddr;
		ccs = devep->ep_ccs;
		trb = devep->ep_tr;
		UPRINTF(LDBG, "doorbell, ccs %lx, trb ccs %x\r\n",
			ep_ctx->qwEpCtx2 & XHCI_TRB_3_CYCLE_BIT,
			trb->dwTrb3 & XHCI_TRB_3_CYCLE_BIT);
	}

	if (XHCI_TRB_3_TYPE_GET(trb->dwTrb3) == 0) {
		UPRINTF(LDBG, "ring %lx trb[%lx] EP %u is RESERVED?\r\n",
			ep_ctx->qwEpCtx2, devep->ep_ringaddr, epid);
		return;
	}

	pci_xhci_handle_transfer(xdev, dev, devep, ep_ctx, trb, slot, epid,
				 ringaddr, ccs, streamid);
}

static void
pci_xhci_dbregs_write(struct pci_xhci_vdev *xdev,
		      uint64_t offset,
		      uint64_t value)
{

	offset = (offset - xdev->dboff) / sizeof(uint32_t);

	UPRINTF(LDBG, "doorbell write offset 0x%lx: 0x%lx\r\n",
		offset, value);

	if (XHCI_HALTED(xdev)) {
		UPRINTF(LWRN, "pci_xhci: controller halted\r\n");
		return;
	}

	if (offset == 0)
		pci_xhci_complete_commands(xdev);
	else if (xdev->portregs != NULL)
		pci_xhci_device_doorbell(xdev, offset,
					 XHCI_DB_TARGET_GET(value),
					 XHCI_DB_SID_GET(value));
}

static void
pci_xhci_rtsregs_write(struct pci_xhci_vdev *xdev,
		       uint64_t offset,
		       uint64_t value)
{
	struct pci_xhci_rtsregs *rts;

	offset -= xdev->rtsoff;

	if (offset == 0) {
		UPRINTF(LWRN, "attempted write to MFINDEX\r\n");
		return;
	}

	UPRINTF(LDBG, "runtime regs write offset 0x%lx: 0x%lx\r\n",
		offset, value);

	offset -= 0x20;		/* start of intrreg */

	rts = &xdev->rtsregs;

	switch (offset) {
	case 0x00:
		if (value & XHCI_IMAN_INTR_PEND)
			rts->intrreg.iman &= ~XHCI_IMAN_INTR_PEND;
		rts->intrreg.iman = (value & XHCI_IMAN_INTR_ENA) |
			(rts->intrreg.iman & XHCI_IMAN_INTR_PEND);

		if (!(value & XHCI_IMAN_INTR_ENA))
			pci_xhci_deassert_interrupt(xdev);

		break;

	case 0x04:
		rts->intrreg.imod = value;
		break;

	case 0x08:
		rts->intrreg.erstsz = value & 0xFFFF;
		break;

	case 0x10:
		/* ERSTBA low bits */
		rts->intrreg.erstba = MASK_64_HI(xdev->rtsregs.intrreg.erstba) |
				      (value & ~0x3F);
		break;

	case 0x14:
		/* ERSTBA high bits */
		rts->intrreg.erstba = (value << 32) |
			MASK_64_LO(xdev->rtsregs.intrreg.erstba);

		rts->erstba_p = XHCI_GADDR(xdev, xdev->rtsregs.intrreg.erstba
				& ~0x3FUL);
		UPRINTF(LDBG, "wr erstba erst (%p) ptr 0x%lx, sz %u\r\n",
				rts->erstba_p,
				rts->erstba_p->qwRingSegBase,
				rts->erstba_p->dwRingSegSize);
		break;

	case 0x18:
		/* ERDP low bits */
		rts->intrreg.erdp =
			MASK_64_HI(xdev->rtsregs.intrreg.erdp) |
			(rts->intrreg.erdp & XHCI_ERDP_LO_BUSY) |
			(value & ~0xF);
		if (value & XHCI_ERDP_LO_BUSY) {
			rts->intrreg.erdp &= ~XHCI_ERDP_LO_BUSY;
			rts->intrreg.iman &= ~XHCI_IMAN_INTR_PEND;
		}

		rts->er_deq_seg = XHCI_ERDP_LO_SINDEX(value);
		break;

	case 0x1C:
		/* ERDP high bits */
		rts->intrreg.erdp = (value << 32) |
			MASK_64_LO(xdev->rtsregs.intrreg.erdp);
		break;

	default:
		UPRINTF(LWRN, "attempted write to RTS offset 0x%lx\r\n",
			offset);
		break;
	}
}

static uint64_t
pci_xhci_portregs_read(struct pci_xhci_vdev *xdev, uint64_t offset)
{
	int port;
	uint32_t *p;

	if (xdev->portregs == NULL)
		return 0;

	port = (offset - 0x3F0) / 0x10;

	if (port > XHCI_MAX_DEVS) {
		UPRINTF(LWRN, "portregs_read port %d >= XHCI_MAX_DEVS\r\n",
			 port);

		/* return default value for unused port */
		return XHCI_PS_SPEED_SET(3);
	}

	offset = (offset - 0x3F0) % 0x10;

	p = &xdev->portregs[port].portsc;
	p += offset / sizeof(uint32_t);

	UPRINTF(LDBG, "portregs read offset 0x%lx port %u -> 0x%x\r\n",
		offset, port, *p);

	return *p;
}

static void
pci_xhci_hostop_write(struct pci_xhci_vdev *xdev,
		      uint64_t offset,
		      uint64_t value)
{
	offset -= XHCI_CAPLEN;

	if (offset < 0x400)
		UPRINTF(LDBG, "hostop write offset 0x%lx: 0x%lx\r\n",
			 offset, value);

	switch (offset) {
	case XHCI_USBCMD:
		xdev->opregs.usbcmd =
			pci_xhci_usbcmd_write(xdev, value & 0x3F0F);
		break;

	case XHCI_USBSTS:
		/* clear bits on write */
		xdev->opregs.usbsts &= ~(value &
		      (XHCI_STS_HSE|XHCI_STS_EINT|XHCI_STS_PCD|XHCI_STS_SSS|
		       XHCI_STS_RSS|XHCI_STS_SRE|XHCI_STS_CNR));
		break;

	case XHCI_PAGESIZE:
		/* read only */
		break;

	case XHCI_DNCTRL:
		xdev->opregs.dnctrl = value & 0xFFFF;
		break;

	case XHCI_CRCR_LO:
		if (xdev->opregs.crcr & XHCI_CRCR_LO_CRR) {
			xdev->opregs.crcr &= ~(XHCI_CRCR_LO_CS|XHCI_CRCR_LO_CA);
			xdev->opregs.crcr |= value &
					   (XHCI_CRCR_LO_CS|XHCI_CRCR_LO_CA);
		} else {
			xdev->opregs.crcr = MASK_64_HI(xdev->opregs.crcr) |
				   (value & (0xFFFFFFC0 | XHCI_CRCR_LO_RCS));
		}
		break;

	case XHCI_CRCR_HI:
		if (!(xdev->opregs.crcr & XHCI_CRCR_LO_CRR)) {
			xdev->opregs.crcr = MASK_64_LO(xdev->opregs.crcr) |
					  (value << 32);

			xdev->opregs.cr_p = XHCI_GADDR(xdev,
					  xdev->opregs.crcr & ~0xF);
		}

		/* if (xdev->opregs.crcr & XHCI_CRCR_LO_CS) */
		/* TODO: Stop operation of Command Ring */

		/* if (xdev->opregs.crcr & XHCI_CRCR_LO_CA) */
		/* TODO: Abort command */

		break;

	case XHCI_DCBAAP_LO:
		xdev->opregs.dcbaap = MASK_64_HI(xdev->opregs.dcbaap) |
				    (value & 0xFFFFFFC0);
		break;

	case XHCI_DCBAAP_HI:
		xdev->opregs.dcbaap =  MASK_64_LO(xdev->opregs.dcbaap) |
				     (value << 32);
		xdev->opregs.dcbaa_p = XHCI_GADDR(xdev, xdev->opregs.dcbaap
				& ~0x3FUL);

		UPRINTF(LDBG, "opregs dcbaap = 0x%lx (vaddr 0x%lx)\r\n",
		    xdev->opregs.dcbaap, (uint64_t)xdev->opregs.dcbaa_p);
		break;

	case XHCI_CONFIG:
		xdev->opregs.config = value & 0x03FF;
		break;

	default:
		if (offset >= 0x400)
			pci_xhci_portregs_write(xdev, offset, value);

		break;
	}
}

static void
pci_xhci_write(struct vmctx *ctx,
	       int vcpu,
	       struct pci_vdev *dev,
	       int baridx,
	       uint64_t offset,
	       int size,
	       uint64_t value)
{
	struct pci_xhci_vdev *xdev;

	xdev = dev->arg;

	pthread_mutex_lock(&xdev->mtx);
	if (offset < XHCI_CAPLEN)	/* read only registers */
		UPRINTF(LWRN, "write RO-CAPs offset %ld\r\n", offset);
	else if (offset < xdev->dboff)
		pci_xhci_hostop_write(xdev, offset, value);
	else if (offset < xdev->rtsoff)
		pci_xhci_dbregs_write(xdev, offset, value);
	else if (offset < xdev->rtsend)
		pci_xhci_rtsregs_write(xdev, offset, value);
	else if (offset < xdev->regsend)
		pci_xhci_excap_write(xdev, offset, value);
	else
		UPRINTF(LWRN, "write invalid offset %ld\r\n", offset);

	pthread_mutex_unlock(&xdev->mtx);
}

static uint64_t
pci_xhci_hostcap_read(struct pci_xhci_vdev *xdev, uint64_t offset)
{
	uint64_t	value;

	switch (offset) {
	case XHCI_CAPLENGTH:	/* 0x00 */
		value = xdev->caplength;
		break;

	case XHCI_HCSPARAMS1:	/* 0x04 */
		value = xdev->hcsparams1;
		break;

	case XHCI_HCSPARAMS2:	/* 0x08 */
		value = xdev->hcsparams2;
		break;

	case XHCI_HCSPARAMS3:	/* 0x0C */
		value = xdev->hcsparams3;
		break;

	case XHCI_HCSPARAMS0:	/* 0x10 */
		value = xdev->hccparams1;
		break;

	case XHCI_DBOFF:	/* 0x14 */
		value = xdev->dboff;
		break;

	case XHCI_RTSOFF:	/* 0x18 */
		value = xdev->rtsoff;
		break;

	case ACRN_XHCI_EXCAP1:
		value = 0x02000402; /* USB 2.0 */
		break;

	case ACRN_XHCI_EXCAP1 + 4:
		value = 0x20425355; /* "USB " */
		break;

	case ACRN_XHCI_EXCAP1 + 8:
		value = (((XHCI_MAX_DEVS/2) << 8) | (XHCI_MAX_DEVS/2+1));
		break;

	case ACRN_XHCI_EXCAP1 + 12:
		value = 0;
		break;

	case ACRN_XHCI_EXCAP2:
		value = 0x03000002; /* USB 3.0 */
		break;

	case ACRN_XHCI_EXCAP2 + 4:
		value = 0x20425355; /* "USB " */
		break;

	case ACRN_XHCI_EXCAP2 + 8:
		value = (((XHCI_MAX_DEVS/2) << 8) | 1);
		break;

	case ACRN_XHCI_EXCAP2 + 12:
		value = 0;
		break;

	default:
		value = 0;
		break;
	}

	UPRINTF(LDBG, "hostcap read offset 0x%lx -> 0x%lx\r\n",
		offset, value);

	return value;
}

static uint64_t
pci_xhci_hostop_read(struct pci_xhci_vdev *xdev, uint64_t offset)
{
	uint64_t value;

	offset = (offset - XHCI_CAPLEN);

	switch (offset) {
	case XHCI_USBCMD:	/* 0x00 */
		value = xdev->opregs.usbcmd;
		break;

	case XHCI_USBSTS:	/* 0x04 */
		value = xdev->opregs.usbsts;
		break;

	case XHCI_PAGESIZE:	/* 0x08 */
		value = xdev->opregs.pgsz;
		break;

	case XHCI_DNCTRL:	/* 0x14 */
		value = xdev->opregs.dnctrl;
		break;

	case XHCI_CRCR_LO:	/* 0x18 */
		value = xdev->opregs.crcr & XHCI_CRCR_LO_CRR;
		break;

	case XHCI_CRCR_HI:	/* 0x1C */
		value = 0;
		break;

	case XHCI_DCBAAP_LO:	/* 0x30 */
		value = xdev->opregs.dcbaap & 0xFFFFFFFF;
		break;

	case XHCI_DCBAAP_HI:	/* 0x34 */
		value = (xdev->opregs.dcbaap >> 32) & 0xFFFFFFFF;
		break;

	case XHCI_CONFIG:	/* 0x38 */
		value = xdev->opregs.config;
		break;

	default:
		if (offset >= 0x400)
			value = pci_xhci_portregs_read(xdev, offset);
		else
			value = 0;

		break;
	}

	if (offset < 0x400)
		UPRINTF(LDBG, "hostop read offset 0x%lx -> 0x%lx\r\n",
			offset, value);

	return value;
}

static uint64_t
pci_xhci_dbregs_read(struct pci_xhci_vdev *xdev, uint64_t offset)
{
	/* read doorbell always returns 0 */
	return 0;
}

static uint64_t
pci_xhci_rtsregs_read(struct pci_xhci_vdev *xdev, uint64_t offset)
{
	uint32_t value;
	struct timespec t;
	uint64_t time_diff;

	offset -= xdev->rtsoff;
	value = 0;

	if (offset == XHCI_MFINDEX) {
		clock_gettime(CLOCK_MONOTONIC, &t);
		/*
		 * let delta seconds be ds, delta nanoseconds be dns,
		 * the following calculation is derived from:
		 *
		 *     ds * 1000000 / 125 + dns / 1000 / 125
		 */
		time_diff = (t.tv_sec - xdev->init_time.tv_sec) * 8000
			+ (t.tv_nsec - xdev->init_time.tv_nsec) / 125000;

		value = XHCI_MFINDEX_GET(time_diff);
	} else if (offset >= XHCI_RT_IR_BASE) {
		int item;
		uint32_t *p;

		offset -= XHCI_RT_IR_BASE;
		item = offset % 32;

		p = &xdev->rtsregs.intrreg.iman;
		p += item / sizeof(uint32_t);
		value = *p;
	}

	UPRINTF(LDBG, "rtsregs read offset 0x%lx -> 0x%x\r\n",
		offset, value);

	return value;
}

static uint64_t
pci_xhci_excap_read(struct pci_xhci_vdev *xdev, uint64_t offset)
{
	uint32_t value = 0;
	uint32_t off = offset;
	struct pci_xhci_excap *excap;

	excap = xdev->excap_ptr;

	while (excap && excap->start != EXCAP_GROUP_END) {
		if (off >= excap->start && off < excap->end)
			break;
		excap++;
	}

	if (!excap || excap->start == EXCAP_GROUP_END) {
		UPRINTF(LWRN, "extended capability 0x%lx can't be found\r\n",
				offset);
		return value;
	}

	if (excap->start != EXCAP_GROUP_END) {
		off -= excap->start;
		memcpy(&value, (uint32_t *)excap->data + off / 4,
				sizeof(uint32_t));
	}

	return value;
}

static uint64_t
pci_xhci_read(struct vmctx *ctx,
	      int vcpu,
	      struct pci_vdev *dev,
	      int baridx,
	      uint64_t offset,
	      int size)
{
	struct pci_xhci_vdev *xdev;
	uint32_t	value;

	xdev = dev->arg;

	pthread_mutex_lock(&xdev->mtx);
	if (offset < XHCI_CAPLEN)
		value = pci_xhci_hostcap_read(xdev, offset);
	else if (offset < xdev->dboff)
		value = pci_xhci_hostop_read(xdev, offset);
	else if (offset < xdev->rtsoff)
		value = pci_xhci_dbregs_read(xdev, offset);
	else if (offset < xdev->rtsend)
		value = pci_xhci_rtsregs_read(xdev, offset);
	else if (offset < xdev->regsend)
		value = pci_xhci_excap_read(xdev, offset);
	else {
		value = 0;
		UPRINTF(LDBG, "read invalid offset %ld\r\n", offset);
	}

	pthread_mutex_unlock(&xdev->mtx);

	switch (size) {
	case 1:
		value &= 0xFF;
		break;
	case 2:
		value &= 0xFFFF;
		break;
	case 4:
		value &= 0xFFFFFFFF;
		break;
	}

	return value;
}

static void
pci_xhci_reset_port(struct pci_xhci_vdev *xdev, int portn, int warm)
{
	struct pci_xhci_portregs *port;
	struct xhci_trb evtrb;
	struct usb_native_devinfo *di;
	int speed, error;
	int index;

	UPRINTF(LINF, "reset port %d\r\n", portn);

	port = XHCI_PORTREG_PTR(xdev, portn);
	index = pci_xhci_get_native_port_index_by_vport(xdev, portn);
	if (index < 0) {
		UPRINTF(LWRN, "fail to reset port %d\r\n", portn);
		return;
	}
	di = &xdev->native_ports[index].info;

	speed = pci_xhci_convert_speed(di->speed);
	port->portsc &= ~(XHCI_PS_PLS_MASK | XHCI_PS_PR | XHCI_PS_PRC);
	port->portsc |= XHCI_PS_PED | XHCI_PS_SPEED_SET(speed);

	if (warm && di->bcd >= 0x300)
		port->portsc |= XHCI_PS_WRC;

	if ((port->portsc & XHCI_PS_PRC) == 0) {
		port->portsc |= XHCI_PS_PRC;

		pci_xhci_set_evtrb(&evtrb, portn,
		     XHCI_TRB_ERROR_SUCCESS,
		     XHCI_TRB_EVENT_PORT_STS_CHANGE);
		error = pci_xhci_insert_event(xdev, &evtrb, 1);
		if (error != XHCI_TRB_ERROR_SUCCESS)
			UPRINTF(LWRN, "reset port insert event "
				"failed\n");
	}
}

static void
pci_xhci_init_port(struct pci_xhci_vdev *xdev, int portn)
{
	XHCI_PORTREG_PTR(xdev, portn)->portsc =
		XHCI_PS_PLS_SET(UPS_PORT_LS_RX_DET) | XHCI_PS_PP;
}

static int
pci_xhci_dev_intr(struct usb_hci *hci, int epctx)
{
	struct pci_xhci_dev_emu *dev;
	struct xhci_dev_ctx	*dev_ctx;
	struct xhci_trb		evtrb;
	struct pci_xhci_vdev	*xdev;
	struct pci_xhci_portregs *p;
	struct xhci_endp_ctx	*ep_ctx;
	int	error = 0;
	int	dir_in;
	int	epid;

	dir_in = epctx & 0x80;
	epid = epctx & ~0x80;

	/* HW endpoint contexts are 0-15; convert to epid based on dir */
	epid = (epid * 2) + (dir_in ? 1 : 0);
	if (epid >= XHCI_MAX_ENDPOINTS)
		return 0;

	dev = hci->dev;
	xdev = dev->xdev;

	/* check if device is ready; OS has to initialise it */
	if (xdev->rtsregs.erstba_p == NULL ||
	    (xdev->opregs.usbcmd & XHCI_CMD_RS) == 0 ||
	    dev->dev_ctx == NULL)
		return 0;

	p = XHCI_PORTREG_PTR(xdev, hci->hci_port);

	/* raise event if link U3 (suspended) state */
	if (XHCI_PS_PLS_GET(p->portsc) == 3) {
		p->portsc &= ~XHCI_PS_PLS_MASK;
		p->portsc |= XHCI_PS_PLS_SET(UPS_PORT_LS_RESUME);
		if ((p->portsc & XHCI_PS_PLC) != 0)
			return 0;

		p->portsc |= XHCI_PS_PLC;

		pci_xhci_set_evtrb(&evtrb, hci->hci_port,
				   XHCI_TRB_ERROR_SUCCESS,
				   XHCI_TRB_EVENT_PORT_STS_CHANGE);
		error = pci_xhci_insert_event(xdev, &evtrb, 0);
		if (error != XHCI_TRB_ERROR_SUCCESS)
			goto done;
	}

	dev_ctx = dev->dev_ctx;
	ep_ctx = &dev_ctx->ctx_ep[epid];
	if ((ep_ctx->dwEpCtx0 & 0x7) == XHCI_ST_EPCTX_DISABLED) {
		UPRINTF(LWRN, "device interrupt on disabled endpoint %d\r\n",
			 epid);
		return 0;
	}

	UPRINTF(LDBG, "device interrupt on endpoint %d\r\n", epid);

	pci_xhci_device_doorbell(xdev, hci->hci_port, epid, 0);

done:
	return error;
}

static int
pci_xhci_dev_event(struct usb_hci *hci, enum hci_usbev evid, void *param)
{
	UPRINTF(LDBG, "xhci device event port %d\r\n", hci->hci_port);
	return 0;
}

static void
pci_xhci_device_usage(char *opt)
{
	static const char *usage_str = "usage:\r\n"
		" -s <n>,xhci,[bus1-port1,bus2-port2]:[tablet]:[log=x]:[cap=x]\r\n"
		" eg: -s 8,xhci,1-2,2-2\r\n"
		" eg: -s 7,xhci,tablet:log=D\r\n"
		" eg: -s 7,xhci,1-2,2-2:tablet\r\n"
		" eg: -s 7,xhci,1-2,2-2:tablet:log=D:cap=apl\r\n"
		" Note: please follow the board hardware design, assign the "
		" ports according to the receptacle connection\r\n";

	UPRINTF(LFTL, "error: invalid options: \"%s\"\r\n", opt);
	UPRINTF(LFTL, "%s", usage_str);
}

static int
pci_xhci_parse_log_level(struct pci_xhci_vdev *xdev, char *opts)
{
	char level;
	char *s, *o;
	int rc = 0;

	o = s = strdup(opts);
	if (!(s && s[0] == 'l' && s[1] == 'o' && s[2] == 'g')) {
		rc = -1;
		goto errout;
	}

	s = strchr(opts, '=');
	if (!s) {
		rc = -2;
		goto errout;
	}

	level = *(s+1);
	usb_parse_log_level(level);

errout:
	if (rc)
		pr_err("USB: fail to set log level, rc=%d\r\n", rc);
	free(o);
	return rc;
}

static int
pci_xhci_parse_bus_port(struct pci_xhci_vdev *xdev, char *opts)
{
	int rc = 0;
	char *tstr;
	int port, bus, index;
	struct usb_devpath path;
	struct usb_native_devinfo di;

	tstr = opts;
	/* 'bus-port' format */
	if (!tstr || dm_strtoi(tstr, &tstr, 10, &bus) || *tstr != '-' ||
			dm_strtoi(tstr + 1, &tstr, 10, &port)) {
		rc = -1;
		goto errout;
	}

	if (bus >= USB_NATIVE_NUM_BUS || port >= USB_NATIVE_NUM_PORT) {
		rc = -1;
		goto errout;
	}

	if (!usb_native_is_bus_existed(bus) ||
			!usb_native_is_port_existed(bus, port)) {
		rc = -2;
		goto errout;
	}

	memset(&path, 0, sizeof(path));
	path.bus = bus;
	path.depth = 1;
	path.path[0] = port;
	di.path = path;
	index = pci_xhci_set_native_port_assigned(xdev, &di);
	if (index < 0) {
		UPRINTF(LFTL, "fail to assign native_port\r\n");
		goto errout;
	}

	return 0;
errout:
	if (rc)
		UPRINTF(LWRN, "%s fails, rc=%d\r\n", __func__, rc);
	return rc;
}

static int
pci_xhci_parse_tablet(struct pci_xhci_vdev *xdev, char *opts)
{
	char *cfg, *str;
	void *devins;
	struct usb_devemu *ue;
	struct pci_xhci_dev_emu *dev = NULL;
	uint8_t port_u2, port_u3;
	int rc = 0;

	if (strncmp(opts, "tablet", sizeof("tablet") - 1)) {
		rc = -1;
		goto errout;
	}

	str = opts;
	cfg = strchr(str, '=');
	cfg = cfg ? cfg + 1 : "";

	ue = usb_emu_finddev(opts);
	if (ue == NULL) {
		rc = -2;
		goto errout;
	}

	dev = calloc(1, sizeof(struct pci_xhci_dev_emu));
	if (!dev) {
		rc = -3;
		goto errout;
	}

	dev->xdev = xdev;
	dev->hci.dev = dev;
	dev->hci.hci_intr = pci_xhci_dev_intr;
	dev->hci.hci_event = pci_xhci_dev_event;

	/*
	 * This is a safe operation because there is no other
	 * device created and port_u2/port_u3 definitely points
	 * to an empty position in xdev->devices
	 */
	port_u2 = xdev->usb3_port_start - 1;
	port_u3 = xdev->usb2_port_start - 1;
	if (ue->ue_usbver == 2) {
		dev->hci.hci_port = port_u2 + 1;
		xdev->devices[port_u2] = dev;
	} else {
		dev->hci.hci_port = port_u3 + 1;
		xdev->devices[port_u3] = dev;
	}

	dev->hci.hci_address = 0;
	devins = ue->ue_init(&dev->hci, cfg);
	if (devins == NULL) {
		rc = -4;
		goto errout;
	}

	dev->dev_ue = ue;
	dev->dev_instance = devins;

	/* assign slot number to device */
	xdev->ndevices++;
	xdev->slots[xdev->ndevices] = dev;
	return 0;

errout:
	if (dev) {
		if (ue) {
			if (dev == xdev->devices[port_u2])
				xdev->devices[port_u2] = NULL;
			if (dev == xdev->devices[port_u3])
				xdev->devices[port_u3] = NULL;
		}
		free(dev);
	}
	UPRINTF(LFTL, "fail to parse tablet, rc=%d\r\n", rc);
	return rc;
}

static int
pci_xhci_parse_extcap(struct pci_xhci_vdev *xdev, char *opts)
{
	char *cap;
	char *s, *o;
	int rc = 0;

	cap = o = s = strdup(opts);

	s = strchr(opts, '=');
	if (!s) {
		rc = -1;
		goto errout;
	}

	cap = s + 1;
	if (!strncmp(cap, "apl", 3)) {
		xdev->excap_write = pci_xhci_apl_drdregs_write;
		xdev->excap_ptr = excap_group_apl;
		xdev->quirks |= XHCI_QUIRK_INTEL_ISOCH_NO_BEI;
		xdev->vid = PCI_INTEL_APL_XHCI_VID;
		xdev->pid = PCI_INTEL_APL_XHCI_PID;
	} else
		rc = -2;

	if (((struct pci_xhci_excap *)(xdev->excap_ptr))->start
			== EXCAP_GROUP_END) {
		xdev->excap_write = NULL;
		xdev->excap_ptr = NULL;
		xdev->vid = PCI_ACRN_XHCI_VID;
		xdev->pid = PCI_ACRN_XHCI_PID;
		UPRINTF(LWRN, "Invalid excap, set to ACRN excap\r\n");
	}

errout:
	if (rc)
		pr_err("USB: fail to set vendor capability, rc=%d\r\n", rc);
	free(o);
	return rc;
}

static int
pci_xhci_parse_opts(struct pci_xhci_vdev *xdev, char *opts)
{
	char *s, *t, *n, *tptr;
	int i, rc = 0;
	struct pci_xhci_option_elem *elem;
	int (*f)(struct pci_xhci_vdev *, char *);
	int elem_cnt;

	if (!opts) {
		rc = -1;
		goto errout;
	}

	/* allocate neccessary resources during parsing*/
	xdev->devices = calloc(XHCI_MAX_DEVS + 1, sizeof(*xdev->devices));
	xdev->slots = calloc(XHCI_MAX_SLOTS + 1, sizeof(*xdev->slots));
	xdev->portregs = calloc(XHCI_MAX_DEVS + 1, sizeof(*xdev->portregs));
	if (!xdev->devices || !xdev->slots || !xdev->portregs) {
		rc = -2;
		goto errout;
	}

	s = strdup(opts);
	UPRINTF(LDBG, "options: %s\r\n", s);

	elem = xhci_option_table;
	elem_cnt = sizeof(xhci_option_table) / sizeof(*elem);

	for (t = strtok_r(s, ",:", &tptr); t; t = strtok_r(NULL, ",:", &tptr)) {
		if (isdigit(t[0])) { /* bus-port */
			if (pci_xhci_parse_bus_port(xdev, t)) {
				rc = -3;
				goto errout;
			}
		} else {
			for (i = 0; i < elem_cnt; i++) {
				n = elem[i].parse_opt;
				f = elem[i].parse_fn;

				if (!n || !f)
					continue;

				if (!strncmp(t, n, strnlen(n, XHCI_OPT_MAX_LEN))) {
					f(xdev, t);
					break;
				}
			}

			if (i >= elem_cnt) {
				rc = -4;
				goto errout;
			}
		}
	}

	/* do not use the zero index element */
	for (i = 1; i <= XHCI_MAX_DEVS; i++)
		pci_xhci_init_port(xdev, i);

errout:
	if (rc) {
		if (xdev->devices) {
			for (i = 1; i <= XHCI_MAX_DEVS && xdev->devices[i]; i++)
				free(xdev->devices[i]);
			xdev->ndevices = 0;
			free(xdev->devices);
			xdev->devices = NULL;
		}
		if (xdev->slots) {
			free(xdev->slots);
			xdev->slots = NULL;
		}
		if (xdev->portregs) {
			free(xdev->portregs);
			xdev->portregs = NULL;
		}
		if (rc < -2 && s)
			free(s);
		UPRINTF(LFTL, "fail to parse xHCI options, rc=%d\r\n", rc);

		if (opts)
			pci_xhci_device_usage(opts);

		return rc;
	}

	free(s);
	return xdev->ndevices;
}

static void
pci_xhci_isoc_handler(void *arg, uint64_t param)
{
	struct xhci_ep_timer_data *pdata;
	struct xhci_trb trb;
	struct xhci_trb	trb_underrun = {
		.qwTrb0 = 0,
		.dwTrb2 = XHCI_TRB_2_ERROR_SET(XHCI_TRB_ERROR_RING_UNDERRUN),
		.dwTrb3 = XHCI_TRB_3_TYPE_SET(XHCI_TRB_EVENT_TRANSFER)
	};
	struct xhci_trb	trb_overrun = {
		.qwTrb0 = 0,
		.dwTrb2 = XHCI_TRB_2_ERROR_SET(XHCI_TRB_ERROR_RING_OVERRUN),
		.dwTrb3 = XHCI_TRB_3_TYPE_SET(XHCI_TRB_EVENT_TRANSFER)
	};

	pdata = arg;
	if (pdata == NULL) {
		UPRINTF(LFTL, "%s NULL arg\r\n", __func__);
		return;
	}

	if (pdata->dir == TOKEN_OUT) {
		trb_underrun.dwTrb3 |= (XHCI_TRB_3_SLOT_SET(pdata->slot) |
				XHCI_TRB_3_EP_SET(pdata->epnum));
		trb = trb_underrun;
	} else {
		trb_overrun.dwTrb3 |= (XHCI_TRB_3_SLOT_SET(pdata->slot) |
				XHCI_TRB_3_EP_SET(pdata->epnum));
		trb = trb_overrun;
	}

	if (!pdata->dev || !pdata->dev->xdev) {
		UPRINTF(LFTL, "%s: error, {x,}dev == NULL\r\n", __func__);
		return;
	}

	pci_xhci_insert_event(pdata->dev->xdev, &trb, 1);
	UPRINTF(LINF, "send %srun to slot %d ep %d\r\n", pdata->dir == TOKEN_IN
			? "under" : "over", pdata->slot, pdata->epnum);
}

static int
pci_xhci_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct pci_xhci_vdev *xdev;
	struct pci_xhci_excap *excap;
	int	error;

	if (xhci_in_use) {
		UPRINTF(LWRN, "controller already defined\r\n");
		return -1;
	}

	xdev = calloc(1, sizeof(struct pci_xhci_vdev));
	if (!xdev) {
		UPRINTF(LWRN, "%s:%d fail to allocate memory\n",
			__func__, __LINE__);
		return -1;
	}

	dev->arg = xdev;
	xdev->dev = dev;

	xdev->usb2_port_start = (XHCI_MAX_DEVS/2) + 1;
	xdev->usb3_port_start = 1;

	xdev->vid = PCI_ACRN_XHCI_VID;
	xdev->pid = PCI_ACRN_XHCI_PID;
	xdev->excapoff = ACRN_XHCI_EXCAP1;
	xdev->excap_ptr = NULL;

	xdev->rtsregs.mfindex = 0;
	clock_gettime(CLOCK_MONOTONIC, &xdev->init_time);

	/* discover devices */
	error = pci_xhci_parse_opts(xdev, opts);
	if (error < 0)
		goto done;
	else
		error = 0;

	if (usb_dev_sys_init(pci_xhci_native_usb_dev_conn_cb,
				pci_xhci_native_usb_dev_disconn_cb,
				pci_xhci_usb_dev_notify_cb,
				pci_xhci_usb_dev_intr_cb,
				pci_xhci_usb_dev_lock_ep_cb,
				pci_xhci_usb_dev_unlock_ep_cb,
				xdev, usb_get_log_level()) < 0) {
		error = -3;
		goto done;
	}

	xdev->caplength = XHCI_SET_CAPLEN(XHCI_CAPLEN) |
			 XHCI_SET_HCIVERSION(0x0100);
	xdev->hcsparams1 = XHCI_SET_HCSP1_MAXPORTS(XHCI_MAX_DEVS) |
			 XHCI_SET_HCSP1_MAXINTR(1) |	/* interrupters */
			 XHCI_SET_HCSP1_MAXSLOTS(XHCI_MAX_SLOTS);
	xdev->hcsparams2 = XHCI_SET_HCSP2_ERSTMAX(XHCI_ERST_MAX) |
			 XHCI_SET_HCSP2_IST(0x04);
	xdev->hcsparams3 = 0;				/* no latency */
	xdev->hccparams1 = XHCI_SET_HCCP1_NSS(1) |	/* no 2nd-streams */
			 XHCI_SET_HCCP1_SPC(1) |	/* short packet */
			 XHCI_SET_HCCP1_MAXPSA(XHCI_STREAMS_MAX);
	xdev->hccparams2 = XHCI_SET_HCCP2_LEC(1) |
			 XHCI_SET_HCCP2_U3C(1);
	xdev->dboff = XHCI_SET_DOORBELL(XHCI_CAPLEN + XHCI_PORTREGS_START +
			 XHCI_MAX_DEVS * sizeof(struct pci_xhci_portregs));

	/* dboff must be 32-bit aligned */
	if (xdev->dboff & 0x3)
		xdev->dboff = (xdev->dboff + 0x3) & ~0x3;

	/* rtsoff must be 32-bytes aligned */
	xdev->rtsoff = XHCI_SET_RTSOFFSET(xdev->dboff +
		(XHCI_MAX_SLOTS+1) * 32);
	if (xdev->rtsoff & 0x1F)
		xdev->rtsoff = (xdev->rtsoff + 0x1F) & ~0x1F;
	xdev->rtsend = xdev->rtsoff + XHCI_RT_IR_BASE + sizeof(xdev->rtsregs);

	UPRINTF(LDBG, "dboff: 0x%x, rtsoff: 0x%x\r\n", xdev->dboff,
		 xdev->rtsoff);

	xdev->opregs.usbsts = XHCI_STS_HCH;
	xdev->opregs.pgsz = XHCI_PAGESIZE_4K;

	pci_xhci_reset(xdev);

	excap = xdev->excap_ptr;
	if (excap == NULL) {
		xdev->regsend = xdev->rtsend;
		xdev->excapoff = ACRN_XHCI_EXCAP1;
	} else {
		xdev->excapoff = excap->start;
		while (excap && excap->start != EXCAP_GROUP_END) {
			xdev->regsend = excap->end;
			excap++;
		}
	}

	/*
	 * Set extended capabilities pointer to be after regsend;
	 * value of excap field is 32-bit offset.
	 */
	xdev->hccparams1 |=
		XHCI_SET_HCCP1_XECP(XHCI_XECP_OFF_SHIFT(xdev->excapoff));

	pci_set_cfgdata16(dev, PCIR_DEVICE, xdev->pid);
	pci_set_cfgdata16(dev, PCIR_VENDOR, xdev->vid);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_SERIALBUS);
	pci_set_cfgdata8(dev, PCIR_SUBCLASS, PCIS_SERIALBUS_USB);
	pci_set_cfgdata8(dev, PCIR_PROGIF, PCIP_SERIALBUS_USB_XHCI);
	pci_set_cfgdata8(dev, PCI_USBREV, PCI_USB_REV_3_0);

	pci_emul_add_msicap(dev, 1);

	/* regsend registers */
	pci_emul_alloc_bar(dev, 0, PCIBAR_MEM32, xdev->regsend);
	UPRINTF(LDBG, "pci_emu_alloc: %d\r\n", xdev->regsend);

	pci_lintr_request(dev);

	pthread_mutex_init(&xdev->mtx, NULL);

	/* create vbdp_thread */
	xdev->vbdp_polling = true;
	sem_init(&xdev->vbdp_sem, 0, 0);
	error = pthread_create(&xdev->vbdp_thread, NULL, xhci_vbdp_thread,
			xdev);
	if (error)
		goto done;

	xhci_in_use = 1;
done:
	if (error) {
		UPRINTF(LFTL, "%s fail, error=%d\n", __func__, error);
		if (xdev) {
			if (xdev->devices) {
				free(xdev->devices);
				xdev->devices = NULL;
			}
			if (xdev->slots) {
				free(xdev->slots);
				xdev->slots = NULL;
			}
			if (xdev->portregs) {
				free(xdev->portregs);
				xdev->portregs = NULL;
			}
			free(xdev);
		}
	}

	return error;
}

static void
pci_xhci_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	int i;
	struct pci_xhci_vdev *xdev;
	struct pci_xhci_dev_emu *de;

	xdev = dev->arg;

	UPRINTF(LINF, "de-initialization\r\n");

	for (i = 1; i <= XHCI_MAX_DEVS; ++i) {
		de = xdev->devices[i];
		if (de) {
			xdev->devices[i] = NULL;
			pci_xhci_dev_destroy(de);
			xdev->ndevices--;
		}
	}

	free(xdev->devices);
	free(xdev->slots);
	free(xdev->portregs);

	usb_dev_sys_deinit();

	xdev->vbdp_polling = false;
	sem_post(&xdev->vbdp_sem);
	pthread_join(xdev->vbdp_thread, NULL);
	sem_close(&xdev->vbdp_sem);

	pthread_mutex_destroy(&xdev->mtx);
	free(xdev);
	xhci_in_use = 0;
}

struct pci_vdev_ops pci_ops_xhci = {
	.class_name	= "xhci",
	.vdev_init	= pci_xhci_init,
	.vdev_deinit	= pci_xhci_deinit,
	.vdev_barwrite	= pci_xhci_write,
	.vdev_barread	= pci_xhci_read
};
DEFINE_PCI_DEVTYPE(pci_ops_xhci);
