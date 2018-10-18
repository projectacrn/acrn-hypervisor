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
 *
 * $FreeBSD$
 */

#ifndef _USB_CORE_H_
#define _USB_CORE_H_

#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include "types.h"

/* FIXME:
 * There are some huge data requests which need more than 256 TRBs in a single
 * transfer, so it is neccessary to expand it.
 * But this is not final solution, this size should be dynamically changed
 * according to the native xhci driver's adjust of trb segements.
 * By default, the native xhci driver use two segments which contain 2 * 256
 * trbs, so 1024 is enough currently.
 */
#define	USB_MAX_XFER_BLOCKS	1024

#define	USB_XFER_OUT		0
#define	USB_XFER_IN		1

#define USB_DIR_OUT              0
#define USB_DIR_IN               0x80

#define LIBUSB_TIMEOUT           10000

#define USB_CFG_ATT_ONE          (1 << 7) /* should always be set */
#define USB_CFG_ATT_SELFPOWER    (1 << 6)
#define USB_CFG_ATT_WAKEUP       (1 << 5)
#define USB_CFG_ATT_BATTERY      (1 << 4)

enum endpoint_type {
	USB_ENDPOINT_CONTROL = 0,
	USB_ENDPOINT_ISOC,
	USB_ENDPOINT_BULK,
	USB_ENDPOINT_INT,
	USB_ENDPOINT_INVALID = 255
};

#define USB_INTERFACE_INVALID 255

enum token_type {
	TOKEN_OUT = 0,
	TOKEN_IN,
	TOKEN_SETUP
};

enum usb_dev_type {
	USB_DEV_STATIC = 0,
	USB_DEV_PORT_MAPPER
};

enum usb_xfer_blk_stat {
	USB_XFER_BLK_FREE = 0,
	USB_XFER_BLK_HANDLING,
	USB_XFER_BLK_HANDLED
};

enum usb_native_devtype {
	USB_TYPE_ROOTHUB,
	USB_TYPE_EXTHUB,
	USB_TYPE_ROOTHUB_SUBDEV,
	USB_TYPE_EXTHUB_SUBDEV,
	USB_TYPE_NONE
};

#define USB_MAX_TIERS 7

struct usb_hci;
struct usb_device_request;
struct usb_data_xfer;

/* Device emulation handlers */
struct usb_devemu {
	char	*ue_emu;	/* name of device emulation */
	int	ue_usbver;	/* usb version: 2 or 3 */
	int	ue_usbspeed;	/* usb device speed */
	int	ue_devtype;

	/* instance creation */
	void	*(*ue_init)(void *pdata, char *opt);

	/* handlers */
	int	(*ue_request)(void *sc, struct usb_data_xfer *xfer);
	int	(*ue_data)(void *sc, struct usb_data_xfer *xfer, int dir,
			   int epctx);
	int	(*ue_info)(void *sc, int type, void *value, int size);
	int	(*ue_reset)(void *sc);
	int	(*ue_remove)(void *sc);
	int	(*ue_stop)(void *sc);
	void	(*ue_deinit)(void *pdata);
};
#define	USB_EMUL_SET(x)	DATA_SET(usb_emu_set, x)

/*
 * USB device events to notify HCD when state changes
 */
enum hci_usbev {
	USBDEV_ATTACH,
	USBDEV_RESET,
	USBDEV_STOP,
	USBDEV_REMOVE,
};

/* usb controller, ie xhci, ehci */
struct usb_hci {
	int	(*hci_intr)(struct usb_hci *hci, int epctx);
	int	(*hci_event)(struct usb_hci *hci, enum hci_usbev evid,
			     void *param);
	void	*dev;			/* private device for hci */

	/* controller managed fields */
	int	hci_address;
	int	hci_port;
};

/*
 * Each xfer block is mapped to the hci transfer block.
 * On input into the device handler, blen is set to the lenght of buf.
 * The device handler is to update blen to reflect on the residual size
 * of the buffer, i.e. len(buf) - len(consumed).
 */
struct usb_data_xfer_block {
	void			*buf;	   /* IN or OUT pointer */
	int			blen;	   /* in:len(buf), out:len(remaining) */
	int			bdone;	   /* bytes transferred */
	enum usb_xfer_blk_stat	processed; /* processed status */
	void			*hci_data; /* HCI private reference */
	int			ccs;
	uint32_t		streamid;
	uint64_t		trbnext;   /* next TRB guest address */
};

struct usb_data_xfer {
	struct usb_data_xfer_block data[USB_MAX_XFER_BLOCKS];
	struct usb_device_request *ureq;	/* setup ctl request */
	int	ndata;				/* # of data items */
	int	head;
	int	tail;
	void    *dev;		/* struct pci_xhci_dev_emu *dev */
	int     epid;		/* related endpoint id */
	int     pid;		/* token id */
	int	status;
	pthread_mutex_t mtx;
};

struct usb_devpath {
	uint8_t bus;
	uint8_t depth;
	uint8_t path[USB_MAX_TIERS];
};
#define ROOTHUB_PORT(x) ((x).path[0])

struct usb_native_devinfo {
	int speed;
	int maxchild;
	uint16_t bcd;
	uint16_t pid;
	uint16_t vid;
	enum usb_native_devtype type;
	struct usb_devpath path;
	void *priv_data;
};

enum USB_ERRCODE {
	USB_ACK,
	USB_NAK,
	USB_STALL,
	USB_NYET,
	USB_ERR,
	USB_SHORT
};

#define	USB_DATA_GET_ERRCODE(x)		((x)->processed >> 8)
#define	USB_DATA_SET_ERRCODE(x, e) \
((x)->processed = ((x)->processed & 0xFF) | (e << 8))

#define	USB_DATA_OK(x, i)	((x)->data[(i)].buf != NULL)

#define	USB_DATA_XFER_INIT(x)	do {					   \
		pthread_mutexattr_t attr;				   \
		pthread_mutexattr_init(&attr);				   \
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE); \
		memset((x), 0, sizeof(*(x)));				   \
		pthread_mutex_init(&((x)->mtx), &attr);			   \
	} while (0)

#define	USB_DATA_XFER_RESET(x)	do {					\
			pthread_mutex_lock(&((x)->mtx));		\
			memset((x)->data, 0, sizeof((x)->data));	\
			(x)->ndata = 0;					\
			(x)->head = (x)->tail = 0;			\
			pthread_mutex_unlock((&(x)->mtx));		\
		} while (0)

#define	USB_DATA_XFER_LOCK(x)	\
	pthread_mutex_lock(&((x)->mtx))

#define	USB_DATA_XFER_UNLOCK(x)	\
	pthread_mutex_unlock(&((x)->mtx))

#define LOG_TAG "USB: "
#define LFTL 0
#define LWRN 1
#define LINF 2
#define LDBG 3
#define LVRB 4
#define UPRINTF(lvl, fmt, args...) \
	do { if (lvl <= usb_log_level) printf(LOG_TAG fmt, ##args); } while (0)

#define NATIVE_USBSYS_DEVDIR "/sys/bus/usb/devices"
#define NATIVE_USB2_SPEED "480"
#define NATIVE_USB3_SPEED "5000"
#define USB_NATIVE_NUM_PORT 20
#define USB_NATIVE_NUM_BUS 4

extern int usb_log_level;
static inline int usb_get_log_level(void)		{ return usb_log_level; }
static inline void usb_set_log_level(int level)	{ usb_log_level = level; }
void usb_parse_log_level(char level);
struct usb_devemu *usb_emu_finddev(char *name);
int usb_native_is_bus_existed(uint8_t bus_num);
int usb_native_is_ss_port(uint8_t bus_of_port);
int usb_native_is_port_existed(uint8_t bus_num, uint8_t port_num);
struct usb_data_xfer_block *usb_data_xfer_append(struct usb_data_xfer *xfer,
						 void *buf,
						 int blen,
						 void *hci_data,
						 int ccs);
int usb_get_hub_port_num(struct usb_devpath *path);
char *usb_dev_path(struct usb_devpath *path);
#endif /* _USB_CORE_H_ */
