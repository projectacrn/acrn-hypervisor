/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */


#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "dm.h"
#include "log.h"
#include "hsm_ioctl_defs.h"
#include "acpi.h"

static uint8_t lapic_ids[ACRN_PLATFORM_LAPIC_IDS_MAX] = {0xff};

int lapicid_from_pcpuid(int pcpu_id)
{
	if (pcpu_id < 0 || pcpu_id >= ACRN_PLATFORM_LAPIC_IDS_MAX || lapic_ids[pcpu_id] == 0xff) {
		return -1;
	}
	return (int)lapic_ids[pcpu_id];
}

static int
local_parse_madt(struct acpi_table_madt *madt)
{
	uint16_t pcpu_num = 0U;
	int ret = 0;
	struct acpi_madt_local_apic *processor;
	struct acpi_table_madt *madt_ptr;
	void *first, *end, *iterator;
	struct acpi_subtable_header *entry;

	madt_ptr = madt;

	first = madt_ptr + 1;
	end = (void *)madt_ptr + madt_ptr->header.length;

	for (iterator = first; (iterator) < (end); iterator += entry->length) {
		entry = (struct acpi_subtable_header *)iterator;
		if (entry->length < sizeof(struct acpi_subtable_header)) {
			break;
		}

		if (entry->type == ACPI_MADT_TYPE_LOCAL_APIC) {
			processor = (struct acpi_madt_local_apic *)iterator;
			if ((processor->lapic_flags & ACPI_MADT_ENABLED) != 0U) {
				if (pcpu_num < ACRN_PLATFORM_LAPIC_IDS_MAX) {
					lapic_ids[pcpu_num] = processor->id;
				}
				pcpu_num++;
			}
		}
	}

	if (pcpu_num == 0) {
		ret = -1;
	}
	return ret;
}

/*
 * There has an assumption. The SOS owned pcpu starts from physical cpu 0,
 * otherwise SOS doesn't know the mapping relationship between its vcpu0
 * and pcpu_id.
 */
int parse_madt(void)
{
	int ret = 0U;
	ssize_t size;
	struct acpi_table_madt *madt;
	struct stat file_state;

	int fd = open("/sys/firmware/acpi/tables/APIC", O_RDONLY);
	if (fd < 0) {
		pr_err("Fail to open sos APIC file!\n");
		return -1;
	}

	if (fstat(fd, &file_state) == -1) {
		pr_err("Fail to get apic file state!\n");
		close(fd);
		return -1;
	}

	madt = (struct acpi_table_madt *)malloc(file_state.st_size);
	if (madt == NULL) {
		pr_err("Failed to malloc %d bytes for MADT!\n", file_state.st_size);
		close(fd);
		return -1;
	}
	size = read(fd, madt, file_state.st_size);
	if (size == file_state.st_size) {
		ret = local_parse_madt(madt);
	} else {
		pr_err("Fail to read sos madt info!");
		ret = -1;
	}

	free(madt);
	close(fd);
	return ret;
}
