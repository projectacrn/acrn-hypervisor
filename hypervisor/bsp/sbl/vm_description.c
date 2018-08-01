/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#if defined(CONFIG_VM0_DESC) && !defined(CONFIG_PARTITION_HV)

/* Number of CPUs in VM0 */
#define VM0_NUM_CPUS    1

/* Logical CPU IDs assigned to VM0 */
uint16_t VM0_CPUS[VM0_NUM_CPUS] = {0U};

struct vm_description vm0_desc = {
	.vm_hw_num_cores = VM0_NUM_CPUS,
	.vm_pcpu_ids = &VM0_CPUS[0],
};

#else

struct vm_description vm0_desc;

#endif // CONFIG_VM0_DESC

#ifdef CONFIG_PARTITION_HV

struct vm_description vm0_desc;

#define NUM_USER_VMS    3

/**********************/
/* VIRTUAL MACHINE 0 */
/*********************/

/* Number of CPUs in this VM*/
#define VM1_NUM_CPUS    2

/* Logical CPU IDs assigned to this VM */
int VM1_CPUS[VM1_NUM_CPUS] = {0, 2};

/*********************/
/* VIRTUAL MACHINE 1 */
/*********************/

/* Number of CPUs in this VM*/
#define VM2_NUM_CPUS    2

/* Logical CPU IDs assigned with this VM */
int VM2_CPUS[VM2_NUM_CPUS] = {3, 1};

/*******************************/
/* User Defined VM definitions */
/*******************************/
const struct vm_description_array vm_desc_mrb = {
		/* Number of user virtual machines */
		.num_vm_desc = NUM_USER_VMS,

		/* Virtual Machine descriptions */
		.vm_desc_array = {
			{
			},

			{
				/* Internal variable, MUSTBE init to -1 */
				.vm_id = 1,
				.vm_hw_num_cores = VM1_NUM_CPUS,
				.vm_pcpu_ids = &VM1_CPUS[0],
				.start_hpa = 0x100000000,
				.mem_size = 0x80000000,
				.vm_vuart = HV_TRUE, /* vuart is enabled by default */
				.bootargs = "root=/dev/sda rw rootwait noxsave maxcpus=2 nohpet console=hvc0 \
						console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M \
						consoleblank=0 tsc=reliable"
			},

			{
				/* Internal variable, MUSTBE init to -1 */
				.vm_id = 2,
				.vm_hw_num_cores = VM2_NUM_CPUS,
				.vm_pcpu_ids = &VM2_CPUS[0],
				.start_hpa = 0x180000000,
				.mem_size = 0x80000000,
				.vm_vuart = HV_TRUE, /* vuart is enabled by default */
				.bootargs = "root=/dev/sda rw rootwait noxsave maxcpus=2 nohpet console=hvc0 \
						console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M \
						consoleblank=0 tsc=reliable"
			},
		}
};

const struct vm_description_array *get_vm_desc_base(void)
{
	return &vm_desc_mrb;
}
#endif
