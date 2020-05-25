/*
 * Copyright (c) 2012 NetApp, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#ifndef _PASSTHROUGH_H_
#define _PASSTHROUGH_H_

#include <pciaccess.h>
#include "types.h"
#include "pci_core.h"
#include "pciio.h"

struct passthru_dev {
	struct pci_vdev *dev;
	struct pcibar bar[PCI_BARMAX + 1];
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
	bool (*has_virt_pcicfg_regs)(int offset);
};

#endif

