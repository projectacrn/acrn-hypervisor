/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _USB_DEVICE_H
#define _USB_DEVICE_H
#include <libusb-1.0/libusb.h>

#define USB_NUM_INTERFACE 16
#define USB_NUM_ENDPOINT  15

struct usb_dev_ep {
	uint8_t pid;
	uint8_t type;
};

struct usb_dev {
	/* physical device info */
	int addr;
	int version;
	int speed;
	int configuration;
	uint8_t port;

	/* interface info */
	int if_num;
	int alts[USB_NUM_INTERFACE];

	/* endpoints info */
	struct usb_dev_ep epc;
	struct usb_dev_ep epi[USB_NUM_ENDPOINT];
	struct usb_dev_ep epo[USB_NUM_ENDPOINT];

	/* libusb data */
	libusb_device           *ldev;
	libusb_device_handle    *handle;
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

	/*
	 * The following callback funtions will be registered by
	 * the code from HCD(eg: XHCI, EHCI...) emulation layer.
	 */
	usb_dev_sys_cb conn_cb;
	usb_dev_sys_cb disconn_cb;

	/*
	 * private data from HCD layer
	 */
	void *hci_data;
};

/* intialize the usb_dev subsystem and register callbacks for HCD layer */
int usb_dev_sys_init(usb_dev_sys_cb conn_cb, usb_dev_sys_cb disconn_cb,
		usb_dev_sys_cb notify_cb, usb_dev_sys_cb intr_cb,
		void *hci_data, int log_level);

#endif
