/*-
 * Copyright (c) 2009 Andrew Thompson
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef _USB_USBDI_H_
#define _USB_USBDI_H_

struct usb_fifo;
struct usb_xfer;
struct usb_device;
struct usb_attach_arg;
struct usb_interface;
struct usb_endpoint;
struct usb_page_cache;
struct usb_page_search;
struct usb_process;
struct usb_proc_msg;
struct usb_mbuf;
struct usb_fs_privdata;
struct mbuf;

typedef enum {	/* keep in sync with usb_errstr_table */
	USB_ERR_NORMAL_COMPLETION = 0,
	USB_ERR_PENDING_REQUESTS,	/* 1 */
	USB_ERR_NOT_STARTED,		/* 2 */
	USB_ERR_INVAL,			/* 3 */
	USB_ERR_NOMEM,			/* 4 */
	USB_ERR_CANCELLED,		/* 5 */
	USB_ERR_BAD_ADDRESS,		/* 6 */
	USB_ERR_BAD_BUFSIZE,		/* 7 */
	USB_ERR_BAD_FLAG,		/* 8 */
	USB_ERR_NO_CALLBACK,		/* 9 */
	USB_ERR_IN_USE,			/* 10 */
	USB_ERR_NO_ADDR,		/* 11 */
	USB_ERR_NO_PIPE,		/* 12 */
	USB_ERR_ZERO_NFRAMES,		/* 13 */
	USB_ERR_ZERO_MAXP,		/* 14 */
	USB_ERR_SET_ADDR_FAILED,	/* 15 */
	USB_ERR_NO_POWER,		/* 16 */
	USB_ERR_TOO_DEEP,		/* 17 */
	USB_ERR_IOERROR,		/* 18 */
	USB_ERR_NOT_CONFIGURED,		/* 19 */
	USB_ERR_TIMEOUT,		/* 20 */
	USB_ERR_SHORT_XFER,		/* 21 */
	USB_ERR_STALLED,		/* 22 */
	USB_ERR_INTERRUPTED,		/* 23 */
	USB_ERR_DMA_LOAD_FAILED,	/* 24 */
	USB_ERR_BAD_CONTEXT,		/* 25 */
	USB_ERR_NO_ROOT_HUB,		/* 26 */
	USB_ERR_NO_INTR_THREAD,		/* 27 */
	USB_ERR_NOT_LOCKED,		/* 28 */
	USB_ERR_MAX
} usb_error_t;

/*
 * Flags for transfers
 */
#define	USB_FORCE_SHORT_XFER	0x0001	/* force a short transmit last */
#define	USB_SHORT_XFER_OK	0x0004	/* allow short reads */
#define	USB_DELAY_STATUS_STAGE	0x0010	/* insert delay before STATUS stage */
#define	USB_USER_DATA_PTR	0x0020	/* internal flag */
#define	USB_MULTI_SHORT_OK	0x0040	/* allow multiple short frames */
#define	USB_MANUAL_STATUS	0x0080	/* manual ctrl status */

#define	USB_NO_TIMEOUT 0
#define	USB_DEFAULT_TIMEOUT 5000	/* 5000 ms = 5 seconds */

#endif /* _USB_USBDI_H_ */
