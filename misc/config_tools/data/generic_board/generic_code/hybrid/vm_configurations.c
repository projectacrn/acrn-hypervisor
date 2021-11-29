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


extern struct pt_intx_config vm0_pt_intx[1U];
extern struct acrn_vm_pci_dev_config service_vm_pci_devs[CONFIG_MAX_PCI_DEV_NUM];
struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] = {
{/* Static configured VM0 */
CONFIG_SAFETY_VM,
.name = "SAFETY_VM0",
.vm_prio = PRIO_LOW,
.guest_flags = GUEST_FLAG_STATIC_VM,
.cpu_affinity = VM0_CONFIG_CPU_AFFINITY,
.memory = {
.start_hpa = VM0_CONFIG_MEM_START_HPA,
.size = VM0_CONFIG_MEM_SIZE,
.start_hpa2 = VM0_CONFIG_MEM_START_HPA2,
.size_hpa2 = VM0_CONFIG_MEM_SIZE_HPA2,
},
.os_config = {
.name = "Zephyr",
.kernel_type = KERNEL_ELF,
.kernel_mod_tag = "Zephyr_ElfImage",
.ramdisk_mod_tag = "",
},
.acpi_config = {
.acpi_mod_tag = "ACPI_VM0",
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
.t_vuart.vm_id = 1U,
.t_vuart.vuart_id = 1U,
},
#ifdef VM0_PASSTHROUGH_TPM
.pt_tpm2 = true,
.mmiodevs[0] = {
.name = "tpm2",
.res[0] = {
.user_vm_pa = VM0_TPM_BUFFER_BASE_ADDR_GPA,
.host_pa = VM0_TPM_BUFFER_BASE_ADDR,
.size = VM0_TPM_BUFFER_SIZE,
.mem_type = EPT_UNCACHED,
},
.res[1] = {
.user_vm_pa = VM0_TPM_EVENTLOG_BASE_ADDR,
.host_pa = VM0_TPM_EVENTLOG_BASE_ADDR_HPA,
.size = VM0_TPM_EVENTLOG_SIZE,
.mem_type = EPT_WB,
},
},
#endif
#ifdef P2SB_BAR_ADDR
.pt_p2sb_bar = true,
.mmiodevs[0] = {
.res[0] = {
.user_vm_pa = P2SB_BAR_ADDR_GPA,
.host_pa = P2SB_BAR_ADDR,
.size = P2SB_BAR_SIZE,
},
},
#endif
.pt_intx_num = 0,
.pt_intx = vm0_pt_intx,
},
{/* Static configured VM1 */
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
.t_vuart.vm_id = 0U,
.t_vuart.vuart_id = 1U,
},
.pci_dev_num = 0U,
.pci_devs = service_vm_pci_devs,
},
{/* Static configured VM2 */
CONFIG_POST_STD_VM,
.name = "POST_STD_VM1",
.vm_prio = PRIO_LOW,
.guest_flags = GUEST_FLAG_STATIC_VM,
.cpu_affinity = VM2_CONFIG_CPU_AFFINITY,
.vuart[0] = {
.type = VUART_LEGACY_PIO,
.addr.port_base = 0x3F8U,
.irq = 4U,
},
.vuart[1] = {
.type = VUART_LEGACY_PIO,
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
{/* Dynamic configured  VM4 */
CONFIG_POST_STD_VM,
}
,
{/* Dynamic configured  VM5 */
CONFIG_POST_STD_VM,
}
,
{/* Dynamic configured  VM6 */
CONFIG_POST_STD_VM,
}
,
{/* Dynamic configured  VM7 */
CONFIG_POST_STD_VM,
}

};
