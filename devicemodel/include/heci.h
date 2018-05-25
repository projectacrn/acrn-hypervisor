/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#ifndef _HECI_H_
#define _HECI_H_
#include <uuid/uuid.h>

#define HECI_HBM_HOST_VERSION			0x01
#define HECI_HBM_HOST_STOP			0x02
#define HECI_HBM_ME_STOP			0x03
#define HECI_HBM_HOST_ENUM			0x04
#define HECI_HBM_HOST_CLIENT_PROP		0x05
#define HECI_HBM_CLIENT_CONNECT			0x06
#define HECI_HBM_CLIENT_DISCONNECT		0x07
#define HECI_HBM_FLOW_CONTROL			0x08
#define HECI_HBM_CLIENT_CONNECTION_RESET	0x09

/*
 * enum heci_hbm_status  - heci host bus messages return values
 *
 * @HECI_HBM_SUCCESS           : status success
 * @HECI_HBM_CLIENT_NOT_FOUND  : client not found
 * @HECI_HBM_ALREADY_EXISTS    : connection already established
 * @HECI_HBM_REJECTED          : connection is rejected
 * @HECI_HBM_INVALID_PARAMETER : invalid parameter
 * @HECI_HBM_NOT_ALLOWED       : operation not allowed
 * @HECI_HBM_ALREADY_STARTED   : system is already started
 * @HECI_HBM_NOT_STARTED       : system not started
 *
 */
enum heci_hbm_status {
	HECI_HBM_SUCCESS           = 0,
	HECI_HBM_CLIENT_NOT_FOUND  = 1,
	HECI_HBM_ALREADY_EXISTS    = 2,
	HECI_HBM_REJECTED          = 3,
	HECI_HBM_INVALID_PARAMETER = 4,
	HECI_HBM_NOT_ALLOWED       = 5,
	HECI_HBM_ALREADY_STARTED   = 6,
	HECI_HBM_NOT_STARTED       = 7,

	HECI_HBM_MAX
};

struct mei_enumerate_me_clients {
	uint8_t valid_addresses[32];
};

struct mei_request_client_params {
	uint8_t client_id;
	uint8_t reserved[3];
	uint8_t data[64];
} __attribute__((packed));

struct heci_client_properties {
	uuid_t	protocol_name;
	uint8_t	protocol_version;
	uint8_t	max_connections;
	uint8_t	fixed_address;
	uint8_t	single_recv_buf;
	uint32_t max_msg_length;
} __attribute__((packed));

/* message header is same in native and virtual */
struct heci_msg_hdr {
	uint32_t me_addr:8;
	uint32_t host_addr:8;
	uint32_t length:9;
	uint32_t reserved:5;
	uint32_t internal:1;
	uint32_t msg_complete:1;
} __attribute__((packed));

struct heci_hbm_cmd {
	uint8_t cmd:7;
	uint8_t is_response:1;
};

struct heci_hbm_host_ver_req {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t reserved;
	uint8_t minor;
	uint8_t major;
};

struct heci_hbm_host_ver_res {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t host_ver_support;
	uint8_t minor;
	uint8_t major;
};

struct heci_hbm_host_stop_req {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t reason;
	uint8_t reserved[2];
};

struct heci_hbm_host_stop_res {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t reserved[3];
};

struct heci_hbm_me_stop_res {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t reason;
	uint8_t reserved[2];
};

struct heci_hbm_me_stop_req {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t reserved[3];
};

struct heci_hbm_host_enum_req {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t reserved[3];
};

struct heci_hbm_host_enum_res {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t reserved[3];
	uint8_t valid_addresses[32];
};

struct heci_hbm_host_client_prop_req {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t address;
	uint8_t reserved[2];
};

struct heci_hbm_host_client_prop_res {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t address;
	uint8_t status;
	uint8_t reserved[1];
	struct heci_client_properties props;
};

struct heci_hbm_client_connect_req {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t host_addr;
	uint8_t reserved;
};

struct heci_hbm_client_connect_res {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t host_addr;
	uint8_t status;
};

struct heci_hbm_client_disconnect_req {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t host_addr;
	uint8_t reserved;
};

struct heci_hbm_client_disconnect_res {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t host_addr;
	uint8_t status;
};

struct heci_hbm_flow_ctl {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t host_addr;
	uint8_t reserved[5];
};

struct heci_hbm_client_connection_reset_req {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t host_addr;
	uint8_t reserved[1];
};

struct heci_hbm_client_connection_reset_res {
	struct heci_hbm_cmd hbm_cmd;
	uint8_t me_addr;
	uint8_t host_addr;
	uint8_t status;
};

#define IOCTL_MEI_ENUMERATE_ME_CLIENTS \
	_IOWR('H', 0x04, struct mei_enumerate_me_clients)
#define IOCTL_MEI_REQUEST_CLIENT_PROP \
	_IOWR('H', 0x05, struct mei_request_client_params)

#endif	/* _HECI_H_ */
