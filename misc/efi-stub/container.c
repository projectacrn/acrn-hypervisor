/*
 * Copyright (c) 2021 - 2022, Intel Corporation.
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
 * @brief Load acrn.32.out ELF file. If the hv_ram_start and hv_ram_size are both zero,
 * these two parameters will be obtained from the ELF header.
 *
 * @param[in]      elf_image     ELF image
 * @param[out]     hv_hpa        The physical memory address the relocated hypervisor is stored
 * @param[in]      hv_ram_start  The link address of the hv, e.g. the address used in a linker script.
 * @param[in,out]  hv_ram_size   A pointer to the size of the hv image. If the value of *hv_ram_size and hv_ram_start is 0,
 *                               *hv_ram_size will be updated to reflect a conservative estimate of hv_ram_size from ELF header.
 * @param[in]      reloc         A pointer to the relocation information. Can be NULL.
 *
 * @return EFI_SUCCESS(0) on success, non-zero on error
 */
static EFI_STATUS load_acrn_elf(const UINT8 *elf_image, EFI_PHYSICAL_ADDRESS *hv_hpa,
	UINT32 hv_ram_start, UINT32 *hv_ram_size, const RELOC_INFO *reloc)
{
	EFI_STATUS err = EFI_SUCCESS;

	if (validate_elf_header((Elf32_Ehdr *)elf_image) < 0) {
		err = EFI_LOAD_ERROR;
		goto out;
	}

	if (hv_ram_start == 0 && *hv_ram_size == 0) {
		UINT64 ram_low, ram_high;

		if (elf_calc_link_addr_range((Elf32_Ehdr *)elf_image, &ram_low, &ram_high) < 0) {
			err = EFI_LOAD_ERROR;
			goto out;
		}

		hv_ram_start = ram_low;
		*hv_ram_size = ram_high - ram_low;

		/* According to board_defconfig.py, the size required might include:
		 * hv_base_ram + post_launched_ram * postlaunched_num + ivshmem (if enabled).
		 *  i.e., 20MB + 16MB * postlaunched_num + 2 * max(total_ivshmem, 0x200000)
		 *
		 * From bootloader we can only do conservative estimate. I.e., we will calculate
		 * using maximum possible number of postlaunched number and total_ivshmem.
		 */

		/* 16MB * postlaunched_num */
		*hv_ram_size += (16 * 1024 * 1024) * 7;

		/* total size of ivshmem will be at least 2 * 200000. Here we double the region. */
		*hv_ram_size += 4 * 0x200000;

		/* Typically this can use memory up to 0xAA00000, compared to the size calculated
		 * for a typical hybrid_rt: 0x3800000.
		 *
		 * It might seemed a little bit wasteful but that's the best we can do without
		 * an address tag in multiboot header.
		 */
	}

	if (reloc) {
		err = emalloc_reserved_aligned(hv_hpa, *hv_ram_size, reloc->align,
			reloc->min_addr, reloc->max_addr);
	}
	else {
		err = emalloc_fixed_addr(hv_hpa, *hv_ram_size, hv_ram_start);
	}

	if (err != EFI_SUCCESS) {
		Print(L"Failed to allocate memory for ACRN HV %r\n", err);
		goto out;
	}

	if (elf_load((Elf32_Ehdr *)elf_image, *hv_hpa, hv_ram_start) < 0) {
		err = EFI_LOAD_ERROR;
		goto out;
	}

out:
	return err;
}

static int parse_boot_image(const UINT8 *data, EFI_PHYSICAL_ADDRESS *hv_entry,
	UINT8 *mb_version, LADDR_INFO **laddr, RELOC_INFO **reloc, const void **mb_header)
{
	const void *mb_hdr;
	UINT8 mbver = 0;

	mb_hdr = find_mb2header(data, MULTIBOOT2_SEARCH);
	if (mb_hdr) {
		struct hv_mb2header_tag_list hv_tags;
		mbver = 2;
		if (parse_mb2header(mb_hdr, &hv_tags) < 0) {
			Print(L"Illegal multiboot2 header tags\n");
			return -1;
		}

		if (hv_tags.addr) *laddr = hv_tags.addr;
		if (hv_tags.entry) *hv_entry = hv_tags.entry->entry_addr;
		if (hv_tags.reloc) *reloc = hv_tags.reloc;
	} else {
		mb_hdr = (struct multiboot_header *)find_mb1header(data, MULTIBOOT_SEARCH);
		if (!mb_hdr) {
			Print(L"Image is not multiboot compatible\n");
			return -1;
		}
		mbver = 1;
	}

	*mb_version = mbver;
	*mb_header = mb_hdr;

	return 0;
}

/**
 * @brief Load hypervisor into memory from a container blob
 *
 * @param[in] hvld Loader handle
 *
 * @return EFI_SUCCESS(0) on success, non-zero on error
 */
static EFI_STATUS container_load_boot_image(HV_LOADER hvld)
{
	int i;
	EFI_STATUS err = EFI_SUCCESS;
	struct container *ctr = (struct container *)hvld;
	const void *mb_hdr;

	LOADER_COMPRESSED_HEADER *lzh = NULL;

	/* prepare boot command line: stitched from hv_cmdline.txt and argument from efibootmgr -u */
	lzh = ctr->lzh_ptr[LZH_BOOT_CMD];
	ctr->boot_cmdsize = lzh->Size + StrnLen(ctr->options, ctr->options_size);
	if (ctr->boot_cmdsize >= MAX_BOOTCMD_SIZE) {
		Print(L"Boot command size 0x%x exceeding limit 0x%x\n", ctr->boot_cmdsize, MAX_BOOTCMD_SIZE);
		return EFI_INVALID_PARAMETER;
	}
	memcpy(ctr->boot_cmd, (const char *)lzh->Data, lzh->Size - 1);
	if (ctr->options) {
		ctr->boot_cmd[lzh->Size - 1] = ' ';
		for (i = lzh->Size; i < ctr->boot_cmdsize; i++) {
			ctr->boot_cmd[i] = ctr->options[i - lzh->Size];
		}
	}

	/* parse and load boot image */
	lzh = ctr->lzh_ptr[LZH_BOOT_IMG];

	if (parse_boot_image((const UINT8 *)lzh->Data, &ctr->hv_entry, &ctr->mb_version,
		&ctr->laddr, &ctr->reloc, &mb_hdr) < 0) {
		err = EFI_INVALID_PARAMETER;
		goto out;
	}

	if (ctr->mb_version == 2) {
		/* Multiboot 2 */
		if (!ctr->laddr) {
			/* GRUB will fail if the elf image contains ".rela" section. We simply ignore it. */
			UINT32 hv_ram_size = 0;
			err = load_acrn_elf(lzh->Data, &ctr->hv_hpa, 0, &hv_ram_size, ctr->reloc);
			ctr->est_hv_ram_size = hv_ram_size;
			ctr->hv_entry = elf_get_entry((Elf32_Ehdr *)lzh->Data);
		} else {
			/*
			 * Multiboot2 specs address tag contains only one pair of load address and end address, which implies that
			 * the text and data segments in image must be consecutive. This is true for the a.out binary format
			 * but not the ELF format.
			 *
			 * We can either implement a "load_acrn_binary" to substitute load_acrn_elf here and tell people
			 * to use a flat binary (acrn.bin), or left it untouched and tell people to use an ELF (which is
			 * what we're doing now).
			 */
			UINT32 load_addr = ctr->laddr->load_addr;
			UINT32 load_size = ctr->laddr->load_end_addr - ctr->laddr->load_addr;

			err = load_acrn_elf(lzh->Data, &ctr->hv_hpa, load_addr, &load_size, ctr->reloc);
		}

		if (err != EFI_SUCCESS) {
			Print(L"Failed to load ACRN HV ELF Image%r\n", err);
			goto out;
		}

		/* Fix up entry address */
		if (ctr->reloc) {
			ctr->hv_entry += (ctr->hv_hpa >= ctr->laddr->load_addr) ?
				ctr->hv_hpa - ctr->laddr->load_addr :
				ctr->laddr->load_addr - ctr->hv_hpa;
		}
	} else {
		/* Multiboot 1. We don't do relocation for MB1 case. The ".rela" section will be ignored. */
		/* TODO: add support for the case when AOUT_KLUDGE flag is set */
		UINT32 hv_ram_size = 0;
		err = load_acrn_elf(lzh->Data, &ctr->hv_hpa, 0, &hv_ram_size, NULL);
		if (err != EFI_SUCCESS) {
			Print(L"Failed to load ACRN HV ELF Image%r\n", err);
			goto out;
		}
		ctr->est_hv_ram_size = hv_ram_size;
		ctr->hv_entry = elf_get_entry((Elf32_Ehdr *)lzh->Data);
	}

out:
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
	struct container *ctr = (struct container *)hvld;

	UINTN i, j;

	UINT8 * p = NULL;
	LOADER_COMPRESSED_HEADER *lzh = NULL;
	LOADER_COMPRESSED_HEADER *cmd_lzh = NULL;

	/* scan module headers to calculate required memory size to store files */
	for (i = LZH_MOD0_CMD; i < ctr->lzh_count - 1; i++) {
		if ((i % 2) == 0) {	/* vm0_tag.txt, vm1_tag.txt, acpi_vm0.txt ... */
			ctr->total_modcmdsize += ctr->lzh_ptr[i]->Size;
		} else {	/* vm0_kernel, vm1_kernel, vm0_acpi.bin ... */
			ctr->total_modsize += ALIGN_UP(ctr->lzh_ptr[i]->Size, EFI_PAGE_SIZE);
		}
	}
	/* exclude hypervisor and SBL signature files. e.g.)
	 *    lzh_count = 9 (hv_cmdline, acrn.32.out, vm0_tag, vm0_kernel, vm1_tag, vm1_kernel, vm0_acpi_tag, vm0_acpi, sig)
	 *    mod_count = 3 (vm0_tag + vm0_kernel, vm1_tag + vm1_kernel, vm0_acpi_tag + vm0_acpi)
	 */
	ctr->mod_count = (ctr->lzh_count - 3) / 2;

	if (ctr->mod_count >= MAX_MODULE_COUNT) {
		Print(L"Too many modules: 0x%x\n", ctr->mod_count);
		return EFI_INVALID_PARAMETER;
	}

	/* allocate single memory region to store all binary files to avoid mmap fragmentation */
	if (ctr->reloc) {
		err = emalloc_reserved_aligned(&(ctr->mod_hpa), ctr->total_modsize,
								EFI_PAGE_SIZE, ctr->reloc->min_addr, ctr->reloc->max_addr);
	} else {
		/* We put modules after hv */
		UINTN hv_ram_size = ctr->laddr->load_end_addr - ctr->laddr->load_addr;
		err = emalloc_fixed_addr(&(ctr->mod_hpa), hv_ram_size, ctr->hv_hpa + ALIGN_UP(hv_ram_size, EFI_PAGE_SIZE));
	}
	if (err != EFI_SUCCESS) {
		Print(L"Failed to allocate memory for modules %r\n", err);
		goto out;
	}

	p = (UINT8 *)ctr->mod_hpa;
	for (i = LZH_BOOT_IMG + 2, j = 0; i < ctr->lzh_count - 1; i = i + 2) {
		lzh = ctr->lzh_ptr[i];
		cmd_lzh = ctr->lzh_ptr[i - 1];
		memcpy((char *)p, (const char *)lzh->Data, lzh->Size);
		ctr->mod_info[j].mod_start = (EFI_PHYSICAL_ADDRESS)p;
		ctr->mod_info[j].mod_end = (EFI_PHYSICAL_ADDRESS)p + lzh->Size;
		ctr->mod_info[j].cmd = (const char *)cmd_lzh->Data;
		ctr->mod_info[j].cmdsize = cmd_lzh->Size;
		p += ALIGN_UP(lzh->Size, EFI_PAGE_SIZE);
		j++;
	}

out:
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
