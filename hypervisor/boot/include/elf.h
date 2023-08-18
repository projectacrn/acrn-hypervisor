/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 *  SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2017, 2018 Dell EMC
 * Copyright (c) 2000, 2001, 2008, 2011, David E. O'Brien
 * Copyright (c) 1998 John D. Polstra.
 * All rights reserved.
 */

/*
 * source:
 * https://github.com/freebsd/freebsd-src/blob/main/sys/sys/elf_common.h
 */

#ifndef	ELF_H
#define	ELF_H

/* Indexes into the e_ident array.  Keep synced with
   http://www.sco.com/developers/gabi/latest/ch4.eheader.html */
#define	EI_MAG0			0	/* Magic number, byte 0. */
#define	EI_MAG1			1	/* Magic number, byte 1. */
#define	EI_MAG2			2	/* Magic number, byte 2. */
#define	EI_MAG3			3	/* Magic number, byte 3. */
#define	EI_CLASS		4	/* Class of machine. */
#define	EI_DATA			5	/* Data format. */
#define	EI_VERSION		6	/* ELF format version. */
#define	EI_OSABI		7	/* Operating system / ABI identification */
#define	EI_ABIVERSION	8	/* ABI version */
#define	OLD_EI_BRAND	8	/* Start of architecture identification. */
#define	EI_PAD			9	/* Start of padding (per SVR4 ABI). */
#define	EI_NIDENT		16	/* Size of e_ident array. */

/* Values for the magic number bytes. */
#define ELFMAGIC 0x464C457f

/* Values for e_ident[EI_CLASS]. */
#define	ELFCLASSNONE	0	/* Unknown class. */
#define	ELFCLASS32		1	/* 32-bit architecture. */
#define	ELFCLASS64		2	/* 64-bit architecture. */

/* Values for e_type of program entry */
#define	ET_NONE		0	/* Unknown type. */
#define	ET_REL		1	/* Relocatable. */
#define	ET_EXEC		2	/* Executable. */
#define	ET_DYN		3	/* Shared object. */
#define	ET_CORE		4	/* Core file. */
#define	ET_LOOS		0xfe00	/* First operating system specific. */
#define	ET_HIOS		0xfeff	/* Last operating system-specific. */
#define	ET_LOPROC	0xff00	/* First processor-specific. */
#define	ET_HIPROC	0xffff	/* Last processor-specific. */

/* Values for p_type of program entry */
#define	PT_NULL		0	/* Unused entry. */
#define	PT_LOAD		1	/* Loadable segment. */
#define	PT_DYNAMIC	2	/* Dynamic linking information segment. */
#define	PT_INTERP	3	/* Pathname of interpreter. */
#define	PT_NOTE		4	/* Auxiliary information. */
#define	PT_SHLIB	5	/* Reserved (not used). */
#define	PT_PHDR		6	/* Location of program header itself. */
#define	PT_TLS		7	/* Thread local storage segment */

/* Header struct for elf64 file */
struct elf64_hdr
{
	uint8_t e_ident[EI_NIDENT];	/*ELF identification */
	uint16_t e_type;	/*Object file type */
	uint16_t e_machine;	/*Machine type */
	uint32_t e_version;	/*Object file version */
	uint64_t e_entry;	/*Entry point address */
	uint64_t e_phoff;	/*Program header offset */
	uint64_t e_shoff;	/*Section header offset */
	uint32_t e_flags;	/*Processor-specific flags */
	uint16_t e_ehsize;	/*ELF header size */
	uint16_t e_phentsize;	/*Size of program header entry */
	uint16_t e_phnum;	/*Number of program header entries */
	uint16_t e_shentsize;	/*Size of section header entry */
	uint16_t e_shnum;	/*Number of section header entries */
	uint16_t e_shstrndx;	/*Section name string table index */
};

/* Program entry struct for elf64, describes segments loaded in ram*/
struct elf64_prog_entry
{
	uint32_t p_type;	/* Type of segment */
	uint32_t p_flags;	/* Segment attributes */
	uint64_t p_offset;	/* Offset in file */
	uint64_t p_vaddr;	/* Virtual address in memory */
	uint64_t p_paddr;	/* Physical address in memory */
	uint64_t p_filesz;	/* Size of segment in file */
	uint64_t p_memsz;	/* Size of segment in memory */
	uint64_t p_align;	/* Alignment of segment */
};

/* Section entry struct for elf64, contains sections info of the program*/
struct elf64_sec_entry
{
	uint32_t sh_name;	/*Section name */
	uint32_t sh_type;	/*Section type */
	uint64_t sh_flags;	/*Section attributes */
	uint64_t sh_addr;	/*Virtual address in memory */
	uint64_t sh_offset;	/*Offset in file */
	uint64_t sh_size;	/*Size of section */
	uint32_t sh_link;	/*Link to other section */
	uint32_t sh_info;	/*Miscellaneous information */
	uint64_t sh_addralign;	/*Address alignment boundary */
	uint64_t sh_entsize;	/*Size of entries, if section has table */
};

/* Header struct for elf32 file */
struct elf32_hdr
{
	uint8_t e_ident[EI_NIDENT];	/*ELF identification */
	uint16_t e_type;	/*Object file type */
	uint16_t e_machine;	/*Machine type */
	uint32_t e_version;	/*Object file version */
	uint32_t e_entry;	/*Entry point address */
	uint32_t e_phoff;	/*Program header offset */
	uint32_t e_shoff;	/*Section header offset */
	uint32_t e_flags;	/*Processor-specific flags */
	uint16_t e_ehsize;	/*ELF header size */
	uint16_t e_phentsize;	/*Size of program header entry */
	uint16_t e_phnum;	/*Number of program header entries */
	uint16_t e_shentsize;	/*Size of section header entry */
	uint16_t e_shnum;	/*Number of section header entries */
	uint16_t e_shstrndx;	/*Section name string table index */

};

/* Program entry struct for elf32, describes segments loaded in ram*/
struct elf32_prog_entry
{
	uint32_t p_type;	/* Type of segment */
	uint32_t p_offset;	/* Offset in file */
	uint32_t p_vaddr;	/* Virtual address in memory */
	uint32_t p_paddr;	/* Physical address in memory */
	uint32_t p_filesz;	/* Size of segment in file */
	uint32_t p_memsz;	/* Size of segment in memory */
	uint32_t p_flags;	/* Segment attributes */
	uint32_t p_align;	/* Alignment of segment */
};

/* Section entry struct for elf32, contains sections info of the program*/
struct elf32_sec_entry
{
	uint32_t sh_name;
	uint32_t sh_type;
	uint32_t sh_flags;
	uint32_t sh_addr;
	uint32_t sh_offset;
	uint32_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint32_t sh_addralign;
	uint32_t sh_entsize;
};

#endif /* !ELF_H */
