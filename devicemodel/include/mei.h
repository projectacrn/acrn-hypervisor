/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#ifndef _MEI_HBM_H_
#define _MEI_HBM_H_
#include <linux/uuid.h>
#include <linux/mei.h>

#ifndef guid_t
#define guid_t uuid_le
#endif

/*
 * Timeouts in Seconds
 */
#define MEI_HW_READY_TIMEOUT        2  /* Timeout on ready message */
#define MEI_CONNECT_TIMEOUT         3  /* HPS: at least 2 seconds */

#define MEI_CL_CONNECT_TIMEOUT     15  /* HPS: Client Connect Timeout */
#define MEI_CLIENTS_INIT_TIMEOUT   15  /* HPS: Clients Enumeration Timeout */

#define MEI_PGI_TIMEOUT             1  /* PG Isolation time response 1 sec */
#define MEI_D0I3_TIMEOUT            5  /* D0i3 set/unset max response time */
#define MEI_HBM_TIMEOUT             1  /* 1 second */

/*
 * MEI Version
 */
#define MEI_HBM_MINOR_VERSION                   0
#define MEI_HBM_MAJOR_VERSION                   2

/*
 * MEI version with PGI support
 */
#define MEI_HBM_MINOR_VERSION_PGI               1
#define MEI_HBM_MAJOR_VERSION_PGI               1

/*
 * MEI version with Dynamic clients support
 */
#define MEI_HBM_MINOR_VERSION_DC               0
#define MEI_HBM_MAJOR_VERSION_DC               2

/*
 * MEI version with immediate reply to enum request support
 */
#define MEI_HBM_MINOR_VERSION_IE               0
#define MEI_HBM_MAJOR_VERSION_IE               2

/*
 * MEI version with disconnect on connection timeout support
 */
#define MEI_HBM_MINOR_VERSION_DOT              0
#define MEI_HBM_MAJOR_VERSION_DOT              2

/*
 * MEI version with notification support
 */
#define MEI_HBM_MINOR_VERSION_EV               0
#define MEI_HBM_MAJOR_VERSION_EV               2

/*
 * MEI version with fixed address client support
 */
#define MEI_HBM_MINOR_VERSION_FA               0
#define MEI_HBM_MAJOR_VERSION_FA               2

/*
 * MEI version with OS ver message support
 */
#define MEI_HBM_MINOR_VERSION_OS              0
#define MEI_HBM_MAJOR_VERSION_OS              2

/*
 * MEI version with dma ring support
 */
#define MEI_HBM_MINOR_VERSION_DR              1
#define HMEI_BM_MAJOR_VERSION_DR              2

/* Host bus message command opcode */
#define MEI_HBM_CMD_OP_MSK                 0x7f
/* Host bus message command RESPONSE */
#define MEI_HBM_CMD_RES_MSK                0x80

#define MEI_HBM_HOST_START                 0x01
#define MEI_HBM_HOST_START_RES             0x81

#define MEI_HBM_HOST_STOP                  0x02
#define MEI_HBM_HOST_STO_RES               0x82

#define MEI_HBM_ME_STOP                    0x03

#define MEI_HBM_HOST_ENUM                  0x04
#define MEI_HBM_HOST_ENUM_RES              0x84

#define MEI_HBM_HOST_CLIENT_PROP           0x05
#define MEI_HBM_HOST_CLIENT_PROP_RES       0x85

#define MEI_HBM_CLIENT_CONNECT             0x06
#define MEI_HBM_CLIENT_CONNECT_RES         0x86

#define MEI_HBM_CLIENT_DISCONNECT          0x07
#define MEI_HBM_CLIENT_DISCONNECT_RES      0x87

#define MEI_HBM_FLOW_CONTROL               0x08

#define MEI_HBM_CLIENT_CONNECTION_RESET    0x09

#define MEI_HBM_PG_ISOLATION_ENTRY         0x0a
#define MEI_HBM_PG_ISOLATION_ENTRY_RES     0x8a

#define MEI_HBM_PG_ISOLATION_EXIT          0x0b
#define MEI_HBM_PG_ISOLATION_EXIT_RES      0x8b

#define MEI_HBM_CLIENT_ADD                 0x0f
#define MEI_HBM_CLIENT_ADD_RES             0x8f

#define MEI_HBM_NOTIFY                     0x10
#define MEI_HBM_NOTIFY_RES                 0x90
#define MEI_HBM_NOTIFICATION               0x11

#define MEI_HBM_DMA_SETUP                  0x12
#define MEI_HBM_DMA_SETUP_RES              0x92

/*
 * MEI Stop Reason
 * used by hbm_host_stop_request.reason
 */
enum mei_stop_reason_types {
	DRIVER_STOP_REQUEST = 0x00,
	DEVICE_D1_ENTRY = 0x01,
	DEVICE_D2_ENTRY = 0x02,
	DEVICE_D3_ENTRY = 0x03,
	SYSTEM_S1_ENTRY = 0x04,
	SYSTEM_S2_ENTRY = 0x05,
	SYSTEM_S3_ENTRY = 0x06,
	SYSTEM_S4_ENTRY = 0x07,
	SYSTEM_S5_ENTRY = 0x08
};

/*
 * enum mei_hbm_status  - mei host bus messages return values
 *
 * @MEI_HBM_SUCCESS           : status success
 * @MEI_HBM_CLIENT_NOT_FOUND  : client not found
 * @MEI_HBM_ALREADY_EXISTS    : connection already established
 * @MEI_HBM_REJECTED          : connection is rejected
 * @MEI_HBM_INVALID_PARAMETER : invalid parameter
 * @MEI_HBM_NOT_ALLOWED       : operation not allowed
 * @MEI_HBM_ALREADY_STARTED   : system is already started
 * @MEI_HBM_NOT_STARTED       : system not started
 *
 */
enum mei_hbm_status {
	MEI_HBM_SUCCESS           = 0,
	MEI_HBM_CLIENT_NOT_FOUND  = 1,
	MEI_HBM_ALREADY_EXISTS    = 2,
	MEI_HBM_REJECTED          = 3,
	MEI_HBM_INVALID_PARAMETER = 4,
	MEI_HBM_NOT_ALLOWED       = 5,
	MEI_HBM_ALREADY_STARTED   = 6,
	MEI_HBM_NOT_STARTED       = 7,

	MEI_HBM_MAX
};

/*
 * Client Connect Status
 * used by hbm_client_connect_response.status
 */
enum mei_cl_connect_status {
	MEI_CL_CONN_SUCCESS          = MEI_HBM_SUCCESS,
	MEI_CL_CONN_NOT_FOUND        = MEI_HBM_CLIENT_NOT_FOUND,
	MEI_CL_CONN_ALREADY_STARTED  = MEI_HBM_ALREADY_EXISTS,
	MEI_CL_CONN_OUT_OF_RESOURCES = MEI_HBM_REJECTED,
	MEI_CL_CONN_MESSAGE_SMALL    = MEI_HBM_INVALID_PARAMETER,
	MEI_CL_CONN_NOT_ALLOWED      = MEI_HBM_NOT_ALLOWED,
};

/*
 * Client Disconnect Status
 */
enum  mei_cl_disconnect_status {
	MEI_CL_DISCONN_SUCCESS = MEI_HBM_SUCCESS
};

struct mei_enumerate_me_clients {
	uint8_t valid_addresses[32];
};

struct mei_client_properties {
	guid_t   protocol_name;
	uint8_t  protocol_version;
	uint8_t  max_connections;
	uint8_t  fixed_address;
	uint8_t  single_recv_buf;
	uint32_t max_msg_length;
} __attribute__((packed));

/* message header is same in native and virtual */
struct mei_msg_hdr {
	uint32_t me_addr:8;
	uint32_t host_addr:8;
	uint32_t length:9;
	uint32_t reserved:5;
	uint32_t internal:1;
	uint32_t msg_complete:1;
} __attribute__((packed));

struct mei_hbm_cmd {
	uint8_t cmd:7;
	uint8_t is_response:1;
} __attribute__((packed));

struct mei_hbm_host_ver_req {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t reserved;
	uint8_t minor;
	uint8_t major;
} __attribute__((packed));

struct mei_hbm_host_ver_res {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t host_ver_support;
	uint8_t minor;
	uint8_t major;
} __attribute__((packed));

struct mei_hbm_host_stop_req {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t reason;
	uint8_t reserved[2];
} __attribute__((packed));

struct mei_hbm_host_stop_res {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t reserved[3];
} __attribute__((packed));

struct mei_hbm_me_stop_res {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t reason;
	uint8_t reserved[2];
} __attribute__((packed));

struct mei_hbm_me_stop_req {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t reserved[3];
} __attribute__((packed));

/**
 * enum hbm_host_enum_flags - enumeration request flags (HBM version >= 2.0)
 *
 * @MEI_HBM_ENUM_F_ALLOW_ADD: allow dynamic clients add
 * @MEI_HBM_ENUM_F_IMMEDIATE_ENUM: allow FW to send answer immediately
 */
enum mei_hbm_host_enum_flags {
	MEI_HBM_ENUM_F_ALLOW_ADD = 0,
	MEI_HBM_ENUM_F_IMMEDIATE_ENUM = 1,
};

struct mei_hbm_host_enum_req {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t flags;
	uint8_t reserved[2];
} __attribute__((packed));

struct mei_hbm_host_enum_res {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t reserved[3];
	uint8_t valid_addresses[32];
};

struct mei_hbm_host_client_prop_req {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t address;
	uint8_t reserved[2];
} __attribute__((packed));


struct mei_hbm_host_client_prop_res {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t address;
	uint8_t status;
	uint8_t reserved[1];
	struct mei_client_properties props;
} __attribute__((packed));

/**
 * struct mei_hbm_add_client_req - request to add a client
 *     might be sent by fw after enumeration has already completed
 *
 * @hbm_cmd: bus message command header
 * @me_addr: address of the client in ME
 * @reserved: reserved
 * @client_properties: client properties
 */
struct mei_hbm_add_client_req {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t reserved[2];
	struct mei_client_properties client_properties;
} __attribute__((packed));

/**
 * struct mei_hbm_add_client_res - response to add a client
 *     sent by the host to report client addition status to fw
 *
 * @hbm_cmd: bus message command header
 * @me_addr: address of the client in ME
 * @status: if HBM_SUCCESS then the client can now accept connections.
 * @reserved: reserved
 */
struct hbm_add_client_res {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t status;
	uint8_t reserved[1];
} __attribute__((packed));

/**
 * struct mei_hbm_power_gate - power gate request/response
 *
 * @hbm_cmd: bus message command header
 * @reserved: reserved
 */
struct mei_hbm_power_gate {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t reserved[3];
} __attribute__((packed));

struct mei_hbm_client_connect_req {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t host_addr;
	uint8_t reserved;
} __attribute__((packed));

struct mei_hbm_client_connect_res {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t host_addr;
	uint8_t status;
} __attribute__((packed));

struct mei_hbm_client_disconnect_req {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t host_addr;
	uint8_t status;
} __attribute__((packed));

struct mei_hbm_client_disconnect_res {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t host_addr;
	uint8_t status;
} __attribute__((packed));

struct mei_hbm_flow_ctl {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t host_addr;
	uint8_t reserved[5];
} __attribute__((packed));

/**
 * struct mei_hbm_notification_req - start/stop notification request
 *
 * @hbm_cmd: bus message command header
 * @me_addr: address of the client in ME
 * @host_addr: address of the client in the driver
 * @start:  start = 1 or stop = 0 asynchronous notifications
 */

struct mei_hbm_notification_req {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t host_addr;
	uint8_t start;
} __attribute__((packed));

/**
 * struct mei_hbm_notification_res - start/stop notification response
 *
 * @hbm_cmd: bus message command header
 * @me_addr: address of the client in ME
 * @host_addr: - address of the client in the driver
 * @status: (mei_hbm_status) response status for the request
 *  - MEI_HBM_SUCCESS: successful stop/start
 *  - MEI_HBM_CLIENT_NOT_FOUND: if the connection could not be found.
 *  - MEI_HBM_ALREADY_STARTED: for start requests for a previously
 *                         started notification.
 *  - MEI_HBM_NOT_STARTED: for stop request for a connected client for whom
 *                         asynchronous notifications are currently disabled.
 *
 * @start:  start = 1 or stop = 0 asynchronous notifications
 * @reserved: reserved
 */
struct mei_hbm_notification_res {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t host_addr;
	uint8_t status;
	uint8_t start;
	uint8_t reserved[3];
} __attribute__((packed));

/**
 * struct mei_hbm_notification - notification event
 *
 * @hbm_cmd: bus message command header
 * @me_addr:  address of the client in ME
 * @host_addr:  address of the client in the driver
 * @reserved: reserved for alignment
 */
struct mei_hbm_notification {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t host_addr;
	uint8_t reserved[1];
} __attribute__((packed));

/**
 * struct mei_hbm_dma_mem_dscr - dma ring
 *
 * @addr_hi: the high 32bits of 64 bit address
 * @addr_lo: the low  32bits of 64 bit address
 * @size   : size in bytes (must be power of 2)
 */
struct mei_hbm_dma_mem_dscr {
	uint32_t addr_hi;
	uint32_t addr_lo;
	uint32_t size;
} __attribute__((packed));

enum {
	MEI_DMA_DSCR_HOST = 0,
	MEI_DMA_DSCR_DEVICE = 1,
	MEI_DMA_DSCR_CTRL = 2,
	MEI_DMA_DSCR_NUM,
};

/**
 * struct mei_hbm_dma_setup_req - dma setup request
 *
 * @hbm_cmd: bus message command header
 * @reserved: reserved for alignment
 * @dma_dscr: dma descriptor for HOST, DEVICE, and CTRL
 */
struct mei_hbm_dma_setup_req {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t reserved[3];
	struct mei_hbm_dma_mem_dscr dma_dscr[MEI_DMA_DSCR_NUM];
} __attribute__((packed));

/**
 * struct mei_hbm_dma_setup_res - dma setup response
 *
 * @hbm_cmd: bus message command header
 * @status: 0 on success; otherwise DMA setup failed.
 * @reserved: reserved for alignment
 */
struct mei_hbm_dma_setup_res {
	struct mei_hbm_cmd hbm_cmd;
	uint8_t status;
	uint8_t reserved[2];
} __attribute__((packed));

#ifndef IOCTL_MEI_CONNECT_CLIENT_VTAG
/*
 * IOCTL Connect Client Data structure with vtag
 */
struct mei_connect_client_vtag {
	uuid_le in_client_uuid;
	__u8 vtag;
	__u8 reserved[3];
};

struct mei_connect_client_data_vtag {
	union {
		struct mei_connect_client_vtag connect;
		struct mei_client out_client_properties;
	};
};

#define IOCTL_MEI_CONNECT_CLIENT_VTAG \
	_IOWR('H', 0x04, struct mei_connect_client_data_vtag)

#endif /* IOCTL_MEI_CONNECT_CLIENT_VTAG */

#endif    /* _MEI_HBM_H_ */
