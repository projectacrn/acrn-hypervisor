/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __PASSTHRU_H__
#define __PASSTHRU_H__

#include <types.h>

#include "pciaccess.h"
#include "pci_core.h"
#include "pciio.h"

struct passthru_dev {
	struct pci_vdev *dev;
	struct pcibar bar[PCI_BARMAX + 2];
	struct {
		int		capoff;
	} msi;
	struct {
		int		capoff;
	} msix;
	struct {
		int 		capoff;
	} pmcap;
	bool pcie_cap;
	struct pcisel sel;
	int phys_pin;
	uint16_t phys_bdf;
	struct pci_device *phys_dev;
	/* Options for passthrough device:
	 *   need_reset - reset dev before passthrough
	 */
	bool need_reset;
	bool d3hot_reset;
	bool need_rombar;
	char *rom_buffer;
	bool (*has_virt_pcicfg_regs)(int offset);
};

#endif
