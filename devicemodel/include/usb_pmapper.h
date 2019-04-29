/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _USB_DEVICE_H
#define _USB_DEVICE_H
#include <libusb-1.0/libusb.h>
#include "usb_core.h"

#define USB_NUM_INTERFACE 16
#define USB_NUM_ENDPOINT  15

#define USB_EP_ADDR(d) ((d)->bEndpointAddress)
#define USB_EP_ATTR(d) ((d)->bmAttributes)
#define USB_EP_PID(d) (USB_EP_ADDR(d) & USB_DIR_IN ? TOKEN_IN : TOKEN_OUT)
#define USB_EP_TYPE(d) (USB_EP_ATTR(d) & 0x3)
#define USB_EP_NR(d) (USB_EP_ADDR(d) & 0xF)
#define USB_EP_MAXP(d) ((d)->wMaxPacketSize)
#define USB_EP_ERR_TYPE 0xFF

#define USB_EP_MAXP_SZ(m) ((m) & 0x7ff)
#define USB_EP_MAXP_MT(m) (((m) >> 11) & 0x3)

enum {
	USB_INFO_VERSION,
	USB_INFO_SPEED,
	USB_INFO_BUS,
	USB_INFO_PORT,
	USB_INFO_VID,
	USB_INFO_PID
};

struct usb_dev_ep {
	uint8_t pid;
	uint8_t type;
	uint16_t maxp;
};

struct usb_dev {
	/* physical device info */
	struct usb_native_devinfo info;
	int addr;
	int version;
	int configuration;

	/* interface info */
	int if_num;
	int alts[USB_NUM_INTERFACE];

	/* endpoints info */
	struct usb_dev_ep epc;
	struct usb_dev_ep epi[USB_NUM_ENDPOINT];
	struct usb_dev_ep epo[USB_NUM_ENDPOINT];

	/* libusb data */
	libusb_device_handle *handle;
};

/*
 * The purpose to implement struct usb_dev_req is to adapt
 * struct usb_data_xfer to make a proper data format to talk
 * with libusb.
 */
struct usb_dev_req {
	struct usb_dev *udev;
	int    in;
	int    seq;
	/*
	 * buffer could include data from multiple
	 * usb_data_xfer_block, so here need some
	 * data to record it.
	 */
	uint8_t	*buffer;
	int     buf_length;
	int     blk_start;
	int     blk_count;

	struct usb_data_xfer *xfer;
	struct libusb_transfer *libusb_xfer;
	struct usb_data_xfer_block *setup_blk;
};

/* callback type used by code from HCD layer */
typedef int (*usb_dev_sys_cb)(void *hci_data, void *dev_data);

struct usb_dev_sys_ctx_info {
	/*
	 * Libusb related global variables
	 */
	libusb_context *libusb_ctx;
	pthread_t thread;
	int thread_exit;

	/* handles of callback */
	libusb_hotplug_callback_handle conn_handle;
	libusb_hotplug_callback_handle disconn_handle;

	/*
	 * The following callback funtions will be registered by
	 * the code from HCD(eg: XHCI, EHCI...) emulation layer.
	 */
	usb_dev_sys_cb conn_cb;
	usb_dev_sys_cb disconn_cb;
	usb_dev_sys_cb notify_cb;
	usb_dev_sys_cb intr_cb;

	libusb_device **devlist;

	/*
	 * private data from HCD layer
	 */
	void *hci_data;
};

/* intialize the usb_dev subsystem and register callbacks for HCD layer */
int usb_dev_sys_init(usb_dev_sys_cb conn_cb, usb_dev_sys_cb disconn_cb,
		usb_dev_sys_cb notify_cb, usb_dev_sys_cb intr_cb,
		void *hci_data, int log_level);
void usb_dev_sys_deinit(void);
void *usb_dev_init(void *pdata, char *opt);
void usb_dev_deinit(void *pdata);
int usb_dev_info(void *pdata, int type, void *value, int size);
int usb_dev_request(void *pdata, struct usb_data_xfer *xfer);
int usb_dev_reset(void *pdata);
int usb_dev_data(void *pdata, struct usb_data_xfer *xfer, int dir, int epctx);
#endif
