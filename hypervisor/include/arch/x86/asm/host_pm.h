/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef	HOST_PM_H
#define	HOST_PM_H

#include <acrn_common.h>

#define	BIT_SLP_TYPx	10U
#define	BIT_SLP_EN	13U
#define	BIT_WAK_STS	15U

struct cpu_state_info {
	uint8_t			 px_cnt;	/* count of all Px states */
	const struct cpu_px_data *px_data;
	uint8_t			 cx_cnt;	/* count of all Cx entries */
	const struct cpu_cx_data *cx_data;
};

struct cpu_state_table {
	char			model_name[64];
	struct cpu_state_info	state_info;
};

struct acpi_reset_reg {
	struct acpi_generic_address reg;
	uint8_t val;
};

struct pm_s_state_data *get_host_sstate_data(void);
void host_enter_s3(const struct pm_s_state_data *sstate_data, uint32_t pm1a_cnt_val, uint32_t pm1b_cnt_val);
void shutdown_system(void);
void save_s5_reg_val(uint32_t pm1a_cnt_val, uint32_t pm1b_cnt_val);
void do_acpi_sx(const struct pm_s_state_data *sstate_data, uint32_t pm1a_cnt_val, uint32_t pm1b_cnt_val);
extern void asm_enter_s3(const struct pm_s_state_data *sstate_data, uint32_t pm1a_cnt_val, uint32_t pm1b_cnt_val);
extern void restore_s3_context(void);
struct cpu_state_info *get_cpu_pm_state_info(void);
struct acpi_reset_reg *get_host_reset_reg_data(void);
void reset_host(void);

#endif	/* HOST_PM_H */
