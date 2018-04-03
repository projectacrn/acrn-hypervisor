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
 */

#ifndef TRUSTY_H_
#define TRUSTY_H_

#define BOOTLOADER_SEED_MAX_ENTRIES    10
#define RPMB_MAX_PARTITION_NUMBER       6
#define MMC_PROD_NAME_WITH_PSN_LEN      15
#define BUP_MKHI_BOOTLOADER_SEED_LEN    64

/* Structure of seed info */
struct seed_info {
	uint8_t cse_svn;
	uint8_t bios_svn;
	uint8_t padding[2];
	uint8_t seed[BUP_MKHI_BOOTLOADER_SEED_LEN];
};

/* Structure of key info */
struct key_info {
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
		3: CWP (APL|ICL + SBL + CWP)
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
	/* The secure world base address of GPA in SOS */
	uint64_t base_gpa;
	/* The secure world base address of HPA */
	uint64_t base_hpa;
	/* Secure world runtime memory size */
	uint64_t length;
};

struct secure_world_control {
	/* Whether secure world is enabled for current VM */
	bool sworld_enabled;
	/* Secure world memory structure */
	struct secure_world_memory sworld_memory;
};

void switch_world(struct vcpu *vcpu, int next_world);
bool initialize_trusty(struct vcpu *vcpu, uint64_t param);
void destroy_secure_world(struct vm *vm);

#endif /* TRUSTY_H_ */

