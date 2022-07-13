/*
 * Copyright (c) 2018-2022 Intel Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <openssl/hmac.h>
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/evp.h>
#endif

#include "rpmb.h"
#include "rpmb_sim.h"
#include "log.h"

static FILE *rpmb_fd = NULL;

/*
 * 0~6 is magic
 * 7~38 is rpmb key
 * 39~41 is write counter
 */
#define KEY_MAGIC		"key_sim"
#define KEY_MAGIC_ADDR		0
#define KEY_MAGIC_LENGTH	7

#define KEY_ADDR		7
#define KEY_LENGTH		32

#define WRITER_COUNTER_ADDR	39

#define TEEDATA_SIZE		(4*1024*1024) //4M
#define TEEDATA_BLOCK_COUNT		(TEEDATA_SIZE/256)

#ifndef offsetof
#define offsetof(s, m)		(size_t) &(((s *) 0)->m)
#endif

static int virtio_rpmb_debug = 1;
#define DPRINTF(params) do { if (virtio_rpmb_debug) pr_dbg params; } while (0)
#define WPRINTF(params) (pr_err params)

/* Make rpmb_mac compatible for different openssl versions */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
int rpmb_mac(const uint8_t *key, const struct rpmb_frame *frames,
			size_t frame_cnt, uint8_t *mac)
{
	int i;
	int hmac_ret;
	unsigned int md_len;
	HMAC_CTX hmac_ctx;

	HMAC_CTX_init(&hmac_ctx);
	hmac_ret = HMAC_Init_ex(&hmac_ctx, key, 32, EVP_sha256(), NULL);
	if (!hmac_ret) {
		DPRINTF(("HMAC_Init_ex failed\n"));
		goto err;
	}

	for (i = 0; i < frame_cnt; i++) {
		hmac_ret = HMAC_Update(&hmac_ctx, frames[i].data, 284);
		if (!hmac_ret) {
			DPRINTF(("HMAC_Update failed\n"));
			goto err;
		}
	}

	hmac_ret = HMAC_Final(&hmac_ctx, mac, &md_len);
	if (md_len != 32) {
		DPRINTF(("bad md_len %d != 32.\n", md_len));
		goto err;
	}

	if (!hmac_ret) {
		DPRINTF(("HMAC_Final failed\n"));
		goto err;
	}

err:
	HMAC_CTX_cleanup(&hmac_ctx);

	return hmac_ret ? 0 : -1;
}
#elif OPENSSL_VERSION_NUMBER >= 0x30000000L
int rpmb_mac(const uint8_t *key, const struct rpmb_frame *frames,
			size_t frame_cnt, uint8_t *mac)
{
	int i;
	int hmac_ret;
	size_t md_len;
	EVP_MAC_CTX *hmac_ctx;
	EVP_MAC *hmac = EVP_MAC_fetch(NULL, "HMAC", NULL);

	hmac_ctx = EVP_MAC_CTX_new(hmac);
	if (hmac_ctx == NULL) {
		DPRINTF(("get hmac_ctx failed\n"));
		EVP_MAC_free(hmac);
		return -1;
	}

	hmac_ret = EVP_MAC_init(hmac_ctx, key, 32, NULL);
	if (!hmac_ret) {
		DPRINTF(("HMAC_Init_ex failed\n"));
		goto err;
	}

	for (i = 0; i < frame_cnt; i++) {
		hmac_ret = EVP_MAC_update(hmac_ctx, frames[i].data, 284);
		if (!hmac_ret) {
			DPRINTF(("HMAC_Update failed\n"));
			goto err;
		}
	}

	hmac_ret = EVP_MAC_final(hmac_ctx, mac, &md_len, 32);
	if (md_len != 32) {
		DPRINTF(("bad md_len %d != 32.\n", md_len));
		goto err;
	}

	if (!hmac_ret) {
		DPRINTF(("HMAC_Final failed\n"));
		goto err;
	}

err:
	EVP_MAC_CTX_free(hmac_ctx);
	EVP_MAC_free(hmac);

	return hmac_ret ? 0 : -1;
}
#else
int rpmb_mac(const uint8_t *key, const struct rpmb_frame *frames,
			size_t frame_cnt, uint8_t *mac)
{
	int i;
	int hmac_ret;
	unsigned int md_len;
	HMAC_CTX *hmac_ctx;

	hmac_ctx = HMAC_CTX_new();
	if (hmac_ctx == NULL) {
		DPRINTF(("get hmac_ctx failed\n"));
		return -1;
	}

	hmac_ret = HMAC_Init_ex(hmac_ctx, key, 32, EVP_sha256(), NULL);
	if (!hmac_ret) {
		DPRINTF(("HMAC_Init_ex failed\n"));
		goto err;
	}

	for (i = 0; i < frame_cnt; i++) {
		hmac_ret = HMAC_Update(hmac_ctx, frames[i].data, 284);
		if (!hmac_ret) {
			DPRINTF(("HMAC_Update failed\n"));
			goto err;
		}
	}

	hmac_ret = HMAC_Final(hmac_ctx, mac, &md_len);
	if (md_len != 32) {
		DPRINTF(("bad md_len %d != 32.\n", md_len));
		goto err;
	}

	if (!hmac_ret) {
		DPRINTF(("HMAC_Final failed\n"));
		goto err;
	}

err:
	HMAC_CTX_free(hmac_ctx);

	return hmac_ret ? 0 : -1;
}
#endif

static void rpmb_sim_close(void)
{
	fclose(rpmb_fd);
	rpmb_fd = NULL;
}

static int file_write(FILE *fp, const void *buf, size_t size, off_t offset)
{
	size_t rc = 0;

	if (fseek(fp, offset, SEEK_SET)) {
		DPRINTF(("%s:Seek to %ld failed.\n", __func__, offset));
		return -1;
	}

	rc = fwrite(buf, sizeof(char), size, fp);
	if (rc != size) {
		return -1;
	}

	/* The flow of file writing sync should be:
	   C lib caches--->fflush--->disk caches--->fsync--->disk */
	if (fflush(fp) < 0) {
		return -1;
	}

	if (fsync(fileno(fp)) < 0) {
		DPRINTF(("%s: fsync failed\n", __func__));
		return -1;
	}

	return rc;
}

static int file_read(FILE *fp, void *buf, size_t size, off_t offset)
{
	size_t rc = 0;

	if (fseek(fp, offset, SEEK_SET)) {
		DPRINTF(("%s:Seek to %ld failed.\n", __func__, offset));
		return -1;
	}

	rc = fread(buf, sizeof(char), size, fp);
	if (rc == size) {
		return rc;
	} else {
		return -1;
	}
}

static int rpmb_sim_open(const char *rpmb_devname)
{
	uint8_t data = 0;
	rpmb_fd = fopen(rpmb_devname, "rb+");

	if (rpmb_fd == NULL) {
		/*if the rpmb device file does not exist, create a new file*/
		rpmb_fd = fopen(rpmb_devname, "wb+");
		DPRINTF(("rpmb device file(%s) does not exist, create a new file\n", rpmb_devname));
		/* Write 0 to the last byte to enable 4MB length access */
		if (rpmb_fd == NULL || file_write(rpmb_fd, &data, 1, TEEDATA_SIZE - 1) < 0) {
			DPRINTF(("Failed to initialize simulated rpmb to 0.\n"));
			rpmb_fd = NULL;
		}
	}

	if (rpmb_fd == NULL) {
		DPRINTF(("%s: unable (%d) to open rpmb device '%s': %s\n",
			__func__, errno, rpmb_devname, strerror(errno)));
		return -1;
	}

	return 0;
}

static int get_counter(uint32_t *counter)
{
	int rc = 0;

	rc = file_read(rpmb_fd, counter, sizeof(*counter), WRITER_COUNTER_ADDR);
	if (rc < 0)
	{
		DPRINTF(("%s failed.\n", __func__));
		return -1;
	}

	swap32(*counter);

	return 0;
}

static int set_counter(const uint32_t *counter)
{
	int rc = 0;
	uint32_t cnt = *counter;

	swap32(cnt);
	rc = file_write(rpmb_fd, &cnt, sizeof(cnt), WRITER_COUNTER_ADDR);
	if (rc < 0)
	{
		DPRINTF(("%s failed.\n", __func__));
		return -1;
	}

	return 0;
}

static int is_key_programmed(void)
{
	int rc = 0;
	uint8_t magic[KEY_MAGIC_LENGTH] = {0};

	rc = file_read(rpmb_fd, magic, KEY_MAGIC_LENGTH, KEY_MAGIC_ADDR);
	if (rc < 0)
	{
		DPRINTF(("%s read magic failed.\n", __func__));
		return 0;
	}

	if (memcmp(KEY_MAGIC, magic, KEY_MAGIC_LENGTH))
		return 0;

	return 1;
}

static int get_key(uint8_t *key)
{
	int rc = 0;

	rc = file_read(rpmb_fd, key, 32, KEY_ADDR);
	if (rc < 0)
	{
		DPRINTF(("%s failed.\n", __func__));
		return -1;
	}

	return 0;
}

static int program_key(const uint8_t *key)
{
	int rc = 0;

	rc = file_write(rpmb_fd, key, 32, KEY_ADDR);
	if (rc < 0)
	{
		DPRINTF(("%s failed at set key.\n", __func__));
		return -1;
	}

	rc = file_write(rpmb_fd, KEY_MAGIC, KEY_MAGIC_LENGTH, KEY_MAGIC_ADDR);
	if (rc < 0)
	{
		DPRINTF(("%s failed at set magic.\n", __func__));
		return -1;
	}

	return 0;
}

static int rpmb_sim_program_key(const struct rpmb_frame* in_frame, uint32_t in_cnt,
							struct rpmb_frame* out_frame, uint32_t out_cnt)
{
	int ret = 0;
	int err = RPMB_RES_WRITE_FAILURE;
	uint32_t counter = 0;

	if (in_cnt == 0  || in_frame == NULL)
		return -EINVAL;

	if (is_key_programmed())
		err = RPMB_RES_GENERAL_FAILURE;
	else
		ret = program_key(in_frame->key_mac);

	if (ret)
		goto out;

	ret = set_counter(&counter);
	if (ret)
		goto out;

	err = RPMB_RES_OK;

out:
	if (out_frame) {
		memset(out_frame, 0, out_cnt*sizeof(*out_frame));
		out_frame->req_resp = swap16(RPMB_RESP_PROGRAM_KEY);
		out_frame->result = swap16(err);
	}

	return ret;
}

static int rpmb_sim_write(const struct rpmb_frame* in_frame, uint32_t in_cnt,
					  struct rpmb_frame* out_frame, uint32_t out_cnt)
{
	int ret = 0;
	int err = RPMB_RES_WRITE_FAILURE;
	uint32_t i;
	uint8_t key[32];
	uint8_t mac[32];
	uint32_t counter;
	uint16_t addr;
	uint16_t block_count;
	uint8_t data[256*in_cnt];

	if (in_cnt == 0 || in_frame == NULL)
		return -EINVAL;

	if (in_frame[0].req_resp != swap16(RPMB_REQ_DATA_WRITE))
		return -EINVAL;

	if (in_cnt > 2) {
		err = RPMB_RES_GENERAL_FAILURE;
		goto out;
	}

	addr = swap16(in_frame[0].addr);
	block_count = swap16(in_frame[0].block_count);

	if (addr >= TEEDATA_BLOCK_COUNT) {
		err = RPMB_RES_ADDR_FAILURE;
		goto out;
	}

	if (addr + block_count > TEEDATA_BLOCK_COUNT)
		goto out;

	if (block_count == 0 || block_count > in_cnt) {
		ret = -EINVAL;
		err = RPMB_RES_GENERAL_FAILURE;
		goto out;
	}

	if (!is_key_programmed()) {
		err = RPMB_RES_NO_AUTH_KEY;
		goto out;
	}

	if (get_counter(&counter))
		goto out;

	if (counter == 0xFFFFFFFF) {
		err = RPMB_RES_WRITE_COUNTER_EXPIRED;
		goto out;
	}

	if (counter != swap32(in_frame[0].write_counter)) {
		err = RPMB_RES_COUNT_FAILURE;
		goto out;
	}

	if (get_key(key)) {
		err = RPMB_RES_GENERAL_FAILURE;
		goto out;
	}

	if (rpmb_mac(key, in_frame, in_cnt, mac)) {
		err = RPMB_RES_GENERAL_FAILURE;
		goto out;
	}

	if (memcmp(in_frame[in_cnt - 1].key_mac, mac, 32)) {
		DPRINTF(("%s wrong mac.\n", __func__));
		err = RPMB_RES_AUTH_FAILURE;
		goto out;
	}

	for (i = 0; i < in_cnt; i++)
		memcpy(data + i * 256, in_frame[i].data, 256);

	if (file_write(rpmb_fd, data, sizeof(data), 256 * addr) < 0) {
		DPRINTF(("%s write_with_retry failed.\n", __func__));
		goto out;
	}

	++counter;
	if (set_counter(&counter)) {
		DPRINTF(("%s set_counter failed.\n", __func__));
		goto out;
	}

	err = RPMB_RES_OK;

out:
	if (out_frame) {
		memset(out_frame, 0, out_cnt*sizeof(*out_frame));
		out_frame->req_resp = swap16(RPMB_RESP_DATA_WRITE);
		out_frame->result = swap16(err);
		if (err == RPMB_RES_OK) {
			out_frame->addr = swap16(addr);
			out_frame->write_counter = swap32(counter);
			rpmb_mac(key, out_frame, 1, out_frame->key_mac);
		}
	}

	return ret;
}

static int rpmb_sim_read(const struct rpmb_frame* in_frame, uint32_t in_cnt,
					 struct rpmb_frame* out_frame, uint32_t out_cnt)
{
	int ret = 0;
	uint32_t i;
	int err = RPMB_RES_READ_FAILURE;
	uint8_t key[32];
	uint8_t mac[32];
	uint16_t addr;
	uint8_t data[256*out_cnt];

	if (in_cnt != 1 || in_frame == NULL)
		return -EINVAL;

	if (in_frame->req_resp != swap16(RPMB_REQ_DATA_READ))
		return -EINVAL;

	addr = swap16(in_frame->addr);

	if (addr >= TEEDATA_BLOCK_COUNT) {
		err = RPMB_RES_ADDR_FAILURE;
		goto out;
	}

	if (addr + out_cnt > TEEDATA_BLOCK_COUNT)
		goto out;

	if (!is_key_programmed()) {
		err = RPMB_RES_NO_AUTH_KEY;
		goto out;
	}

	if (file_read(rpmb_fd, data, sizeof(data), 256 * addr) < 0) {
		DPRINTF(("%s read_with_retry failed.\n", __func__));
		goto out;
	}

	err = RPMB_RES_OK;

out:
	if (out_frame) {
		memset(out_frame, 0, out_cnt*sizeof(*out_frame));
		for (i = 0; i < out_cnt; i++) {
			memcpy(out_frame[i].nonce, in_frame[0].nonce, sizeof(in_frame[0].nonce));
			out_frame[i].req_resp = swap16(RPMB_RESP_DATA_READ);
			out_frame[i].block_count = swap16(out_cnt);
			out_frame[i].addr = in_frame[0].addr;
			memcpy(out_frame[i].data, data+256*i, 256);
		}
		if (get_key(key))
			DPRINTF(("%s, get_key failed.\n", __func__));

		out_frame[out_cnt - 1].result = swap16(err);
		rpmb_mac(key, out_frame, out_cnt, mac);
		memcpy(out_frame[out_cnt - 1].key_mac, mac, sizeof(mac));
	}

	return ret;
}

static int rpmb_sim_get_counter(const struct rpmb_frame* in_frame, uint32_t in_cnt,
							struct rpmb_frame* out_frame, uint32_t out_cnt)
{
	int ret = 0;
	int err = RPMB_RES_COUNT_FAILURE;
	uint8_t key[32];
	uint32_t counter;

	if (in_cnt != 1 || in_frame == NULL)
		return -EINVAL;

	if (in_frame->req_resp != swap16(RPMB_REQ_GET_COUNTER))
		return -EINVAL;

	if (!is_key_programmed()) {
		err = RPMB_RES_NO_AUTH_KEY;
		goto out;
	}

	if (get_key(key))
		goto out;

	if (get_counter(&counter))
		goto out;

	err = RPMB_RES_OK;

out:
	if (out_frame) {
		memset(out_frame, 0, sizeof(*out_frame)*out_cnt);
		out_frame->result = swap16(err);
		out_frame->req_resp = swap16(RPMB_RESP_GET_COUNTER);
		memcpy(out_frame->nonce, in_frame[0].nonce, sizeof(in_frame[0].nonce));

	if (err == RPMB_RES_OK) {
			out_frame->write_counter = swap32(counter);
			rpmb_mac(key, out_frame, out_cnt, out_frame->key_mac);
		}
	}

	return ret;
}

int is_use_sim_rpmb(void)
{
	int ret;

	ret = rpmb_sim_open(RPMB_SIM_PATH_NAME);
	if (ret) {
		DPRINTF(("%s: rpmb_sim_open failed\n", __func__));
		return 0;
	}

	ret = is_key_programmed();

	rpmb_sim_close();

	return ret;
}

int rpmb_sim_key_init(uint8_t *key)
{
	int ret;
	uint32_t counter = 0;

	ret = rpmb_sim_open(RPMB_SIM_PATH_NAME);
	if (ret) {
		DPRINTF(("%s: rpmb_sim_open failed\n", __func__));
		return ret;
	}

	if (!is_key_programmed()) {
		ret = program_key(key);
		if (ret) {
			DPRINTF(("%s: program_key failed\n", __func__));
			goto out;
		}
	}

	ret = get_counter(&counter);
	if (ret) {
		counter = 0;
		ret = set_counter(&counter);
		if (ret) {
			DPRINTF(("%s: set_counter failed\n", __func__));
			goto out;
		}
	}

out:
	rpmb_sim_close();

	return ret;
}

/*
 *                rel_write       write      read
 * RPMB_READ          0             1        1~N
 * RPMB_WRITE        1~N            1         1
 * GET_COUNTER        0             1         1
 * PROGRAM_KEY        1             1         1
 */
static int rpmb_sim_operations(const void *rel_write_data, size_t rel_write_size,
						const void *write_data, size_t write_size,
						void *read_buf, size_t read_size)
{
	int ret = -1;

	if (rel_write_size) {
		size_t nframe = rel_write_size/RPMB_FRAME_SIZE;
		struct rpmb_frame rel_write_frame[nframe];
		memcpy(rel_write_frame, rel_write_data, sizeof(rel_write_frame));
		if (rel_write_frame[0].req_resp == swap16(RPMB_REQ_DATA_WRITE))  {
			if (write_size/RPMB_FRAME_SIZE &&
					((struct rpmb_frame*)write_data)->req_resp == swap16(RPMB_REQ_RESULT_READ))
				ret = rpmb_sim_write(rel_write_frame, nframe, read_buf, read_size/RPMB_FRAME_SIZE);
			else
				ret = rpmb_sim_write(rel_write_frame, nframe, NULL, 0);
		}
		else if (rel_write_frame[0].req_resp == swap16(RPMB_REQ_PROGRAM_KEY)) {
			if (write_size/RPMB_FRAME_SIZE &&
					((struct rpmb_frame*)write_data)->req_resp == swap16(RPMB_REQ_RESULT_READ))
				ret = rpmb_sim_program_key(rel_write_frame, 1, read_buf, read_size/RPMB_FRAME_SIZE);
			else
				ret = rpmb_sim_program_key(rel_write_frame, 1, NULL, 0);
		}
	}
	else if (write_size) {
		struct rpmb_frame write_frame[write_size/RPMB_FRAME_SIZE];
		memcpy(write_frame, write_data, sizeof(write_frame));
		if (write_frame[0].req_resp == swap16(RPMB_REQ_DATA_READ)) {
			ret = rpmb_sim_read(write_frame, 1, read_buf, read_size/RPMB_FRAME_SIZE);
		}
		else if (write_frame[0].req_resp == swap16(RPMB_REQ_GET_COUNTER)) {
			ret = rpmb_sim_get_counter(write_frame, 1, read_buf, 1);
		}
	}

	return ret;
}

int rpmb_sim_send(const void *r)
{
	int ret;
	uint16_t i;
	uint32_t write_size = 0;
	uint32_t rel_write_size = 0;
	uint32_t read_size = 0;
	struct rpmb_frame *frame_write = NULL;
	struct rpmb_frame *frame_rel_write = NULL;
	struct rpmb_frame *frame_read = NULL;
	struct rpmb_ioc_cmd *ioc_cmd = NULL;
	const struct rpmb_ioc_seq_data *iseq = r;

	for (i = 0; i < iseq->h.num_of_cmds; i++) {
	ioc_cmd = (struct rpmb_ioc_cmd *)(&iseq->cmd[i]);
		if (ioc_cmd->flags == 0) {
			frame_read = (struct rpmb_frame *)ioc_cmd->frames_ptr;
			read_size = ioc_cmd->nframes * RPMB_FRAME_SIZE;
		} else if (ioc_cmd->flags == RPMB_F_WRITE) {
			frame_write = (struct rpmb_frame *)ioc_cmd->frames_ptr;
			write_size = ioc_cmd->nframes * RPMB_FRAME_SIZE;
		} else if (ioc_cmd->flags == (RPMB_F_WRITE | RPMB_F_REL_WRITE)) {
			frame_rel_write = (struct rpmb_frame *)ioc_cmd->frames_ptr;
			rel_write_size = ioc_cmd->nframes * RPMB_FRAME_SIZE;
		} else {
			DPRINTF(("%s: rpmb_ioc_cmd is invalid in the rpmb_ioc_seq_data\n", __func__));
			goto err_response;
		}
	}

	ret = rpmb_sim_open(RPMB_SIM_PATH_NAME);
	if (ret) {
		DPRINTF(("%s: rpmb_sim_open failed\n", __func__));
		goto err_response;
	}

	/* execute rpmb command */
	ret = rpmb_sim_operations(frame_rel_write, rel_write_size,
							 frame_write, write_size,
							 frame_read, read_size);
	rpmb_sim_close();

	if (ret) {
		DPRINTF(("%s: rpmb_sim_operations failed\n", __func__));
		goto err_response;
	}

	return 0;

err_response:
	return -1;
}
