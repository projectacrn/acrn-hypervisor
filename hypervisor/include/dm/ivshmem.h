/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IVSHMEM_H
#define IVSHMEM_H

#define	IVSHMEM_VENDOR_ID	0x1af4U
#define	IVSHMEM_DEVICE_ID	0x1110U
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

int32_t create_ivshmem_vdev(struct acrn_vm *vm, struct acrn_emul_dev *dev);
int32_t destroy_ivshmem_vdev(struct acrn_vm *vm, struct acrn_emul_dev *dev);
#endif /* CONFIG_IVSHMEM_ENABLED */

#endif /* IVSHMEM_H */
