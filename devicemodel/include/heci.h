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
#ifndef _HECI_H_
#define _HECI_H_
#include <uuid/uuid.h>

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

#define IOCTL_MEI_ENUMERATE_ME_CLIENTS \
	_IOWR('H', 0x04, struct mei_enumerate_me_clients)
#define IOCTL_MEI_REQUEST_CLIENT_PROP \
	_IOWR('H', 0x05, struct mei_request_client_params)

#endif	/* _HECI_H_ */
