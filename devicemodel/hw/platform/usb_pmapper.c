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


#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "usb.h"
#include "usbdi.h"
#include "usb_core.h"
#include "usb_pmapper.h"

#undef LOG_TAG
#define LOG_TAG "USBPM: "

static struct usb_dev_sys_ctx_info g_ctx;

static inline int
usb_dev_err_convert(int err)
{
	switch (err) {
	case LIBUSB_ERROR_TIMEOUT: return USB_ERR_TIMEOUT;
	case LIBUSB_ERROR_PIPE: return USB_ERR_STALLED;
	case LIBUSB_ERROR_NO_DEVICE: return USB_ERR_INVAL;
	case LIBUSB_ERROR_BUSY: return USB_ERR_IN_USE;
	case LIBUSB_ERROR_OVERFLOW: return USB_ERR_TOO_DEEP;
	default:
		break; /* add more when required */
	}
	return USB_ERR_IOERROR;
}

static int
usb_dev_native_toggle_if_drivers(struct usb_dev *udev, int attach)
{
	struct libusb_config_descriptor *config;
	uint8_t b, p, c, i;
	int rc = 0, r;

	assert(udev);
	assert(udev->handle);
	assert(udev->ldev);
	assert(attach == 1 || attach == 0);

	b = udev->bus;
	p = udev->port;

	r = libusb_get_active_config_descriptor(udev->ldev, &config);
	if (r) {
		UPRINTF(LWRN, "%d-%d: can't get config\r\n", b, p);
		return -1;
	}

	c = config->bConfigurationValue;
	for (i = 0; i < config->bNumInterfaces; i++) {
		if (attach == 1)
			r = libusb_attach_kernel_driver(udev->handle, i);
		else
			r = libusb_detach_kernel_driver(udev->handle, i);

		if (r) {
			rc = -1;
			UPRINTF(LWRN, "%d-%d:%d.%d can't %stach if driver\r\n",
					b, p, c, i, attach == 1 ? "at" : "de");
		}
	}
	if (rc)
		UPRINTF(LWRN, "%d-%d fail to %s rc %d\r\n", b, p,
				attach == 1 ? "attach" : "detach", rc);
	libusb_free_config_descriptor(config);
	return rc;
}

void *
usb_dev_init(void *pdata, char *opt)
{
	struct usb_dev *udev = NULL;
	struct libusb_device *ldev;
	struct libusb_device_descriptor desc;
	uint8_t bus, port;
	int speed, ver;

	assert(pdata);

	ldev  = pdata;
	speed = libusb_get_device_speed(ldev);
	port  = libusb_get_port_number(ldev);
	bus   = libusb_get_bus_number(ldev);
	libusb_get_device_descriptor(ldev, &desc);
	UPRINTF(LINF, "Found USB device: %d-%d\r\nPID(0x%X), VID(0x%X) "
			"CLASS(0x%X) SUBCLASS(0x%x) BCD(0x%x)\r\n", bus, port,
			desc.idProduct, desc.idVendor,
			desc.bDeviceClass, desc.bDeviceSubClass, desc.bcdUSB);

	/* allocate and populate udev */
	udev = calloc(1, sizeof(struct usb_dev));
	if (!udev)
		goto errout;

	/* this is a root hub */
	if (port == 0)
		goto errout;

	switch (desc.bcdUSB) {	/* TODO: implemnt USB3.0 */
	case 0x300:
		ver = 2; break;
	case 0x200:
	case 0x110:
		ver = 2; break;
	default:
		goto errout;
	}

	udev->speed   = speed;
	udev->ldev    = ldev;
	udev->version = ver;
	udev->handle  = NULL;
	udev->port    = port;
	udev->bus     = bus;
	udev->pid     = desc.idProduct;
	udev->vid     = desc.idVendor;

	/* configure physical device through libusb library */
	if (libusb_open(udev->ldev, &udev->handle)) {
		UPRINTF(LWRN, "fail to open device.\r\n");
		goto errout;
	}

	if (usb_dev_native_toggle_if_drivers(udev, 0) < 0) {
		UPRINTF(LWRN, "fail to detach interface driver.\r\n");
		goto errout;
	}
	return udev;

errout:
	if (udev && udev->handle)
		libusb_close(udev->handle);

	free(udev);
	return NULL;
}

void
usb_dev_deinit(void *pdata)
{
	int rc = 0;
	struct usb_dev *udev;

	udev = pdata;
	if (udev) {
		if (udev->handle) {
			rc = usb_dev_native_toggle_if_drivers(udev, 1);
			if (rc)
				UPRINTF(LWRN, "fail to attach if drv rc:%d\r\n",
						rc);
			libusb_close(udev->handle);
		}
		free(udev);
	}
}

int
usb_dev_info(void *pdata, int type, void *value, int size)
{
	struct usb_dev *udev;
	int sz;
	void *pv;

	udev = pdata;
	assert(udev);
	assert(value);

	switch (type) {
	case USB_INFO_VERSION:
		sz = sizeof(udev->version);
		pv = &udev->version;
		break;
	case USB_INFO_SPEED:
		sz = sizeof(udev->speed);
		pv = &udev->speed;
		break;
	case USB_INFO_BUS:
		sz = sizeof(udev->bus);
		pv = &udev->bus;
		break;
	case USB_INFO_PORT:
		sz = sizeof(udev->port);
		pv = &udev->port;
		break;
	case USB_INFO_VID:
		sz = sizeof(udev->vid);
		pv = &udev->vid;
		break;
	case USB_INFO_PID:
		sz = sizeof(udev->pid);
		pv = &udev->pid;
		break;
	default:
		return -1;
	}

	if (size == sz)
		memcpy(value, pv, sz);

	return sz == size ? 0 : -1;
}

static void *
usb_dev_sys_thread(void *arg)
{
	struct timeval t = {1, 0};

	while (g_ctx.thread_exit == 0 &&
		libusb_handle_events_timeout(g_ctx.libusb_ctx, &t) >= 0)
		; /* nothing */

	UPRINTF(LINF, "poll thread exit\n\r");
	return NULL;
}

static int
usb_dev_native_sys_conn_cb(struct libusb_context *ctx, struct libusb_device
		*ldev, libusb_hotplug_event event, void *pdata)
{
	UPRINTF(LDBG, "connect event\r\n");

	if (!ctx || !ldev) {
		UPRINTF(LFTL, "connect callback fails!\n");
		return -1;
	}

	if (g_ctx.conn_cb)
		g_ctx.conn_cb(g_ctx.hci_data, ldev);

	return 0;
}

static int
usb_dev_native_sys_disconn_cb(struct libusb_context *ctx, struct libusb_device
		*ldev, libusb_hotplug_event event, void *pdata)
{
	UPRINTF(LDBG, "disconnect event\r\n");

	if (!ctx || !ldev) {
		UPRINTF(LFTL, "disconnect callback fails!\n");
		return -1;
	}

	if (g_ctx.disconn_cb)
		g_ctx.disconn_cb(g_ctx.hci_data, NULL);

	return 0;
}

int
usb_dev_sys_init(usb_dev_sys_cb conn_cb, usb_dev_sys_cb disconn_cb,
		usb_dev_sys_cb notify_cb, usb_dev_sys_cb intr_cb,
		void *hci_data, int log_level)
{
	libusb_hotplug_event native_conn_evt;
	libusb_hotplug_event native_disconn_evt;
	libusb_hotplug_flag flags;
	libusb_hotplug_callback_handle native_conn_handle;
	libusb_hotplug_callback_handle native_disconn_handle;
	int native_pid, native_vid, native_cls, rc;

	assert(conn_cb);
	assert(disconn_cb);
	usb_set_log_level(log_level);

	if (g_ctx.libusb_ctx) {
		UPRINTF(LFTL, "port mapper is already initialized.\r\n");
		return -1;
	}

	rc = libusb_init(&g_ctx.libusb_ctx);
	if (rc < 0) {
		UPRINTF(LFTL, "libusb_init fails, rc:%d\r\n", rc);
		return -1;
	}

	g_ctx.hci_data     = hci_data;
	g_ctx.conn_cb      = conn_cb;
	g_ctx.disconn_cb   = disconn_cb;
	native_conn_evt    = LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED;
	native_disconn_evt = LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT;
	native_pid         = LIBUSB_HOTPLUG_MATCH_ANY;
	native_vid         = LIBUSB_HOTPLUG_MATCH_ANY;
	native_cls         = LIBUSB_HOTPLUG_MATCH_ANY;
	flags              = 0;

	/* register connect callback */
	rc = libusb_hotplug_register_callback(g_ctx.libusb_ctx, native_conn_evt,
			flags, native_vid, native_pid, native_cls,
			usb_dev_native_sys_conn_cb, NULL, &native_conn_handle);
	if (rc != LIBUSB_SUCCESS)
		goto errout;

	/* register disconnect callback */
	rc = libusb_hotplug_register_callback(g_ctx.libusb_ctx,
			native_disconn_evt, flags, native_vid, native_pid,
			native_cls, usb_dev_native_sys_disconn_cb, NULL,
			&native_disconn_handle);
	if (rc != LIBUSB_SUCCESS) {
		libusb_hotplug_deregister_callback(g_ctx.libusb_ctx,
				native_conn_handle);
		goto errout;
	}

	if (pthread_create(&g_ctx.thread, NULL, usb_dev_sys_thread, NULL)) {
		libusb_hotplug_deregister_callback(g_ctx.libusb_ctx,
				native_conn_handle);
		libusb_hotplug_deregister_callback(g_ctx.libusb_ctx,
				native_disconn_handle);
		goto errout;
	}
	return 0;

errout:
	if (g_ctx.libusb_ctx)
		libusb_exit(g_ctx.libusb_ctx);

	g_ctx.libusb_ctx = NULL;
	return -1;
}
