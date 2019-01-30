/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CAT_H
#define CAT_H

/* The intel Resource Director Tech(RDT) based Cache Allocation Tech support */
struct cat_hw_info {
	bool support;		/* If L2/L3 CAT supported */
	bool enabled;		/* If any VM setup CLOS */
	uint32_t bitmask;	/* Used by other entities */
	uint16_t cbm_len;	/* Length of Cache mask in bits */
	uint16_t clos_max;	/* Maximum CLOS supported, the number of cache masks */

	uint32_t res_id;
};

extern struct cat_hw_info cat_cap_info;
void setup_clos(uint16_t pcpu_id);

#define CAT_RESID_L3   1U
#define CAT_RESID_L2   2U

int32_t init_cat_cap_info(void);

#endif	/* CAT_H */
