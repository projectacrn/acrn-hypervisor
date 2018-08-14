/*
 * Copyright (C) 2018 Intel Corporation
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "vmmapi.h"
#include "inout.h"
#include "mem.h"
#include "tpm.h"
#include "tpm_internal.h"

static int tpm_crb_debug;
#define LOG_TAG "tpm_crb: "
#define DPRINTF(fmt, args...) \
	do { if (tpm_crb_debug) printf(LOG_TAG "%s: " fmt, __func__, ##args); } while (0)
#define WPRINTF(fmt, args...) \
	do { printf(LOG_TAG "%s: " fmt, __func__, ##args); } while (0)

#define __packed __attribute__((packed))

#define CRB_LOC_CTRL_REQUEST_ACCESS      (1U << 0U)
#define CRB_LOC_CTRL_RELINQUISH          (1U << 1U)
#define CRB_LOC_CTRL_SEIZE               (1U << 2U)
#define CRB_LOC_CTRL_RESET_ESTABLISHMENT (1U << 3U)

#define CRB_CTRL_REQ_CMD_READY (1U << 0U)
#define CRB_CTRL_REQ_CMD_IDLE  (1U << 1U)

#define CRB_CTRL_CANCEL_CMD     0x00000001U
#define CRB_CTRL_CMD_CANCELLED  0x00000000U

#define CRB_CTRL_START_CMD     0x00000001U
#define CRB_CTRL_CMD_COMPLETED 0x00000000U

struct locality_state {
	uint32_t tpmEstablished : 1;
	uint32_t locAssigned    : 1;
	uint32_t activeLocality : 3;
	uint32_t reserved0      : 2;
	uint32_t tpmRegValidSts : 1;
	uint32_t reserved1      : 24;
} __packed;

struct locality_ctrl {
	uint32_t requestAccess         : 1;
	uint32_t relinquish            : 1;
	uint32_t seize                 : 1;
	uint32_t resetEstablishmentBit : 1;
	uint32_t reserved              : 28;
} __packed;

struct locality_sts {
	uint32_t granted    : 1;
	uint32_t beenSeized : 1;
	uint32_t reserved   : 30;
} __packed;

struct interface_identifier {
	struct {
		uint32_t interfaceType          : 4;
		uint32_t interfaceVersion       : 4;
		uint32_t capLocality            : 1;
		uint32_t capCRBIdleBypass       : 1;
		uint32_t reserved0              : 1;
		uint32_t capDataXferSizeSupport : 2;
		uint32_t capFIFO                : 1;
		uint32_t capCRB                 : 1;
		uint32_t capIFRes               : 2;
		uint32_t interfaceSelector      : 2;
		uint32_t intfSelLock            : 1;
		uint32_t reserved1              : 4;
		uint32_t RID                    : 8;
	} lo __packed;

	struct {
		uint32_t VID : 16;
		uint32_t DID : 16;
	} hi __packed;
} __packed;

struct control_area_ext {
	uint32_t clear;
	uint32_t remaining_bytes;
} __packed;

struct control_area_req {
	uint32_t cmdReady : 1;
	uint32_t goIdle   : 1;
	uint32_t reserved : 30;
} __packed;

struct control_area_sts {
	uint32_t tpmSts   : 1;
	uint32_t tpmIdle  : 1;
	uint32_t reserved : 30;
} __packed;

struct interrupt_enable {
	uint32_t startIntEnable              : 1;
	uint32_t cmdReadyIntEnable           : 1;
	uint32_t establishmentClearIntEnable : 1;
	uint32_t localityChangeIntEnable     : 1;
	uint32_t reserved                    : 27;
	uint32_t globalInterruptEnable       : 1;
} __packed;

struct interrupt_status {
	uint32_t startInt              : 1;
	uint32_t cmdReadyInt           : 1;
	uint32_t establishmentClearInt : 1;
	uint32_t localityChangeInt     : 1;
	uint32_t reserved              : 28;
} __packed;

struct crb_reg_space {
	union {
		struct {
			struct locality_state loc_state;
			uint32_t reserved0;
			struct locality_ctrl loc_ctrl;
			struct locality_sts loc_sts;
			uint32_t reserved1[8];
			struct interface_identifier intf_id;
			struct control_area_ext ctrl_ext;
			struct control_area_req ctrl_req;
			struct control_area_sts ctrl_sts;
			uint32_t ctrl_cancel;
			uint32_t ctrl_start;
			struct interrupt_enable int_enable;
			struct interrupt_status int_status;
			uint32_t ctrl_cmd_size;
			uint32_t ctrl_cmd_addr_lo;
			uint32_t ctrl_cmd_addr_hi;
			uint32_t ctrl_rsp_size;
			uint64_t ctrl_rsp_addr;
		};
		uint8_t bytes[TPM_CRB_REG_SIZE];
	} regs;
} __packed;

/* TPM CRB virtual device structure */
struct tpm_crb_vdev {
	struct crb_reg_space crb_regs;
	uint8_t data_buffer[TPM_CRB_DATA_BUFFER_SIZE];
	TPMCommBuffer cmd;

	pthread_t request_thread;
	pthread_mutex_t request_mutex;
	pthread_cond_t request_cond;
};

static uint64_t mmio_read(void *addr, int size)
{
	uint64_t val = 0;
	switch (size) {
	case 1:
		val = *(uint8_t *)addr;
		break;
	case 2:
		val = *(uint16_t *)addr;
		break;
	case 4:
		val = *(uint32_t *)addr;
		break;
	case 8:
		val = *(uint64_t *)addr;
		break;
	default:
		break;
	}
	return val;
}

static void mmio_write(void *addr, int size, uint64_t val)
{
	switch (size) {
	case 1:
		*(uint8_t *)addr = val;
		break;
	case 2:
		*(uint16_t *)addr = val;
		break;
	case 4:
		*(uint32_t *)addr = val;
		break;
	case 8:
		*(uint64_t *)addr = val;
		break;
	default:
		break;
	}
}

static uint64_t crb_reg_read(struct tpm_crb_vdev *tpm_vdev, uint64_t addr, int size)
{
	uint32_t val;
	uint64_t off;

	off = (addr & ~3UL) - TPM_CRB_MMIO_ADDR;

	val = mmio_read(&tpm_vdev->crb_regs.regs.bytes[off], size);

	if (addr == CRB_REGS_LOC_STATE) {
		val |= !swtpm_get_tpm_established_flag();
	}

	return val;
}

static void clear_data_buffer(struct tpm_crb_vdev *vdev)
{
	memset(vdev->data_buffer, 0, sizeof(vdev->data_buffer));
}

static uint8_t get_active_locality(struct tpm_crb_vdev *vdev)
{
	if (vdev->crb_regs.regs.loc_state.locAssigned == 0) {
		return 0xFF;
	}

	return vdev->crb_regs.regs.loc_state.activeLocality;
}

static uint32_t get_tpm_cmd_size(void *data_buffer)
{
	if (!data_buffer)
		return 0;
	/*
	 * The command header is formated by:
	 *     tag    (2 bytes): 80 01
	 *     length (4 bytes): 00 00 00 00
	 *     ordinal(4 bytes): 00 00 00 00
	 */
	return be32dec(data_buffer + 2);
}

static void tpm_crb_request_completed(struct tpm_crb_vdev *vdev, int err)
{
	vdev->crb_regs.regs.ctrl_start = CRB_CTRL_CMD_COMPLETED;
	if (err) {
		/* Fatal error */
		vdev->crb_regs.regs.ctrl_sts.tpmSts = 0b1;
	}
}

static void tpm_crb_request_deliver(void *arg)
{
	struct tpm_crb_vdev *tpm_vdev = (struct tpm_crb_vdev *)arg;
	int ret;

	while (1) {
		ret = pthread_mutex_lock(&tpm_vdev->request_mutex);
		if (ret) {
			DPRINTF("ERROR: Failed to acquire mutex lock(%d)\n", ret);
			break;
		}

		ret = pthread_cond_wait(&tpm_vdev->request_cond, &tpm_vdev->request_mutex);
		if (ret) {
			DPRINTF("ERROR: Failed to wait condition(%d)\n", ret);
			break;
		}

		ret = swtpm_handle_request(&tpm_vdev->cmd);
		tpm_crb_request_completed(tpm_vdev, ret);

		ret = pthread_mutex_unlock(&tpm_vdev->request_mutex);
		if (ret) {
			DPRINTF("ERROR: Failed to release mutex lock(%d)\n", ret);
			break;
		}
	}
}

static void crb_reg_write(struct tpm_crb_vdev *tpm_vdev, uint64_t addr, int size, uint64_t val)
{
	uint8_t target_loc = (addr >> 12) & 0b111; /* convert address to locality */
	uint32_t cmd_size;

	switch (addr) {
	case CRB_REGS_CTRL_REQ:
		if (tpm_vdev->crb_regs.regs.ctrl_start == CRB_CTRL_START_CMD)
			break;

		if (val == CRB_CTRL_REQ_CMD_READY) {
			tpm_vdev->crb_regs.regs.ctrl_sts.tpmIdle = 0;
		} else if (val == CRB_CTRL_REQ_CMD_IDLE) {
			clear_data_buffer(tpm_vdev);
			tpm_vdev->crb_regs.regs.ctrl_sts.tpmIdle = 1;
		}
		break;
	case CRB_REGS_CTRL_CANCEL:
		if ((val == CRB_CTRL_CANCEL_CMD) &&
			(tpm_vdev->crb_regs.regs.ctrl_sts.tpmIdle != 1) &&
			(tpm_vdev->crb_regs.regs.ctrl_start == CRB_CTRL_START_CMD)) {
			swtpm_cancel_cmd();
		}
		break;
	case CRB_REGS_CTRL_START:
		if ((val == CRB_CTRL_START_CMD) &&
			(tpm_vdev->crb_regs.regs.ctrl_start != CRB_CTRL_START_CMD) &&
			(tpm_vdev->crb_regs.regs.ctrl_sts.tpmIdle != 1) &&
			(get_active_locality(tpm_vdev) == target_loc)) {

			tpm_vdev->crb_regs.regs.ctrl_start = CRB_CTRL_START_CMD;
			cmd_size = MIN(get_tpm_cmd_size(tpm_vdev->data_buffer),
					TPM_CRB_DATA_BUFFER_SIZE);

			tpm_vdev->cmd.locty = 0;
			tpm_vdev->cmd.in = &tpm_vdev->data_buffer[0];
			tpm_vdev->cmd.in_len = cmd_size;
			tpm_vdev->cmd.out = &tpm_vdev->data_buffer[0];
			tpm_vdev->cmd.out_len = TPM_CRB_DATA_BUFFER_SIZE;

			if (pthread_mutex_lock(&tpm_vdev->request_mutex)) {
				DPRINTF("ERROR: Failed to acquire mutex lock\n");
				break;
			}

			if (pthread_cond_signal(&tpm_vdev->request_cond)) {
				DPRINTF("ERROR: Failed to wait condition\n");
				break;
			}

			if (pthread_mutex_unlock(&tpm_vdev->request_mutex)) {
				DPRINTF("ERROR: Failed to release mutex lock\n");
				break;
			}
		}
		break;
	case CRB_REGS_LOC_CTRL:
		switch (val) {
		case CRB_LOC_CTRL_RESET_ESTABLISHMENT:
			break;
		case CRB_LOC_CTRL_RELINQUISH:
			tpm_vdev->crb_regs.regs.loc_state.locAssigned = 0;
			tpm_vdev->crb_regs.regs.loc_sts.granted = 0;
			break;
		case CRB_LOC_CTRL_REQUEST_ACCESS:
			tpm_vdev->crb_regs.regs.loc_sts.granted = 1;
			tpm_vdev->crb_regs.regs.loc_sts.beenSeized = 0;
			tpm_vdev->crb_regs.regs.loc_state.locAssigned = 1;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

static int tpm_crb_reg_handler(struct vmctx *ctx, int vcpu, int dir, uint64_t addr,
		int size, uint64_t *val, void *arg1, long arg2)
{
	struct tpm_crb_vdev *tpm_vdev;
	tpm_vdev = (struct tpm_crb_vdev *)arg1;

	if (dir == MEM_F_READ) {
		*val = crb_reg_read(tpm_vdev, addr, size);
	} else {
		crb_reg_write(tpm_vdev, addr, size, *val);
	}

	return 0;
}

static int tpm_crb_data_buffer_handler(struct vmctx *ctx, int vcpu, int dir, uint64_t addr,
		int size, uint64_t *val, void *arg1, long arg2)
{
	struct tpm_crb_vdev *tpm_vdev;
	uint64_t off;

	tpm_vdev = (struct tpm_crb_vdev *)arg1;
	if (tpm_vdev->crb_regs.regs.ctrl_sts.tpmIdle == 1)
		return 0;

	off = addr - CRB_DATA_BUFFER;

	if (dir == MEM_F_READ) {
		*val = mmio_read(&tpm_vdev->data_buffer[off], size);
	} else {
		mmio_write(&tpm_vdev->data_buffer[off], size, *val);
	}

	return 0;
}

#define CRB_INTF_ID_TYPE_CRB_ACTIVE     0b0001
#define CRB_INTF_VERSION                0b0001
#define CRB_INTF_CAP_LOC_0_ONLY         0b0
#define CRB_INTF_CAP_FAST_IDLE          0b0
#define CRB_INTF_CAP_DATAXFER_SIZE_64   0b11
#define CRB_INTF_CAP_FIFO_NOT_SUPPORTED 0b0
#define CRB_INTF_CAP_CRB_SUPPORTED      0b1
#define CRB_INTF_CAP_INTERFACE_SEL_CRB  0b01
#define CRB_INTF_REVISION_ID            0b0000
#define CRB_INTF_VENDOR_ID              0x8086
static int tpm_crb_reset(void *dev)
{
	struct tpm_crb_vdev *tpm_vdev = (struct tpm_crb_vdev *)dev;

	memset(&tpm_vdev->crb_regs, 0, sizeof(tpm_vdev->crb_regs));

	tpm_vdev->crb_regs.regs.loc_state.tpmRegValidSts = 1U;
	tpm_vdev->crb_regs.regs.ctrl_sts.tpmIdle = 1U;
	tpm_vdev->crb_regs.regs.intf_id.lo.interfaceType = CRB_INTF_ID_TYPE_CRB_ACTIVE;
	tpm_vdev->crb_regs.regs.intf_id.lo.interfaceVersion = CRB_INTF_VERSION;
	tpm_vdev->crb_regs.regs.intf_id.lo.capLocality = CRB_INTF_CAP_LOC_0_ONLY;
	tpm_vdev->crb_regs.regs.intf_id.lo.capCRBIdleBypass = CRB_INTF_CAP_FAST_IDLE;
	tpm_vdev->crb_regs.regs.intf_id.lo.capDataXferSizeSupport = CRB_INTF_CAP_DATAXFER_SIZE_64;
	tpm_vdev->crb_regs.regs.intf_id.lo.capFIFO = CRB_INTF_CAP_FIFO_NOT_SUPPORTED;
	tpm_vdev->crb_regs.regs.intf_id.lo.capCRB = CRB_INTF_CAP_CRB_SUPPORTED;
	tpm_vdev->crb_regs.regs.intf_id.lo.interfaceSelector = CRB_INTF_CAP_INTERFACE_SEL_CRB;
	tpm_vdev->crb_regs.regs.intf_id.lo.RID = CRB_INTF_REVISION_ID;
	tpm_vdev->crb_regs.regs.intf_id.hi.VID = CRB_INTF_VENDOR_ID;

	tpm_vdev->crb_regs.regs.ctrl_cmd_size = TPM_CRB_DATA_BUFFER_SIZE;
	tpm_vdev->crb_regs.regs.ctrl_cmd_addr_lo = CRB_DATA_BUFFER;
	tpm_vdev->crb_regs.regs.ctrl_rsp_size = TPM_CRB_DATA_BUFFER_SIZE;
	tpm_vdev->crb_regs.regs.ctrl_rsp_addr = CRB_DATA_BUFFER;

	/* Emulator startup */
	if (swtpm_startup(TPM_CRB_DATA_BUFFER_SIZE)) {
		WPRINTF("Failed to startup TPM emulator!\n");
		return -1;
	}

	return 0;
}

int init_tpm_crb(struct vmctx *ctx)
{
	struct mem_range mr_cmd, mr_data;
	int error;
	struct tpm_crb_vdev *tpm_vdev;

	tpm_vdev = calloc(1, sizeof(struct tpm_crb_vdev));
	if (tpm_vdev == NULL) {
		WPRINTF("Failed alloc resource tpm device\n");
		goto fail;
	}

	ctx->tpm_dev = tpm_vdev;

	mr_cmd.name = "tpm_crb_reg";
	mr_cmd.base = TPM_CRB_MMIO_ADDR;
	mr_cmd.size = TPM_CRB_REG_SIZE;
	mr_cmd.flags = MEM_F_RW;
	mr_cmd.handler = tpm_crb_reg_handler;
	mr_cmd.arg1 = tpm_vdev;
	mr_cmd.arg2 = 0;

	error = register_mem(&mr_cmd);
	if (error) {
		WPRINTF("Failed register command area for vTPM\n");
		goto fail;
	}

	mr_data.name = "tpm_crb_buffer";
	mr_data.base = CRB_DATA_BUFFER;
	mr_data.size = TPM_CRB_DATA_BUFFER_SIZE;
	mr_data.flags = MEM_F_RW;
	mr_data.handler = tpm_crb_data_buffer_handler;
	mr_data.arg1 = tpm_vdev;
	mr_data.arg2 = 0;

	error = register_mem(&mr_data);
	if (error) {
		WPRINTF("Failed register data area for vTPM\n");
		goto fail_reset;
	}

	error = tpm_crb_reset(tpm_vdev);
	if (error) {
		WPRINTF("Failed reset vtpm device!\n");
		goto fail_reset;
	}

	error = pthread_mutex_init(&tpm_vdev->request_mutex, NULL);
	if (error) {
		WPRINTF("Failed init mutex!\n");
		goto fail_mutex;
	}

	error = pthread_cond_init(&tpm_vdev->request_cond, NULL);
	if (error) {
		WPRINTF("Failed init condition!\n");
		goto fail_cond;
	}

	error = pthread_create(&tpm_vdev->request_thread, NULL, (void *)&tpm_crb_request_deliver, (void *)tpm_vdev);
	if (error) {
		WPRINTF("Failed init request thread!\n");
		goto fail_thread;
	}

	return 0;

fail_thread:
	pthread_cond_destroy(&tpm_vdev->request_cond);

fail_cond:
	pthread_mutex_destroy(&tpm_vdev->request_mutex);

fail_mutex:
	unregister_mem(&mr_data);

fail_reset:
	unregister_mem(&mr_cmd);

fail:
	if (ctx->tpm_dev)
		free(ctx->tpm_dev);
	ctx->tpm_dev = NULL;

	return -1;
}

void deinit_tpm_crb(struct vmctx *ctx)
{
	struct mem_range mr;
	struct tpm_crb_vdev *tpm_vdev = (struct tpm_crb_vdev *)ctx->tpm_dev;
	void *status;

	mr.name = "tpm_crb_reg";
	mr.base = TPM_CRB_MMIO_ADDR;
	mr.size = TPM_CRB_REG_SIZE;
	unregister_mem(&mr);

	mr.name = "tpm_crb_buffer";
	mr.base = CRB_DATA_BUFFER;
	mr.size = TPM_CRB_DATA_BUFFER_SIZE;
	unregister_mem(&mr);

	pthread_cancel(tpm_vdev->request_thread);
	pthread_join(tpm_vdev->request_thread, &status);
	if (status != PTHREAD_CANCELED) {
		WPRINTF("Failed to cancel TPM command request thread!\n");
	}

	pthread_cond_destroy(&tpm_vdev->request_cond);
	pthread_mutex_destroy(&tpm_vdev->request_mutex);

	if (ctx->tpm_dev) {
		free(ctx->tpm_dev);
		ctx->tpm_dev = NULL;
	}
}
