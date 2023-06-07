/*
 * Copyright (C) 2020-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IVSHMEM_H
#define IVSHMEM_H

#define	IVSHMEM_VENDOR_ID	0x1af4U
#define	IVSHMEM_DEVICE_ID	0x1110U
#ifdef CONFIG_IVSHMEM_ENABLED

/*
 * Max number of peers for each ivshmem region, and
 * VM ID is used as peer IDs of this VM's ivshmem devices.
 */
#define MAX_IVSHMEM_PEER_NUM (CONFIG_MAX_VM_NUM)

/* Max number of MSIX table entries of shmem device. */
#define MAX_IVSHMEM_MSIX_TBL_ENTRY_NUM 8U
struct ivshmem_shm_region {
	char name[32];
	uint64_t hpa;
	uint64_t size;
	struct ivshmem_device *doorbell_peers[MAX_IVSHMEM_PEER_NUM];
};

extern const struct pci_vdev_ops vpci_ivshmem_ops;

/**
 * @brief Initialize ivshmem shared memory regions
 *
 * Initialize ivshmem shared memory regions based on user configuration.
 */
void init_ivshmem_shared_memory(void);

int32_t create_ivshmem_vdev(struct acrn_vm *vm, struct acrn_vdev *dev);
int32_t destroy_ivshmem_vdev(struct pci_vdev *vdev);
#endif /* CONFIG_IVSHMEM_ENABLED */

#endif /* IVSHMEM_H */
