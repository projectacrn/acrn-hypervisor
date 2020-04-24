/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RDT_H
#define RDT_H

enum {
	RDT_RESOURCE_L3,
	RDT_RESOURCE_L2,
	RDT_RESOURCE_MBA,

	/* Must be the last */
	RDT_NUM_RESOURCES,
};

#define RDT_RESID_L3    1U
#define RDT_RESID_L2    2U
#define RDT_RESID_MBA   3U

extern const uint16_t hv_clos;
extern const uint16_t platform_clos_num;

/* The intel Resource Director Tech(RDT) based Allocation Tech support */
struct rdt_info {
	union {
		struct {
			uint32_t bitmask;	/* A bitmask where each set bit indicates the corresponding cache way
						   may be used by other entities in the platform (e.g. GPU) */
			uint16_t cbm_len;	/* Length of Cache mask in bits */
		} cache;
		struct rdt_membw {
			uint16_t mba_max;	/* Max MBA delay throttling value supported */
			bool delay_linear;	/* True if memory B/W delay is in linear scale */
		} membw;
	} res;
	uint16_t clos_max;	/* Maximum CLOS supported, 0 indicates resource is not supported.*/
	uint32_t res_id;
	uint32_t msr_base;	/* MSR base to program clos mask*/
	struct platform_clos_info *platform_clos_array; /* user configured mask and MSR info for each CLOS*/
};

int32_t init_rdt_cap_info(void);
bool setup_clos(uint16_t pcpu_id);
uint64_t clos2pqr_msr(uint16_t clos);
bool is_platform_rdt_capable(void);

#endif	/* RDT_H */
