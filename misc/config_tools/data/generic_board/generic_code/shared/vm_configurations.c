/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/vm_config.h>
#include <vuart.h>
#include <asm/pci_dev.h>
#include <asm/pgtable.h>
#include <schedule.h>


extern struct acrn_vm_pci_dev_config service_vm_pci_devs[CONFIG_MAX_PCI_DEV_NUM];
struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {
{/* Static configured VM0 */
CONFIG_SERVICE_VM,
.name = "ACRN_Service_VM",
/* Allow Service VM to reboot the system since it is the highest priority VM. */
.vm_prio = PRIO_LOW,
.guest_flags = GUEST_FLAG_STATIC_VM,
.cpu_affinity = SERVICE_VM_CONFIG_CPU_AFFINITY,
.memory = {
.start_hpa = 0UL,
},
.os_config = {
.name = "ACRN Service VM OS",
.kernel_type = KERNEL_BZIMAGE,
.kernel_mod_tag = "Linux_bzImage",
.ramdisk_mod_tag = "",
.bootargs = SERVICE_VM_OS_BOOTARGS,
},
.vuart[0] = {
.type = VUART_LEGACY_PIO,
.addr.port_base = 0x3F8U,
.irq = 4U,
},
.vuart[1] = {
.type = VUART_LEGACY_PIO,
.addr.port_base = 0x2F8U,
.irq = 3U,
.t_vuart.vm_id = 2U,
.t_vuart.vuart_id = 1U,
},
.pci_dev_num = 0U,
.pci_devs = service_vm_pci_devs,
},
{/* Static configured VM1 */
CONFIG_POST_STD_VM,
.name = "POST_STD_VM1",
.vm_prio = PRIO_LOW,
.guest_flags = GUEST_FLAG_STATIC_VM,
.cpu_affinity = VM1_CONFIG_CPU_AFFINITY,
.vuart[0] = {
.type = VUART_LEGACY_PIO,
.addr.port_base = 0x3F8U,
.irq = 4U,
},
.vuart[1] = {
.type = VUART_LEGACY_PIO,
},
},
{/* Static configured VM2 */
CONFIG_POST_RT_VM,
.name = "POST_RT_VM1",
.vm_prio = PRIO_LOW,
.guest_flags = (GUEST_FLAG_STATIC_VM|GUEST_FLAG_LAPIC_PASSTHROUGH|GUEST_FLAG_RT),
.cpu_affinity = VM2_CONFIG_CPU_AFFINITY,
.vuart[0] = {
.type = VUART_LEGACY_PIO,
.addr.port_base = 0x3F8U,
.irq = 4U,
},
.vuart[1] = {
.type = VUART_LEGACY_PIO,
.addr.port_base = 0x2F8U,
.irq = 3U,
.t_vuart.vm_id = 0U,
.t_vuart.vuart_id = 1U,
},
},
{/* Static configured VM3 */
CONFIG_POST_STD_VM,
.name = "POST_STD_VM2",
.vm_prio = PRIO_LOW,
.guest_flags = GUEST_FLAG_STATIC_VM,
.cpu_affinity = VM3_CONFIG_CPU_AFFINITY,
.vuart[0] = {
.type = VUART_LEGACY_PIO,
.addr.port_base = 0x3F8U,
.irq = 4U,
},
.vuart[1] = {
.type = VUART_LEGACY_PIO,
},
},
{/* Static configured VM4 */
CONFIG_POST_STD_VM,
.name = "POST_STD_VM3",
.vm_prio = PRIO_LOW,
.guest_flags = GUEST_FLAG_STATIC_VM,
.cpu_affinity = VM4_CONFIG_CPU_AFFINITY,
.vuart[0] = {
.type = VUART_LEGACY_PIO,
.addr.port_base = 0x3F8U,
.irq = 4U,
},
.vuart[1] = {
.type = VUART_LEGACY_PIO,
},
},
{/* Static configured VM5 */
CONFIG_POST_STD_VM,
.name = "POST_STD_VM4",
.vm_prio = PRIO_LOW,
.guest_flags = GUEST_FLAG_STATIC_VM,
.cpu_affinity = VM5_CONFIG_CPU_AFFINITY,
.vuart[0] = {
.type = VUART_LEGACY_PIO,
.addr.port_base = 0x3F8U,
.irq = 4U,
},
.vuart[1] = {
.type = VUART_LEGACY_PIO,
},
},
{/* Static configured VM6 */
CONFIG_POST_STD_VM,
.name = "POST_STD_VM5",
.vm_prio = PRIO_LOW,
.guest_flags = GUEST_FLAG_STATIC_VM,
.cpu_affinity = VM6_CONFIG_CPU_AFFINITY,
.vuart[0] = {
.type = VUART_LEGACY_PIO,
.addr.port_base = 0x3F8U,
.irq = 4U,
},
.vuart[1] = {
.type = VUART_LEGACY_PIO,
},
},
{/* Dynamic configured  VM7 */
CONFIG_POST_STD_VM,
}

};
