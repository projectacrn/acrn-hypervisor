/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <vcpu.h>
#include <vm.h>
#include <vuart.h>
#include <guest_memory.h>

#include <asm/sbi.h>
#include <asm/guest/vsbi.h>

#include <debug/console.h>

static int32_t vcpu_sbi_debug_console_handler(struct acrn_vcpu *vcpu, __unused uint64_t ext_id,
			       uint64_t func_id, uint64_t *args, struct vsbi_ret *out)
{
	int32_t ret = SBI_SUCCESS;
	uint64_t gpa_hi, gpa_lo;
	char *hva, ch;
	size_t nbytes, i;

	switch (func_id) {
		case SBI_EXT_DBCN_CONSOLE_WRITE:
		case SBI_EXT_DBCN_CONSOLE_READ:
			nbytes = args[0];
			gpa_lo = args[1];
			gpa_hi = args[2];
			/* TODO: Check that address is valid */
			hva = (char *)gpa2hva(vcpu->vm, (gpa_hi << 32) | gpa_lo);

			if (hva != NULL) {
				pre_user_access();
				if (func_id == SBI_EXT_DBCN_CONSOLE_WRITE) {
					for (i = 0; i < nbytes; i++) {
						/* TODO: We should redirect the output to ACRN shell's vm_console buffer.
						 * For now just output directly to physical uart.
						 */
						console_putc(&hva[i]);
					}
				} else {
					for (i = 0; i < nbytes; i++) {
						ch = console_getc();
						if (ch == -1) {
							break;
						}
						hva[i] = ch;
					}
				}
				out->value = i;
				post_user_access();
			} else {
				ret = SBI_ERR_INVALID_PARAM;
			}
			break;
		case SBI_EXT_DBCN_CONSOLE_WRITE_BYTE:
			ch = (char)args[0];
			console_putc(&ch);
			out->value = 0;
			break;
		default:
			ret = SBI_ERR_NOT_SUPPORTED;
			break;
	}

	return ret;
}

const struct acrn_vsbi_extension vsbi_ext_dbcn = {
	.name = "dbcn",
	.eid_start = SBI_EID_DBCN,
	.eid_end = SBI_EID_DBCN,
	.handler = vcpu_sbi_debug_console_handler,
};
