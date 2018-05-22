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

#include <hypervisor.h>

/* IOAPIC id */
#define SBL_IOAPIC_ID   8
/* IOAPIC base address */
#define SBL_IOAPIC_ADDR 0xfec00000
/* IOAPIC range size */
#define SBL_IOAPIC_SIZE 0x100000
/* Local APIC base address */
#define SBL_LAPIC_ADDR 0xfee00000
/* Local APIC range size */
#define SBL_LAPIC_SIZE 0x100000
/* Number of PCI IRQ assignments */
#define SBL_PCI_IRQ_ASSIGNMENT_NUM 28

#ifndef CONFIG_DMAR_PARSE_ENABLED
static struct dmar_dev_scope default_drhd_unit_dev_scope0[] = {
	{ .bus = 0, .devfun = DEVFUN(0x2, 0), },
};

static struct dmar_drhd drhd_info_array[] = {
	{
		.dev_cnt = 1,
		.segment = 0,
		.flags = 0,
		.reg_base_addr = 0xFED64000,
		/* Ignore the iommu for intel graphic device since GVT-g needs
		 * vtd disabled for gpu
		 */
		.ignore = true,
		.devices = default_drhd_unit_dev_scope0,
	},
	{
		/* No need to specify devices since
		 * DRHD_FLAG_INCLUDE_PCI_ALL_MASK set
		 */
		.dev_cnt = 0,
		.segment = 0,
		.flags = DRHD_FLAG_INCLUDE_PCI_ALL_MASK,
		.reg_base_addr = 0xFED65000,
		.ignore = false,
		.devices = NULL,
	},
};

static struct dmar_info sbl_dmar_info = {
	.drhd_count = 2,
	.drhd_units = drhd_info_array,
};

struct dmar_info *get_dmar_info(void)
{
	return &sbl_dmar_info;
}
#endif

void    init_bsp(void)
{
}
