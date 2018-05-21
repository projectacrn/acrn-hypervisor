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
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>
#include <multiboot.h>

#define MAX_PORT			0x10000  /* port 0 - 64K */
#define DEFAULT_UART_PORT	0x3F8

#define ACRN_DBG_PARSE		6

#define MAX_CMD_LEN		64

static const char * const cmd_list[] = {
	"uart=disabled",	/* to disable uart */
	"uart=port@",		/* like uart=port@0x3F8 */
	"uart=mmio@",	/*like: uart=mmio@0xFC000000 */
};

enum IDX_CMD {
	IDX_DISABLE_UART,
	IDX_PORT_UART,
	IDX_MMIO_UART,

	IDX_MAX_CMD,
};

static void handle_cmd(const char *cmd, int len)
{
	int i;

	for (i = 0; i < IDX_MAX_CMD; i++) {
		int tmp = strnlen_s(cmd_list[i], MAX_CMD_LEN);

		/*cmd prefix should be same with one in cmd_list */
		if (len < tmp)
			continue;

		if (strncmp(cmd_list[i], cmd, tmp) != 0)
			continue;

		if (i == IDX_DISABLE_UART) {
			/* set uart disabled*/
			uart16550_set_property(0, 0, 0);
		} else if ((i == IDX_PORT_UART) || (i == IDX_MMIO_UART)) {
			uint64_t addr = strtoul(cmd + tmp, NULL, 16);

			dev_dbg(ACRN_DBG_PARSE, "uart addr=0x%llx", addr);

			if (i == IDX_PORT_UART) {
				if (addr > MAX_PORT)
					addr = DEFAULT_UART_PORT;

				uart16550_set_property(1, 1, addr);
			} else {
				uart16550_set_property(1, 0, addr);
			}
		}
	}
}

int parse_hv_cmdline(void)
{
	const char *start;
	const char *end;
	struct multiboot_info *mbi = NULL;

	if (boot_regs[0] != MULTIBOOT_INFO_MAGIC) {
		ASSERT(0, "no multiboot info found");
		return -EINVAL;
	}

	mbi = (struct multiboot_info *)(HPA2HVA((uint64_t)boot_regs[1]));
	dev_dbg(ACRN_DBG_PARSE, "Multiboot detected, flag=0x%x", mbi->mi_flags);

	if (!(mbi->mi_flags & MULTIBOOT_INFO_HAS_CMDLINE)) {
		dev_dbg(ACRN_DBG_PARSE, "no hv cmdline!");
		return -EINVAL;
	}

	start = (char *)HPA2HVA((uint64_t)mbi->mi_cmdline);
	dev_dbg(ACRN_DBG_PARSE, "hv cmdline: %s", start);

	do {
		while (*start == ' ')
			start++;

		end = start + 1;
		while (*end != ' ' && *end)
			end++;

		handle_cmd(start, end - start);
		start = end + 1;

	} while (*end && *start);

	return 0;
}
