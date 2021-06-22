/*
 * Copyright (c) 2021, Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *    * Neither the name of Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products
 *      derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Library to support ACRN HV booting with Slim Bootloader container
 *
*/

#include <elf.h>
#include <efi.h>
#include <efilib.h>
#include "boot.h"
#include "stdlib.h"
#include "efilinux.h"
#include "multiboot.h"
#include "container.h"
#include "elf32.h"

#define LZH_BOOT_CMD	0u
#define LZH_BOOT_IMG	1u
#define LZH_MOD0_CMD	2u

#define MAX_BOOTCMD_SIZE	(2048 + 256)    /* Max linux command line size plus uefi boot options */
#define MAX_MODULE_COUNT	32

typedef struct multiboot2_header_tag_relocatable RELOC_INFO;
typedef struct multiboot2_header_tag_address LADDR_INFO;

typedef struct {
  UINT32           Signature;
  UINT8            Version;
  UINT8            Svn;
  UINT16           DataOffset;
  UINT32           DataSize;
  UINT8            AuthType;
  UINT8            ImageType;
  UINT8            Flags;
  UINT8            Count;
} CONTAINER_HDR;

typedef struct {
  UINT32           Name;
  UINT32           Offset;
  UINT32           Size;
  UINT8            Attribute;
  UINT8            Alignment;
  UINT8            AuthType;
  UINT8            HashSize;
  UINT8            HashData[0];
} COMPONENT_ENTRY;

typedef struct {
  UINT32        Signature;
  UINT32        CompressedSize;
  UINT32        Size;
  UINT16        Version;
  UINT8         Svn;
  UINT8         Attribute;
  UINT8         Data[];
} LOADER_COMPRESSED_HEADER;

struct container {
	struct hv_loader ops;   /* loader operation table */

	UINT8 mb_version;       /* multiboot version of hv image. Can be either 1 or 2. */

	CHAR16 *options;        /* uefi boot option passed by efibootmgr -u */
	UINT32 options_size;    /* length of UEFI boot option */
	char boot_cmd[MAX_BOOTCMD_SIZE];    /* hv boot command line */
	UINTN boot_cmdsize;     /* length of boot command to pass hypervisor */

	EFI_PHYSICAL_ADDRESS hv_hpa;    /* start of memory stored hv image */
	EFI_PHYSICAL_ADDRESS mod_hpa;	/* start of memory stored module files */
	EFI_PHYSICAL_ADDRESS hv_entry;	/* entry point of hv */
	RELOC_INFO *reloc;              /* relocation info */
	LADDR_INFO *laddr;              /* load address info */
	UINT32 est_hv_ram_size;         /* estimated hv ram size when load address info is NULL. */

	MB_MODULE_INFO mod_info[MAX_MODULE_COUNT]; /* modules info */
	UINTN mod_count;        /* num of modules */
	UINTN total_modsize;    /* memory size allocated to load modules */
	UINTN total_modcmdsize; /* memory size to store module commands */

	UINTN lzh_count;        /* num of files in container */
	LOADER_COMPRESSED_HEADER *lzh_ptr[];	/* cache of each file header in container */
};

/**
 * @brief Load hypervisor into memory from a container blob
 *
 * @param[in] hvld Loader handle
 *
 * @return EFI_SUCCESS(0) on success, non-zero on error
 */
static EFI_STATUS container_load_boot_image(HV_LOADER hvld)
{
	EFI_STATUS err = EFI_SUCCESS;
	return err;
}

/**
 * @brief Load kernel modules and acpi tables into memory from a container blob
 *
 * @param[in] hvld Loader handle
 *
 * @return EFI_SUCCESS(0) on success, non-zero on error
 */
 static EFI_STATUS container_load_modules(HV_LOADER hvld)
{
	EFI_STATUS err = EFI_SUCCESS;
	return err;
}

/**
 * @brief Get hypervisor boot commandline.
 *
 * @param[in] hvld Loader handle
 *
 * @return Hypervisor boot command line.
 */
static const char *container_get_boot_cmd(HV_LOADER hvld)
{
	return ((struct container *)hvld)->boot_cmd;
}

/**
 * @brief Get hypervisor boot command length
 *
 * @param[in] hvld Loader handle
 *
 * @return the length of hypervisor boot command
 */
static UINTN container_get_boot_cmdsize(HV_LOADER hvld)
{
	/* boot_cmd = hv_cmdline.txt in container + extra arg given by the 'efibootmgr -u' option */
	return ((struct container *)hvld)->boot_cmdsize;
}

/**
 * @brief Get boot module info
 *
 * @param[in] hvld Loader handle
 * @param[in] index index to the list of boot modules
 *
 * @return the boot module info at index
 */
static MB_MODULE_INFO *container_get_mods_info(HV_LOADER hvld, UINTN index)
{
	return &(((struct container *)hvld)->mod_info)[index];
}

/**
 * @brief Get the number of multiboot2 modules
 *
 * @param[in] hvld Loader handle
 *
 * @return the number of multiboot2 modules
 */
static UINTN container_get_mod_count(HV_LOADER hvld)
{
	return ((struct container *)hvld)->mod_count;
}

/**
 * @brief Get the total memory size allocated to load module files
 *
 * @param[in] hvld Loader handle
 *
 * @return the total size of memory allocated to store the module files
 */
static UINTN container_get_total_modsize(HV_LOADER hvld)
{
	return ((struct container *)hvld)->total_modsize;
}

/**
 * @brief Get the total lengths of the module commands
 *
 * @param[in] hvld Loader handle
 *
 * @return the total lengths of module command files
 */
static UINTN container_get_total_modcmdsize(HV_LOADER hvld)
{
	return ((struct container *)hvld)->total_modcmdsize;
}

/**
 * @brief Get the start address of the memory region stored ACRN hypervisor image
 *
 * @param[in] hvld Loader handle
 *
 * @return the address of hv image
 */
static EFI_PHYSICAL_ADDRESS container_get_hv_hpa(HV_LOADER hvld)
{
	return ((struct container *)hvld)->hv_hpa;
}

/**
 * @brief Get the start address of the memory region stored module files
 *
 * @param[in] hvld Loader handle
 *
 * @return the address of modules
 */
static EFI_PHYSICAL_ADDRESS container_get_mod_hpa(HV_LOADER hvld)
{
	return ((struct container *)hvld)->mod_hpa;
}

/**
 * @brief Get the supported multiboot version of ACRN hypervisor image
 *
 * @param[in] hvld Loader handle
 *
 * @return supported multiboot version. Can be either 1 or 2.
 */
static int container_get_multiboot_version(HV_LOADER hvld)
{
	return ((struct container *)hvld)->mb_version;
}

/**
 * @brief Get the entry point of ACRN hypervisor
 *
 * @param[in] hvld Loader handle
 *
 * @return the entry point of hypervisor
 */
static EFI_PHYSICAL_ADDRESS container_get_hv_entry(HV_LOADER hvld)
{
	return ((struct container *)hvld)->hv_entry;
}

/**
 * @brief Get the total memory size of hv image
 *
 * @param[in] hvld Loader handle
 *
 * @return the memory size of hv image
 */
static UINTN container_get_hv_ram_size(HV_LOADER hvld)
{
	struct container *ctr = (struct container *)hvld;
	if (ctr->laddr) {
		return ctr->laddr->load_end_addr - ctr->laddr->load_addr;
	}

	return ctr->est_hv_ram_size;
}

/**
 * @brief Free up memory allocated by the container loader
 *
 * @param[in]  hvld Loader handle
 *
 * @return None
 */
static void container_deinit(HV_LOADER hvld)
{
	struct container *ctr = (struct container *)hvld;

	if (ctr->lzh_ptr) {
		free_pool(ctr->lzh_ptr);
		free_pool(ctr);
	}

	if (ctr->mod_hpa) {
		free_pages(ctr->mod_hpa, EFI_SIZE_TO_PAGES(ctr->total_modsize));
	}
}

/* hypervisor loader operation table */
static struct hv_loader container_ops = {
	.load_boot_image = container_load_boot_image,
	.load_modules = container_load_modules,
	.get_boot_cmd = container_get_boot_cmd,
	.get_boot_cmdsize = container_get_boot_cmdsize,
	.get_mods_info = container_get_mods_info,
	.get_total_modsize = container_get_total_modsize,
	.get_total_modcmdsize = container_get_total_modcmdsize,
	.get_mod_count = container_get_mod_count,
	.get_hv_hpa = container_get_hv_hpa,
	.get_mod_hpa = container_get_mod_hpa,
	.get_hv_entry = container_get_hv_entry,
	.get_multiboot_version = container_get_multiboot_version,
	.get_hv_ram_size = container_get_hv_ram_size,
	.deinit = container_deinit,
};

/**
 * @brief Initialize Container Library and returned the loader operation table
 *
 * @param[in]  info Firmware-allocated handle that identifies the EFI application image (i.e. acrn.efi)
 * @param[out] info Allocated loader operation table
 *
 * @return EFI_SUCCESS(0) on success, non-zero on error
 */
EFI_STATUS container_init(EFI_LOADED_IMAGE *info, HV_LOADER *hvld)
{
	EFI_STATUS err = EFI_SUCCESS;

	struct container *ctr = NULL;

	UINTN sec_addr = 0u;
	UINTN sec_size = 0u;
	char *section  = ".hv";

	UINTN i;
	CONTAINER_HDR   *hdr  = NULL;
	COMPONENT_ENTRY *comp = NULL;

	UINTN offset = 0u;

	err = allocate_pool(EfiLoaderData, sizeof(struct container), (void **)&ctr);
	if (EFI_ERROR(err)) {
		Print(L"Failed to allocate memory for Container Library %r\n", err);
		goto out;
	}

	(void)memset((void *)ctr, 0x0, sizeof(struct container));
	memcpy((char *)&ctr->ops, (const char *)&container_ops, sizeof(struct hv_loader));

	/* store the options */
	ctr->options = info->LoadOptions;
	ctr->options_size = info->LoadOptionsSize;

	/* read a container stitched at the .hv section */
	err = get_pe_section(info->ImageBase, section, strlen(section), &sec_addr, &sec_size);
	if (EFI_ERROR(err)) {
		Print(L"Unable to locate section of ACRNHV Container %r ", err);
		goto out;
	}

	hdr = (CONTAINER_HDR*)(info->ImageBase + sec_addr);
	ctr->lzh_count = hdr->Count;

	err = allocate_pool(EfiLoaderData, sizeof(LOADER_COMPRESSED_HEADER *) * hdr->Count, (void **)&ctr->lzh_ptr);
	if (EFI_ERROR(err)) {
		Print(L"Failed to allocate memory for Container Library %r\n", err);
		goto out;
	}

	/* cache each file's header point for later use */
	comp = (COMPONENT_ENTRY *)(hdr + 1);
	for (i = 0; i < hdr->Count; i++) {
		offset = hdr->DataOffset + comp->Offset;
		ctr->lzh_ptr[i] = (LOADER_COMPRESSED_HEADER *)((UINT8 *)(hdr) + offset);

		comp = (COMPONENT_ENTRY *)((UINT8 *)(comp + 1) + comp->HashSize);
	}

	*hvld = (struct hv_loader *)ctr;
out:
	if (EFI_ERROR(err)) {
		if (ctr) {
			container_deinit((HV_LOADER)ctr);
		}
	}
	return err;
}
