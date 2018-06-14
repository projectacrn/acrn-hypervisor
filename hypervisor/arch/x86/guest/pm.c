/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

int validate_pstate(struct vm *vm, uint64_t perf_ctl)
{
	struct cpu_px_data *px_data;
	int i, px_cnt;

	if (is_vm0(vm)) {
		return 0;
	}

	px_cnt = vm->pm.px_cnt;
	px_data = vm->pm.px_data;

	if (px_cnt == 0 || px_data == NULL) {
		return -1;
	}

	for (i = 0; i < px_cnt; i++) {
		if ((px_data + i)->control == (perf_ctl & 0xffffUL)) {
			return 0;
		}
	}

	return -1;
}

static void vm_setup_cpu_px(struct vm *vm)
{
	uint32_t px_data_size;

	vm->pm.px_cnt = 0;
	memset(vm->pm.px_data, 0, MAX_PSTATE * sizeof(struct cpu_px_data));

	if ((boot_cpu_data.state_info.px_cnt == 0)
		|| (boot_cpu_data.state_info.px_data == NULL)) {
		return;
	}

	ASSERT ((boot_cpu_data.state_info.px_cnt <= MAX_PSTATE),
		"failed to setup cpu px");

	vm->pm.px_cnt = boot_cpu_data.state_info.px_cnt;

	px_data_size = vm->pm.px_cnt * sizeof(struct cpu_px_data);

	memcpy_s(vm->pm.px_data, px_data_size,
			boot_cpu_data.state_info.px_data, px_data_size);

}

static void vm_setup_cpu_cx(struct vm *vm)
{
	uint32_t cx_data_size;

	vm->pm.cx_cnt = 0;
	memset(vm->pm.cx_data, 0, MAX_CSTATE * sizeof(struct cpu_cx_data));

	if ((boot_cpu_data.state_info.cx_cnt == 0)
		|| (boot_cpu_data.state_info.cx_data == NULL)) {
		return;
	}

	ASSERT ((boot_cpu_data.state_info.cx_cnt <= MAX_CX_ENTRY),
		"failed to setup cpu cx");

	vm->pm.cx_cnt = boot_cpu_data.state_info.cx_cnt;

	cx_data_size = vm->pm.cx_cnt * sizeof(struct cpu_cx_data);

	/* please note pm.cx_data[0] is a empty space holder,
	 * pm.cx_data[1...MAX_CX_ENTRY] would be used to store cx entry datas.
	 */
	memcpy_s(vm->pm.cx_data + 1, cx_data_size,
			boot_cpu_data.state_info.cx_data, cx_data_size);

}

static inline void init_cx_port(struct vm *vm)
{
	uint8_t cx_idx;

	for (cx_idx = 2; cx_idx <= vm->pm.cx_cnt; cx_idx++) {
		struct cpu_cx_data *cx_data = vm->pm.cx_data + cx_idx;

		if (cx_data->cx_reg.space_id == SPACE_SYSTEM_IO) {
			uint16_t port = (uint16_t)cx_data->cx_reg.address;

			allow_guest_io_access(vm, port, 1);
		}
	}
}

void vm_setup_cpu_state(struct vm *vm)
{
	vm_setup_cpu_px(vm);
	vm_setup_cpu_cx(vm);
	init_cx_port(vm);
}

/* This function is for power management Sx state implementation,
 * VM need to load the Sx state data to implement S3/S5.
 */
int vm_load_pm_s_state(struct vm *vm)
{
	if ((boot_cpu_data.x86 == host_acpi_info.x86_family)
		&& (boot_cpu_data.x86_model == host_acpi_info.x86_model)) {
		vm->pm.sx_state_data = (struct pm_s_state_data *)
						&host_acpi_info.pm_s_state;
		pr_info("System S3/S5 is supported.");
		return 0;
	} else {
		vm->pm.sx_state_data = NULL;
		pr_err("System S3/S5 is NOT supported.");
		return -1;
	}
}

static inline uint16_t s3_enabled(uint16_t pm1_cnt)
{
	return pm1_cnt & (1 << BIT_SLP_EN);
}

static inline uint8_t get_slp_typx(uint16_t pm1_cnt)
{
	return (pm1_cnt & 0x1fff) >> BIT_SLP_TYPx;
}

static uint32_t pm1ab_io_read(__unused struct vm_io_handler *hdlr,
		__unused struct vm *vm, uint16_t addr, size_t width)
{
	uint32_t val = io_read(addr, width);

	if (host_enter_s3_success == 0) {
		/* If host S3 enter failes, we should set BIT_WAK_STS
		 * bit for vm0 and let vm0 back from S3 failure path.
		 */
		if (addr == vm->pm.sx_state_data->pm1a_evt.address) {
			val |= (1 << BIT_WAK_STS);
		}
	}
	return val;
}

static void pm1ab_io_write(__unused struct vm_io_handler *hdlr,
		__unused struct vm *vm, uint16_t addr, size_t width,
		uint32_t v)
{
	static uint32_t pm1a_cnt_ready = 0;

	if (width == 2) {
		uint8_t val = get_slp_typx(v);

		if ((addr == vm->pm.sx_state_data->pm1a_cnt.address)
			&& (val == vm->pm.sx_state_data->s3_pkg.val_pm1a)
			&& s3_enabled(v)) {

			if (vm->pm.sx_state_data->pm1b_cnt.address) {
				pm1a_cnt_ready = v;
			} else {
				enter_s3(vm, v, 0);
			}
			return;
		}

		if ((addr == vm->pm.sx_state_data->pm1b_cnt.address)
			&& (val == vm->pm.sx_state_data->s3_pkg.val_pm1b)
			&& s3_enabled(v)) {

			if (pm1a_cnt_ready) {
				enter_s3(vm, pm1a_cnt_ready, v);
				pm1a_cnt_ready = 0;
			} else {
				/* the case broke ACPI spec */
				pr_err("PM1B_CNT write error!");
			}
			return;
		}
	}

	io_write(v, addr, width);
}

void register_gas_io_handler(struct vm *vm, struct acpi_generic_address *gas)
{
	uint8_t io_len[5] = {0, 1, 2, 4, 8};
	struct vm_io_range gas_io;

	if ((gas->address == 0)
			|| (gas->space_id != SPACE_SYSTEM_IO)
			|| (gas->access_size == 0)
			|| (gas->access_size > 4))
		return;

	gas_io.flags = IO_ATTR_RW,
	gas_io.base = gas->address,
	gas_io.len = io_len[gas->access_size];

	register_io_emulation_handler(vm, &gas_io,
			&pm1ab_io_read, &pm1ab_io_write);

	pr_dbg("Enable PM1A trap for VM %d, port 0x%x, size %d\n",
			vm->attr.id, gas_io.base, gas_io.len);
}

void register_pm1ab_handler(struct vm *vm)
{
	register_gas_io_handler(vm, &vm->pm.sx_state_data->pm1a_evt);
	register_gas_io_handler(vm, &vm->pm.sx_state_data->pm1b_evt);
	register_gas_io_handler(vm, &vm->pm.sx_state_data->pm1a_cnt);
	register_gas_io_handler(vm, &vm->pm.sx_state_data->pm1b_cnt);
}
