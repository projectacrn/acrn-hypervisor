/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IVSHMEM_H
#define IVSHMEM_H

#ifdef CONFIG_IVSHMEM_ENABLED
struct ivshmem_shm_region {
	char name[32];
	uint64_t hpa;
	uint64_t size;
};

extern const struct pci_vdev_ops vpci_ivshmem_ops;

/**
 * @brief Initialize ivshmem shared memory regions
 *
 * Initialize ivshmem shared memory regions based on user configuration.
 *
 * @return None
 */
void init_ivshmem_shared_memory(void);

#endif /* CONFIG_IVSHMEM_ENABLED */

#endif /* IVSHMEM_H */
