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

#ifndef ASSIGN_H
#define ASSIGN_H

enum ptdev_intr_type {
	PTDEV_INTR_MSI,
	PTDEV_INTR_INTX,
	PTDEV_INTR_INV,
};

enum ptdev_vpin_source {
	PTDEV_VPIN_IOAPIC,
	PTDEV_VPIN_PIC,
};

/* entry per guest virt vector */
struct ptdev_msi_info {
	uint32_t vmsi_addr; /* virt msi_addr */
	uint32_t vmsi_data; /* virt msi_data */
	uint16_t vmsi_ctl; /* virt msi_ctl */
	uint32_t pmsi_addr; /* phys msi_addr */
	uint32_t pmsi_data; /* phys msi_data */
	int msix;	/* 0-MSI, 1-MSIX */
	int msix_entry_index; /* MSI: 0, MSIX: index of vector table*/
	int virt_vector;
	int phys_vector;
};

/* entry per guest vioapic pin */
struct ptdev_intx_info {
	enum ptdev_vpin_source vpin_src;
	uint8_t virt_pin;
	uint8_t phys_pin;
};

/* entry per each allocated irq/vector
 * it represents a pass-thru device's remapping data entry which collecting
 * information related with its vm and msi/intx mapping & interaction nodes
 * with interrupt handler and softirq.
 */
struct ptdev_remapping_info {
	struct vm *vm;
	uint16_t virt_bdf;	/* PCI bus:slot.func*/
	uint16_t phys_bdf;	/* PCI bus:slot.func*/
	uint32_t active;	/* 1=active, 0=inactive and to free*/
	enum ptdev_intr_type type;
	struct dev_handler_node *node;
	struct list_head softirq_node;
	struct list_head entry_node;

	union {
		struct ptdev_msi_info msi;
		struct ptdev_intx_info intx;
	} ptdev_intr_info;
};

void ptdev_intx_ack(struct vm *vm, int virt_pin,
		enum ptdev_vpin_source vpin_src);
int ptdev_msix_remap(struct vm *vm, uint16_t virt_bdf,
		struct ptdev_msi_info *info);
int ptdev_intx_pin_remap(struct vm *vm, struct ptdev_intx_info *info);
void ptdev_softirq(int cpu);
void ptdev_init(void);
void ptdev_release_all_entries(struct vm *vm);
int ptdev_add_intx_remapping(struct vm *vm, uint16_t virt_bdf,
	uint16_t phys_bdf, uint8_t virt_pin, uint8_t phys_pin, bool pic_pin);
void ptdev_remove_intx_remapping(struct vm *vm, uint8_t virt_pin, bool pic_pin);
int ptdev_add_msix_remapping(struct vm *vm, uint16_t virt_bdf,
	uint16_t phys_bdf, int vector_count);
void ptdev_remove_msix_remapping(struct vm *vm, uint16_t virt_bdf,
		int vector_count);
int get_ptdev_info(char *str, int str_max);

#endif /* ASSIGN_H */
