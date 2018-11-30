/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

int validate_pstate(const struct acrn_vm *vm, uint64_t perf_ctl)
{
	const struct cpu_px_data *px_data;
	uint8_t i, px_cnt;

	if (is_vm0(vm)) {
		return 0;
	}

	px_cnt = vm->pm.px_cnt;
	px_data = vm->pm.px_data;

	if ((px_cnt == 0U) || (px_data == NULL)) {
		return -1;
	}

	for (i = 0U; i < px_cnt; i++) {
		if ((px_data + i)->control == (perf_ctl & 0xffffUL)) {
			return 0;
		}
	}

	return -1;
}

static void vm_setup_cpu_px(struct acrn_vm *vm)
{
	uint32_t px_data_size;

	vm->pm.px_cnt = 0U;
	(void)memset(vm->pm.px_data, 0U,
			MAX_PSTATE * sizeof(struct cpu_px_data));

	if ((boot_cpu_data.state_info.px_cnt != 0U)
		&& (boot_cpu_data.state_info.px_data != NULL)) {
		ASSERT ((boot_cpu_data.state_info.px_cnt <= MAX_PSTATE),
			"failed to setup cpu px");

		vm->pm.px_cnt = boot_cpu_data.state_info.px_cnt;

		px_data_size = ((uint32_t)vm->pm.px_cnt) * sizeof(struct cpu_px_data);

		(void)memcpy_s(vm->pm.px_data, px_data_size,
				boot_cpu_data.state_info.px_data, px_data_size);
	}
}

static void vm_setup_cpu_cx(struct acrn_vm *vm)
{
	uint32_t cx_data_size;

	vm->pm.cx_cnt = 0U;
	(void)memset(vm->pm.cx_data, 0U,
			MAX_CSTATE * sizeof(struct cpu_cx_data));

	if ((boot_cpu_data.state_info.cx_cnt != 0U)
		&& (boot_cpu_data.state_info.cx_data != NULL)) {
		ASSERT ((boot_cpu_data.state_info.cx_cnt <= MAX_CX_ENTRY),
			"failed to setup cpu cx");

		vm->pm.cx_cnt = boot_cpu_data.state_info.cx_cnt;

		cx_data_size = ((uint32_t)vm->pm.cx_cnt) * sizeof(struct cpu_cx_data);

		/* please note pm.cx_data[0] is a empty space holder,
		 * pm.cx_data[1...MAX_CX_ENTRY] would be used to store cx entry datas.
		 */
		(void)memcpy_s(vm->pm.cx_data + 1, cx_data_size,
				boot_cpu_data.state_info.cx_data, cx_data_size);
	}
}

static inline void init_cx_port(struct acrn_vm *vm)
{
	uint8_t cx_idx;

	for (cx_idx = 2U; cx_idx <= vm->pm.cx_cnt; cx_idx++) {
		struct cpu_cx_data *cx_data = vm->pm.cx_data + cx_idx;

		if (cx_data->cx_reg.space_id == SPACE_SYSTEM_IO) {
			uint16_t port = (uint16_t)cx_data->cx_reg.address;

			allow_guest_pio_access(vm, port, 1U);
		}
	}
}

void vm_setup_cpu_state(struct acrn_vm *vm)
{
	vm_setup_cpu_px(vm);
	vm_setup_cpu_cx(vm);
	init_cx_port(vm);
}

/* This function is for power management Sx state implementation,
 * VM need to load the Sx state data to implement S3/S5.
 */
int vm_load_pm_s_state(struct acrn_vm *vm)
{
#ifdef ACPI_INFO_VALIDATED
	vm->pm.sx_state_data = (struct pm_s_state_data *)&host_pm_s_state;
	pr_info("System S3/S5 is supported.");
	return 0;
#else
	vm->pm.sx_state_data = NULL;
	pr_err("System S3/S5 is NOT supported.");
	return -1;
#endif
}

static inline uint32_t s3_enabled(uint32_t pm1_cnt)
{
	return pm1_cnt & (1U << BIT_SLP_EN);
}

static inline uint8_t get_slp_typx(uint32_t pm1_cnt)
{
	return (uint8_t)((pm1_cnt & 0x1fffU) >> BIT_SLP_TYPx);
}

static uint32_t pm1ab_io_read(__unused struct acrn_vm *vm, uint16_t addr,
			size_t width)
{
	uint32_t val = pio_read(addr, width);

	if (host_enter_s3_success == 0U) {
		/* If host S3 enter failes, we should set BIT_WAK_STS
		 * bit for vm0 and let vm0 back from S3 failure path.
		 */
		if (addr == vm->pm.sx_state_data->pm1a_evt.address) {
			val |= (1U << BIT_WAK_STS);
		}
	}
	return val;
}

static void pm1ab_io_write(__unused struct acrn_vm *vm, uint16_t addr, size_t width,
			uint32_t v)
{
	static uint32_t pm1a_cnt_ready = 0U;

	if (width == 2U) {
		uint8_t val = get_slp_typx(v);

		if ((addr == vm->pm.sx_state_data->pm1a_cnt.address)
			&& (val == vm->pm.sx_state_data->s3_pkg.val_pm1a)
			&& (s3_enabled(v) != 0U)) {

			if (vm->pm.sx_state_data->pm1b_cnt.address != 0UL) {
				pm1a_cnt_ready = v;
			} else {
				enter_s3(vm, v, 0U);
			}
			return;
		}

		if ((addr == vm->pm.sx_state_data->pm1b_cnt.address)
			&& (val == vm->pm.sx_state_data->s3_pkg.val_pm1b)
			&& (s3_enabled(v) != 0U)) {

			if (pm1a_cnt_ready != 0U) {
				enter_s3(vm, pm1a_cnt_ready, v);
				pm1a_cnt_ready = 0U;
			} else {
				/* the case broke ACPI spec */
				pr_err("PM1B_CNT write error!");
			}
			return;
		}
	}

	pio_write(v, addr, width);
}

static void
register_gas_io_handler(struct acrn_vm *vm, uint32_t pio_idx, const struct acpi_generic_address *gas)
{
	uint8_t io_len[5] = {0, 1, 2, 4, 8};
	struct vm_io_range gas_io;

	if ((gas->address != 0UL)
			&& (gas->space_id == SPACE_SYSTEM_IO)
			&& (gas->access_size != 0U)
			&& (gas->access_size <= 4U)) {
		gas_io.flags = IO_ATTR_RW;
		gas_io.base = (uint16_t)gas->address;
		gas_io.len = io_len[gas->access_size];

		register_io_emulation_handler(vm, pio_idx, &gas_io,
				&pm1ab_io_read, &pm1ab_io_write);

		pr_dbg("Enable PM1A trap for VM %d, port 0x%x, size %d\n",
				vm->vm_id, gas_io.base, gas_io.len);
	}
}

void register_pm1ab_handler(struct acrn_vm *vm)
{
	struct pm_s_state_data *sx_data = vm->pm.sx_state_data;

	register_gas_io_handler(vm, PM1A_EVT_PIO_IDX, &(sx_data->pm1a_evt));
	register_gas_io_handler(vm, PM1B_EVT_PIO_IDX, &(sx_data->pm1b_evt));
	register_gas_io_handler(vm, PM1A_CNT_PIO_IDX, &(sx_data->pm1a_cnt));
	register_gas_io_handler(vm, PM1B_CNT_PIO_IDX, &(sx_data->pm1b_cnt));
}
