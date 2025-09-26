/*
 * Copyright (C) 2018-2025 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef	HOST_PM_H
#define	HOST_PM_H

#include <asm/host_pm.h>
#include <logmsg.h>

void arch_shutdown_host(void);
void arch_reset_host(bool warm);

static inline void shutdown_host(void) {
	pr_info("Shutting down ACRN");
	arch_shutdown_host();
}

static inline void reset_host(bool warm) {
	pr_info("%s resetting ACRN", warm ? "Warm" : "Cold");
	arch_reset_host(warm);
}

#endif /* HOST_PM_H */
