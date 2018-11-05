/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef UCODE_H
#define UCODE_H

struct ucode_header {
	uint32_t	header_ver;
	uint32_t	update_ver;
	uint32_t	date;
	uint32_t	proc_sig;
	uint32_t	checksum;
	uint32_t	loader_ver;
	uint32_t	proc_flags;
	uint32_t	data_size;
	uint32_t	total_size;
	uint32_t	reserved[3];
};

void acrn_update_ucode(struct acrn_vcpu *vcpu, uint64_t v);
uint64_t get_microcode_version(void);

#endif /* UCODE_H */
