/*
 * Copyright (C) 2019 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <linux/mei.h>
#include "att_keybox.h"

static const uuid_le HECI_CLIENT_GUID =
	UUID_LE(0x8e6a6715, 0x9abc, 0x4043,
			0x88, 0xef, 0x9e, 0x39, 0xc6, 0xf6, 0x3e, 0x0f);

#define MEI_DEV                     "/dev/mei0"
#define HECI_READ_ATTKB_GRP_ID      0x0a
#define HECI_READ_ATTKB_EX_CMD_REQ  0x1a
#define HECI_MTU 2048

uint16_t get_attkb_size(void)
{
	struct mei_connect_client_data mdcd;
	int fd = -1, ret = 0;
	HECI_READ_ATTKB_EX_Request req;
	HECI_READ_ATTKB_EX_Response resp;

	fd = open(MEI_DEV, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "Failed to open %s, %s\n",
				MEI_DEV, strerror(errno));
		return 0;
	}

	memset(&mdcd, 0, sizeof(mdcd));
	memcpy(&mdcd.in_client_uuid, &HECI_CLIENT_GUID, sizeof(HECI_CLIENT_GUID));

	ret = ioctl(fd, IOCTL_MEI_CONNECT_CLIENT, &mdcd);
	if (ret) {
		fprintf(stderr, "HECI connection failed, %s\n",
				strerror(errno));
		goto err;
	}

	/* Get the size of encrypted attestation keybox */
	memset(&req, 0, sizeof(req));
	req.header.groupid = HECI_READ_ATTKB_GRP_ID;
	req.header.command = HECI_READ_ATTKB_EX_CMD_REQ;
	req.size = 0;
	req.offset = 0;
	req.flags.encryption = 1;

	ret = write(fd, &req, sizeof(req));
	if (ret != sizeof(req)) {
		fprintf(stderr, "Failed to send HECI command, %s\n",
				strerror(errno));
		goto err;
	}

	ret = read(fd, &resp, sizeof(resp));
	if (ret != sizeof(resp)) {
		fprintf(stderr, "ret = %d,Failed to read HECI command result, %d:%s\n",
				ret, errno, strerror(errno));
		goto err;
	}

	if ((resp.header.is_response != 1) || (resp.header.result != 0)) {
		fprintf(stderr,	"Failed to check resp header = 0x%x\n",
				resp.header.result);
		goto err;
	}

	if (resp.total_file_size == 0) {
		fprintf(stderr,	"ret = %d, Unexpected filesize 0!\n", ret);
	}

	close(fd);

	return resp.total_file_size;

err:
	close(fd);
	return 0;
}

uint16_t read_attkb(void *data, uint16_t size)
{
	struct mei_connect_client_data mdcd;
	HECI_READ_ATTKB_EX_Request req;
	HECI_READ_ATTKB_EX_Response resp;
	int fd = -1, ret = 0;
	uint16_t left_size = 0;
	uint16_t bytes_read = 0;
	void *ptr = data;

	if ((ptr == NULL) || (size == 0)) {
		return 0;
	}

	fd = open(MEI_DEV, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "Failed to open %s, %s\n",
				MEI_DEV, strerror(errno));
		return 0;
	}

	memset(&mdcd, 0, sizeof(mdcd));
	memcpy(&mdcd.in_client_uuid, &HECI_CLIENT_GUID, sizeof(HECI_CLIENT_GUID));

	ret = ioctl(fd, IOCTL_MEI_CONNECT_CLIENT, &mdcd);
	if (ret) {
		fprintf(stderr, "HECI connection failed, %s\n",
				strerror(errno));
		goto err;
	}

	left_size = size;

	memset(&req, 0, sizeof(req));
	req.header.groupid = HECI_READ_ATTKB_GRP_ID;
	req.header.command = HECI_READ_ATTKB_EX_CMD_REQ;
	req.offset = 0;
	req.size = HECI_MTU > left_size ? left_size : HECI_MTU;
	req.flags.encryption = 1;

	while (left_size) {
		req.offset = bytes_read;
		req.size = HECI_MTU > left_size ? left_size : HECI_MTU;

		ret = write(fd, &req, sizeof(req));
		if (ret != sizeof(req)) {
			fprintf(stderr, "Failed to send HECI command, %s\n",
					strerror(errno));
			goto err;
		}

		ret = read(fd, &resp, sizeof(resp));
		if (ret != sizeof(resp)) {
			fprintf(stderr, "Failed to read HECI command result, %s\n",
					strerror(errno));
			goto err;
		}

		if ((resp.header.is_response != 1) || (resp.header.result != 0)) {
			fprintf(stderr,	"Failed to check resp header = 0x%x\n",
					resp.header.result);
			goto err;
		}

		ret = read(fd, (uint8_t *)data + bytes_read, req.size);
		if (ret != req.size) {
			fprintf(stderr, "Failed to read attkb, %s\n",
					strerror(errno));
			goto err;
		}

		ptr += ret;
		bytes_read += ret;
		left_size -= ret;
	}

	close(fd);

	return bytes_read;

err:
	close(fd);
	return 0;
}
