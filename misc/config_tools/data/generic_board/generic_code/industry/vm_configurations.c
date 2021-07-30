/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <asm/vm_config.h>
#include <vuart.h>
#include <asm/pci_dev.h>

extern struct acrn_vm_pci_dev_config sos_pci_devs[CONFIG_MAX_PCI_DEV_NUM];

extern struct pt_intx_config vm0_pt_intx[1U];

struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {
	{	/* VM0 */
		CONFIG_SOS_VM,
		.name = "ACRN SOS VM",

		/* Allow SOS to reboot the host since there is supposed to be the highest severity guest */
		.guest_flags = 0UL,
#ifdef CONFIG_RDT_ENABLED
		.clos = VM0_VCPU_CLOS,
#endif
		.cpu_affinity = SOS_VM_CONFIG_CPU_AFFINITY,
		.memory = {
			.start_hpa = 0UL,
		},
		.os_config = {
			.name = "ACRN Service OS",
			.kernel_type = KERNEL_BZIMAGE,
			.kernel_mod_tag = "Linux_bzImage",
			.bootargs = SOS_VM_BOOTARGS,
		},
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = SOS_COM1_BASE,
			.irq = SOS_COM1_IRQ,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = SOS_COM2_BASE,
			.irq = SOS_COM2_IRQ,
			.t_vuart.vm_id = 2U,
			.t_vuart.vuart_id = 1U,
		},
		.pci_dev_num = 0U,
		.pci_devs = sos_pci_devs,
	},
	{	/* VM1 */
		CONFIG_POST_STD_VM(1),
#ifdef CONFIG_RDT_ENABLED
		.clos = VM1_VCPU_CLOS,
#endif
		.cpu_affinity = VM1_CONFIG_CPU_AFFINITY,
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = COM1_BASE,
			.irq = COM1_IRQ,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		},
	},
	{	/* VM2 */
		CONFIG_POST_RT_VM(1),
#ifdef CONFIG_RDT_ENABLED
		.clos = VM2_VCPU_CLOS,
#endif
		.cpu_affinity = VM2_CONFIG_CPU_AFFINITY,
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = COM1_BASE,
			.irq = COM1_IRQ,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = COM2_BASE,
			.irq = COM2_IRQ,
			.t_vuart.vm_id = 0U,
			.t_vuart.vuart_id = 1U,
		},
	},
	{	/* VM3 */
		CONFIG_POST_STD_VM(2),
#ifdef CONFIG_RDT_ENABLED
		.clos = VM3_VCPU_CLOS,
#endif
		.cpu_affinity = VM3_CONFIG_CPU_AFFINITY,
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = COM1_BASE,
			.irq = COM1_IRQ,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		},
	},
	{	/* VM4 */
		CONFIG_POST_STD_VM(3),
#ifdef CONFIG_RDT_ENABLED
		.clos = VM4_VCPU_CLOS,
#endif
		.cpu_affinity = VM4_CONFIG_CPU_AFFINITY,
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = COM1_BASE,
			.irq = COM1_IRQ,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		},
	},
	{	/* VM5 */
		CONFIG_POST_STD_VM(4),
#ifdef CONFIG_RDT_ENABLED
		.clos = VM5_VCPU_CLOS,
#endif
		.cpu_affinity = VM5_CONFIG_CPU_AFFINITY,
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = COM1_BASE,
			.irq = COM1_IRQ,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		},
	},
	{	/* VM6 */
		CONFIG_POST_STD_VM(5),
#ifdef CONFIG_RDT_ENABLED
		.clos = VM6_VCPU_CLOS,
#endif
		.cpu_affinity = VM6_CONFIG_CPU_AFFINITY,
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = COM1_BASE,
			.irq = COM1_IRQ,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		},
	},
	{	/* VM7 */
		CONFIG_KATA_VM(1),
#ifdef CONFIG_RDT_ENABLED
		.clos = VM7_VCPU_CLOS,
#endif
		.cpu_affinity = VM7_CONFIG_CPU_AFFINITY,
		.vuart[0] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
			.irq = COM1_IRQ,
		},
		.vuart[1] = {
			.type = VUART_LEGACY_PIO,
			.addr.port_base = INVALID_COM_BASE,
		},
	},
};
