/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/* Shared data structure between VBS-U and VBS-K */

#ifndef _VBS_COMMON_IF_H_
#define _VBS_COMMON_IF_H_

#include "types.h"

#define VBS_MAX_VQ_CNT	10
#define VBS_NAME_LEN	32
struct vbs_vq_info {
	uint16_t qsize;		/* size of this queue (a power of 2) */
	uint32_t pfn;		/* PFN of virt queue (not shifted!) */
	uint16_t msix_idx;	/* MSI-X index, or VIRTIO_MSI_NO_VECTOR */
	uint64_t msix_addr;
	uint32_t msix_data;
};

struct vbs_vqs_info {
	uint32_t nvq;		/* number of virtqueues */
	struct vbs_vq_info vqs[VBS_MAX_VQ_CNT];
				/* array of struct vbs_vq_info */
};

struct vbs_dev_info {
	char name[VBS_NAME_LEN];/* VBS name */
	int vmid;		/* VMID this device belongs to */
	int nvq;		/* virtqueue # */
	uint32_t negotiated_features;
				/* features after VIRTIO_CONFIG_S_DRIVER_OK */
	uint64_t pio_range_start;
	uint64_t pio_range_len;	/* PIO bar address initialized by guest OS */
};

/* reuse vhost ioctl index */
#define VBS_K_IOCTL	0xAF

#define VBS_K_SET_DEV _IOW(VBS_K_IOCTL, 0x00, struct vbs_dev_info)
#define VBS_K_SET_VQ _IOW(VBS_K_IOCTL, 0x01, struct vbs_vqs_info)
#define VBS_K_RESET_DEV _IO(VBS_K_IOCTL, 0x02)

#endif /* _VBS_COMMON_IF_H_ */
