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

/* Routines to notify the VBS-K in kernel */

#include <stdio.h>
#include <sys/ioctl.h>
#include "virtio_kernel.h"

static int virtio_kernel_debug;
#define DPRINTF(params) do { if (virtio_kernel_debug) printf params; } while (0)
#define WPRINTF(params) (printf params)

static int
vbs_dev_info_set(int fd, void *arg)
{
	return ioctl(fd, VBS_K_SET_DEV, arg);
}

static int
vbs_vqs_info_set(int fd, void *arg)
{
	return ioctl(fd, VBS_K_SET_VQ, arg);
}

/* VBS-K common ops */
/* VBS-K init/reset */
int
vbs_kernel_init(int fd)
{
	return VIRTIO_SUCCESS;
}

int
vbs_kernel_reset(int fd)
{
	return VIRTIO_SUCCESS;
}

/*
 * We need a way to start/stop vbs_k execution since guest might want to
 * change the configuration of the virtio device after VBS-K has been
 * initialized.
 */
/* VBS-K start/stop */
int
vbs_kernel_start(int fd, struct vbs_dev_info *dev, struct vbs_vqs_info *vqs)
{
	int ret;

	if (fd < 0) {
		WPRINTF(("%s: fd < 0\n", __func__));
		return -VIRTIO_ERROR_FD_OPEN_FAILED;
	}

	ret = vbs_dev_info_set(fd, dev);
	if (ret < 0) {
		WPRINTF(("vbs_kernel_set_dev failed: ret %d\n", ret));
		return ret;
	}

	ret = vbs_vqs_info_set(fd, vqs);
	if (ret < 0) {
		WPRINTF(("vbs_kernel_set_vqs failed: ret %d\n", ret));
		return ret;
	}

	return VIRTIO_SUCCESS;
}

int
vbs_kernel_stop(int fd)
{
	DPRINTF(("%s\n", __func__));
	return VIRTIO_SUCCESS;
}
