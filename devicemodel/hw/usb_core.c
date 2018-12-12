/*-
 * Copyright (c) 2014 Nahanni Systems Inc.
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
 * USB emulation designed as following diagram. It devided into three layers:
 *   HCD: USB host controller device layer, like xHCI, eHCI...
 *   USB core: middle abstraction layer for USB statck.
 *   USB Port Mapper: This layer supports to share physical USB
 *     devices(physical ports) to spcified virtual USB port of
 *     HCD DM. All the commands and data transfers are through
 *     Libusb to access native USB stack.
 *
 *              +---------------+
 *              |               |
 *   +----------+----------+    |
 *   |       ACRN DM       |    |
 *   | +-----------------+ |    |
 *   | |   HCD (xHCI)    | |    |
 *   | +-----------------+ |    |
 *   |          |          |    |
 *   | +-----------------+ |    |
 *   | |    USB Core     | |    |
 *   | +-----------------+ |    |
 *   |          |          |    |
 *   | +-----------------+ |    |
 *   | | USB Port Mapper | |    |
 *   | +-----------------+ |    |
 *   |          |          |    |
 *   | +-----------------+ |    |
 *   | |      Libusb     | |    |
 *   | +-----------------+ |    |
 *   +---------------------+    |
 *    Service OS User Space     |     User OS User Space
 *                              |
 *  --------------------------  |  ---------------------------
 *                              |
 *   Service OS Kernel Space    |    User OS Kernel Space
 *                              |
 *                              |    +-----------------+
 *                              |    |     USB Core    |
 *                              |    +-----------------+
 *                              |             |
 *                              |    +-----------------+
 *                              |    |       HCD       |
 *                              |    | (xHCI/uHCI/...) |
 *                              |    +--------+--------+
 *                              |             |
 *                              +-------------+
 * Current distribution:
 *   HCD: xhci.{h,c}
 *   USB core: usb_core.{h,c}
 *   USB device: usb_mouse.c usb_pmapper.{h,c}
 */

#include <sys/types.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "usb_core.h"
#include "dm_string.h"

SET_DECLARE(usb_emu_set, struct usb_devemu);
int usb_log_level;

struct usb_devemu *
usb_emu_finddev(char *name)
{
	struct usb_devemu **udpp, *udp;

	SET_FOREACH(udpp, usb_emu_set) {
		udp = *udpp;
		if (!strcmp(udp->ue_emu, name))
			return udp;
	}

	return NULL;
}

struct usb_data_xfer_block *
usb_data_xfer_append(struct usb_data_xfer *xfer, void *buf, int blen,
		     void *hci_data, int ccs)
{
	struct usb_data_xfer_block *xb;

	if (xfer->ndata >= USB_MAX_XFER_BLOCKS)
		return NULL;

	xb = &xfer->data[xfer->tail];
	xb->buf = buf;
	xb->blen = blen;
	xb->hci_data = hci_data;
	xb->ccs = ccs;
	xb->processed = USB_XFER_BLK_FREE;
	xb->bdone = 0;
	xfer->ndata++;
	xfer->tail = (xfer->tail + 1) % USB_MAX_XFER_BLOCKS;
	return xb;
}

int
usb_native_is_bus_existed(uint8_t bus_num)
{
	char buf[128];

	snprintf(buf, sizeof(buf), "%s/usb%d", NATIVE_USBSYS_DEVDIR, bus_num);
	return access(buf, R_OK) ? 0 : 1;
}

int
usb_native_is_ss_port(uint8_t bus_of_port)
{
	char buf[128];
	char speed[8];
	int rc, fd;
	int usb2_speed_sz = sizeof(NATIVE_USB2_SPEED);
	int usb3_speed_sz = sizeof(NATIVE_USB3_SPEED);

	assert(usb_native_is_bus_existed(bus_of_port));
	snprintf(buf, sizeof(buf), "%s/usb%d/speed", NATIVE_USBSYS_DEVDIR,
			bus_of_port);
	if (access(buf, R_OK)) {
		UPRINTF(LWRN, "can't find speed file\r\n");
		return 0;
	}

	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		UPRINTF(LWRN, "fail to open maxchild file\r\n");
		return 0;
	}

	rc = read(fd, &speed, sizeof(speed));
	if (rc < 0) {
		UPRINTF(LWRN, "fail to read speed file\r\n");
		goto errout;
	}

	if (rc < usb2_speed_sz) {
		UPRINTF(LWRN, "read invalid speed data\r\n");
		goto errout;
	}

	if (strncmp(speed, NATIVE_USB3_SPEED, usb3_speed_sz))
		goto errout;

	close(fd);
	return 1;
errout:
	close(fd);
	return 0;
}

int
usb_native_is_port_existed(uint8_t bus_num, uint8_t port_num)
{
	int native_port_cnt;
	int rc, fd;
	char buf[128];
	char cnt[8];

	if (!usb_native_is_bus_existed(bus_num))
		return 0;

	snprintf(buf, sizeof(buf), "%s/usb%d/maxchild", NATIVE_USBSYS_DEVDIR,
			bus_num);
	if (access(buf, R_OK)) {
		UPRINTF(LWRN, "can't find maxchild file\r\n");
		return 0;
	}

	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		UPRINTF(LWRN, "fail to open maxchild file\r\n");
		return 0;
	}

	rc = read(fd, &cnt, sizeof(cnt));
	if (rc < 0) {
		UPRINTF(LWRN, "fail to read maxchild file\r\n");
		close(fd);
		return 0;
	}

	rc = dm_strtoi(cnt, (char **)&cnt, 10, &native_port_cnt);
	if (rc) {
		UPRINTF(LWRN, "fail to get maxchild number\r\n");
		close(fd);
		return 0;
	}

	if (port_num > native_port_cnt || port_num < 0) {
		UPRINTF(LWRN, "invalid port_num %d, max port count %d\r\n",
				port_num, native_port_cnt);
		close(fd);
		return 0;
	}
	close(fd);
	return 1;
}

void usb_parse_log_level(char level)
{
	switch (level) {
	case 'F':
	case 'f':
		usb_set_log_level(LFTL);
		break;
	case 'W':
	case 'w':
		usb_set_log_level(LWRN);
		break;
	case 'I':
	case 'i':
		usb_set_log_level(LINF);
		break;
	case 'D':
	case 'd':
		usb_set_log_level(LDBG);
		break;
	case 'V':
	case 'v':
		usb_set_log_level(LVRB);
		break;
	default:
		usb_set_log_level(LFTL);
	}
}

char *
usb_dev_path(struct usb_devpath *path)
{
	static char output[sizeof("01.02.03.04.05.06.07")+1];
	int i, r, n;

	if (!path)
		return NULL;

	r = n = sizeof(output);
	r -= snprintf(output, n, "%d", path->path[0]);

	for (i = 1; i < path->depth; i++) {
		r -= snprintf(output + n - r, r, ".%d", path->path[i]);
		if (r < 0)
			return NULL;
	}

	return output;
}

bool
usb_dev_path_cmp(struct usb_devpath *p1, struct usb_devpath *p2)
{
	if (!p1 || !p2)
		return false;

	return (p1->bus == p2->bus && p1->depth == p2->depth &&
				memcmp(p1->path, p2->path, p1->depth) == 0);
}

int
usb_get_hub_port_num(struct usb_devpath *path)
{
	int rc, fd;
	int icnt;
	char buf[128];
	char cnt[8];

	if (!usb_native_is_bus_existed(path->bus))
		return -1;

	snprintf(buf, sizeof(buf), "%s/%d-%s/maxchild", NATIVE_USBSYS_DEVDIR,
			path->bus, usb_dev_path(path));
	if (access(buf, R_OK)) {
		UPRINTF(LWRN, "can't find maxchild file\r\n");
		return -1;
	}

	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		UPRINTF(LWRN, "fail to open maxchild file\r\n");
		return -1;
	}

	rc = read(fd, &cnt, sizeof(cnt));
	if (rc < 0) {
		UPRINTF(LWRN, "fail to read maxchild file\r\n");
		close(fd);
		return -1;
	}

	close(fd);

	rc = dm_strtoi(cnt, (char **)&cnt, 10, &icnt);
	if (rc) {
		UPRINTF(LWRN, "fail to get maxchild\r\n");
		return -1;
	}

	return icnt;
}
