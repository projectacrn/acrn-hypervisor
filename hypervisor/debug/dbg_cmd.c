/*
 * Copyright (C) 2018 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <rtl.h>
#include <pci.h>
#include <uart16550.h>
#include <dbg_cmd.h>

#define MAX_PORT			0x10000  /* port 0 - 64K */
#define DEFAULT_UART_PORT	0x3F8

#define MAX_CMD_LEN		64

static struct uart_cmd {
	const char *const str;
	int type;
} cmd_list[] = {
	{ "uart=port@",	PIO },	/* uart=port@0x3F8 */
	{ "uart=bdf@",	PCI },	/* uart=bdf@0xc1 */
	{ "uart=mmio@",	MMIO },	/* uart=mmio@0xfe040000 */
	{ "uart=disabled", INVALID }
};

bool handle_dbg_cmd(const char *cmd, int32_t len)
{
	uint32_t i;
	uint64_t data;

	for (i = 0; i < ARRAY_SIZE(cmd_list); i++) {
		int32_t tmp = strnlen_s(cmd_list[i].str, MAX_CMD_LEN);
		int type = cmd_list[i].type;

		/* cmd prefix should be same with one in cmd_list */
		if (len < tmp)
			continue;

		if (strncmp(cmd_list[i].str, cmd, tmp) != 0)
			continue;

		if (type == INVALID) {
			/* set uart disabled*/
			uart16550_set_property(false, type, 0UL);
		} else if (type == PIO) {
			data = strtoul_hex(cmd + tmp);

			if (data > MAX_PORT) {
				data = DEFAULT_UART_PORT;
			}

			uart16550_set_property(true, type, data);
		} else {
			data = strtoul_hex(cmd + tmp);
			uart16550_set_property(true, type, data);
		}
	}

	return (i < ARRAY_SIZE(cmd_list));
}
