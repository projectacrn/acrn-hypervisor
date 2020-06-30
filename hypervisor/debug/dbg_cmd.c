/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <rtl.h>
#include <pci.h>
#include <uart16550.h>
#include <dbg_cmd.h>
#include <vm_config.h>

#define MAX_PORT			0x10000  /* port 0 - 64K */
#define DEFAULT_UART_PORT	0x3F8

#define MAX_CMD_LEN		64

static const char * const cmd_list[] = {
	"uart=disabled",	/* to disable uart */
	"uart=port@",		/* like uart=port@0x3F8 */
	"uart=bdf@",	/*like: uart=bdf@0:18.2, it is for ttyS2 */
};

enum IDX_CMD_DBG {
	IDX_DISABLE_UART,
	IDX_PORT_UART,
	IDX_PCI_UART,

	IDX_MAX_CMD,
};

static void update_sos_vm_config_uart_irq(uint64_t irq)
{
	uint16_t vm_id;
	struct acrn_vm_config *vm_config;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);
		if (vm_config->load_order == SOS_VM) {
			break;
		}
	}

	if (vm_id != CONFIG_MAX_VM_NUM) {
		vm_config->vuart[0].irq = irq & 0xFFFFU;
	}
}

static void update_sos_vm_config_uart_ioport(uint64_t addr)
{
	uint16_t vm_id;
	struct acrn_vm_config *vm_config;

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm_config = get_vm_config(vm_id);
		if (vm_config->load_order == SOS_VM) {
			break;
		}
	}

	if (vm_id != CONFIG_MAX_VM_NUM) {
		vm_config->vuart[0].addr.port_base = addr & 0xFFFFU;
	}
}

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
			char *pos, *start = (char *)cmd + tmp;
			uint64_t addr, irq;

			pos = strchr(start, ',');
			if (pos != NULL) {
				*pos = '\0';
				pos++;
				irq = strtoul_hex(pos);
				update_sos_vm_config_uart_irq(irq);
			}
			addr = strtoul_hex(start);

			if (addr > MAX_PORT) {
				addr = DEFAULT_UART_PORT;
			}

			update_sos_vm_config_uart_ioport(addr);
			uart16550_set_property(true, true, addr);

		} else if (i == IDX_PCI_UART) {
			uart16550_set_property(true, false, (uint64_t)(cmd+tmp));
		} else {
			/* No other state currently, do nothing */
		}
	}

	if (i < IDX_MAX_CMD) {
		handled = true;
	}

	return handled;
}
