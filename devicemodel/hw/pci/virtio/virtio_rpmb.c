/*
 * Virtio Rpmb backend.
 *
 * Copyright (C) 2018 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Contact Information: weideng <wei.a.deng@intel.com>
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
 *
 * Create virtio rpmb backend VBS-U. This component will work with RPMB FE
 * driver to provide one communication channel between UOS and SOS.
 * The message from RPMB daemon in Android will be transferred over the
 * channel and finally arrived RPMB physical driver on SOS kernel.
 *
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include "dm.h"
#include "pci_core.h"
#include "virtio.h"
#include "vmmapi.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include "rpmb.h"
#include "rpmb_sim.h"
#include "rpmb_backend.h"
#include "att_keybox.h"

#define VIRTIO_RPMB_RINGSZ			64
#define VIRTIO_RPMB_MAXSEGS			5
#define ERROR_ADDRESS_OUT_OF_RANGE	-2
#define BLOCK_BARA_BASE_ADDRESS		1
#define BLOCK_BARA_SIGNATURE		"BARA"
#define SIGNATURE_LENGTH			(sizeof(BLOCK_BARA_SIGNATURE) - 1)
#define ATTKB_PRESENT_FLAG_BIT		0x1

static const char PHYSICAL_RPMB_STR[] = "physical_rpmb";
static int virtio_rpmb_debug = 1;
#define DPRINTF(params) do { if (virtio_rpmb_debug) pr_dbg params; } while (0)
#define WPRINTF(params) (pr_err params)

static __u16 rpmb_block_count = 0;
/*
 * virtio-rpmb struct
 */
struct virtio_rpmb {
	struct virtio_base base;
	struct virtio_vq_info vq;
	pthread_mutex_t mtx;
	/*
	 * Different UOS (with vmid) will access physical rpmb area
	 * with different offsets.
	 */
	int vmid;
};

struct virtio_rpmb_ioctl_cmd {
	unsigned int cmd;	/*ioctl cmd*/
	int result;		/* result for ioctl cmd*/
	__u8 target;     /* for emmc */
	__u8 reserved[3];   /* not used */
};

struct virtio_rpmb_ioc_seq_data {
	__u64 num_of_cmds;	/*num of  seq cmds*/
	struct rpmb_cmd cmds[SEQ_CMD_MAX + 1];
};

static int
rpmb_check_mac(__u8 *key, struct rpmb_frame *frames, __u8 frame_cnt)
{
	int rc = -1;
	__u8 mac[32];

	if (!key || !frames) {
		DPRINTF(("key or frames is NULL\n"));
		return -1;
	}

	if (frame_cnt == 0) {
		DPRINTF(("frame count is zero\n"));
		return -1;
	}

	rc = rpmb_mac(key, frames, frame_cnt, mac);
	if (rc < 0) {
		DPRINTF(("rpmb_calc_mac failed\n"));
		return rc;
	}

	if (memcmp(mac, frames[frame_cnt - 1].key_mac, 32)) {
		DPRINTF(("rpmb mac mismatch:\n"));
		return -1;
	}

	return rc;
}

static int
rpmb_check_response(const char *cmd_str, enum rpmb_response response_type,
				const struct rpmb_frame *frames, __u32 frame_cnt,
				const __u8 *key, const __u8 *nonce, const __u16 *addr)
{
	__u32 i;
	__u8 mac[32];

	for (i = 0; i < frame_cnt; i++) {
		if (swap16(frames[i].req_resp) != response_type) {
			DPRINTF(("%s: Bad response type, 0x%x, expected 0x%x\n",
					cmd_str, swap16(frames[i].req_resp), response_type));
			return -1;
		}

		if (swap16(frames[i].result) != RPMB_RES_OK) {
			if (swap16(frames[i].result) == RPMB_RES_ADDR_FAILURE) {
				DPRINTF(("%s: Addr failure, %u\n", cmd_str, swap16(frames[i].addr)));
				return ERROR_ADDRESS_OUT_OF_RANGE;
			}
			DPRINTF(("%s: Bad result, 0x%x\n", cmd_str, swap16(frames[i].result)));
			return -1;
		}

		if (nonce && memcmp(frames[i].nonce, nonce, sizeof(frames[i].nonce))) {
			DPRINTF(("%s: Bad nonce\n", cmd_str));
			return -1;
		}

		if (addr && *addr != swap16(frames[i].addr)) {
			DPRINTF(("%s: Bad addr, got %u, expected %u\n",
				cmd_str, swap16(frames[i].addr), *addr));
			return -1;
		}
	}

	if (key) {
		if (rpmb_mac(key, frames, frame_cnt, mac)) {
			DPRINTF(("%s: rpmb_mac failed\n", cmd_str));
			return -1;
		}

		if (memcmp(frames[frame_cnt - 1].key_mac, mac, sizeof(mac))) {
			DPRINTF(("%s: Bad MAC\n", cmd_str));
			return -1;
		}
	}

	return 0;
}

int
rpmb_get_counter(__u8 mode, __u8 *key, __u32 *counter, __u16 *result)
{
	int rc;
	int fd;
	struct {
		struct rpmb_ioc_seq_cmd h;
		struct rpmb_ioc_cmd cmd[3];
	} iseq = {};
	struct rpmb_frame frame_in;
	struct rpmb_frame frame_out;

	if (!key || !counter || !result) {
		DPRINTF(("key, counter or result is NULL!\n"));
		return -1;
	}

	frame_in.req_resp = swap16(RPMB_REQ_GET_COUNTER);

	iseq.cmd[0].flags = RPMB_F_WRITE;
	iseq.cmd[0].nframes = 1;
	iseq.cmd[0].frames_ptr = (__aligned_u64)(intptr_t)(&frame_in);
	iseq.cmd[1].flags = 0;
	iseq.cmd[1].nframes = 1;
	iseq.cmd[1].frames_ptr = (__aligned_u64)(intptr_t)(&frame_out);
	iseq.h.num_of_cmds = 2;

	if (mode == RPMB_PHY_MODE) {
		fd = open(RPMB_PHY_PATH_NAME, O_RDWR | O_NONBLOCK);
		if (fd < 0) {
			DPRINTF(("failed to open %s.\n", RPMB_PHY_PATH_NAME));
			return fd;
		}

		rc = ioctl(fd, RPMB_IOC_SEQ_CMD, &iseq);
		close(fd);
		if (rc) {
			DPRINTF(("get counter for physical rpmb failed.\n"));
			return rc;
		}
	} else {
		rc = rpmb_sim_send(&iseq);
		if (rc) {
			DPRINTF(("get counter for simulated rpmb failed.\n"));
			return rc;
		}
	}

	*result = swap16(frame_out.result);
	if (*result != RPMB_RES_OK ) {
		DPRINTF(("get rpmb counter failed(0x%x).\n", *result));
		return -1;
	}

	/*In PHY RPMB MODE, DM doesn't have real RPMB key,
	 *so no necessary to check the mac in the response.
	 */
	if (mode != RPMB_PHY_MODE) {
		rc = rpmb_check_mac(key, &frame_out, 1);
		if (rc) {
			DPRINTF(("rpmb counter check mac failed.\n"));
			return rc;
		}
	}

	*counter = swap32(frame_out.write_counter);
	DPRINTF(("rpmb counter value: 0x%x.\n", *counter));

	return rc;
}

static int
rpmb_write_block(__u8 mode, __u8 *key, __u16 addr, void *buf, __u32 count)
{
	int rc;
	int fd;
	__u32 i;
	__u32 write_counter;
	__u16 result;
	struct {
		struct rpmb_ioc_seq_cmd h;
		struct rpmb_ioc_cmd cmd[3];
	} iseq = {};
	struct rpmb_frame frame_write;
	struct rpmb_frame frame_rel[count];
	struct rpmb_frame frame_read;

	if (!buf || count == 0) {
		DPRINTF(("%s:buf or count is invalid!\n", __func__));
		return -ENOBUFS;
	}

	rc = rpmb_get_counter(mode, key, &write_counter, &result);
	if (rc) {
		DPRINTF(("%s: virtio_rpmb_get_counter failed\n", __func__));
		return rc;
	}

	frame_write.addr = swap16(addr);
	frame_write.req_resp = swap16(RPMB_REQ_RESULT_READ);
	frame_write.write_counter = swap32(write_counter);

	for (i = 0; i < count; i++) {
		memset(&frame_rel[i], 0, sizeof(frame_rel[i]));
		memcpy(frame_rel[i].data, buf + i * sizeof(frame_rel[i].data), sizeof(frame_rel[i].data));
		frame_rel[i].write_counter = swap32(write_counter);
		frame_rel[i].addr = swap16(addr);
		frame_rel[i].block_count = swap16(count);
		frame_rel[i].req_resp = swap16(RPMB_REQ_DATA_WRITE);
	}

	iseq.cmd[0].flags = RPMB_F_WRITE | RPMB_F_REL_WRITE;
	iseq.cmd[0].nframes = count;
	iseq.cmd[0].frames_ptr = (__aligned_u64)(intptr_t)(frame_rel);
	iseq.cmd[1].flags = RPMB_F_WRITE;
	iseq.cmd[1].nframes = 1;
	iseq.cmd[1].frames_ptr = (__aligned_u64)(intptr_t)(&frame_write);
	iseq.cmd[2].flags = 0;
	iseq.cmd[2].nframes = 1;
	iseq.cmd[2].frames_ptr = (__aligned_u64)(intptr_t)(&frame_read);
	iseq.h.num_of_cmds = 3;

	if (mode == RPMB_PHY_MODE) {
		fd = open(RPMB_PHY_PATH_NAME, O_RDWR | O_NONBLOCK);
		if (fd < 0) {
			DPRINTF(("failed to open %s for read blocks.\n", RPMB_PHY_PATH_NAME));
			return fd;
		}

		rc = ioctl(fd, RPMB_IOC_SEQ_CMD, &iseq);
		close(fd);
		if (rc) {
			DPRINTF(("read blocks for physical rpmb failed.\n"));
			return rc;
		}

		/*In PHY RPMB MODE, DM doesn't have real RPMB key,
		 *so no necessary to check the mac in the response.
		 */
		rc = rpmb_check_response("write blocks", RPMB_RESP_DATA_WRITE,
								&frame_read, 1, NULL, NULL, &addr);
	} else {
		rc = rpmb_sim_send(&iseq);
		if (rc) {
			DPRINTF(("read blocks for simulated rpmb failed.\n"));
			return rc;
		}

		rc = rpmb_check_response("write blocks", RPMB_RESP_DATA_WRITE,
								&frame_read, 1, key, NULL, &addr);
	}

	return rc;
}

static int
rpmb_read_block(__u8 mode, __u8 *key, __u16 addr, void *buf, __u32 count)
{
	int rc;
	int fd;
	__u8 *bufp;
	__u32 i;
	struct {
		struct rpmb_ioc_seq_cmd h;
		struct rpmb_ioc_cmd cmd[3];
	} iseq = {};
	struct rpmb_frame frame_in;
	struct rpmb_frame frame_out[count];

	if (!buf || count == 0) {
		DPRINTF(("buf or count is invalid!.\n"));
		return -1;
	}

	frame_in.addr = swap16(addr);
	frame_in.req_resp = swap16(RPMB_REQ_DATA_READ);

	iseq.cmd[0].flags = RPMB_F_WRITE;
	iseq.cmd[0].nframes = 1;
	iseq.cmd[0].frames_ptr = (__aligned_u64)(intptr_t)(&frame_in);
	iseq.cmd[1].flags = 0;
	iseq.cmd[1].nframes = count;
	iseq.cmd[1].frames_ptr = (__aligned_u64)(intptr_t)(frame_out);
	iseq.h.num_of_cmds = 2;

	if (mode == RPMB_PHY_MODE) {
		fd = open(RPMB_PHY_PATH_NAME, O_RDWR | O_NONBLOCK);
		if (fd < 0) {
			DPRINTF(("failed to open %s for read blocks.\n", RPMB_PHY_PATH_NAME));
			return fd;
		}

		rc = ioctl(fd, RPMB_IOC_SEQ_CMD, &iseq);
		close(fd);
		if (rc) {
			DPRINTF(("read blocks for physical rpmb failed.\n"));
			return rc;
		}
	} else {
		rc = rpmb_sim_send(&iseq);
		if (rc) {
			DPRINTF(("read blocks for simulated rpmb failed.\n"));
			return rc;
		}
	}

	rc = rpmb_check_response("read blocks", RPMB_RESP_DATA_READ,
							frame_out, count, NULL, NULL, &addr);

	if (rc)
		return rc;

	for (bufp = buf, i = 0; i < count; i++, bufp += sizeof(frame_out[i].data))
		memcpy(bufp, frame_out[i].data, sizeof(frame_out[i].data));

	return rc;
}

static int
rpmb_check(__u8 mode, __u8 *key, __u16 block)
{
	int rc;
	__u8 tmp[RPMB_BLOCK_SIZE];

	rc = rpmb_read_block(mode, key, block, tmp, 1);
	DPRINTF(("check rpmb_block %d, ret %d\n", block, rc));

	return rc;
}

static __u32
rpmb_search_size(__u8 mode, __u8 *key, __u16 hint)
{
	int ret;
	__u32 low = 0;
	__u16 high = ~0;
	__u16 curr = hint - 1;

	while (low <= high) {
		ret = rpmb_check(mode, key, curr);
		switch (ret) {
		case 0:
			low = curr + 1;
			break;
		case ERROR_ADDRESS_OUT_OF_RANGE:
			high = curr - 1;
			break;
		default:
			return 0;
		};
		if (ret || curr != hint) {
			curr = low + (high - low) / 2;
			hint = curr;
		} else {
			curr = curr + 1;
		}
	}
	if ((__u32)high + 1 != low) {
		DPRINTF(("rpmb search size fail!\n"));
		return 0;
	}
	return low;
}

__u16
rpmb_get_blocks(void)
{
	return rpmb_block_count;
}

static int
rpmb_read_bara(__u8 mode, __u8 *key, rpmb_block_t *block_table)
{
	int ret;

	ret = rpmb_read_block(mode, key, BLOCK_BARA_BASE_ADDRESS, block_table, 1);
	if (ret) {
		DPRINTF(("rpmb read block table fail!\n"));
	}

	return ret;
}

static void
rpmb_bara_init(rpmb_block_t *block_table, uint16_t size)
{
	memcpy(block_table->signature, BLOCK_BARA_SIGNATURE, SIGNATURE_LENGTH);
	block_table->length = RPMB_BLOCK_SIZE;
	block_table->revision = 0;
	block_table->flag |= ATTKB_PRESENT_FLAG_BIT;
	block_table->attkb_addr = BLOCK_BARA_BASE_ADDRESS + 1;
	block_table->attkb_size = size;
	block_table->attkb_svn = 0;
}

/* Read RPMB BARA block from RPMB to check if AttKB exists or not.
 * If does NOT exist, DM will send cmd to CSE via HECI to get AttKB size
 * and AttKB. Both AttKB and BARA block will be written to specified RPMB address.
 */
static int
rpmb_keybox_retrieve(__u8 mode, __u8 *key)
{
	int ret;
	uint32_t i;
	uint32_t block_num;
	rpmb_block_t *block_table;
	uint8_t *attkb = NULL;
	uint16_t kb_size;
	uint32_t kb_buf_size = 0;

	block_table = malloc(sizeof(rpmb_block_t));
	if (!block_table) {
		DPRINTF(("%s: block table malloc fail!\n", __func__));
		return -ENOMEM;
	}

	memset(block_table, 0, sizeof(rpmb_block_t));
	/* read block table */
	ret = rpmb_read_bara(mode, key, block_table);
	if (ret) {
		DPRINTF(("get block table fail!\n"));
		goto out;
	}

	if (memcmp(BLOCK_BARA_SIGNATURE, block_table->signature, SIGNATURE_LENGTH) || !(block_table->flag & ATTKB_PRESENT_FLAG_BIT)) {
		kb_size = get_attkb_size();
		if (kb_size == 0) {
			DPRINTF(("rpmb get_attkb_size fail!\n"));
			ret = -1;
			goto out;
		}

		if (kb_size % RPMB_BLOCK_SIZE) {
			kb_buf_size = (kb_size / RPMB_BLOCK_SIZE + 1) * RPMB_BLOCK_SIZE;
		} else {
			kb_buf_size = kb_size;
		}

		attkb = (uint8_t *)malloc(kb_buf_size);
		if (!attkb) {
			DPRINTF(("%s: attkb malloc fail!\n", __func__));
			ret = -ENOMEM;
			goto out;
		}

		memset(attkb, 0, kb_buf_size);
		ret = read_attkb(attkb, kb_size);
		if (ret == 0) {
			DPRINTF(("failed to read attkb"));
			ret = -1;
			goto out;
		}

		rpmb_bara_init(block_table, kb_size);
		block_num = (kb_size - 1) / RPMB_BLOCK_SIZE + 1;
		for (i = 0; i < block_num; i++) {
			ret = rpmb_write_block(mode, key, block_table->attkb_addr + i, attkb + i * RPMB_BLOCK_SIZE, 1);
			if (ret) {
				DPRINTF(("rpmb write key box fail!\n"));
				goto out;
			}
		}

		ret = rpmb_write_block(mode, key, BLOCK_BARA_BASE_ADDRESS, block_table, 1);
		if (ret) {
			DPRINTF(("rpmb write block table fail!\n"));
			goto out;
		}
	}

out:
	if (attkb) {
		memset(attkb, 0, kb_buf_size);
		free(attkb);
	}

	free(block_table);
	return ret;
}

static int
virtio_rpmb_seq_handler(struct virtio_rpmb *rpmb, struct iovec *iov,
	int n, int *tlen)
{
	struct virtio_rpmb_ioctl_cmd *ioc = NULL;
	struct virtio_rpmb_ioc_seq_data *seq;
	struct rpmb_frame *frames;
	void *pdata;
	int rc;
	int i;
	int size;

	if (n < 3 || n > VIRTIO_RPMB_MAXSEGS) {
		DPRINTF(("found invalid args!!!\n"));
		return -1;
	}
	if (!rpmb || !iov || !tlen) {
		DPRINTF(("found invalid args!!!\n"));
		return -1;
	}

	ioc = (struct virtio_rpmb_ioctl_cmd *)(iov[0].iov_base);
	if (!ioc) {
		DPRINTF(("error, get ioc is NULL!\n"));
		return -1;
	}
	*tlen = iov[0].iov_len;

	seq = (struct virtio_rpmb_ioc_seq_data *)(iov[1].iov_base);
	if (!seq) {
		DPRINTF(("fail to get seq data\n"));
		return -1;
	}
	if ((n-2) != seq->num_of_cmds) {
		DPRINTF(("found invalid command number!!!\n"));
		return -1;
	}
	pdata = (void *)seq;

	for (i = 2; i < n; i++) {
		frames = (struct rpmb_frame *)(iov[i].iov_base);
		if (!frames) {
			DPRINTF(("fail to get frame data\n"));
			return -1;
		}

		size = (seq->cmds[i-2].nframes ? :1)* sizeof(struct rpmb_frame);
		if (size != iov[i].iov_len) {
			DPRINTF(("Invalid frame length!!!\n"));
			return -1;
		}
		seq->cmds[i-2].frames =
			(struct rpmb_frame *)(iov[i].iov_base);
	}

	rc = rpmb_handler(ioc->cmd, pdata);
	if (rc)
		DPRINTF(("seq ioctl cmd failed(%d).\n", rc));

	return rc;
}

/*
 * To meet the communication protocol from vRPMB FE,
 * each time we will receive 3 or 4 or 5 iovs.
 */
static void
virtio_rpmb_notify(void *base, struct virtio_vq_info *vq)
{
	struct iovec iov[VIRTIO_RPMB_MAXSEGS + 1];
	int n;
	int tlen = 0;
	uint16_t idx;
	struct virtio_rpmb *rpmb = (struct virtio_rpmb *)base;
	struct virtio_rpmb_ioctl_cmd *ioc;

	while (vq_has_descs(vq)) {
		n = vq_getchain(vq, &idx, iov, VIRTIO_RPMB_MAXSEGS, NULL);
		if (n < 3 || n > VIRTIO_RPMB_MAXSEGS) {
			DPRINTF(("Invalid package number.\n"));
			return;
		}
		ioc = (struct virtio_rpmb_ioctl_cmd *)(iov[0].iov_base);
		if (RPMB_IOC_SEQ_CMD != ioc->cmd) {
			DPRINTF(("found invalid command type.\n"));
			return;
		}
		ioc->result = virtio_rpmb_seq_handler(rpmb, iov, n, &tlen);
		if (tlen <= 0) {
			DPRINTF(("Invalid return length.\n"));
			return;
		}
		/*
		 * Release this chain and handle more
		 */
		vq_relchain(vq, idx, tlen);
	}
	vq_endchains(vq, 1);	/* Generate interrupt if appropriate. */
}

static void
virtio_rpmb_reset(void *base)
{
	struct virtio_rpmb *rpmb;

	if (!base) {
		DPRINTF(("error, invalid args!\n"));
		return;
	}

	rpmb = base;

	DPRINTF(("virtio_rpmb: device reset requested !\n"));
	virtio_reset_dev(&rpmb->base);
}

static struct virtio_ops virtio_rpmb_ops = {
	"virtio_rpmb",		/* our name */
	1,			/* we support 1 virtqueue */
	0,			/* config reg size */
	virtio_rpmb_reset,	/* reset */
	virtio_rpmb_notify,	/* device-wide qnotify */
	NULL,			/* read virtio config */
	NULL,			/* write virtio config */
	NULL,			/* apply negotiated features */
	NULL,			/* called on guest set status */
};

static int
virtio_rpmb_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_rpmb *rpmb;
	pthread_mutexattr_t attr;
	int rc;
	__u8 key[RPMB_KEY_32_LEN];
	__u32 rpmb_counter = 0;
	__u16 rpmb_result = 0;

	rpmb = calloc(1, sizeof(struct virtio_rpmb));
	if (!rpmb) {
		DPRINTF(("error, unable to calloc rpmb buffer!\n"));
		return -1;
	}

	/* init mutex attribute properly */
	rc = pthread_mutexattr_init(&attr);
	if (rc) {
		DPRINTF(("mutexattr init failed with error %d!\n", rc));
		goto out;
	}
	if (virtio_uses_msix()) {
		rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_DEFAULT);
		if (rc) {
			DPRINTF(("settype(DEFAULT) failed %d!\n", rc));
			goto out;
		}
	} else {
		rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		if (rc) {
			DPRINTF(("settype(RESURSIVE) failed %d!\n", rc));
			goto out;
		}
	}
	rc = pthread_mutex_init(&rpmb->mtx, &attr);
	if (rc) {
		DPRINTF(("mutex init failed with error %d!\n", rc));
		goto out;
	}

	virtio_linkup(&rpmb->base, &virtio_rpmb_ops, rpmb, dev, &rpmb->vq, BACKEND_VBSU);

	rpmb->base.mtx = &rpmb->mtx;
	rpmb->vq.qsize = VIRTIO_RPMB_RINGSZ;
	rpmb->vmid = ctx->vmid;

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_RPMB);
	pci_set_cfgdata16(dev, PCIR_VENDOR, INTEL_VENDOR_ID);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_OTHER);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_RPMB);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, INTEL_VENDOR_ID);

	rc = virtio_interrupt_init(&rpmb->base, virtio_uses_msix());
	if (rc) {
		DPRINTF(("virtio_interrupt_init failed (%d)!\n", rc));
		goto out;
	}

	virtio_set_io_bar(&rpmb->base, 0);

	rc = get_virt_rpmb_key();
	if (rc == 0) {
		DPRINTF(("%s: get uos key failed!\n", __func__));
		goto out;
	}

	memset(key, 0, RPMB_KEY_32_LEN);

	if (opts && !strncmp(opts, PHYSICAL_RPMB_STR, sizeof(PHYSICAL_RPMB_STR))) {
		DPRINTF(("RPMB in physical mode!\n"));
		rpmb_mode_init(RPMB_PHY_MODE);
		rpmb_block_count = rpmb_search_size(RPMB_PHY_MODE, key, 0);
		rc = rpmb_keybox_retrieve(RPMB_PHY_MODE, key);
		if (rc < 0) {
			DPRINTF(("rpmb_keybox_retrieve failed!\n"));
		}
	} else {
		DPRINTF(("RPMB in simulated mode!\n"));
		rc = rpmb_sim_key_init(key);
		if (rc) {
			DPRINTF(("rpmb_sim_key_init failed!\n"));
			goto out;
		}

		rc = rpmb_get_counter(RPMB_SIM_MODE, key, &rpmb_counter, &rpmb_result);
		if (rc) {
			DPRINTF(("rpmb_get_counter failed\n"));
			goto out;
		}

		rpmb_mode_init(RPMB_SIM_MODE);
		rpmb_block_count = rpmb_search_size(RPMB_SIM_MODE, key, 0);
	}

	memset(key, 0, RPMB_KEY_32_LEN);
	rpmb_counter_init(rpmb_counter);

	return 0;

out:
	free(rpmb);
	memset(key, 0, RPMB_KEY_32_LEN);
	return rc;
}

static void
virtio_rpmb_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	if (dev->arg) {
		DPRINTF(("virtio_rpmb_be_deinit: free struct virtio_rpmb!\n"));
		free((struct virtio_rpmb *)dev->arg);
	}
}

struct pci_vdev_ops pci_ops_virtio_rpmb = {
	.class_name	= "virtio-rpmb",
	.vdev_init	= virtio_rpmb_init,
	.vdev_deinit	= virtio_rpmb_deinit,
	.vdev_barwrite	= virtio_pci_write,
	.vdev_barread	= virtio_pci_read
};
DEFINE_PCI_DEVTYPE(pci_ops_virtio_rpmb);
