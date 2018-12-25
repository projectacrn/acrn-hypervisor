/*
 * Copyright (C) 2018 Intel Corporation
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

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdbool.h>

#include "rpmb.h"
#include "rpmb_sim.h"
#include "vrpmb.h"
#include "rpmb_backend.h"

static int virtio_rpmb_debug = 1;
#define DPRINTF(params) do { if (virtio_rpmb_debug) printf params; } while (0)
#define WPRINTF(params) (printf params)

#define READ_STR_LEN 10
#define WRITE_STR_LEN 11
static uint32_t phy_counter = 0;
static uint32_t virt_counter = 0;
static uint8_t rpmb_key[RPMB_KEY_32_LEN] = {0};
static uint8_t virt_rpmb_key[RPMB_KEY_32_LEN] = {0};
static uint16_t g_rpmb_mode = RPMB_SIM_MODE;
static const char READ_DATA_STR[READ_STR_LEN] = "read data";
static const char WRITE_DATA_STR[WRITE_STR_LEN] = "write data";

//TODO: will be read from config file.
static uint16_t get_uos_count(void)
{
	return 1;
}

//TODO: will be predefined and read from config file.
static uint16_t get_rpmb_blocks(void)
{
	return rpmb_get_blocks();
}

/* Common area of RPMB refers to the start area of RPMB
 * shared among all UOS with RO access.
 * It's predefined to 32KB in size which contains:
 * AttKB(up to 16KB), RPMB info header (256B)
 * and the remaining size for future uasge.
 */
static uint16_t get_common_blocks(void)
{
	uint16_t common_size = 32 * 1024;
	return common_size / RPMB_BLOCK_SIZE;
}

static uint16_t get_accessible_blocks(void)
{
	return (get_rpmb_blocks() - get_common_blocks()) /
			get_uos_count() + get_common_blocks();
}

/* Todo: To get the uos number, e.g. No.0 or No.1, which is
  used for calculating UOS RPMB range address.
  But this will be removed after config file is supported.
  We plan to predefine such info and save to config file.
*/
static uint8_t get_uos_id(void)
{
	return (get_uos_count() - 1);
}

void rpmb_mode_init(uint16_t mode)
{
	g_rpmb_mode = mode;
	DPRINTF(("%s: rpmb mode is %d\n", __func__, g_rpmb_mode));
}

void rpmb_counter_init(uint32_t counter)
{
	phy_counter = counter;
}

static bool is_key_programmed(void)
{
	if (g_rpmb_mode == RPMB_PHY_MODE) {
		return true;
	} else if (g_rpmb_mode == RPMB_SIM_MODE) {
		if (is_use_sim_rpmb())
			return true;
	}

	DPRINTF(("%s: rpmb mode 0x%x is unsupported\n", __func__, g_rpmb_mode));
	return false;
}

static uint16_t get_phy_addr(uint8_t uos_id, uint16_t vaddr)
{
	uint16_t common_blocks = get_common_blocks();
	uint16_t accessible_blocks = get_accessible_blocks();

	if (vaddr < get_common_blocks()) {
		return vaddr;
	} else {
		return (((accessible_blocks - common_blocks) * uos_id) + vaddr);
	}
}

int get_virt_rpmb_key(void)
{
	int rc = -1;
	uint8_t key[RPMB_KEY_LEN];

	rc = get_vrpmb_key(key, sizeof(key));
	if (rc == 0){
		DPRINTF(("%s: get uos key fail\n", __func__));
	}

	memcpy(virt_rpmb_key, key, RPMB_KEY_32_LEN);
	memset(key, 0, RPMB_KEY_LEN);
	return rc;
}

static int rpmb_replace_frame(struct rpmb_frame *frames, uint32_t frame_cnt,
	const uint8_t *key, const uint8_t *nonce, const uint32_t *write_counter,
	const uint16_t *addr, const uint16_t *block_count, const int *result, const int *req_resp)
{
	uint32_t i;

	for (i = 0; i < frame_cnt; i++) {
		if (nonce)
			memcpy(frames[i].nonce, nonce, sizeof(frames[i].nonce));
		if (write_counter)
			frames[i].write_counter = swap32(*write_counter);
		if (addr)
			frames[i].addr = swap16(*addr);
		if (block_count)
			frames[i].block_count = swap16(*block_count);
		if (result)
			frames[i].result = swap16(*result);
		if (req_resp)
			frames[i].req_resp = swap16(*req_resp);
	}

	if (key) {
		if (rpmb_mac(key, frames, frame_cnt, frames[frame_cnt - 1].key_mac)) {
			DPRINTF(("%s: rpmb_mac failed\n", __func__));
			return -1;
		}
	}

	return 0;
}

static int rpmb_check_frame(const char *cmd_str, int *err,
		const struct rpmb_frame *frames, uint32_t frame_cnt,
		const uint8_t *key, const uint32_t *write_counter,
		const uint16_t *addr, const uint16_t *block_count)
{
	uint32_t i;
	uint8_t mac[32];
	size_t len;

	for (i = 0; i < frame_cnt; i++) {
		if (write_counter && *write_counter != swap32(frames[i].write_counter)) {
			*err = RPMB_RES_COUNT_FAILURE;
			DPRINTF(("%s: Bad write counter %u\n", cmd_str, *write_counter));
			return -1;
		}
	}

	if (addr && *addr >= get_accessible_blocks()) {
		*err = RPMB_RES_ADDR_FAILURE;
		 DPRINTF(("%s: Bad addr, got %u, expected %u\n",
			cmd_str, swap16(frames[i].addr), *addr));
		return -1;
	}

	if (addr && block_count && (*addr + *block_count) > get_accessible_blocks()) {
		*err = RPMB_RES_GENERAL_FAILURE;
		 DPRINTF(("%s: Bad block count %u\n",
			cmd_str, *block_count));
		return -1;
	}

	len = strnlen(cmd_str, sizeof(WRITE_DATA_STR)) + 1;
	if (len > sizeof(WRITE_DATA_STR))
		len = sizeof(WRITE_DATA_STR);

	if (addr && !memcmp(cmd_str, WRITE_DATA_STR, len)) {
		if (*addr < get_common_blocks()) {
			*err = RPMB_RES_WRITE_FAILURE;
			DPRINTF(("%s: Common block is read only\n", cmd_str));
			return -1;
		}
	}

	if (key) {
		if (rpmb_mac(key, frames, frame_cnt, mac)) {
			*err = RPMB_RES_GENERAL_FAILURE;
			DPRINTF(("%s: rpmb_mac failed\n", cmd_str));
			return -1;
		}

		if (memcmp(frames[frame_cnt - 1].key_mac, mac, sizeof(mac))) {
			*err = RPMB_RES_AUTH_FAILURE;
			DPRINTF(("%s: Bad MAC\n", cmd_str));
			return -1;
		}
	}

	return 0;
}

static int rpmb_phy_ioctl(uint32_t ioc_cmd, void* seq_data)
{
	int rc = -1;
	int fd;

	if (seq_data == NULL) {
		DPRINTF(("%s: seq_data is NULL\n", __func__));
		return rc;
	}

	/* open rpmb device.*/
	fd = open(RPMB_PHY_PATH_NAME, O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		DPRINTF(("%s: failed to open %s.\n", __func__, RPMB_PHY_PATH_NAME));
		return fd;
	}

	/* send ioctl cmd.*/
	rc = ioctl(fd, ioc_cmd, seq_data);
	if (rc)
		DPRINTF(("%s: seq ioctl cmd failed(%d).\n", __func__, rc));

	/* close rpmb device.*/
	close(fd);

	return rc;
}

static int rpmb_sim_ioctl(uint32_t ioc_cmd, void* seq_data)
{
	if (seq_data == NULL) {
		DPRINTF(("%s: seq_data is NULL\n", __func__));
		return -1;
	}

	switch (ioc_cmd) {
	case RPMB_IOC_REQ_CMD:
	case RPMB_IOC_SEQ_CMD:
		return rpmb_sim_send(seq_data);
	default:
		DPRINTF(("%s: ioctl 0x%x is unsupported\n", __func__, ioc_cmd));
		return -1;
	}
}

static int rpmb_virt_ioctl(uint32_t ioc_cmd, void* seq_data)
{
	if (g_rpmb_mode == RPMB_PHY_MODE) {
		return rpmb_phy_ioctl(ioc_cmd, seq_data);
	} else if (g_rpmb_mode == RPMB_SIM_MODE) {
		return rpmb_sim_ioctl(ioc_cmd, seq_data);
	}

	DPRINTF(("%s: rpmb mode 0x%x is unsupported\n", __func__, g_rpmb_mode));
	return -1;
}

static int rpmb_virt_write(uint32_t ioc_cmd, void* seq_data,
		struct rpmb_frame* in_frame, uint32_t in_cnt,
		struct rpmb_frame* out_frame, uint32_t out_cnt)
{
	int err = RPMB_RES_WRITE_FAILURE;
	int resp = RPMB_RESP_DATA_WRITE;
	uint16_t vaddr;
	uint16_t paddr;
	uint16_t block_count;
	uint8_t uos_id;
	__u16 rpmb_result = 0;
	int rc;

	if (in_cnt == 0 || in_frame == NULL || seq_data == NULL) {
		DPRINTF(("%s: in_frame, in_cnt or seq_data is not available.\n", __func__));
		return -1;
	}

	if (out_cnt > 0 && out_frame != NULL) {
		memset(out_frame, 0, out_cnt * RPMB_FRAME_SIZE);
	} else {
		DPRINTF(("%s: vrpmb must be aware of the result in out_frame.\n", __func__));
		return -1;
	}

	uos_id = get_uos_id();

	vaddr = swap16(in_frame->addr);
	block_count = in_cnt;
	if (rpmb_check_frame(WRITE_DATA_STR, &err, in_frame, in_cnt,
			virt_rpmb_key, &virt_counter, &vaddr, &block_count))
		goto out;

	paddr = get_phy_addr(uos_id, vaddr);

	if (rpmb_replace_frame(in_frame, in_cnt, rpmb_key, NULL,
			&phy_counter, &paddr, NULL, NULL, NULL))
	{
		err = RPMB_RES_GENERAL_FAILURE;
		goto out;
	}

	if (rpmb_virt_ioctl(ioc_cmd, seq_data)) {
		DPRINTF(("%s: rpmb virt ioctl failed.\n", __func__));
		return -1;
	}

	if (out_frame->result == swap16(RPMB_RES_COUNT_FAILURE)) {
		memset(out_frame, 0, out_cnt * RPMB_FRAME_SIZE);
		rc = rpmb_get_counter(g_rpmb_mode, rpmb_key, &phy_counter, &rpmb_result);
		if (rc) {
			DPRINTF(("%s: rpmb_get_counter failed(0x%x)\n", __func__, rpmb_result));
			return -1;
		}

		/* Since phy_counter has changed, so we have to generate mac again*/
		if (rpmb_replace_frame(in_frame, in_cnt, rpmb_key, NULL,
				&phy_counter, &paddr, NULL, NULL, NULL))
		{
			err = RPMB_RES_GENERAL_FAILURE;
			goto out;
		}

		if (rpmb_virt_ioctl(ioc_cmd, seq_data)) {
			DPRINTF(("%s: rpmb virt retry ioctl failed.\n", __func__));
			return -1;
		}
	}

	if (out_frame->result == RPMB_RES_OK) {
		phy_counter++;
		virt_counter++;
	}

	rpmb_replace_frame(out_frame, out_cnt, virt_rpmb_key, NULL,
			&virt_counter, &vaddr, NULL, NULL, NULL);

	return 0;
out:
	rpmb_replace_frame(out_frame, out_cnt, virt_rpmb_key, in_frame[0].nonce,
			&virt_counter, &vaddr, &block_count, &err, &resp);

	return 0;
}

static int rpmb_virt_read(uint32_t ioc_cmd, void* seq_data,
		struct rpmb_frame* in_frame, uint32_t in_cnt,
		struct rpmb_frame* out_frame, uint32_t out_cnt)
{
	int err = RPMB_RES_READ_FAILURE;
	int resp = RPMB_RESP_DATA_READ;
	uint16_t vaddr;
	uint16_t paddr;
	uint16_t block_count;
	uint8_t uos_id;

	if (in_cnt == 0 || in_frame == NULL) {
		DPRINTF(("%s: in_frame, in_cnt or seq_data is not available\n", __func__));
		return -1;
	}

	if (out_cnt == 0 || out_frame == NULL) {
		DPRINTF(("%s: out_frame or out_cnt is not available\n", __func__));
		return -1;
	}

	uos_id = get_uos_id();

	memset(out_frame, 0, out_cnt * RPMB_FRAME_SIZE);
	vaddr = swap16(in_frame->addr);
	block_count = out_cnt;

	if (rpmb_check_frame(READ_DATA_STR, &err, in_frame,
			in_cnt, NULL, NULL, &vaddr, &block_count))
		goto out;

	paddr = get_phy_addr(uos_id, vaddr);
	if (rpmb_replace_frame(in_frame, in_cnt, NULL,
			NULL, NULL, &paddr, NULL, NULL, NULL )) {
		err = RPMB_RES_GENERAL_FAILURE;
		goto out;
	}

	if (rpmb_virt_ioctl(ioc_cmd, seq_data)) {
		DPRINTF(("%s: rpmb ioctl failed\n", __func__));
		return -1;
	}

	rpmb_replace_frame(out_frame, out_cnt, virt_rpmb_key, NULL,
				NULL, &vaddr, NULL, NULL, NULL);

	return 0;
out:
	rpmb_replace_frame(out_frame, out_cnt, virt_rpmb_key, in_frame[0].nonce,
					NULL, &vaddr, &block_count, &err, &resp);

	return 0;
}

static int rpmb_virt_get_counter(struct rpmb_frame* in_frame, uint32_t in_cnt,
				struct rpmb_frame* out_frame, uint32_t out_cnt)
{
	int err = RPMB_RES_OK;
	int resp = RPMB_RESP_GET_COUNTER;

	if (in_cnt == 0 || in_frame == NULL) {
		DPRINTF(("%s: in_frame or in_cnt is not available\n", __func__));
		return -1;
	}

	if (out_cnt == 0 || out_frame == NULL) {
		DPRINTF(("%s: out_frame or out_cnt is not available\n", __func__));
		return -1;
	}

	memset(out_frame, 0, out_cnt * RPMB_FRAME_SIZE);

	if (!is_key_programmed()) {
		DPRINTF(("%s: rpmb key is not programmed\n", __func__));
		err = RPMB_RES_NO_AUTH_KEY;
		goto out;
	}

	rpmb_replace_frame(out_frame, out_cnt, virt_rpmb_key,
			in_frame[0].nonce, &virt_counter, NULL, NULL, &err, &resp);

	return 0;

out:
	rpmb_replace_frame(out_frame, out_cnt, virt_rpmb_key,
			in_frame[0].nonce, NULL, NULL, NULL, &err, &resp);

	return 0;
}

int rpmb_handler(uint32_t cmd, void *r)
{
	int rc = -1;
	uint16_t i;
	uint32_t write_cnt = 0;
	uint32_t rel_write_cnt = 0;
	uint32_t read_cnt = 0;
	struct rpmb_frame *frame_write = NULL;
	struct rpmb_frame *frame_rel_write = NULL;
	struct rpmb_frame *frame_read = NULL;
	struct rpmb_ioc_cmd *ioc_cmd = NULL;
	struct rpmb_ioc_seq_data *iseq = r;

	if (r == NULL) {
		DPRINTF(("%s: rpmb iocctl seq data is NULL\n", __func__));
		goto err_response;
	}

	for (i = 0; i < iseq->h.num_of_cmds; i++) {
		ioc_cmd = (struct rpmb_ioc_cmd *)(&iseq->cmd[i]);
		if (ioc_cmd->flags == 0) {
			frame_read = (struct rpmb_frame *)ioc_cmd->frames_ptr;
			read_cnt = ioc_cmd->nframes;
		} else if (ioc_cmd->flags == RPMB_F_WRITE) {
			frame_write = (struct rpmb_frame *)ioc_cmd->frames_ptr;
			write_cnt = ioc_cmd->nframes;
		} else if (ioc_cmd->flags == (RPMB_F_WRITE | RPMB_F_REL_WRITE)) {
			frame_rel_write = (struct rpmb_frame *)ioc_cmd->frames_ptr;
			rel_write_cnt = ioc_cmd->nframes;
		} else {
			DPRINTF(("%s: rpmb_ioc_cmd is invalid in the rpmb_ioc_seq_data\n", __func__));
			goto err_response;
		}
	}

	if (rel_write_cnt) {
		if (frame_rel_write[0].req_resp == swap16(RPMB_REQ_DATA_WRITE)) {
			if (write_cnt && (frame_write->req_resp == swap16(RPMB_REQ_RESULT_READ)))
				rc = rpmb_virt_write(cmd, r, frame_rel_write,
						rel_write_cnt, frame_read, read_cnt);
			else
				rc = rpmb_virt_write(cmd, r, frame_rel_write, rel_write_cnt, NULL, 0);
		} else if (frame_rel_write[0].req_resp == swap16(RPMB_REQ_PROGRAM_KEY)) {
			DPRINTF(("%s: rpmb grogram key is unsupported\n", __func__));
			goto err_response;
		} else {
			DPRINTF(("%s: rpmb ioctl frame is invalid\n", __func__));
			goto err_response;
		}
	} else if (write_cnt) {
		if (frame_write[0].req_resp == swap16(RPMB_REQ_DATA_READ)) {
			rc = rpmb_virt_read(cmd, r, frame_write, 1, frame_read, read_cnt);
		} else if (frame_write[0].req_resp == swap16(RPMB_REQ_GET_COUNTER)) {
			rc = rpmb_virt_get_counter(frame_write, 1, frame_read, 1);
		} else {
			DPRINTF(("%s: rpmb get counter frame is invalid\n", __func__));
			goto err_response;
		}
	} else {
		DPRINTF(("%s: rpmb ioctl frame is invalid\n", __func__));
		goto err_response;
	}

	return rc;

err_response:
	return -1;
}
