/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
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

static void
usb_dev_comp_req(struct libusb_transfer *libusb_xfer)
{
	struct usb_dev_req *req;
	struct usb_data_xfer *xfer;
	struct usb_data_xfer_block *block;
	int len, do_intr = 0, short_data = 0;
	int i, idx, buf_idx, done;

	assert(libusb_xfer);
	assert(libusb_xfer->user_data);

	req = libusb_xfer->user_data;
	len = libusb_xfer->actual_length;
	xfer = req->xfer;

	assert(xfer);
	assert(xfer->dev);
	UPRINTF(LDBG, "xfer_comp: ep %d with %d bytes. status %d,%d\n",
			req->xfer->epid, len, libusb_xfer->status,
			xfer->ndata);

	/* lock for protecting the transfer */
	USB_DATA_XFER_LOCK(xfer);
	xfer->status = USB_ERR_NORMAL_COMPLETION;

	/* in case the xfer is reset by the USB_DATA_XFER_RESET */
	if (xfer->reset == 1 ||
			libusb_xfer->status != LIBUSB_TRANSFER_COMPLETED) {
		UPRINTF(LDBG, "ep%d reset detected\r\n", xfer->epid);
		xfer->reset = 0;
		USB_DATA_XFER_UNLOCK(xfer);
		goto reset_out;
	}

	/* post process the usb transfer data */
	buf_idx = 0;
	idx = req->blk_start;
	for (i = 0; i < req->blk_count; i++) {
		done = 0;
		block = &xfer->data[idx % USB_MAX_XFER_BLOCKS];
		if (len > buf_idx) {
			done = block->blen;
			if (done > len - buf_idx) {
				done = len - buf_idx;
				short_data = 1;
			}
			if (req->in)
				memcpy(block->buf, &req->buffer[buf_idx], done);
		}

		assert(block->processed);
		buf_idx += done;
		block->bdone = done;
		block->blen -= done;
		idx = (idx + 1) % USB_MAX_XFER_BLOCKS;
	}

reset_out:
	if (short_data)
		xfer->status = USB_ERR_SHORT_XFER;

	/* notify the USB core this transfer is over */
	if (g_ctx.notify_cb)
		do_intr = g_ctx.notify_cb(xfer->dev, xfer);

	/* if a interrupt is needed, send it to guest */
	if (do_intr && g_ctx.intr_cb)
		g_ctx.intr_cb(xfer->dev, NULL);

	/* unlock and release memory */
	USB_DATA_XFER_UNLOCK(xfer);
	libusb_free_transfer(libusb_xfer);
	if (req && req->buffer)
		free(req->buffer);

	free(req);
}

static struct usb_dev_req *
usb_dev_alloc_req(struct usb_dev *udev, struct usb_data_xfer *xfer, int in,
		size_t size)
{
	struct usb_dev_req *req;
	static int seq = 1;

	if (!udev || !xfer)
		return NULL;

	req = calloc(1, sizeof(*req));
	if (!req)
		return NULL;

	req->udev = udev;
	req->in = in;
	req->xfer = xfer;
	req->seq = seq++;
	req->libusb_xfer = libusb_alloc_transfer(0);
	if (!req->libusb_xfer)
		goto errout;

	if (size)
		req->buffer = malloc(size);

	if (!req->buffer)
		goto errout;

	return req;

errout:
	if (req && req->buffer)
		free(req->buffer);
	if (req && req->libusb_xfer)
		libusb_free_transfer(req->libusb_xfer);
	if (req)
		free(req);
	return NULL;
}

static int
usb_dev_prepare_xfer(struct usb_data_xfer *xfer, int *count, int *size)
{
	int found, i, idx, c, s, first;
	struct usb_data_xfer_block *block = NULL;

	assert(xfer);
	idx = xfer->head;
	found = 0;
	first = -1;
	c = s = 0;
	if (!count || !size)
		return -1;

	for (i = 0; i < xfer->ndata; i++) {
		block = &xfer->data[idx];

		if (block->processed) {
			idx = (idx + 1) % USB_MAX_XFER_BLOCKS;
			continue;
		}
		if (block->buf && block->blen > 0) {
			if (!found) {
				found = 1;
				first = idx;
			}
			c++;
			s += block->blen;

		} else if (found) {
			UPRINTF(LWRN, "find a NULL data. %d total %d\n",
				i, xfer->ndata);
		}
		block->processed = 1;
		idx = (idx + 1) % USB_MAX_XFER_BLOCKS;
	}

	*count = c;
	*size = s;
	return first;
}

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

static inline struct usb_dev_ep *
usb_dev_get_ep(struct usb_dev *udev, int pid, int ep)
{
	assert(udev);

	if (ep < 0 || ep >= USB_NUM_ENDPOINT) {
		UPRINTF(LWRN, "invalid ep %d\r\n", ep);
		return NULL;
	}

	if (ep == 0)
		return &udev->epc;

	if (pid == TOKEN_IN)
		return udev->epi + ep - 1;
	else
		return udev->epo + ep - 1;
}

static inline void
usb_dev_set_ep_type(struct usb_dev *udev, int pid, int epnum,
		uint8_t type)
{
	struct usb_dev_ep *ep;

	ep = usb_dev_get_ep(udev, pid, epnum);
	if (ep)
		ep->type = type;
}

static inline uint8_t
usb_dev_get_ep_type(struct usb_dev *udev, int pid, int epnum)
{
	struct usb_dev_ep *ep;

	ep = usb_dev_get_ep(udev, pid, epnum);
	if (!ep)
		return USB_EP_ERR_TYPE;
	else
		return ep->type;
}

static void
usb_dev_reset_ep(struct usb_dev *udev)
{
	int ep;

	udev->epc.type = USB_ENDPOINT_CONTROL;
	for (ep = 0; ep < USB_NUM_ENDPOINT; ep++) {
		udev->epi[ep].pid = TOKEN_IN;
		udev->epo[ep].pid = TOKEN_OUT;
		udev->epi[ep].type = USB_ENDPOINT_INVALID;
		udev->epo[ep].type = USB_ENDPOINT_INVALID;
	}
}

static void
usb_dev_update_ep(struct usb_dev *udev)
{
	struct libusb_config_descriptor *cfg;
	const struct libusb_interface_descriptor *_if;
	const struct libusb_endpoint_descriptor *desc;
	int i, j;

	assert(udev);
	if (libusb_get_active_config_descriptor(udev->ldev, &cfg))
		return;

	for (i = 0; i < cfg->bNumInterfaces; i++) {
		_if = &cfg->interface[i].altsetting[udev->alts[i]];

		for (j = 0; j < _if->bNumEndpoints; j++) {
			desc = &_if->endpoint[j];
			usb_dev_set_ep_type(udev,
					USB_EP_PID(desc),
					USB_EP_NR(desc),
					USB_EP_TYPE(desc));
		}
	}
	libusb_free_config_descriptor(cfg);
}

static int
usb_dev_native_toggle_if(struct usb_dev *udev, int claim)
{
	struct libusb_config_descriptor *config;
	uint8_t b, p, c, i;
	int rc = 0, r;

	assert(udev);
	assert(udev->handle);
	assert(udev->ldev);
	assert(claim == 1 || claim == 0);

	b = udev->bus;
	p = udev->port;

	r = libusb_get_active_config_descriptor(udev->ldev, &config);
	if (r) {
		UPRINTF(LWRN, "%d-%d: can't get config\r\n", b, p);
		return -1;
	}

	c = config->bConfigurationValue;
	for (i = 0; i < config->bNumInterfaces; i++) {
		if (claim == 1)
			r = libusb_claim_interface(udev->handle, i);
		else
			r = libusb_release_interface(udev->handle, i);

		if (r) {
			rc = -1;
			UPRINTF(LWRN, "%d-%d:%d.%d can't %s if\r\n", b, p, c, i,
					claim == 1 ? "claim" : "release");
		}
	}
	if (rc)
		UPRINTF(LWRN, "%d-%d fail to %s rc %d\r\n", b, p,
				claim == 1 ? "claim" : "release", rc);
	libusb_free_config_descriptor(config);
	return rc;
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

static void
usb_dev_set_config(struct usb_dev *udev, struct usb_data_xfer *xfer, int config)
{
	int rc = 0;
	struct libusb_config_descriptor *cfg;

	assert(udev);
	assert(udev->ldev);
	assert(udev->handle);

	/*
	 * set configuration
	 * according to the libusb doc, the detach and release work
	 * should be done before set configuration.
	 */
	usb_dev_native_toggle_if_drivers(udev, 0);
	usb_dev_native_toggle_if(udev, 0);

	rc = libusb_set_configuration(udev->handle, config);
	if (rc) {
		UPRINTF(LWRN, "fail to set config rc %d\r\n", rc);
		goto err2;
	}

	/* claim all the interfaces of this configuration */
	rc = libusb_get_active_config_descriptor(udev->ldev, &cfg);
	if (rc) {
		UPRINTF(LWRN, "fail to get config rc %d\r\n", rc);
		goto err2;
	}

	rc = usb_dev_native_toggle_if(udev, 1);
	if (rc) {
		UPRINTF(LWRN, "fail to claim if, rc %d\r\n", rc);
		goto err1;
	}

	udev->if_num = cfg->bNumInterfaces;
	udev->configuration = config;

	usb_dev_reset_ep(udev);
	usb_dev_update_ep(udev);
	libusb_free_config_descriptor(cfg);
	return;

err1:
	usb_dev_native_toggle_if(udev, 0);
	libusb_free_config_descriptor(cfg);
err2:
	UPRINTF(LWRN, "%d-%d: fail to set config\r\n", udev->bus, udev->port);
	xfer->status = USB_ERR_STALLED;
}

static void
usb_dev_set_if(struct usb_dev *udev, int iface, int alt, struct usb_data_xfer
		*xfer)
{
	assert(udev);
	assert(xfer);
	assert(udev->handle);

	if (iface >= USB_NUM_INTERFACE)
		goto errout;

	UPRINTF(LDBG, "%d-%d set if, iface %d alt %d\r\n", udev->bus,
			udev->port, iface, alt);

	if (libusb_set_interface_alt_setting(udev->handle, iface, alt))
		goto errout;

	udev->alts[iface] = alt;
	/*
	 * FIXME: Only support single interface USB device first. Need fix in
	 * future to support composite USB device.
	 */
	usb_dev_reset_ep(udev);
	usb_dev_update_ep(udev);
	return;

errout:
	xfer->status = USB_ERR_STALLED;
	UPRINTF(LDBG, "%d-%d fail to set if, iface %d alt %d\r\n",
			udev->bus, udev->port, iface, alt);
}

static struct usb_data_xfer_block *
usb_dev_prepare_ctrl_xfer(struct usb_data_xfer *xfer)
{
	int i, idx;
	struct usb_data_xfer_block *ret = NULL;
	struct usb_data_xfer_block *blk = NULL;

	idx = xfer->head;
	for (i = 0; i < xfer->ndata; i++) {
		/*
		 * find out the data block and set every
		 * block to be processed
		 */
		blk = &xfer->data[idx];
		if (blk->blen > 0 && !ret)
			ret = blk;

		blk->processed = 1;
		idx = (idx + 1) % USB_MAX_XFER_BLOCKS;
	}
	return ret;
}

int
usb_dev_reset(void *pdata)
{
	struct usb_dev *udev;

	udev = pdata;
	assert(udev);

	UPRINTF(LDBG, "reset endpoints\n");
	libusb_reset_device(udev->handle);
	usb_dev_reset_ep(udev);
	usb_dev_update_ep(udev);
	return 0;
}

int
usb_dev_data(void *pdata, struct usb_data_xfer *xfer, int dir, int epctx)
{
	struct usb_dev *udev;
	struct usb_dev_req *req;
	int rc = 0, epid;
	uint8_t type;
	int blk_start, data_size, blk_count;
	int retries = 3, i, buf_idx;
	struct usb_data_xfer_block *b;

	udev = pdata;
	assert(udev);
	xfer->status = USB_ERR_NORMAL_COMPLETION;

	blk_start = usb_dev_prepare_xfer(xfer, &blk_count, &data_size);
	if (blk_start < 0)
		goto done;

	type = usb_dev_get_ep_type(udev, dir ? TOKEN_IN : TOKEN_OUT, epctx);
	epid = dir ? (0x80 | epctx) : epctx;

	if (data_size <= 0)
		goto done;

	UPRINTF(LDBG, "%s: DIR=%s|EP=%x|*%s*, data %d %d-%d\n", __func__,
			dir ? "IN" : "OUT", epid, type == USB_ENDPOINT_BULK ?
			"BULK" : "INT", data_size,
			blk_start, blk_start + blk_count - 1);

	req = usb_dev_alloc_req(udev, xfer, dir, data_size);
	if (!req) {
		xfer->status = USB_ERR_IOERROR;
		goto done;
	}

	req->buf_length = data_size;
	req->blk_start = blk_start;
	req->blk_count = blk_count;

	if (!dir) {
		for (i = 0, buf_idx = 0; i < blk_count; i++) {
			b = &xfer->data[(blk_start + i) % USB_MAX_XFER_BLOCKS];
			if (b->buf) {
				memcpy(&req->buffer[buf_idx], b->buf, b->blen);
				buf_idx += b->blen;
			}
		}
	}

	if (type == USB_ENDPOINT_BULK) {
		/*
		 * give data to physical device through libusb.
		 * This is an asynchronous process, data is sent to libusb.so,
		 * and it may be not sent to physical device instantly, but
		 * just return here. After the data is really received by the
		 * physical device, the callback function usb_dev_comp_req
		 * will be triggered.
		 */
		/*
		 * TODO: Is there any risk of data missing?
		 */
		libusb_fill_bulk_transfer(req->libusb_xfer,
				udev->handle, epid,
				req->buffer,
				data_size,
				usb_dev_comp_req,
				req,
				0);
		do {
			rc = libusb_submit_transfer(req->libusb_xfer);
		} while (rc && retries--);

	} else if (type == USB_ENDPOINT_INT) {
		/* give data to physical device through libusb */
		libusb_fill_interrupt_transfer(req->libusb_xfer,
				udev->handle,
				epid,
				req->buffer,
				data_size,
				usb_dev_comp_req,
				req,
				0);
		rc = libusb_submit_transfer(req->libusb_xfer);
	} else {
		/* TODO isoch transfer is not implemented */
		UPRINTF(LWRN, "ISOCH transfer still not supported.\n");
		xfer->status = USB_ERR_INVAL;
	}

	if (rc) {
		xfer->status = USB_ERR_IOERROR;
		UPRINTF(LDBG, "libusb_submit_transfer fail: %d\n", rc);
	}
done:
	return xfer->status;
}

int
usb_dev_request(void *pdata, struct usb_data_xfer *xfer)
{
	struct usb_dev *udev;
	uint8_t  request_type;
	uint8_t  request;
	uint16_t value;
	uint16_t index;
	uint16_t len;
	struct usb_data_xfer_block *blk;
	uint8_t *data;
	int rc;

	udev = pdata;

	assert(xfer);
	assert(udev);

	xfer->status = USB_ERR_NORMAL_COMPLETION;
	if (!udev->ldev || !xfer->ureq) {
		UPRINTF(LWRN, "invalid request\r\n");
		xfer->status = USB_ERR_IOERROR;
		goto out;
	}

	request_type = xfer->ureq->bmRequestType;
	request      = xfer->ureq->bRequest;
	value        = xfer->ureq->wValue;
	index        = xfer->ureq->wIndex;
	len          = xfer->ureq->wLength;

	blk = usb_dev_prepare_ctrl_xfer(xfer);
	data = blk ? blk->buf : NULL;

	UPRINTF(LDBG,
		"urb: type 0x%x req 0x%x val 0x%x idx %d len %d data %d\n",
		 request_type, request, value, index, len,
		 blk ? blk->blen : 0);

	/*
	 * according to usb spec, control transfer may have no
	 * DATA STAGE, so the valid situations are:
	 *   a. with DATA STAGE: blk != NULL && len > 0
	 *   b. without DATA STAGE: blk == NULL && len == 0
	 * any other situations, just skip process
	 */
	if ((!blk && len > 0) || (blk && len <= 0))
		goto out;

	switch (UREQ(request, request_type)) {
	case UREQ(UR_SET_ADDRESS, UT_READ_DEVICE):
		UPRINTF(LDBG, "UR_SET_ADDRESS\n");
		udev->addr = value;
		goto out;
	case UREQ(UR_SET_CONFIG, UT_READ_DEVICE):
		UPRINTF(LDBG, "UR_SET_CONFIG\n");
		usb_dev_set_config(udev, xfer, value & 0xff);
		goto out;
	case UREQ(UR_SET_INTERFACE, UT_READ_INTERFACE):
		UPRINTF(LDBG, "UR_SET_INTERFACE\n");
		usb_dev_set_if(udev, index, value, xfer);
		goto out;
	case UREQ(UR_CLEAR_FEATURE, UT_WRITE_ENDPOINT):
		if (value == 0) {
			UPRINTF(LDBG, "UR_CLEAR_HALT\n");
			libusb_clear_halt(udev->handle, index);
			goto out;
		}
	}

	/* send it to physical device */
	/* TODO: should this be async operation? */
	rc = libusb_control_transfer(udev->handle, request_type, request,
			value, index, data, len, 100);

	if (rc >= 0 && blk) {
		blk->blen = len - rc;
		blk->bdone += rc;
		xfer->status = blk->blen > 0 ? USB_ERR_SHORT_XFER :
			USB_ERR_NORMAL_COMPLETION;
	} else if (rc >= 0)
		xfer->status = USB_ERR_NORMAL_COMPLETION;
	else
		xfer->status = usb_dev_err_convert(rc);

	UPRINTF(LDBG, "usb rc %d, blk %p, blen %u bdon %u\n", rc, blk,
			blk ? blk->blen : 0, blk ? blk->bdone : 0);
out:
	return xfer->status;
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
	uint8_t port;

	UPRINTF(LDBG, "disconnect event\r\n");

	if (!ctx || !ldev) {
		UPRINTF(LFTL, "disconnect callback fails!\n");
		return -1;
	}

	port = libusb_get_port_number(ldev);
	if (g_ctx.disconn_cb)
		g_ctx.disconn_cb(g_ctx.hci_data, &port);

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
	g_ctx.notify_cb    = notify_cb;
	g_ctx.intr_cb      = intr_cb;
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

	/* this is for guest rebooting purpose */
	g_ctx.conn_handle = native_conn_handle;
	g_ctx.disconn_handle = native_disconn_handle;
	g_ctx.thread_exit = 0;

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

void
usb_dev_sys_deinit(void)
{
	if (!g_ctx.libusb_ctx)
		return;

	UPRINTF(LINF, "port-mapper de-initialization\r\n");
	libusb_hotplug_deregister_callback(g_ctx.libusb_ctx, g_ctx.conn_handle);
	libusb_hotplug_deregister_callback(g_ctx.libusb_ctx,
			g_ctx.disconn_handle);

	g_ctx.thread_exit = 1;
	pthread_join(g_ctx.thread, NULL);

	libusb_exit(g_ctx.libusb_ctx);
	g_ctx.libusb_ctx = NULL;
}
