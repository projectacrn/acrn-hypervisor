/*
 * Copyright (C) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _TCC_BUFFER_H_
#define _TCC__BUFFER_H_

#include <linux/ioctl.h>
#include <stdlib.h>

#define TCC_BUFFER_DEVNODE "/dev/tcc/tcc_buffer"
#define UNDEFINED_DEVNODE 0x55555555
/*
 * IOCTL MAGIC number
 */
#define IOCTL_TCC_MAGIC 'T'

/*
 * IN:
 * id: Software SRAM region id from which user request for attribute.
 * OUT:
 * latency: delay in cpu clocks
 * type: the type of the memory software SRAM region
 * size: total size in bytes
 * ways: the cache ways used to create the software SRAM region.
 * cpu_mask_p: affinity bitmask of the logical cores available for access to the software SRAM region
 */
struct tcc_mem_region_config {
	unsigned int id;
	unsigned int latency;
	size_t size;
	int type;
	unsigned int ways;
	void *cpu_mask_p;
};

/*
 * IN:
 * id: Software SRAM region id, from which user request for buffer
 * size: buffer size (bytes).
 * OUT:
 * devnode: driver returns device node to user
 */
struct tcc_buf_mem_req {
	unsigned int id;
	size_t size;
	unsigned int devnode_id;
};

enum ioctl_index {
	IOCTL_TCC_GET_REGION_COUNT = 1,
	IOCTL_TCC_GET_MEMORY_CONFIG,
	IOCTL_TCC_REQ_BUFFER,
	IOCTL_TCC_QUERY_RTCT_SIZE,
	IOCTL_TCC_GET_RTCT,
};

/*
 * User to get software SRAM region counts
 */
#define TCC_GET_REGION_COUNT _IOR(IOCTL_TCC_MAGIC, IOCTL_TCC_GET_REGION_COUNT, unsigned int *)

/*
 * For regions with software SRAM mem_type, user library asks for memory config
 */
#define TCC_GET_MEMORY_CONFIG _IOWR(IOCTL_TCC_MAGIC, IOCTL_TCC_GET_MEMORY_CONFIG, struct tcc_buf_mem_config_s *)

/*
 * User to get software SRAM region counts
 */
#define TCC_QUERY_RTCT_SIZE _IOR(IOCTL_TCC_MAGIC, IOCTL_TCC_QUERY_RTCT_SIZE, unsigned int *)

/*
 * User to get software SRAM region counts
 */
#define TCC_GET_RTCT _IOR(IOCTL_TCC_MAGIC, IOCTL_TCC_GET_RTCT, unsigned int *)

/*
 * User request tcc buffer; obtain device node
 */
#define TCC_REQ_BUFFER _IOWR(IOCTL_TCC_MAGIC, IOCTL_TCC_REQ_BUFFER, struct tcc_buf_mem_req *)

#endif
