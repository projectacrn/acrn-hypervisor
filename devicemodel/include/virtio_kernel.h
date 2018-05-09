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

/* This file provides common routines to interface with VBS-K kernel modules */

#ifndef _VIRTIO_KERNEL_H_
#define _VIRTIO_KERNEL_H_

#include "vbs_common_if.h"		/* data format between VBS-U & VBS-K */

enum VBS_K_STATUS {
	VIRTIO_DEV_INITIAL = 1,		/* initial status */
	VIRTIO_DEV_PRE_INIT,		/* detected thru cmdline option */
	VIRTIO_DEV_INIT_FAILED,		/* init failed */
	VIRTIO_DEV_INIT_SUCCESS,	/* init success */
	VIRTIO_DEV_START_FAILED,	/* start failed */
	VIRTIO_DEV_STARTED,		/* start success */
};

/* Return codes */
#define VIRTIO_SUCCESS				0
#define VIRTIO_ERROR_REENTER			1
#define VIRTIO_ERROR_FD_OPEN_FAILED		2
#define VIRTIO_ERROR_MEM_ALLOC_FAILED		3
#define VIRTIO_ERROR_START			4
#define VIRTIO_ERROR_GENERAL			5

/* VBS-K common ops */
/* VBS-K init/reset*/
int vbs_kernel_init(int fd);
int vbs_kernel_reset(int fd);

/* VBS-K start/stop */
int vbs_kernel_start(int fd, struct vbs_dev_info *dev,
		     struct vbs_vqs_info *vqs);
int vbs_kernel_stop(int fd);

#endif
