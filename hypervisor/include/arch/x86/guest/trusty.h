/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TRUSTY_H_
#define TRUSTY_H_
#include <acrn_hv_defs.h>
#include <x86/seed.h>

#define RPMB_MAX_PARTITION_NUMBER       6U
#define MMC_PROD_NAME_WITH_PSN_LEN      15U

#define TRUSTY_RAM_SIZE	(16UL * 1024UL * 1024UL)	/* 16 MB for now */

/* Trusty EPT rebase gpa: 511G */
#define TRUSTY_EPT_REBASE_GPA (511UL * 1024UL * 1024UL * 1024UL)

#define NON_TRUSTY_PDPT_ENTRIES         511U

struct acrn_vcpu;
struct acrn_vm;

/* Structure of key info */
struct trusty_key_info {
	uint32_t size_of_this_struct;

	/* version info:
		0: baseline structure
		1: add ** new field
	 */
	uint32_t version;

	/* platform:
		0: Dummy (fake secret)
		1: APL (APL + ABL)
		2: ICL (ICL + SBL)
		3: ACRN (APL|ICL + SBL + ACRN)
		4: Brillo (Android Things)
	*/
	uint32_t platform;

	/* flags info:
		Bit 0: manufacturing state (0:manufacturing done;
					    1:in manufacturing mode)
		Bit 1: secure boot state (0:disabled; 1: enabled)
		Bit 2: test seeds (ICL only - 0:production seeds; 1: test seeds)
		other bits all reserved as 0
	*/
	uint32_t flags;

	/* Keep 64-bit align */
	uint32_t pad1;

	/* Seed list, include useeds(user seeds) and dseed(device seeds) */
	uint32_t num_seeds;
	struct seed_info useed_list[BOOTLOADER_SEED_MAX_ENTRIES];
	struct seed_info dseed_list[BOOTLOADER_SEED_MAX_ENTRIES];

	/* For ICL+ */
	/* rpmb keys, Currently HMAC-SHA256 is used in RPMB spec
	 * and 256-bit (32byte) is enough. Hence only lower 32 bytes will be
	 * used for now for each entry. But keep higher 32 bytes for future
	 * extension. Note that, RPMB keys are already tied to storage device
	 * serial number.If there are multiple RPMB partitions, then we will
	 * get multiple available RPMB keys. And if rpmb_key[n][64] == 0,
	 * then the n-th RPMB key is unavailable (Either because of no such
	 *  RPMB partition, or because OSloader doesn't want to share
	 *  the n-th RPMB key with Trusty)
	 */
	uint8_t rpmb_key[RPMB_MAX_PARTITION_NUMBER][64];

	/* 256-bit AES encryption key to encrypt/decrypt attestation keybox,
	   this key should be derived from a fixed key which is RPMB seed.
	   RPMB key (HMAC key) and this encryption key (AES key) are both
	   derived from the same RPMB seed.
	*/
	uint8_t attkb_enc_key[32];

	/* For APL only */
	/* RPMB key is derived with dseed together with this serial number,
	 * for ICL +, CSE directly provides the rpmb_key which is already
	 * tied to serial number. Concatenation of emmc product name
	 * with a string representation of PSN
	 */
	char serial[MMC_PROD_NAME_WITH_PSN_LEN];
	char pad2;
};

struct secure_world_memory {
	/* The original secure world base address allocated by bootloader */
	uint64_t base_gpa_in_uos;
	/* The secure world base address of HPA */
	uint64_t base_hpa;
	/* Secure world runtime memory size */
	uint64_t length;
};

struct secure_world_control {
	/* Flag indicates Secure World's state */
	struct {
		/* sworld supporting: 0(unsupported), 1(supported) */
		uint64_t supported :  1;
		/* sworld running status: 0(inactive), 1(active) */
		uint64_t active    :  1;
		/* sworld context saving status: 0(unsaved), 1(saved) */
		uint64_t ctx_saved :  1;
		uint64_t reserved  : 61;
	} flag;
	/* Secure world memory structure */
	struct secure_world_memory sworld_memory;
};

struct trusty_startup_param {
	uint32_t size_of_this_struct;
	uint32_t mem_size;
	uint64_t tsc_per_ms;
	uint64_t trusty_mem_base;
	uint32_t reserved;
	uint8_t padding[4];
};

void switch_world(struct acrn_vcpu *vcpu, int32_t next_world);
bool initialize_trusty(struct acrn_vcpu *vcpu, struct trusty_boot_param *boot_param);
void destroy_secure_world(struct acrn_vm *vm, bool need_clr_mem);
void save_sworld_context(struct acrn_vcpu *vcpu);
void restore_sworld_context(struct acrn_vcpu *vcpu);

#endif /* TRUSTY_H_ */
