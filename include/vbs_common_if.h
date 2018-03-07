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

#endif /* _VBS_COMMON_IF_H_ */
