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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "usb.h"
#include "usbdi.h"
#include "usb_core.h"
#include "console.h"
#include "gc.h"

#undef LOG_TAG
#define LOG_TAG "umouse: "
/* USB endpoint context (1-15) for reporting mouse data events*/
#define	UMOUSE_INTR_ENDPT	1

#define UMOUSE_REPORT_DESC_TYPE	0x22

#define	UMOUSE_GET_REPORT	0x01
#define	UMOUSE_GET_IDLE		0x02
#define	UMOUSE_GET_PROTOCOL	0x03
#define	UMOUSE_SET_REPORT	0x09
#define	UMOUSE_SET_IDLE		0x0A
#define	UMOUSE_SET_PROTOCOL	0x0B

enum {
	UMSTR_LANG,
	UMSTR_MANUFACTURER,
	UMSTR_PRODUCT,
	UMSTR_SERIAL,
	UMSTR_CONFIG,
	UMSTR_MAX
};

#define UMOUSE_DESC_MAX_LEN	32
static const char *const umouse_desc_strings[] = {
	"\x04\x09",
	"ACRN-DM",
	"HID Tablet",
	"01",
	"HID Tablet Device",
};

struct umouse_hid_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint8_t	bcdHID[2];
	uint8_t	bCountryCode;
	uint8_t	bNumDescriptors;
	uint8_t	bReportDescriptorType;
	uint8_t	wItemLength[2];
} __attribute__((packed));

struct umouse_config_desc {
	struct usb_config_descriptor		confd;
	struct usb_interface_descriptor		ifcd;
	struct umouse_hid_descriptor		hidd;
	struct usb_endpoint_descriptor		endpd;
	struct usb_endpoint_ss_comp_descriptor	sscompd;
} __attribute__((packed));

#define MOUSE_MAX_X	0x8000
#define MOUSE_MAX_Y	0x8000

static const uint8_t umouse_report_desc[] = {
	0x05, 0x01,		/* USAGE_PAGE (Generic Desktop)		*/
	0x09, 0x02,		/* USAGE (Mouse)			*/
	0xa1, 0x01,		/* COLLECTION (Application)		*/
	0x09, 0x01,		/*   USAGE (Pointer)			*/
	0xa1, 0x00,		/*   COLLECTION (Physical)		*/
	0x05, 0x09,		/*     USAGE_PAGE (Button)		*/
	0x19, 0x01,		/*     USAGE_MINIMUM (Button 1)		*/
	0x29, 0x03,		/*     USAGE_MAXIMUM (Button 3)		*/
	0x15, 0x00,		/*     LOGICAL_MINIMUM (0)		*/
	0x25, 0x01,		/*     LOGICAL_MAXIMUM (1)		*/
	0x75, 0x01,		/*     REPORT_SIZE (1)			*/
	0x95, 0x03,		/*     REPORT_COUNT (3)			*/
	0x81, 0x02,		/*     INPUT (Data,Var,Abs); 3 buttons	*/
	0x75, 0x05,		/*     REPORT_SIZE (5)			*/
	0x95, 0x01,		/*     REPORT_COUNT (1)			*/
	0x81, 0x03,		/*     INPUT (Cnst,Var,Abs); padding	*/
	0x05, 0x01,		/*     USAGE_PAGE (Generic Desktop)	*/
	0x09, 0x30,		/*     USAGE (X)			*/
	0x09, 0x31,		/*     USAGE (Y)			*/
	0x35, 0x00,		/*     PHYSICAL_MINIMUM (0)		*/
	0x46, 0xff, 0x7f,	/*     PHYSICAL_MAXIMUM (0x7fff)	*/
	0x15, 0x00,		/*     LOGICAL_MINIMUM (0)		*/
	0x26, 0xff, 0x7f,	/*     LOGICAL_MAXIMUM (0x7fff)		*/
	0x75, 0x10,		/*     REPORT_SIZE (16)			*/
	0x95, 0x02,		/*     REPORT_COUNT (2)			*/
	0x81, 0x02,		/*     INPUT (Data,Var,Abs)		*/
	0x05, 0x01,		/*     USAGE Page (Generic Desktop)	*/
	0x09, 0x38,		/*     USAGE (Wheel)			*/
	0x35, 0x00,		/*     PHYSICAL_MINIMUM (0)		*/
	0x45, 0x00,		/*     PHYSICAL_MAXIMUM (0)		*/
	0x15, 0x81,		/*     LOGICAL_MINIMUM (-127)		*/
	0x25, 0x7f,		/*     LOGICAL_MAXIMUM (127)		*/
	0x75, 0x08,		/*     REPORT_SIZE (8)			*/
	0x95, 0x01,		/*     REPORT_COUNT (1)			*/
	0x81, 0x06,		/*     INPUT (Data,Var,Rel)		*/
	0xc0,			/*   END_COLLECTION			*/
	0xc0			/* END_COLLECTION			*/
};

struct umouse_report {
	uint8_t	buttons;	/* bits: 0 left, 1 right, 2 middle */
	int16_t	x;		/* x position */
	int16_t	y;		/* y position */
	int8_t	z;		/* z wheel position */
} __attribute__((packed));

static struct usb_device_descriptor umouse_dev_desc = {
	.bLength = sizeof(umouse_dev_desc),
	.bDescriptorType = UDESC_DEVICE,
	.bcdUSB = UD_USB_3_0,
	.bMaxPacketSize = 8,	/* max packet size */
	.idVendor = 0xFB5D,	/* vendor */
	.idProduct = 0x0001,/* product */
	.bcdDevice = 0,	/* device version */
	.iManufacturer = UMSTR_MANUFACTURER,
	.iProduct = UMSTR_PRODUCT,
	.iSerialNumber = UMSTR_SERIAL,
	.bNumConfigurations = 1,
};

static struct umouse_config_desc umouse_confd = {
	.confd = {
		.bLength = sizeof(umouse_confd.confd),
		.bDescriptorType = UDESC_CONFIG,
		.wTotalLength = sizeof(umouse_confd),
		.bNumInterface = 1,
		.bConfigurationValue = 1,
		.iConfiguration = UMSTR_CONFIG,
		.bmAttributes = UC_BUS_POWERED | UC_REMOTE_WAKEUP,
		.bMaxPower = 0,
	},
	.ifcd = {
		.bLength = sizeof(umouse_confd.ifcd),
		.bDescriptorType = UDESC_INTERFACE,
		.bNumEndpoints = 1,
		.bInterfaceClass = UICLASS_HID,
		.bInterfaceSubClass = UISUBCLASS_BOOT,
		.bInterfaceProtocol = UIPROTO_MOUSE,
	},
	.hidd = {
		.bLength = sizeof(umouse_confd.hidd),
		.bDescriptorType = 0x21,
		.bcdHID = { 0x01, 0x10 },
		.bCountryCode = 0,
		.bNumDescriptors = 1,
		.bReportDescriptorType = UMOUSE_REPORT_DESC_TYPE,
		.wItemLength = { sizeof(umouse_report_desc), 0 },
	},
	.endpd = {
		.bLength = sizeof(umouse_confd.endpd),
		.bDescriptorType = UDESC_ENDPOINT,
		.bEndpointAddress = UE_DIR_IN | UMOUSE_INTR_ENDPT,
		.bmAttributes = UE_INTERRUPT,
		.wMaxPacketSize = 8,
		.bInterval = 0xA,
	},
	.sscompd = {
		.bLength = sizeof(umouse_confd.sscompd),
		.bDescriptorType = UDESC_ENDPOINT_SS_COMP,
		.bMaxBurst = 0,
		.bmAttributes = 0,
		.wBytesPerInterval = 0,
	},
};

struct umouse_bos_desc {
	struct usb_bos_descriptor		bosd;
	struct usb_devcap_ss_descriptor		usbssd;
} __attribute__((packed));

struct umouse_bos_desc umouse_bosd = {
	.bosd = {
		.bLength = sizeof(umouse_bosd.bosd),
		.bDescriptorType = UDESC_BOS,
		.wTotalLength = sizeof(umouse_bosd),
		.bNumDeviceCaps = 1,
	},
	.usbssd = {
		.bLength = sizeof(umouse_bosd.usbssd),
		.bDescriptorType = UDESC_DEVICE_CAPABILITY,
		.bDevCapabilityType = 3,
		.bmAttributes = 0,
		.wSpeedsSupported = 0x08,
		.bFunctionalitySupport = 3,
		.bU1DevExitLat = 0xa,   /* dummy - not used */
		.wU2DevExitLat = 0x20,
	}
};

struct umouse_vdev {
	struct usb_hci *hci;

	char	*opt;

	struct umouse_report um_report;
	int	newdata;
	struct {
		uint8_t	idle;
		uint8_t	protocol;
		uint8_t	feature;
	} hid;

	pthread_mutex_t	mtx;
	pthread_mutex_t	ev_mtx;
	int		polling;
	struct timeval	prev_evt;
};

static void
umouse_event(uint8_t button, int x, int y, void *arg)
{
	struct umouse_vdev *dev;
	struct gfx_ctx_image *gc;

	gc = console_get_image();
	if (gc == NULL) {
		/* not ready */
		return;
	}

	dev = arg;

	pthread_mutex_lock(&dev->mtx);

	dev->um_report.buttons = 0;
	dev->um_report.z = 0;

	if (button & 0x01)
		dev->um_report.buttons |= 0x01;	/* left */
	if (button & 0x02)
		dev->um_report.buttons |= 0x04;	/* middle */
	if (button & 0x04)
		dev->um_report.buttons |= 0x02;	/* right */
	if (button & 0x8)
		dev->um_report.z = 1;
	if (button & 0x10)
		dev->um_report.z = -1;

	/* scale coords to mouse resolution */
	dev->um_report.x = MOUSE_MAX_X * x / gc->width;
	dev->um_report.y = MOUSE_MAX_Y * y / gc->height;
	dev->newdata = 1;
	pthread_mutex_unlock(&dev->mtx);

	pthread_mutex_lock(&dev->ev_mtx);
	dev->hci->hci_intr(dev->hci, UE_DIR_IN | UMOUSE_INTR_ENDPT);
	pthread_mutex_unlock(&dev->ev_mtx);
}

static void *
umouse_init(void *pdata, char *opt)
{
	struct umouse_vdev *dev;
	struct usb_hci *hci;

	hci = pdata;
	if (!hci) {
		UPRINTF(LFTL, "umouse: invalid hci\r\n");
		return NULL;
	}

	dev = calloc(1, sizeof(struct umouse_vdev));
	if (!dev) {
		UPRINTF(LFTL, "calloc returns NULL\n");
		return NULL;
	}
	dev->hci = hci;

	dev->hid.protocol = 1;	/* REPORT protocol */
	dev->opt = strdup(opt);
	pthread_mutex_init(&dev->mtx, NULL);
	pthread_mutex_init(&dev->ev_mtx, NULL);

	console_ptr_register(umouse_event, dev, 10);

	return dev;
}

static int
umouse_request(void *scarg, struct usb_data_xfer *xfer)
{
	struct umouse_vdev *dev;
	struct usb_data_xfer_block *data;
	const char *str;
	uint16_t value;
	uint16_t index;
	uint16_t len;
	uint16_t slen;
	uint8_t *udata;
	int	err;
	int	i, idx;
	int	eshort;

	dev = scarg;

	data = NULL;
	udata = NULL;

	assert(xfer != NULL && xfer->head >= 0);

	idx = xfer->head;
	for (i = 0; i < xfer->ndata; i++) {
		xfer->data[idx].bdone = 0;
		if (data == NULL && USB_DATA_OK(xfer, i)) {
			data = &xfer->data[idx];
			udata = data->buf;
		}

		xfer->data[idx].processed = USB_XFER_BLK_HANDLED;
		idx = (idx + 1) % USB_MAX_XFER_BLOCKS;
	}

	err = USB_ERR_NORMAL_COMPLETION;
	eshort = 0;

	if (!xfer->ureq) {
		UPRINTF(LDBG, "%s: port %d\r\n", __func__, dev->hci->hci_port);
		goto done;
	}

	value = xfer->ureq->wValue;
	index = xfer->ureq->wIndex;
	len = xfer->ureq->wLength;

	UPRINTF(LDBG, "%s: port %d, type 0x%x, req 0x%x,"
		 "val 0x%x, idx 0x%x, len %u\r\n", __func__,
		 dev->hci->hci_port, xfer->ureq->bmRequestType,
		 xfer->ureq->bRequest, value, index, len);

	switch (UREQ(xfer->ureq->bRequest, xfer->ureq->bmRequestType)) {
	case UREQ(UR_GET_CONFIG, UT_READ_DEVICE):
		UPRINTF(LDBG, "(UR_GET_CONFIG, UT_READ_DEVICE)\r\n");
		if (!data)
			break;

		*udata = umouse_confd.confd.bConfigurationValue;
		data->blen = len > 0 ? len - 1 : 0;
		eshort = data->blen > 0;
		data->bdone += 1;
		break;

	case UREQ(UR_GET_DESCRIPTOR, UT_READ_DEVICE):
		UPRINTF(LDBG, "(UR_GET_DESCRIPTOR,UT_READ_DEVICE)"
			 "val %x\r\n",
			value >> 8);
		if (!data)
			break;

		switch (value >> 8) {
		case UDESC_DEVICE:
			UPRINTF(LDBG, "(->UDESC_DEVICE) len %u"
				 "?= sizeof(umouse_dev_desc) %lu\r\n",
				 len, sizeof(umouse_dev_desc));
			if ((value & 0xFF) != 0) {
				err = USB_ERR_IOERROR;
				goto done;
			}
			if (len > sizeof(umouse_dev_desc)) {
				data->blen = len - sizeof(umouse_dev_desc);
				len = sizeof(umouse_dev_desc);
			} else
				data->blen = 0;
			memcpy(data->buf, &umouse_dev_desc, len);
			data->bdone += len;
			break;

		case UDESC_CONFIG:
			UPRINTF(LDBG, "(->UDESC_CONFIG)\r\n");
			if ((value & 0xFF) != 0) {
				err = USB_ERR_IOERROR;
				goto done;
			}
			if (len > sizeof(umouse_confd)) {
				data->blen = len - sizeof(umouse_confd);
				len = sizeof(umouse_confd);
			} else
				data->blen = 0;

			memcpy(data->buf, &umouse_confd, len);
			data->bdone += len;
			break;

		case UDESC_STRING:
			UPRINTF(LDBG, "(->UDESC_STRING)\r\n");
			str = NULL;
			if ((value & 0xFF) < UMSTR_MAX)
				str = umouse_desc_strings[value & 0xFF];
			else
				goto done;

			if ((value & 0xFF) == UMSTR_LANG) {
				udata[0] = 4;
				udata[1] = UDESC_STRING;
				data->blen = len - 2;
				len -= 2;
				data->bdone += 2;

				if (len >= 2) {
					udata[2] = str[0];
					udata[3] = str[1];
					data->blen -= 2;
					data->bdone += 2;
				} else
					data->blen = 0;

				goto done;
			}

			slen = 2 + strnlen(str, UMOUSE_DESC_MAX_LEN) * 2;
			udata[0] = slen;
			udata[1] = UDESC_STRING;

			if (len > slen) {
				data->blen = len - slen;
				len = slen;
			} else
				data->blen = 0;
			for (i = 2; i < len; i += 2) {
				udata[i] = *str++;
				udata[i+1] = '\0';
			}
			data->bdone += slen;

			break;

		case UDESC_BOS:
			UPRINTF(LDBG, "USB3 BOS\r\n");
			if (len > sizeof(umouse_bosd)) {
				data->blen = len - sizeof(umouse_bosd);
				len = sizeof(umouse_bosd);
			} else
				data->blen = 0;
			memcpy(udata, &umouse_bosd, len);
			data->bdone += len;
			break;

		default:
			UPRINTF(LDBG, "unknown(%d)->ERROR\r\n", value >> 8);
			err = USB_ERR_IOERROR;
			goto done;
		}
		eshort = data->blen > 0;
		break;

	case UREQ(UR_GET_DESCRIPTOR, UT_READ_INTERFACE):
		UPRINTF(LDBG, "(UR_GET_DESCRIPTOR, UT_READ_INTERFACE)"
			 "0x%x\r\n",
			 (value >> 8));
		if (!data)
			break;

		switch (value >> 8) {
		case UMOUSE_REPORT_DESC_TYPE:
			if (len > sizeof(umouse_report_desc)) {
				data->blen = len - sizeof(umouse_report_desc);
				len = sizeof(umouse_report_desc);
			} else
				data->blen = 0;
			memcpy(data->buf, umouse_report_desc, len);
			data->bdone += len;
			break;
		default:
			UPRINTF(LWRN, "IO ERROR\r\n");
			err = USB_ERR_IOERROR;
			goto done;
		}
		eshort = data->blen > 0;
		break;

	case UREQ(UR_GET_INTERFACE, UT_READ_INTERFACE):
		UPRINTF(LDBG, "(UR_GET_INTERFACE, UT_READ_INTERFACE)\r\n");
		if (index != 0) {
			UPRINTF(LDBG, "get_interface, invalid index %d\r\n",
				index);
			err = USB_ERR_IOERROR;
			goto done;
		}

		if (!data)
			break;

		if (len > 0) {
			*udata = 0;
			data->blen = len - 1;
		}
		eshort = data->blen > 0;
		data->bdone += 1;
		break;

	case UREQ(UR_GET_STATUS, UT_READ_DEVICE):
		UPRINTF(LDBG, "umouse: (UR_GET_STATUS, UT_READ_DEVICE)\r\n");
		if (!data)
			break;

		if (data != NULL && len > 1) {
			if (dev->hid.feature == UF_DEVICE_REMOTE_WAKEUP)
				USETW(udata, UDS_REMOTE_WAKEUP);
			else
				USETW(udata, 0);
			data->blen = len - 2;
			data->bdone += 2;
		}

		eshort = data->blen > 0;
		break;

	case UREQ(UR_GET_STATUS, UT_READ_INTERFACE):
	case UREQ(UR_GET_STATUS, UT_READ_ENDPOINT):
		UPRINTF(LDBG, "(UR_GET_STATUS, UT_READ_INTERFACE)\r\n");
		if (!data)
			break;

		if (data != NULL && len > 1) {
			USETW(udata, 0);
			data->blen = len - 2;
			data->bdone += 2;
		}
		eshort = data->blen > 0;
		break;

	case UREQ(UR_SET_ADDRESS, UT_WRITE_DEVICE):
		/* XXX Controller should've handled this */
		UPRINTF(LDBG, "set address %u\r\n", value);
		break;

	case UREQ(UR_SET_CONFIG, UT_WRITE_DEVICE):
		UPRINTF(LDBG, "set config %u\r\n", value);
		break;

	case UREQ(UR_SET_DESCRIPTOR, UT_WRITE_DEVICE):
		UPRINTF(LDBG, "set descriptor %u\r\n", value);
		break;


	case UREQ(UR_CLEAR_FEATURE, UT_WRITE_DEVICE):
		UPRINTF(LDBG, "(UR_SET_FEATURE,UT_WRITE_DEVICE) %x\r\n",
			 value);
		if (value == UF_DEVICE_REMOTE_WAKEUP)
			dev->hid.feature = 0;
		break;

	case UREQ(UR_SET_FEATURE, UT_WRITE_DEVICE):
		UPRINTF(LDBG, "(UR_SET_FEATURE,UT_WRITE_DEVICE) %x\r\n",
			 value);
		if (value == UF_DEVICE_REMOTE_WAKEUP)
			dev->hid.feature = UF_DEVICE_REMOTE_WAKEUP;
		break;

	case UREQ(UR_CLEAR_FEATURE, UT_WRITE_INTERFACE):
	case UREQ(UR_CLEAR_FEATURE, UT_WRITE_ENDPOINT):
	case UREQ(UR_SET_FEATURE, UT_WRITE_INTERFACE):
	case UREQ(UR_SET_FEATURE, UT_WRITE_ENDPOINT):
		UPRINTF(LDBG, "(UR_CLEAR_FEATURE,UT_WRITE_INTERFACE)\r\n"
			 );
		err = USB_ERR_IOERROR;
		goto done;

	case UREQ(UR_SET_INTERFACE, UT_WRITE_INTERFACE):
		UPRINTF(LDBG, "set interface %u\r\n", value);
		break;

	case UREQ(UR_ISOCH_DELAY, UT_WRITE_DEVICE):
		UPRINTF(LDBG, "set isoch delay %u\r\n", value);
		break;

	case UREQ(UR_SET_SEL, 0):
		UPRINTF(LDBG, "set sel\r\n");
		break;

	case UREQ(UR_SYNCH_FRAME, UT_WRITE_ENDPOINT):
		UPRINTF(LDBG, "synch frame\r\n");
		break;

	/* HID device requests */

	case UREQ(UMOUSE_GET_REPORT, UT_READ_CLASS_INTERFACE):
		UPRINTF(LDBG, "(UMOUSE_GET_REPORT,UT_READ_CLASS_INTERFACE) "
			 "0x%x\r\n", (value >> 8));
		if (!data)
			break;

		if ((value >> 8) == 0x01 && len >= sizeof(dev->um_report)) {
			/* TODO read from backend */

			if (len > sizeof(dev->um_report)) {
				data->blen = len - sizeof(dev->um_report);
				len = sizeof(dev->um_report);
			} else
				data->blen = 0;

			memcpy(data->buf, &dev->um_report, len);
			data->bdone += len;
		} else {
			err = USB_ERR_IOERROR;
			goto done;
		}
		eshort = data->blen > 0;
		break;

	case UREQ(UMOUSE_GET_IDLE, UT_READ_CLASS_INTERFACE):
		if (!data)
			break;

		if (data != NULL && len > 0) {
			*udata = dev->hid.idle;
			data->blen = len - 1;
			data->bdone += 1;
		}
		eshort = data->blen > 0;
		break;

	case UREQ(UMOUSE_GET_PROTOCOL, UT_READ_CLASS_INTERFACE):
		if (!data)
			break;

		if (data != NULL && len > 0) {
			*udata = dev->hid.protocol;
			data->blen = len - 1;
			data->bdone += 1;
		}
		eshort = data->blen > 0;
		break;

	case UREQ(UMOUSE_SET_REPORT, UT_WRITE_CLASS_INTERFACE):
		UPRINTF(LDBG, "(UMOUSE_SET_REPORT,"
			 "UT_WRITE_CLASS_INTERFACE) ignored\r\n"
			 );
		break;

	case UREQ(UMOUSE_SET_IDLE, UT_WRITE_CLASS_INTERFACE):
		dev->hid.idle = xfer->ureq->wValue >> 8;
		UPRINTF(LDBG, "(UMOUSE_SET_IDLE,"
			 "UT_WRITE_CLASS_INTERFACE) %x\r\n",
			dev->hid.idle);
		break;

	case UREQ(UMOUSE_SET_PROTOCOL, UT_WRITE_CLASS_INTERFACE):
		dev->hid.protocol = xfer->ureq->wValue >> 8;
		UPRINTF(LDBG, "(UR_CLEAR_FEATURE,"
			 "UT_WRITE_CLASS_INTERFACE) %x\r\n",
			dev->hid.protocol);
		break;

	default:
		UPRINTF(LDBG, "**** umouse request unhandled\r\n");
		err = USB_ERR_IOERROR;
		break;
	}

done:
	if (xfer->ureq && (xfer->ureq->bmRequestType & UT_WRITE) &&
	    (err == USB_ERR_NORMAL_COMPLETION) && (data != NULL))
		data->blen = 0;
	else if (eshort)
		err = USB_ERR_SHORT_XFER;

	UPRINTF(LDBG, "request error code %d (0=ok), blen %u txlen %u\r\n",
		err, (data ? data->blen : 0), (data ? data->bdone : 0));

	return err;
}

static int
umouse_data_handler(void *scarg, struct usb_data_xfer *xfer, int dir,
		    int epctx)
{
	struct umouse_vdev *dev;
	struct usb_data_xfer_block *data;
	uint8_t *udata;
	int len, i, idx;
	int err;

	assert(xfer != NULL && xfer->head >= 0);

	UPRINTF(LDBG, "handle data - DIR=%s|EP=%d, blen %d\r\n",
		dir ? "IN" : "OUT", epctx, xfer->data[0].blen);

	/* find buffer to add data */
	udata = NULL;
	err = USB_ERR_NORMAL_COMPLETION;

	/* handle xfer at first unprocessed item with buffer */
	data = NULL;
	idx = xfer->head;
	for (i = 0; i < xfer->ndata; i++) {
		data = &xfer->data[idx];
		if (data->buf != NULL && data->blen != 0)
			break;

		data->processed = USB_XFER_BLK_HANDLED;
		data = NULL;
		idx = (idx + 1) % USB_MAX_XFER_BLOCKS;
	}
	if (!data)
		goto done;

	udata = data->buf;
	len = data->blen;

	if (udata == NULL) {
		UPRINTF(LDBG, "no buffer provided for input\r\n");
		err = USB_ERR_NOMEM;
		goto done;
	}

	dev = scarg;

	if (dir) {

		pthread_mutex_lock(&dev->mtx);

		if (!dev->newdata) {
			err = USB_ERR_CANCELLED;
			USB_DATA_SET_ERRCODE(&xfer->data[xfer->head], USB_NAK);
			pthread_mutex_unlock(&dev->mtx);
			goto done;
		}

		if (dev->polling) {
			err = USB_ERR_STALLED;
			USB_DATA_SET_ERRCODE(data, USB_STALL);
			pthread_mutex_unlock(&dev->mtx);
			goto done;
		}
		dev->polling = 1;

		if (len > 0) {
			dev->newdata = 0;

			data->processed = USB_XFER_BLK_HANDLED;
			data->bdone += 6;
			memcpy(udata, &dev->um_report, 6);
			data->blen = len - 6;
			if (data->blen > 0)
				err = USB_ERR_SHORT_XFER;
		}

		dev->polling = 0;
		pthread_mutex_unlock(&dev->mtx);
	} else {
		USB_DATA_SET_ERRCODE(data, USB_STALL);
		err = USB_ERR_STALLED;
	}

done:
	return err;
}

static int
umouse_reset(void *scarg)
{
	struct umouse_vdev *dev;

	dev = scarg;

	dev->newdata = 0;

	return 0;
}

static int
umouse_remove(void *scarg)
{
	return 0;
}

static int
umouse_stop(void *scarg)
{
	return 0;
}


struct usb_devemu ue_mouse = {
	.ue_emu =	"tablet",
	.ue_usbver =	3,
	.ue_usbspeed =	USB_SPEED_HIGH,
	.ue_devtype =	USB_DEV_STATIC,
	.ue_init =	umouse_init,
	.ue_request =	umouse_request,
	.ue_data =	umouse_data_handler,
	.ue_reset =	umouse_reset,
	.ue_remove =	umouse_remove,
	.ue_stop =	umouse_stop
};
USB_EMUL_SET(ue_mouse);
