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
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "vmm.h"
#include "vmmapi.h"
#include "dm.h"
#include "acpi.h"

static uint8_t get_vcpu_px_cnt(struct vmctx *ctx, int vcpu_id)
{
	uint64_t pm_ioctl_buf = 0;
	enum pm_cmd_type cmd_type = PMCMD_GET_PX_CNT;

	pm_ioctl_buf = ((ctx->vmid << PMCMD_VMID_SHIFT) & PMCMD_VMID_MASK)
			| ((vcpu_id << PMCMD_VCPUID_SHIFT) & PMCMD_VCPUID_MASK)
			| cmd_type;

	if (vm_get_cpu_state(ctx, &pm_ioctl_buf)) {
		return 0;
	}

	return (uint8_t)pm_ioctl_buf;
}

static int get_vcpu_px_data(struct vmctx *ctx, int vcpu_id,
			int px_num, struct cpu_px_data *vcpu_px_data)
{
	uint64_t *pm_ioctl_buf;
	enum pm_cmd_type cmd_type = PMCMD_GET_PX_DATA;

	pm_ioctl_buf = malloc(sizeof(struct cpu_px_data));
	if (!pm_ioctl_buf) {
		return -1;
	}

	*pm_ioctl_buf = ((ctx->vmid << PMCMD_VMID_SHIFT) & PMCMD_VMID_MASK)
		| ((vcpu_id << PMCMD_VCPUID_SHIFT) & PMCMD_VCPUID_MASK)
		| ((px_num << PMCMD_STATE_NUM_SHIFT) & PMCMD_STATE_NUM_MASK)
		| cmd_type;

	/* get and validate px data */
	if (vm_get_cpu_state(ctx, pm_ioctl_buf)) {
		free(pm_ioctl_buf);
		return -1;
	}

	memcpy(vcpu_px_data, pm_ioctl_buf,
			sizeof(struct cpu_px_data));

	free(pm_ioctl_buf);
	return 0;
}

/* _PPC: Performance Present Capabilities
 * hard code _PPC to 0,  all states are available.
 */
static void dsdt_write_ppc(void)
{
	dsdt_line("    Name (_PPC, Zero)");
}

/* _PCT: Performance Control
 * Both Performance Control and Status Register are set to FFixedHW
 */
static void dsdt_write_pct(void)
{
	dsdt_line("        Method (_PCT, 0, NotSerialized)");
	dsdt_line("        {");
	dsdt_line("            Return (Package (0x02)");
	dsdt_line("            {");
	dsdt_line("                ResourceTemplate ()");
	dsdt_line("                {");
	dsdt_line("                    Register (FFixedHW,");
	dsdt_line("                        0x00,");
	dsdt_line("                        0x00,");
	dsdt_line("                        0x0000000000000000,");
	dsdt_line("                        ,)");
	dsdt_line("                },");
	dsdt_line("");
	dsdt_line("                ResourceTemplate ()");
	dsdt_line("                {");
	dsdt_line("                    Register (FFixedHW,");
	dsdt_line("                        0x00,");
	dsdt_line("                        0x00,");
	dsdt_line("                        0x0000000000000000,");
	dsdt_line("                        ,)");
	dsdt_line("                }");
	dsdt_line("            })");
	dsdt_line("        }");

}

/* _PSS: Performance Supported States
 */
static void dsdt_write_pss(struct vmctx *ctx, int vcpu_id)
{
	uint8_t vcpu_px_cnt;
	int i;
	struct cpu_px_data *vcpu_px_data;

	vcpu_px_cnt = get_vcpu_px_cnt(ctx, vcpu_id);
	if (!vcpu_px_cnt) {
		return;
	}

	vcpu_px_data = malloc(vcpu_px_cnt * sizeof(struct cpu_px_data));
	if (!vcpu_px_data) {
		return;
	}

	/* copy and validate px data first */
	for (i = 0; i < vcpu_px_cnt; i++) {
		if (get_vcpu_px_data(ctx, vcpu_id, i, vcpu_px_data + i)) {
			/* something must be wrong, so skip the write. */
			free(vcpu_px_data);
			return;
		}
	}

	dsdt_line("");
	dsdt_line("    Method (_PSS, 0, NotSerialized)");
	dsdt_line("    {");
	dsdt_line("      Return (Package (0x%02X)", vcpu_px_cnt);
	dsdt_line("      {");

	for (i = 0; i < vcpu_px_cnt; i++) {

		dsdt_line("          Package (0x%02X)", 6);
		dsdt_line("          {");
		dsdt_line("             0x%08X,",
				(vcpu_px_data + i)->core_frequency);
		dsdt_line("             0x%08X,",
				(vcpu_px_data + i)->power);
		dsdt_line("             0x%08X,",
				(vcpu_px_data + i)->transition_latency);
		dsdt_line("             0x%08X,",
				(vcpu_px_data + i)->bus_master_latency);
		dsdt_line("             0x%08X,",
				(vcpu_px_data + i)->control);
		dsdt_line("             0x%08X",
				(vcpu_px_data + i)->status);

		if (i == (vcpu_px_cnt - 1)) {
			dsdt_line("          }");
		} else {
			dsdt_line("          },");
		}
	}
	dsdt_line("      })");
	dsdt_line("    }");

	free(vcpu_px_data);

}

void pm_write_dsdt(struct vmctx *ctx, int ncpu)
{
	int i;

	/* Scope (_PR) */
	dsdt_line("");
	dsdt_line("  Scope (_PR)");
	dsdt_line("  {");
	for (i = 0; i < ncpu; i++) {
		dsdt_line("    Processor (CPU%d, 0x%02X, 0x00000000, 0x00) {}",
					i, i);
	}
	dsdt_line("  }");
	dsdt_line("");

	/* Scope (_PR.CPU(N)) */
	for (i = 0; i < ncpu; i++) {
		dsdt_line("  Scope (_PR.CPU%d)", i);
		dsdt_line("  {");
		dsdt_line("");

		dsdt_write_pss(ctx, i);

		/* hard code _PPC and _PCT for all vpu */
		if (i == 0) {
			dsdt_write_ppc();
			dsdt_write_pct();
		} else {
			dsdt_line("    Method (_PPC, 0, NotSerialized)");
			dsdt_line("    {");
			dsdt_line("      Return (^^CPU0._PPC)");
			dsdt_line("    }");
			dsdt_line("");
			dsdt_line("    Method (_PCT, 0, NotSerialized)");
			dsdt_line("    {");
			dsdt_line("      Return (^^CPU0._PCT)");
			dsdt_line("    }");
			dsdt_line("");
		}

		dsdt_line("  }");
	}
}
