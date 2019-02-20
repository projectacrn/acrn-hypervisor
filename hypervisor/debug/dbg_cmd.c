/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <rtl.h>
#include <pci.h>
#include <uart16550.h>
#include <vuart.h>

#define MAX_PORT			0x10000  /* port 0 - 64K */
#define DEFAULT_UART_PORT	0x3F8

#define MAX_CMD_LEN		64

static const char * const cmd_list[] = {
	"uart=disabled",	/* to disable uart */
	"uart=port@",		/* like uart=port@0x3F8 */
	"uart=bdf@",	/*like: uart=bdf@0:18.2, it is for ttyS2 */

	/* format: vuart=ttySx@irqN, like vuart=ttyS1@irq6; better to unify
	 * uart & vuart & SOS console the same one, and irq same with the native.
	 * ttySx range (0-3), irqN (0-255)
	 */
	"vuart=ttyS",
};

enum IDX_CMD_DBG {
	IDX_DISABLE_UART,
	IDX_PORT_UART,
	IDX_PCI_UART,
	IDX_SET_VUART,

	IDX_MAX_CMD,
};

bool handle_dbg_cmd(const char *cmd, int32_t len)
{
	int32_t i;
	bool handled = false;

	for (i = 0; i < IDX_MAX_CMD; i++) {
		int32_t tmp = strnlen_s(cmd_list[i], MAX_CMD_LEN);

		/*cmd prefix should be same with one in cmd_list */
		if (len < tmp)
			continue;

		if (strncmp(cmd_list[i], cmd, tmp) != 0)
			continue;

		if (i == IDX_DISABLE_UART) {
			/* set uart disabled*/
			uart16550_set_property(false, false, 0UL);
		} else if (i == IDX_PORT_UART) {
			uint64_t addr = strtoul_hex(cmd + tmp);

			if (addr > MAX_PORT) {
				addr = DEFAULT_UART_PORT;
			}

			uart16550_set_property(true, true, addr);

		} else if (i == IDX_PCI_UART) {
			uart16550_set_property(true, false, (uint64_t)(cmd+tmp));
		} else if (i == IDX_SET_VUART) {
			vuart_set_property(cmd+tmp);
		}
	}

	if (i < IDX_MAX_CMD) {
		handled = true;
	}

	return handled;
}
