/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


/*
* Emulate GPIO registers which are only accessible through Primary to Sideband Bridge (P2SB).
*
* Intercept accesses to MISCCFG.GPDMINTSEL[31:24] and PADCFG1.INTSEL[7:0] GPIO registers which hold physical interrupt
* lines and return virtualized values upon read in accordance with the gsi to vgsi mappings given by the VM config.
*
* P2SB_BAR_ADDR: 0xFD000000 (fixed by BIOS)
*
*
*	--------------------------------------------------------------------------------------
*		SideBand Endpoint Name		|	Port ID
*	--------------------------------------------------------------------------------------
*		GPIO Community 5		|	0x69
*		GPIO Community 4		|	0x6A
*		GPIO Community 3		|	0x6B
*		GPIO Community 2		|	0x6C
*		GPIO Community 1		|	0x6D
*		GPIO Community 0		|	0x6E
*	--------------------------------------------------------------------------------------
*
* Private Configuration Register (PCR) Address = P2SB_BAR_ADDR + (Port ID << 16) + Register Offset
* e.g.)
*	GPIO_COMMUNITY_5_PCR_BASE  = P2SB_BAR_ADDR + (0x69 << 16) = 0xFD690000
*	GPIO_COMMUNITY_5_MISCCFG   = GPIO_COMMUNITY_5_PCR_BASE + 0x010 = 0xFD690010
*	GPIO_COMMUNITY_5_PAD0_CFG1 = GPIO_COMMUNITY_5_PCR_BASE + 0x704 = 0xFD690704
*	GPIO_COMMUNITY_5_PAD1_CFG1 = GPIO_COMMUNITY_5_PCR_BASE + 0x714 = 0xFD690714
*	GPIO_COMMUNITY_5_PAD2_CFG1 = GPIO_COMMUNITY_5_PCR_BASE + 0x724 = 0xFD690724
*	....
*
*/

#include <types.h>
#include <errno.h>
#include <x86/guest/vm.h>
#include <x86/guest/ept.h>
#include <x86/guest/assign.h>
#include <x86/io.h>
#include <x86/mmu.h>

#ifdef P2SB_VGPIO_DM_ENABLED

#define P2SB_PORTID_SHIFT		16U
#define P2SB_AGENT_NUM			256U
#define P2SB_PCR_SPACE_SIZE_PER_AGENT	0x10000U
#define P2SB_PCR_SPACE_SIZE_TOTAL	(P2SB_AGENT_NUM * P2SB_PCR_SPACE_SIZE_PER_AGENT)
#define P2SB_PCR_SPACE_MASK		((1UL << P2SB_PORTID_SHIFT) - 1UL)

#define GPIO_MISCCFG			0x010U
#define GPIO_MISGCFG_GPDMINTSEL_SHIFT	24U
#define GPIO_PADBAR			0x00CU
#define GPIO_PADCFG1			0x004U
#define GPIO_PADCFG1_INTSEL_SHIFT	0U

#define GPIO_INVALID_PIN		0xFFU

/**
 * @return vpin mapped to the given phys_pin in accordance with the VM config, if not found return 0xFF as invalid pin
 */
static uint32_t ioapic_pin_to_vpin(struct acrn_vm *vm, const struct acrn_vm_config *vm_config, const uint32_t phys_pin)
{
	uint32_t i;
	uint32_t vpin = GPIO_INVALID_PIN;
	struct acrn_single_vioapic *vioapic;

	for (i = 0U; i < vm_config->pt_intx_num; i++) {
		if (phys_pin == gsi_to_ioapic_pin(vm_config->pt_intx[i].phys_gsi)) {
			vioapic = vgsi_to_vioapic_and_vpin(vm, vm_config->pt_intx[i].virt_gsi, &vpin);
			if (!vioapic) {
				vpin = GPIO_INVALID_PIN;
			}
			break;
		}
	}

	return vpin;
}

static int32_t vgpio_mmio_handler(struct io_request *io_req, void *data)
{
	struct mmio_request *mmio = &io_req->reqs.mmio;
	struct acrn_vm *vm = (struct acrn_vm *) data;
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	int32_t ret = 0;

	uint64_t hpa = P2SB_BAR_ADDR + (mmio->address & (uint64_t)(P2SB_PCR_SPACE_SIZE_TOTAL - 1));
	void    *hva = hpa2hva(hpa);
	uint64_t reg_offset = hpa & P2SB_PCR_SPACE_MASK;

	uint32_t value, shift;
	uint32_t padbar, pad0;
	uint32_t phys_pin, virt_pin;

	/* all gpio registers have 4 bytes size */
	if (mmio->size == 4U) {
		if (mmio->direction == REQUEST_READ) {
			padbar = mmio_read32((const void *)hpa2hva((hpa & ~P2SB_PCR_SPACE_MASK) + GPIO_PADBAR));
			pad0   = padbar & P2SB_PCR_SPACE_MASK;
			value  = mmio_read32((const void *)hva);

			if ((reg_offset == GPIO_MISCCFG) ||
				((reg_offset >= pad0) && ((reg_offset & 0x0FU) == GPIO_PADCFG1))) {
				shift = (reg_offset == GPIO_MISCCFG) ? GPIO_MISGCFG_GPDMINTSEL_SHIFT :
									GPIO_PADCFG1_INTSEL_SHIFT;
				phys_pin = (value >> shift) & 0xFFU;
				virt_pin = ioapic_pin_to_vpin(vm, vm_config, phys_pin);
				value = (value & ~(0xFFU << shift)) | (virt_pin << shift);
			}
			mmio->value = (uint64_t)value;
		} else {
			value = (uint32_t)mmio->value;
			if (reg_offset == GPIO_MISCCFG) { /* discard writes to MISCCFG.GPDMINTSEL[31:24] */
				value = (value & ~(0xFFU << GPIO_MISGCFG_GPDMINTSEL_SHIFT)) |
					(mmio_read32((const void *)hva) & (0xFFU << GPIO_MISGCFG_GPDMINTSEL_SHIFT));
			}
			mmio_write32(value, (void *)hva);
		}
	} else {
		ret = -EINVAL;
	}

    return ret;
}

/**
 * @pre vm != NULL && mmiodev != NULL
 */
void register_vgpio_handler(struct acrn_vm *vm, const struct acrn_mmiodev *mmiodev)
{
	uint64_t gpa_start, gpa_end, gpio_pcr_sz;
	uint64_t base_hpa;

	gpa_start   = mmiodev->base_gpa + (P2SB_BASE_GPIO_PORT_ID << P2SB_PORTID_SHIFT);
	gpio_pcr_sz = P2SB_PCR_SPACE_SIZE_PER_AGENT * P2SB_MAX_GPIO_COMMUNITIES;
	gpa_end     = gpa_start + gpio_pcr_sz;
	base_hpa    = mmiodev->base_hpa + (P2SB_BASE_GPIO_PORT_ID << P2SB_PORTID_SHIFT);

	/* emulate MMIO access to the GPIO private configuration space registers */
	ppt_clear_user_bit((uint64_t)hpa2hva(base_hpa), gpio_pcr_sz);
	register_mmio_emulation_handler(vm, vgpio_mmio_handler, gpa_start, gpa_end, (void *)vm, false);
	ept_del_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, gpa_start, gpio_pcr_sz);
}

#endif
