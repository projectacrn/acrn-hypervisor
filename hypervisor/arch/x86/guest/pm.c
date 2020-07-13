/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <host_pm.h>
#include <vm.h>
#include <io.h>
#include <logmsg.h>
#include <platform_acpi_info.h>
#include <guest_pm.h>
#include <per_cpu.h>

int32_t validate_pstate(const struct acrn_vm *vm, uint64_t perf_ctl)
{
	/* Note:
	 * 1. We don't validate Px request from SOS_VM for now;
	 * 2. Px request will be rejected if no VM Px data is set, even guest is running intel_pstate driver;
	 * 3. The Pstate frequency varies from LFM to HFM and then TFM, but not all frequencies between
	 *     LFM to TFM are mapped in ACPI table. For acpi-cpufreq driver, the target Px value in MSR
	 *     PERF_CTL should be matched with control value of px_data which come from ACPI table,
	 *     but for intel_pstate driver the target Px value could be any value that between LFM to HFM.
	 *     HV has no idea what driver guest is running, so we just check the LFM/TFM range here.
	 *     Only checking Px by indexing control value in px_data might lost the guest Px request.
	 */
	int32_t ret = -1;

	if (is_sos_vm(vm)) {
		ret = 0;
	} else {
		uint8_t px_cnt = vm->pm.px_cnt;
		const struct cpu_px_data *px_data = vm->pm.px_data;

		if ((px_cnt != 0U) && (px_data != NULL)) {
			uint64_t px_target_val, max_px_ctl_val, min_px_ctl_val;

			/* get max px control value, should be for p(0), i.e. TFM. */
			max_px_ctl_val = ((px_data[0].control & 0xff00UL) >> 8U);

			/* get min px control value, should be for p(px_cnt-1), i.e. LFM. */
			min_px_ctl_val = ((px_data[px_cnt - 1U].control & 0xff00UL) >> 8U);

			px_target_val = ((perf_ctl & 0xff00UL) >> 8U);
			if ((px_target_val <= max_px_ctl_val) && (px_target_val >= min_px_ctl_val)) {
				ret = 0;
			}
		}
	}
	return ret;
}

static void vm_setup_cpu_px(struct acrn_vm *vm)
{
	uint32_t px_data_size;
	struct cpu_state_info *pm_state_info = get_cpu_pm_state_info();

	vm->pm.px_cnt = 0U;
	(void)memset(vm->pm.px_data, 0U, MAX_PSTATE * sizeof(struct cpu_px_data));

	if ((pm_state_info->px_cnt != 0U) && (pm_state_info->px_data != NULL)) {
		ASSERT((pm_state_info->px_cnt <= MAX_PSTATE), "failed to setup cpu px");

		vm->pm.px_cnt = pm_state_info->px_cnt;
		px_data_size = ((uint32_t)vm->pm.px_cnt) * sizeof(struct cpu_px_data);
		(void)memcpy_s(vm->pm.px_data, px_data_size, pm_state_info->px_data, px_data_size);
	}
}

static void vm_setup_cpu_cx(struct acrn_vm *vm)
{
	uint32_t cx_data_size;
	struct cpu_state_info *pm_state_info = get_cpu_pm_state_info();

	vm->pm.cx_cnt = 0U;
	(void)memset(vm->pm.cx_data, 0U, MAX_CSTATE * sizeof(struct cpu_cx_data));

	if ((pm_state_info->cx_cnt != 0U) && (pm_state_info->cx_data != NULL)) {
		ASSERT((pm_state_info->cx_cnt <= MAX_CX_ENTRY), "failed to setup cpu cx");

		vm->pm.cx_cnt = pm_state_info->cx_cnt;
		cx_data_size = ((uint32_t)vm->pm.cx_cnt) * sizeof(struct cpu_cx_data);

		/* please note pm.cx_data[0] is a empty space holder,
		 * pm.cx_data[1...MAX_CX_ENTRY] would be used to store cx entry datas.
		 */
		(void)memcpy_s(vm->pm.cx_data + 1, cx_data_size, pm_state_info->cx_data, cx_data_size);
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

static void vm_setup_cpu_state(struct acrn_vm *vm)
{
	vm_setup_cpu_px(vm);
	vm_setup_cpu_cx(vm);
	init_cx_port(vm);
}

/* This function is for power management Sx state implementation,
 * VM need to load the Sx state data to implement S3/S5.
 */
static int32_t vm_load_pm_s_state(struct acrn_vm *vm)
{
	int32_t ret;
	struct pm_s_state_data *sx_data = get_host_sstate_data();

	if ((sx_data->pm1a_evt.address == 0UL) || (sx_data->pm1a_cnt.address == 0UL)
			|| (sx_data->wake_vector_32 == NULL)) {
		pr_err("System S3/S5 is NOT supported.");
		ret = -1;
	} else {
		pr_info("System S3/S5 is supported.");
		vm->pm.sx_state_data = sx_data;
		ret = 0;
	}
	return ret;
}

static inline bool is_s3_enabled(uint32_t pm1_cnt)
{
	return ((pm1_cnt & (1U << BIT_SLP_EN)) != 0U);
}

static inline uint8_t get_slp_typx(uint32_t pm1_cnt)
{
	return (uint8_t)((pm1_cnt & 0x1fffU) >> BIT_SLP_TYPx);
}

static bool pm1ab_io_read(struct acrn_vcpu *vcpu, uint16_t addr, size_t width)
{
	struct pio_request *pio_req = &vcpu->req.reqs.pio;

	pio_req->value = pio_read(addr, width);

	return true;
}

static inline void enter_s5(struct acrn_vm *vm, uint32_t pm1a_cnt_val, uint32_t pm1b_cnt_val)
{
	get_vm_lock(vm);
	/*
	 * It's possible that ACRN come here from SOS and pre-launched VM. Currently, we
	 * assume SOS has full ACPI power management stack. That means the value from SOS
	 * should be saved and used to shutdown the system.
	 */
	if (is_sos_vm(vm)) {
		save_s5_reg_val(pm1a_cnt_val, pm1b_cnt_val);
	}
	pause_vm(vm);
	(void)shutdown_vm(vm);
	put_vm_lock(vm);
}

static inline void enter_s3(struct acrn_vm *vm, uint32_t pm1a_cnt_val, uint32_t pm1b_cnt_val)
{
	uint32_t guest_wakeup_vec32;

	get_vm_lock(vm);
	/* Save the wakeup vec set by guest OS. Will return to guest
	 * with this wakeup vec as entry.
	 */
	stac();
	guest_wakeup_vec32 = *(vm->pm.sx_state_data->wake_vector_32);
	clac();

	pause_vm(vm);	/* pause sos_vm before suspend system */
	host_enter_s3(vm->pm.sx_state_data, pm1a_cnt_val, pm1b_cnt_val);
	resume_vm_from_s3(vm, guest_wakeup_vec32);	/* jump back to vm */
	put_vm_lock(vm);
}

/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool pm1ab_io_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t width, uint32_t v)
{
	static uint32_t pm1a_cnt_ready = 0U;
	uint32_t pm1a_cnt_val;
	bool to_write = true;
	struct acrn_vm *vm = vcpu->vm;

	if (width == 2U) {
		uint8_t val = get_slp_typx(v);

		if ((addr == vm->pm.sx_state_data->pm1a_cnt.address) && is_s3_enabled(v)) {

			if (vm->pm.sx_state_data->pm1b_cnt.address != 0UL) {
				pm1a_cnt_ready = v;
			} else {
				if (vm->pm.sx_state_data->s3_pkg.val_pm1a == val) {
					enter_s3(vm, v, 0U);
				} else if (vm->pm.sx_state_data->s5_pkg.val_pm1a == val) {
					enter_s5(vm, v, 0U);
				}
			}

			to_write = false;
		} else if ((addr == vm->pm.sx_state_data->pm1b_cnt.address) && is_s3_enabled(v)) {

			if (pm1a_cnt_ready != 0U) {
				pm1a_cnt_val = pm1a_cnt_ready;
				pm1a_cnt_ready = 0U;

				if (vm->pm.sx_state_data->s3_pkg.val_pm1b == val) {
					enter_s3(vm, pm1a_cnt_val, v);
				} else if (vm->pm.sx_state_data->s5_pkg.val_pm1b == val) {
					enter_s5(vm, pm1a_cnt_val, v);
				}
			} else {
				/* the case broke ACPI spec */
				pr_err("PM1B_CNT write error!");
			}

			to_write = false;
		} else {
			/* No other state currently, do nothing */
		}
	}

	if (to_write) {
		pio_write(v, addr, width);
	}

	return true;
}

static void register_gas_io_handler(struct acrn_vm *vm, uint32_t pio_idx, const struct acpi_generic_address *gas)
{
	uint8_t io_len[5] = {0U, 1U, 2U, 4U, 8U};
	struct vm_io_range gas_io;

	if ((gas->address != 0UL) && (gas->space_id == SPACE_SYSTEM_IO)
			&& (gas->access_size != 0U) && (gas->access_size <= 4U)) {
		gas_io.base = (uint16_t)gas->address;
		gas_io.len = io_len[gas->access_size];

		register_pio_emulation_handler(vm, pio_idx, &gas_io, &pm1ab_io_read, &pm1ab_io_write);

		pr_dbg("Enable PM1A trap for VM %d, port 0x%x, size %d\n", vm->vm_id, gas_io.base, gas_io.len);
	}
}

static void register_pm1ab_handler(struct acrn_vm *vm)
{
	struct pm_s_state_data *sx_data = vm->pm.sx_state_data;

	register_gas_io_handler(vm, PM1A_EVT_PIO_IDX, &(sx_data->pm1a_evt));
	register_gas_io_handler(vm, PM1B_EVT_PIO_IDX, &(sx_data->pm1b_evt));
	register_gas_io_handler(vm, PM1A_CNT_PIO_IDX, &(sx_data->pm1a_cnt));
	register_gas_io_handler(vm, PM1B_CNT_PIO_IDX, &(sx_data->pm1b_cnt));
}

static bool rt_vm_pm1a_io_read(__unused struct acrn_vcpu *vcpu,
						 __unused uint16_t addr, __unused size_t width)
{
	return false;
}

/*
 * retval true means that we complete the emulation in HV and no need to re-inject the request to DM.
 * retval false means that we should re-inject the request to DM.
 */
/**
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
static bool rt_vm_pm1a_io_write(struct acrn_vcpu *vcpu, uint16_t addr, size_t width, uint32_t v)
{
	if (width != 2U) {
		pr_dbg("Invalid address (0x%x) or width (0x%x)", addr, width);
	} else {
		if ((((v & VIRTUAL_PM1A_SLP_EN) != 0U) && (((v & VIRTUAL_PM1A_SLP_TYP) >> 10U) == 5U)) != 0U) {
			vcpu->vm->state = VM_READY_TO_POWEROFF;
		}
	}

	return false;
}

static void register_rt_vm_pm1a_ctl_handler(struct acrn_vm *vm)
{
	struct vm_io_range io_range;

	io_range.base = VIRTUAL_PM1A_CNT_ADDR;
	io_range.len = 1U;

	register_pio_emulation_handler(vm, VIRTUAL_PM1A_CNT_PIO_IDX, &io_range,
					&rt_vm_pm1a_io_read, &rt_vm_pm1a_io_write);
}

void init_guest_pm(struct acrn_vm *vm)
{
	vm_setup_cpu_state(vm);

	if (is_sos_vm(vm)) {
		/* Load pm S state data */
		if (vm_load_pm_s_state(vm) == 0) {
			register_pm1ab_handler(vm);
		}
	}

	/* Intercept the virtual pm port for RTVM */
	if (is_rt_vm(vm)) {
		register_rt_vm_pm1a_ctl_handler(vm);
	}
}
