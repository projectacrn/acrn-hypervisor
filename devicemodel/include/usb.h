/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
 * Copyright (c) 1998 The NetBSD Foundation, Inc. All rights reserved.
 * Copyright (c) 1998 Lennart Augustsson. All rights reserved.
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
 * This file contains standard definitions for the following USB
 * protocol versions:
 *
 * USB v1.0
 * USB v1.1
 * USB v2.0
 * USB v3.0
 */

#ifndef _USB_H_
#define	_USB_H_

#include "types.h"

#define	USB_STACK_VERSION 2000		/* 2.0 */

/* Definition of some hardcoded USB constants. */

#define	USB_MAX_IPACKET		8	/* initial USB packet size */
#define	USB_EP_MAX (2*16)		/* hardcoded */
#define	USB_ROOT_HUB_ADDR 1		/* index */
#define	USB_MIN_DEVICES 2		/* unused + root HUB */
#define	USB_UNCONFIG_INDEX 0xFF		/* internal use only */
#define	USB_IFACE_INDEX_ANY 0xFF	/* internal use only */
#define	USB_START_ADDR 0		/* default USB device BUS address
					 * after USB bus reset
					 */
#define	USB_CONTROL_ENDPOINT 0		/* default control endpoint */

#define	USB_FRAMES_PER_SECOND_FS 1000	/* full speed */
#define	USB_FRAMES_PER_SECOND_HS 8000	/* high speed */

#define	USB_FS_BYTES_PER_HS_UFRAME 188	/* bytes */
#define	USB_HS_MICRO_FRAMES_MAX 8	/* units */

#define	USB_ISOC_TIME_MAX 128		/* ms */

/*
 * Minimum time a device needs to be powered down to go through a
 * power cycle. These values are not in the USB specification.
 */
#define	USB_POWER_DOWN_TIME	200	/* ms */
#define	USB_PORT_POWER_DOWN_TIME	100	/* ms */

/* Definition of software USB power modes */
#define	USB_POWER_MODE_OFF 0		/* turn off device */
#define	USB_POWER_MODE_ON 1		/* always on */
#define	USB_POWER_MODE_SAVE 2		/* automatic suspend and resume */
#define	USB_POWER_MODE_SUSPEND 3	/* force suspend */
#define	USB_POWER_MODE_RESUME 4		/* force resume */

/* These are the values from the USB specification. */
#define	USB_PORT_RESET_DELAY_SPEC	10	/* ms */
#define	USB_PORT_ROOT_RESET_DELAY_SPEC	50	/* ms */
#define	USB_PORT_RESET_RECOVERY_SPEC	10	/* ms */
#define	USB_PORT_POWERUP_DELAY_SPEC	100	/* ms */
#define	USB_PORT_RESUME_DELAY_SPEC	20	/* ms */
#define	USB_SET_ADDRESS_SETTLE_SPEC	2	/* ms */
#define	USB_RESUME_DELAY_SPEC		(20*5)	/* ms */
#define	USB_RESUME_WAIT_SPEC		10	/* ms */
#define	USB_RESUME_RECOVERY_SPEC	10	/* ms */
#define	USB_EXTRA_POWER_UP_TIME_SPEC	0	/* ms */

/* Allow for marginal and non-conforming devices. */
#define	USB_PORT_RESET_DELAY		50	/* ms */
#define	USB_PORT_ROOT_RESET_DELAY	200	/* ms */
#define	USB_PORT_RESET_RECOVERY		250	/* ms */
#define	USB_PORT_POWERUP_DELAY		300	/* ms */
#define	USB_PORT_RESUME_DELAY		(20*2)	/* ms */
#define	USB_SET_ADDRESS_SETTLE		10	/* ms */
#define	USB_RESUME_DELAY		(50*5)	/* ms */
#define	USB_RESUME_WAIT			50	/* ms */
#define	USB_RESUME_RECOVERY		50	/* ms */
#define	USB_EXTRA_POWER_UP_TIME		20	/* ms */

#define	USB_MIN_POWER		100	/* mA */
#define	USB_MAX_POWER		500	/* mA */

#define	USB_BUS_RESET_DELAY	100	/* ms */

#define	UREQ(x, y)	((x) | ((y) << 8))
/*
 * USB record layout in memory:
 *
 * - USB config 0
 *   - USB interfaces
 *     - USB alternative interfaces
 *       - USB endpoints
 *
 * - USB config 1
 *   - USB interfaces
 *     - USB alternative interfaces
 *       - USB endpoints
 */

/* Declaration of USB records */

struct usb_device_request {
	uint8_t		bmRequestType;
	uint8_t		bRequest;
	uint16_t	wValue;
	uint16_t	wIndex;
	uint16_t	wLength;
} __attribute__((packed));
typedef struct usb_device_request usb_device_request_t;

#define	UT_WRITE		0x00
#define	UT_READ			0x80
#define	UT_STANDARD		0x00
#define	UT_CLASS		0x20
#define	UT_VENDOR		0x40
#define	UT_DEVICE		0x00
#define	UT_INTERFACE		0x01
#define	UT_ENDPOINT		0x02
#define	UT_OTHER		0x03

#define	UT_READ_DEVICE		(UT_READ  | UT_STANDARD | UT_DEVICE)
#define	UT_READ_INTERFACE	(UT_READ  | UT_STANDARD | UT_INTERFACE)
#define	UT_READ_ENDPOINT	(UT_READ  | UT_STANDARD | UT_ENDPOINT)
#define	UT_WRITE_DEVICE		(UT_WRITE | UT_STANDARD | UT_DEVICE)
#define	UT_WRITE_INTERFACE	(UT_WRITE | UT_STANDARD | UT_INTERFACE)
#define	UT_WRITE_ENDPOINT	(UT_WRITE | UT_STANDARD | UT_ENDPOINT)
#define	UT_READ_CLASS_DEVICE	(UT_READ  | UT_CLASS | UT_DEVICE)
#define	UT_READ_CLASS_INTERFACE	(UT_READ  | UT_CLASS | UT_INTERFACE)
#define	UT_READ_CLASS_OTHER	(UT_READ  | UT_CLASS | UT_OTHER)
#define	UT_READ_CLASS_ENDPOINT	(UT_READ  | UT_CLASS | UT_ENDPOINT)
#define	UT_WRITE_CLASS_DEVICE	(UT_WRITE | UT_CLASS | UT_DEVICE)
#define	UT_WRITE_CLASS_INTERFACE (UT_WRITE | UT_CLASS | UT_INTERFACE)
#define	UT_WRITE_CLASS_OTHER	(UT_WRITE | UT_CLASS | UT_OTHER)
#define	UT_WRITE_CLASS_ENDPOINT	(UT_WRITE | UT_CLASS | UT_ENDPOINT)
#define	UT_READ_VENDOR_DEVICE	(UT_READ  | UT_VENDOR | UT_DEVICE)
#define	UT_READ_VENDOR_INTERFACE (UT_READ  | UT_VENDOR | UT_INTERFACE)
#define	UT_READ_VENDOR_OTHER	(UT_READ  | UT_VENDOR | UT_OTHER)
#define	UT_READ_VENDOR_ENDPOINT	(UT_READ  | UT_VENDOR | UT_ENDPOINT)
#define	UT_WRITE_VENDOR_DEVICE	(UT_WRITE | UT_VENDOR | UT_DEVICE)
#define	UT_WRITE_VENDOR_INTERFACE (UT_WRITE | UT_VENDOR | UT_INTERFACE)
#define	UT_WRITE_VENDOR_OTHER	(UT_WRITE | UT_VENDOR | UT_OTHER)
#define	UT_WRITE_VENDOR_ENDPOINT (UT_WRITE | UT_VENDOR | UT_ENDPOINT)

/* Requests */
#define	UR_GET_STATUS		0x00
#define	UR_CLEAR_FEATURE	0x01
#define	UR_SET_FEATURE		0x03
#define	UR_SET_ADDRESS		0x05
#define	UR_GET_DESCRIPTOR	0x06
#define	UDESC_DEVICE		0x01
#define	UDESC_CONFIG		0x02
#define	UDESC_STRING		0x03
#define	USB_LANGUAGE_TABLE	0x00	/* language ID string index */
#define	UDESC_INTERFACE		0x04
#define	UDESC_ENDPOINT		0x05
#define	UDESC_DEVICE_QUALIFIER	0x06
#define	UDESC_OTHER_SPEED_CONFIGURATION 0x07
#define	UDESC_INTERFACE_POWER	0x08
#define	UDESC_OTG		0x09
#define	UDESC_DEBUG		0x0A
#define	UDESC_IFACE_ASSOC	0x0B	/* interface association */
#define	UDESC_BOS		0x0F	/* binary object store */
#define	UDESC_DEVICE_CAPABILITY	0x10
#define	UDESC_CS_DEVICE		0x21	/* class specific */
#define	UDESC_CS_CONFIG		0x22
#define	UDESC_CS_STRING		0x23
#define	UDESC_CS_INTERFACE	0x24
#define	UDESC_CS_ENDPOINT	0x25
#define	UDESC_HUB		0x29
#define	UDESC_SS_HUB		0x2A	/* super speed */
#define	UDESC_ENDPOINT_SS_COMP	0x30	/* super speed */
#define	UR_SET_DESCRIPTOR	0x07
#define	UR_GET_CONFIG		0x08
#define	UR_SET_CONFIG		0x09
#define	UR_GET_INTERFACE	0x0a
#define	UR_SET_INTERFACE	0x0b
#define	UR_SYNCH_FRAME		0x0c
#define	UR_SET_SEL		0x30
#define	UR_ISOCH_DELAY		0x31

/* HUB specific request */
#define	UR_GET_BUS_STATE	0x02
#define	UR_CLEAR_TT_BUFFER	0x08
#define	UR_RESET_TT		0x09
#define	UR_GET_TT_STATE		0x0a
#define	UR_STOP_TT		0x0b
#define	UR_SET_AND_TEST		0x0c	/* USB 2.0 only */
#define	UR_SET_HUB_DEPTH	0x0c	/* USB 3.0 only */
#define	USB_SS_HUB_DEPTH_MAX	5
#define	UR_GET_PORT_ERR_COUNT	0x0d

/* Feature numbers */
#define	UF_ENDPOINT_HALT	0
#define	UF_DEVICE_REMOTE_WAKEUP	1
#define	UF_TEST_MODE		2
#define	UF_U1_ENABLE		0x30
#define	UF_U2_ENABLE		0x31
#define	UF_LTM_ENABLE		0x32

/* HUB specific features */
#define	UHF_C_HUB_LOCAL_POWER	0
#define	UHF_C_HUB_OVER_CURRENT	1
#define	UHF_PORT_CONNECTION	0
#define	UHF_PORT_ENABLE		1
#define	UHF_PORT_SUSPEND	2
#define	UHF_PORT_OVER_CURRENT	3
#define	UHF_PORT_RESET		4
#define	UHF_PORT_LINK_STATE	5
#define	UHF_PORT_POWER		8
#define	UHF_PORT_LOW_SPEED	9
#define	UHF_PORT_L1		10
#define	UHF_C_PORT_CONNECTION	16
#define	UHF_C_PORT_ENABLE	17
#define	UHF_C_PORT_SUSPEND	18
#define	UHF_C_PORT_OVER_CURRENT	19
#define	UHF_C_PORT_RESET	20
#define	UHF_PORT_TEST		21
#define	UHF_PORT_INDICATOR	22
#define	UHF_C_PORT_L1		23

/* SuperSpeed HUB specific features */
#define	UHF_PORT_U1_TIMEOUT	23
#define	UHF_PORT_U2_TIMEOUT	24
#define	UHF_C_PORT_LINK_STATE	25
#define	UHF_C_PORT_CONFIG_ERROR	26
#define	UHF_PORT_REMOTE_WAKE_MASK	27
#define	UHF_BH_PORT_RESET	28
#define	UHF_C_BH_PORT_RESET	29
#define	UHF_FORCE_LINKPM_ACCEPT	30

struct usb_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint8_t	bDescriptorSubtype;
} __attribute__((packed));
typedef struct usb_descriptor usb_descriptor_t;

struct usb_device_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint16_t	bcdUSB;
#define	UD_USB_2_0		0x0200
#define	UD_USB_3_0		0x0300
#define	UD_IS_USB2(d) ((d)->bcdUSB[1] == 0x02)
#define	UD_IS_USB3(d) ((d)->bcdUSB[1] == 0x03)
	uint8_t	bDeviceClass;
	uint8_t	bDeviceSubClass;
	uint8_t	bDeviceProtocol;
	uint8_t	bMaxPacketSize;
	/* The fields below are not part of the initial descriptor. */
	uint16_t	idVendor;
	uint16_t	idProduct;
	uint16_t	bcdDevice;
	uint8_t	iManufacturer;
	uint8_t	iProduct;
	uint8_t	iSerialNumber;
	uint8_t	bNumConfigurations;
} __attribute__((packed));
typedef struct usb_device_descriptor usb_device_descriptor_t;

/* Binary Device Object Store (BOS) */
struct usb_bos_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint16_t	wTotalLength;
	uint8_t	bNumDeviceCaps;
} __attribute__((packed));
typedef struct usb_bos_descriptor usb_bos_descriptor_t;

/* Binary Device Object Store Capability */
struct usb_bos_cap_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint8_t	bDevCapabilityType;
#define	USB_DEVCAP_RESERVED	0x00
#define	USB_DEVCAP_WUSB		0x01
#define	USB_DEVCAP_USB2EXT	0x02
#define	USB_DEVCAP_SUPER_SPEED	0x03
#define	USB_DEVCAP_CONTAINER_ID	0x04
	/* data ... */
} __attribute__((packed));
typedef struct usb_bos_cap_descriptor usb_bos_cap_descriptor_t;

struct usb_devcap_usb2ext_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint8_t	bDevCapabilityType;
	uint32_t bmAttributes;
#define	USB_V2EXT_LPM (1U << 1)
#define	USB_V2EXT_BESL_SUPPORTED (1U << 2)
#define	USB_V2EXT_BESL_BASELINE_VALID (1U << 3)
#define	USB_V2EXT_BESL_DEEP_VALID (1U << 4)
#define	USB_V2EXT_BESL_BASELINE_GET(x) (((x) >> 8) & 0xF)
#define	USB_V2EXT_BESL_DEEP_GET(x) (((x) >> 12) & 0xF)
} __attribute__((packed));
typedef struct usb_devcap_usb2ext_descriptor usb_devcap_usb2ext_descriptor_t;

struct usb_devcap_ss_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint8_t	bDevCapabilityType;
	uint8_t	bmAttributes;
	uint16_t	wSpeedsSupported;
	uint8_t	bFunctionalitySupport;
	uint8_t	bU1DevExitLat;
	uint16_t	wU2DevExitLat;
} __attribute__((packed));
typedef struct usb_devcap_ss_descriptor usb_devcap_ss_descriptor_t;

struct usb_devcap_container_id_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint8_t	bDevCapabilityType;
	uint8_t	bReserved;
	uint8_t	bContainerID;
} __attribute__((packed));
typedef struct usb_devcap_container_id_descriptor
		usb_devcap_container_id_descriptor_t;

/* Device class codes */
#define	UDCLASS_IN_INTERFACE	0x00
#define	UDCLASS_COMM		0x02
#define	UDCLASS_HUB		0x09
#define	UDSUBCLASS_HUB		0x00
#define	UDPROTO_FSHUB		0x00
#define	UDPROTO_HSHUBSTT	0x01
#define	UDPROTO_HSHUBMTT	0x02
#define	UDPROTO_SSHUB		0x03
#define	UDCLASS_DIAGNOSTIC	0xdc
#define	UDCLASS_WIRELESS	0xe0
#define	UDSUBCLASS_RF		0x01
#define	UDPROTO_BLUETOOTH	0x01
#define	UDCLASS_VENDOR		0xff

struct usb_config_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint16_t	wTotalLength;
	uint8_t	bNumInterface;
	uint8_t	bConfigurationValue;
#define	USB_UNCONFIG_NO 0
	uint8_t	iConfiguration;
	uint8_t	bmAttributes;
#define	UC_BUS_POWERED		0x80
#define	UC_SELF_POWERED		0x40
#define	UC_REMOTE_WAKEUP	0x20
	uint8_t	bMaxPower;		/* max current in 2 mA units */
#define	UC_POWER_FACTOR 2
} __attribute__((packed));
typedef struct usb_config_descriptor usb_config_descriptor_t;

struct usb_interface_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint8_t	bInterfaceNumber;
	uint8_t	bAlternateSetting;
	uint8_t	bNumEndpoints;
	uint8_t	bInterfaceClass;
	uint8_t	bInterfaceSubClass;
	uint8_t	bInterfaceProtocol;
	uint8_t	iInterface;
} __attribute__((packed));
typedef struct usb_interface_descriptor usb_interface_descriptor_t;

struct usb_interface_assoc_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint8_t	bFirstInterface;
	uint8_t	bInterfaceCount;
	uint8_t	bFunctionClass;
	uint8_t	bFunctionSubClass;
	uint8_t	bFunctionProtocol;
	uint8_t	iFunction;
} __attribute__((packed));
typedef struct usb_interface_assoc_descriptor usb_interface_assoc_descriptor_t;

/* Interface class codes */
#define	UICLASS_UNSPEC		0x00
#define	UICLASS_AUDIO		0x01	/* audio */
#define	UISUBCLASS_AUDIOCONTROL	1
#define	UISUBCLASS_AUDIOSTREAM		2
#define	UISUBCLASS_MIDISTREAM		3

#define	UICLASS_CDC		0x02	/* communication */
#define	UISUBCLASS_DIRECT_LINE_CONTROL_MODEL	1
#define	UISUBCLASS_ABSTRACT_CONTROL_MODEL	2
#define	UISUBCLASS_TELEPHONE_CONTROL_MODEL	3
#define	UISUBCLASS_MULTICHANNEL_CONTROL_MODEL	4
#define	UISUBCLASS_CAPI_CONTROLMODEL		5
#define	UISUBCLASS_ETHERNET_NETWORKING_CONTROL_MODEL 6
#define	UISUBCLASS_ATM_NETWORKING_CONTROL_MODEL 7
#define	UISUBCLASS_WIRELESS_HANDSET_CM 8
#define	UISUBCLASS_DEVICE_MGMT 9
#define	UISUBCLASS_MOBILE_DIRECT_LINE_MODEL 10
#define	UISUBCLASS_OBEX 11
#define	UISUBCLASS_ETHERNET_EMULATION_MODEL 12
#define	UISUBCLASS_NETWORK_CONTROL_MODEL 13

#define	UIPROTO_CDC_NONE		0
#define	UIPROTO_CDC_AT			1

#define	UICLASS_HID		0x03
#define	UISUBCLASS_BOOT		1
#define	UIPROTO_BOOT_KEYBOARD	1
#define	UIPROTO_MOUSE		2

#define	UICLASS_PHYSICAL	0x05
#define	UICLASS_IMAGE		0x06
#define	UISUBCLASS_SIC		1	/* still image class */
#define	UICLASS_PRINTER		0x07
#define	UISUBCLASS_PRINTER	1
#define	UIPROTO_PRINTER_UNI	1
#define	UIPROTO_PRINTER_BI	2
#define	UIPROTO_PRINTER_1284	3

#define	UICLASS_MASS		0x08
#define	UISUBCLASS_RBC		1
#define	UISUBCLASS_SFF8020I	2
#define	UISUBCLASS_QIC157	3
#define	UISUBCLASS_UFI		4
#define	UISUBCLASS_SFF8070I	5
#define	UISUBCLASS_SCSI		6
#define	UIPROTO_MASS_CBI_I	0
#define	UIPROTO_MASS_CBI	1
#define	UIPROTO_MASS_BBB_OLD	2	/* Not in the spec anymore */
#define	UIPROTO_MASS_BBB	80	/* 'P' for the Iomega Zip drive */

#define	UICLASS_HUB		0x09
#define	UISUBCLASS_HUB		0
#define	UIPROTO_FSHUB		0
#define	UIPROTO_HSHUBSTT	0	/* Yes, same as previous */
#define	UIPROTO_HSHUBMTT	1

#define	UICLASS_CDC_DATA	0x0a
#define	UISUBCLASS_DATA		0x00
#define	UIPROTO_DATA_ISDNBRI		0x30	/* Physical iface */
#define	UIPROTO_DATA_HDLC		0x31	/* HDLC */
#define	UIPROTO_DATA_TRANSPARENT	0x32	/* Transparent */
#define	UIPROTO_DATA_Q921M		0x50	/* Management for Q921 */
#define	UIPROTO_DATA_Q921		0x51	/* Data for Q921 */
#define	UIPROTO_DATA_Q921TM		0x52	/* TEI multiplexer for Q921 */
#define	UIPROTO_DATA_V42BIS		0x90	/* Data compression */
#define	UIPROTO_DATA_Q931		0x91	/* Euro-ISDN */
#define	UIPROTO_DATA_V120		0x92	/* V.24 rate adaption */
#define	UIPROTO_DATA_CAPI		0x93	/* CAPI 2.0 commands */
#define	UIPROTO_DATA_HOST_BASED		0xfd	/* Host based driver */
#define	UIPROTO_DATA_PUF		0xfe	/* see Prot. Unit Func. Desc. */
#define	UIPROTO_DATA_VENDOR		0xff	/* Vendor specific */
#define	UIPROTO_DATA_NCM		0x01	/* Network Control Model */

#define	UICLASS_SMARTCARD	0x0b
#define	UICLASS_FIRM_UPD	0x0c
#define	UICLASS_SECURITY	0x0d
#define	UICLASS_DIAGNOSTIC	0xdc
#define	UICLASS_WIRELESS	0xe0
#define	UISUBCLASS_RF			0x01
#define	UIPROTO_BLUETOOTH		0x01
#define	UIPROTO_RNDIS			0x03

#define	UICLASS_IAD		0xEF	/* Interface Association Descriptor */
#define	UISUBCLASS_SYNC			0x01
#define	UIPROTO_ACTIVESYNC		0x01

#define	UICLASS_APPL_SPEC	0xfe
#define	UISUBCLASS_FIRMWARE_DOWNLOAD	1
#define	UISUBCLASS_IRDA			2
#define	UIPROTO_IRDA			0

#define	UICLASS_VENDOR		0xff
#define	UISUBCLASS_XBOX360_CONTROLLER	0x5d
#define	UIPROTO_XBOX360_GAMEPAD	0x01

struct usb_endpoint_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint8_t	bEndpointAddress;
#define	UE_GET_DIR(a)	((a) & 0x80)
#define	UE_SET_DIR(a, d)	((a) | (((d)&1) << 7))
#define	UE_DIR_IN	0x80		/* IN-token endpoint, fixed */
#define	UE_DIR_OUT	0x00		/* OUT-token endpoint, fixed */
#define	UE_DIR_RX	0xfd		/* for internal use only! */
#define	UE_DIR_TX	0xfe		/* for internal use only! */
#define	UE_DIR_ANY	0xff		/* for internal use only! */
#define	UE_ADDR		0x0f
#define	UE_ADDR_ANY	0xff		/* for internal use only! */
#define	UE_GET_ADDR(a)	((a) & UE_ADDR)
	uint8_t	bmAttributes;
#define	UE_XFERTYPE	0x03
#define	UE_CONTROL	0x00
#define	UE_ISOCHRONOUS	0x01
#define	UE_BULK	0x02
#define	UE_INTERRUPT	0x03
#define	UE_BULK_INTR	0xfe		/* for internal use only! */
#define	UE_TYPE_ANY	0xff		/* for internal use only! */
#define	UE_GET_XFERTYPE(a)	((a) & UE_XFERTYPE)
#define	UE_ISO_TYPE	0x0c
#define	UE_ISO_ASYNC	0x04
#define	UE_ISO_ADAPT	0x08
#define	UE_ISO_SYNC	0x0c
#define	UE_GET_ISO_TYPE(a)	((a) & UE_ISO_TYPE)
#define	UE_ISO_USAGE	0x30
#define	UE_ISO_USAGE_DATA	0x00
#define	UE_ISO_USAGE_FEEDBACK	0x10
#define	UE_ISO_USAGE_IMPLICT_FB	0x20
#define	UE_GET_ISO_USAGE(a)	((a) & UE_ISO_USAGE)
	uint16_t	wMaxPacketSize;
#define	UE_ZERO_MPS 0xFFFF		/* for internal use only */
	uint8_t	bInterval;
} __attribute__((packed));
typedef struct usb_endpoint_descriptor usb_endpoint_descriptor_t;

struct usb_endpoint_ss_comp_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint8_t	bMaxBurst;
	uint8_t	bmAttributes;
#define	UE_GET_BULK_STREAMS(x) ((x) & 0x0F)
#define	UE_GET_SS_ISO_MULT(x) ((x) & 0x03)
	uint16_t	wBytesPerInterval;
} __attribute__((packed));
typedef struct usb_endpoint_ss_comp_descriptor
		usb_endpoint_ss_comp_descriptor_t;

struct usb_string_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint16_t	bString[126];
	uint8_t	bUnused;
} __attribute__((packed));
typedef struct usb_string_descriptor usb_string_descriptor_t;

#define	USB_MAKE_STRING_DESC(m, name)	\
static const struct {			\
	uint8_t bLength;			\
	uint8_t bDescriptorType;		\
	uint8_t bData[sizeof((uint8_t []){m})];	\
} __attribute__((packed)) name = {			\
	.bLength = sizeof(name),		\
	.bDescriptorType = UDESC_STRING,	\
	.bData = { m },			\
}

struct usb_string_lang {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bData[2];
} __attribute__((packed));
typedef struct usb_string_lang usb_string_lang_t;

struct usb_hub_descriptor {
	uint8_t	bDescLength;
	uint8_t	bDescriptorType;
	uint8_t	bNbrPorts;
	uint16_t	wHubCharacteristics;
#define	UHD_PWR			0x0003
#define	UHD_PWR_GANGED		0x0000
#define	UHD_PWR_INDIVIDUAL	0x0001
#define	UHD_PWR_NO_SWITCH	0x0002
#define	UHD_COMPOUND		0x0004
#define	UHD_OC			0x0018
#define	UHD_OC_GLOBAL		0x0000
#define	UHD_OC_INDIVIDUAL	0x0008
#define	UHD_OC_NONE		0x0010
#define	UHD_TT_THINK		0x0060
#define	UHD_TT_THINK_8		0x0000
#define	UHD_TT_THINK_16		0x0020
#define	UHD_TT_THINK_24		0x0040
#define	UHD_TT_THINK_32		0x0060
#define	UHD_PORT_IND		0x0080
	uint8_t	bPwrOn2PwrGood;		/* delay in 2 ms units */
#define	UHD_PWRON_FACTOR 2
	uint8_t	bHubContrCurrent;
	uint8_t	DeviceRemovable[32];	/* max 255 ports */
#define	UHD_NOT_REMOV(desc, i) \
	(((desc)->DeviceRemovable[(i)/8] >> ((i) % 8)) & 1)
	uint8_t	PortPowerCtrlMask[1];	/* deprecated */
} __attribute__((packed));
typedef struct usb_hub_descriptor usb_hub_descriptor_t;

struct usb_hub_ss_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint8_t	bNbrPorts;
	uint16_t	wHubCharacteristics;
	uint8_t	bPwrOn2PwrGood;		/* delay in 2 ms units */
	uint8_t	bHubContrCurrent;
	uint8_t	bHubHdrDecLat;
	uint16_t	wHubDelay;
	uint8_t	DeviceRemovable[32];	/* max 255 ports */
} __attribute__((packed));
typedef struct usb_hub_ss_descriptor usb_hub_ss_descriptor_t;

/* minimum HUB descriptor (8-ports maximum) */
struct usb_hub_descriptor_min {
	uint8_t	bDescLength;
	uint8_t	bDescriptorType;
	uint8_t	bNbrPorts;
	uint16_t	wHubCharacteristics;
	uint8_t	bPwrOn2PwrGood;
	uint8_t	bHubContrCurrent;
	uint8_t	DeviceRemovable[1];
	uint8_t	PortPowerCtrlMask[1];
} __attribute__((packed));
typedef struct usb_hub_descriptor_min usb_hub_descriptor_min_t;

struct usb_device_qualifier {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint16_t	bcdUSB;
	uint8_t	bDeviceClass;
	uint8_t	bDeviceSubClass;
	uint8_t	bDeviceProtocol;
	uint8_t	bMaxPacketSize0;
	uint8_t	bNumConfigurations;
	uint8_t	bReserved;
} __attribute__((packed));
typedef struct usb_device_qualifier usb_device_qualifier_t;

struct usb_otg_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint8_t	bmAttributes;
#define	UOTG_SRP	0x01
#define	UOTG_HNP	0x02
} __attribute__((packed));
typedef struct usb_otg_descriptor usb_otg_descriptor_t;

/* OTG feature selectors */
#define	UOTG_B_HNP_ENABLE	3
#define	UOTG_A_HNP_SUPPORT	4
#define	UOTG_A_ALT_HNP_SUPPORT	5

struct usb_status {
	uint16_t	wStatus;
/* Device status flags */
#define	UDS_SELF_POWERED		0x0001
#define	UDS_REMOTE_WAKEUP		0x0002
/* Endpoint status flags */
#define	UES_HALT			0x0001
} __attribute__((packed));
typedef struct usb_status usb_status_t;

struct usb_hub_status {
	uint16_t	wHubStatus;
#define	UHS_LOCAL_POWER			0x0001
#define	UHS_OVER_CURRENT		0x0002
	uint16_t	wHubChange;
} __attribute__((packed));
typedef struct usb_hub_status usb_hub_status_t;

struct usb_port_status {
	uint16_t	wPortStatus;
#define	UPS_CURRENT_CONNECT_STATUS	0x0001
#define	UPS_PORT_ENABLED		0x0002
#define	UPS_SUSPEND			0x0004
#define	UPS_OVERCURRENT_INDICATOR	0x0008
#define	UPS_RESET			0x0010
#define	UPS_PORT_L1			0x0020	/* USB 2.0 only */
/* The link-state bits are valid for Super-Speed USB HUBs */
#define	UPS_PORT_LINK_STATE_GET(x)	(((x) >> 5) & 0xF)
#define	UPS_PORT_LINK_STATE_SET(x)	(((x) & 0xF) << 5)
#define	UPS_PORT_LS_U0		0x00
#define	UPS_PORT_LS_U1		0x01
#define	UPS_PORT_LS_U2		0x02
#define	UPS_PORT_LS_U3		0x03
#define	UPS_PORT_LS_SS_DIS	0x04
#define	UPS_PORT_LS_RX_DET	0x05
#define	UPS_PORT_LS_SS_INA	0x06
#define	UPS_PORT_LS_POLL	0x07
#define	UPS_PORT_LS_RECOVER	0x08
#define	UPS_PORT_LS_HOT_RST	0x09
#define	UPS_PORT_LS_COMP_MODE	0x0A
#define	UPS_PORT_LS_LOOPBACK	0x0B
#define	UPS_PORT_LS_RESUME	0x0F
#define	UPS_PORT_POWER			0x0100
#define	UPS_PORT_POWER_SS		0x0200	/* super-speed only */
#define	UPS_LOW_SPEED			0x0200
#define	UPS_HIGH_SPEED			0x0400
#define	UPS_OTHER_SPEED			0x0600	/* currently FreeBSD specific */
#define	UPS_PORT_TEST			0x0800
#define	UPS_PORT_INDICATOR		0x1000
#define	UPS_PORT_MODE_DEVICE		0x8000	/* currently FreeBSD specific */
	uint16_t	wPortChange;
#define	UPS_C_CONNECT_STATUS		0x0001
#define	UPS_C_PORT_ENABLED		0x0002
#define	UPS_C_SUSPEND			0x0004
#define	UPS_C_OVERCURRENT_INDICATOR	0x0008
#define	UPS_C_PORT_RESET		0x0010
#define	UPS_C_PORT_L1			0x0020	/* USB 2.0 only */
#define	UPS_C_BH_PORT_RESET		0x0020	/* USB 3.0 only */
#define	UPS_C_PORT_LINK_STATE		0x0040
#define	UPS_C_PORT_CONFIG_ERROR		0x0080
} __attribute__((packed));
typedef struct usb_port_status usb_port_status_t;

/*
 * The "USB_SPEED" macros defines all the supported USB speeds.
 */
enum usb_dev_speed {
	USB_SPEED_VARIABLE,
	USB_SPEED_LOW,
	USB_SPEED_FULL,
	USB_SPEED_HIGH,
	USB_SPEED_SUPER,
};
#define	USB_SPEED_MAX	(USB_SPEED_SUPER+1)

/*
 * The "USB_REV" macros defines all the supported USB revisions.
 */
enum usb_revision {
	USB_REV_UNKNOWN,
	USB_REV_PRE_1_0,
	USB_REV_1_0,
	USB_REV_1_1,
	USB_REV_2_0,
	USB_REV_2_5,
	USB_REV_3_0
};
#define	USB_REV_MAX	(USB_REV_3_0+1)

/*
 * Supported host controller modes.
 */
enum usb_hc_mode {
	USB_MODE_HOST,		/* initiates transfers */
	USB_MODE_DEVICE,	/* bus transfer target */
	USB_MODE_DUAL		/* can be host or device */
};
#define	USB_MODE_MAX	(USB_MODE_DUAL+1)

/*
 * The "USB_STATE" enums define all the supported device states.
 */
enum usb_dev_state {
	USB_STATE_DETACHED,
	USB_STATE_ATTACHED,
	USB_STATE_POWERED,
	USB_STATE_ADDRESSED,
	USB_STATE_CONFIGURED,
};
#define	USB_STATE_MAX	(USB_STATE_CONFIGURED+1)

/*
 * The "USB_EP_MODE" macros define all the currently supported
 * endpoint modes.
 */
enum usb_ep_mode {
	USB_EP_MODE_DEFAULT,
	USB_EP_MODE_STREAMS,	/* USB3.0 specific */
	USB_EP_MODE_HW_MASS_STORAGE,
	USB_EP_MODE_HW_SERIAL,
	USB_EP_MODE_HW_ETHERNET_CDC,
	USB_EP_MODE_HW_ETHERNET_NCM,
	USB_EP_MODE_MAX
};

/* Default USB configuration */
#define	USB_HAVE_UGEN 1
#define	USB_HAVE_DEVCTL 1
#define	USB_HAVE_BUSDMA 1
#define	USB_HAVE_COMPAT_LINUX 1
#define	USB_HAVE_USER_IO 1
#define	USB_HAVE_MBUF 1
#define	USB_HAVE_TT_SUPPORT 1
#define	USB_HAVE_POWERD 1
#define	USB_HAVE_MSCTEST 1
#define	USB_HAVE_MSCTEST_DETACH 1
#define	USB_HAVE_PF 1
#define	USB_HAVE_ROOT_MOUNT_HOLD 1
#define	USB_HAVE_ID_SECTION 1
#define	USB_HAVE_PER_BUS_PROCESS 1
#define	USB_HAVE_FIXED_ENDPOINT 0
#define	USB_HAVE_FIXED_IFACE 0
#define	USB_HAVE_FIXED_CONFIG 0
#define	USB_HAVE_FIXED_PORT 0
#define	USB_HAVE_DISABLE_ENUM 1

/* define zero ticks callout value */
#define	USB_CALLOUT_ZERO_TICKS 1


#if (!defined(USB_HOST_ALIGN)) || (USB_HOST_ALIGN <= 0)
/* Use default value. */
#undef USB_HOST_ALIGN
#if defined(__arm__) || defined(__mips__) || defined(__powerpc__)
/* Arm and MIPS need at least this much, if not more */
#define USB_HOST_ALIGN	32
#else
#define	USB_HOST_ALIGN    8	/* bytes, must be power of two */
#endif
#endif
/* Sanity check for USB_HOST_ALIGN: Verify power of two. */
#if ((-USB_HOST_ALIGN) & USB_HOST_ALIGN) != USB_HOST_ALIGN
#error "USB_HOST_ALIGN is not power of two."
#endif
#define	USB_FS_ISOC_UFRAME_MAX 4	/* exclusive unit */
#define	USB_BUS_MAX 256			/* units */
#define	USB_MAX_DEVICES 128		/* units */
#define	USB_CONFIG_MAX 65535		/* bytes */
#define	USB_IFACE_MAX 32		/* units */
#define	USB_FIFO_MAX 128		/* units */
#define	USB_MAX_EP_STREAMS 8		/* units */
#define	USB_MAX_EP_UNITS 32		/* units */
#define	USB_MAX_PORTS 255		/* units */

#define	USB_MAX_FS_ISOC_FRAMES_PER_XFER (120)	/* units */
#define	USB_MAX_HS_ISOC_FRAMES_PER_XFER (8*120)	/* units */

#define	USB_HUB_MAX_DEPTH	5
#define	USB_EP0_BUFSIZE		1024	/* bytes */
#define	USB_CS_RESET_LIMIT	20	/* failures = 20 * 50 ms = 1sec */

#define	USB_MAX_AUTO_QUIRK	8	/* maximum number of dynamic quirks */

#define	USB_IN_POLLING_MODE_FUNC() usbd_in_polling_mode()
#define	USB_IN_POLLING_MODE_VALUE() (SCHEDULER_STOPPED() || kdb_active)

typedef uint32_t usb_timeout_t;		/* milliseconds */
typedef uint32_t usb_frlength_t;	/* bytes */
typedef uint32_t usb_frcount_t;		/* units */
typedef uint32_t usb_size_t;		/* bytes */
typedef uint32_t usb_ticks_t;		/* system defined */
typedef uint16_t usb_power_mask_t;	/* see "USB_HW_POWER_XXX" */
typedef uint16_t usb_stream_t;		/* stream ID */

#endif	/* _USB__H_ */
