/*-
 * Copyright (c) 2014 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
 * Copyright (c) 2015 Nahanni Systems Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdint.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "acpi.h"
#include "inout.h"
#include "irq.h"
#include "lpc.h"
#include "atkbdc.h"
#include "ps2kbd.h"
#include "ps2mouse.h"
#include "vmmapi.h"
#include "mevent.h"
#include "log.h"

static void
atkbdc_assert_kbd_intr(struct atkbdc_base *base)
{
	if ((base->ram[0] & KBD_ENABLE_KBD_INT) != 0) {
		base->kbd.irq_active = true;
		vm_set_gsi_irq(base->ctx, base->kbd.irq, GSI_RAISING_PULSE);
	}
}

static void
atkbdc_assert_aux_intr(struct atkbdc_base *base)
{
	if ((base->ram[0] & KBD_ENABLE_AUX_INT) != 0) {
		base->aux.irq_active = true;
		vm_set_gsi_irq(base->ctx, base->aux.irq, GSI_RAISING_PULSE);
	}
}

static int
atkbdc_kbd_queue_data(struct atkbdc_base *base, uint8_t val)
{
	if (base->kbd.bcnt < FIFOSZ) {
		base->kbd.buffer[base->kbd.bwr] = val;
		base->kbd.bwr = (base->kbd.bwr + 1) % FIFOSZ;
		base->kbd.bcnt++;
		base->status |= KBDS_KBD_BUFFER_FULL;
		base->outport |= KBDO_KBD_OUTFULL;
	} else {
		printf("atkbd data buffer full\n");
	}

	return (base->kbd.bcnt < FIFOSZ);
}

static void
atkbdc_kbd_read(struct atkbdc_base *base)
{
	const uint8_t translation[256] = {
		0xff, 0x43, 0x41, 0x3f, 0x3d, 0x3b, 0x3c, 0x58,
		0x64, 0x44, 0x42, 0x40, 0x3e, 0x0f, 0x29, 0x59,
		0x65, 0x38, 0x2a, 0x70, 0x1d, 0x10, 0x02, 0x5a,
		0x66, 0x71, 0x2c, 0x1f, 0x1e, 0x11, 0x03, 0x5b,
		0x67, 0x2e, 0x2d, 0x20, 0x12, 0x05, 0x04, 0x5c,
		0x68, 0x39, 0x2f, 0x21, 0x14, 0x13, 0x06, 0x5d,
		0x69, 0x31, 0x30, 0x23, 0x22, 0x15, 0x07, 0x5e,
		0x6a, 0x72, 0x32, 0x24, 0x16, 0x08, 0x09, 0x5f,
		0x6b, 0x33, 0x25, 0x17, 0x18, 0x0b, 0x0a, 0x60,
		0x6c, 0x34, 0x35, 0x26, 0x27, 0x19, 0x0c, 0x61,
		0x6d, 0x73, 0x28, 0x74, 0x1a, 0x0d, 0x62, 0x6e,
		0x3a, 0x36, 0x1c, 0x1b, 0x75, 0x2b, 0x63, 0x76,
		0x55, 0x56, 0x77, 0x78, 0x79, 0x7a, 0x0e, 0x7b,
		0x7c, 0x4f, 0x7d, 0x4b, 0x47, 0x7e, 0x7f, 0x6f,
		0x52, 0x53, 0x50, 0x4c, 0x4d, 0x48, 0x01, 0x45,
		0x57, 0x4e, 0x51, 0x4a, 0x37, 0x49, 0x46, 0x54,
		0x80, 0x81, 0x82, 0x41, 0x54, 0x85, 0x86, 0x87,
		0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
		0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
		0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
		0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
		0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
		0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
		0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
		0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
		0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
		0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
		0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
		0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
		0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
		0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
		0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
	};
	uint8_t val;
	uint8_t release = 0;

	if (base->ram[0] & KBD_TRANSLATION) {
		while (ps2kbd_read(base->ps2kbd, &val) != -1) {
			if (val == 0xf0) {
				release = 0x80;
				continue;
			} else {
				val = translation[val] | release;
			}
			atkbdc_kbd_queue_data(base, val);
			break;
		}
	} else {
		while (base->kbd.bcnt < FIFOSZ) {
			if (ps2kbd_read(base->ps2kbd, &val) != -1)
				atkbdc_kbd_queue_data(base, val);
			else
				break;
		}
	}

	if (((base->ram[0] & KBD_DISABLE_AUX_PORT) ||
	    ps2mouse_fifocnt(base->ps2mouse) == 0) && base->kbd.bcnt > 0)
		atkbdc_assert_kbd_intr(base);
}

static void
atkbdc_aux_poll(struct atkbdc_base *base)
{
	if (ps2mouse_fifocnt(base->ps2mouse) > 0) {
		base->status |= KBDS_AUX_BUFFER_FULL | KBDS_KBD_BUFFER_FULL;
		base->outport |= KBDO_AUX_OUTFULL;
		atkbdc_assert_aux_intr(base);
	}
}

static void
atkbdc_kbd_poll(struct atkbdc_base *base)
{
	atkbdc_kbd_read(base);
}

static void
atkbdc_poll(struct atkbdc_base *base)
{
	atkbdc_aux_poll(base);
	atkbdc_kbd_poll(base);
}

static void
atkbdc_dequeue_data(struct atkbdc_base *base, uint8_t *buf)
{
	if (ps2mouse_read(base->ps2mouse, buf) == 0) {
		if (ps2mouse_fifocnt(base->ps2mouse) == 0) {
			if (base->kbd.bcnt == 0)
				base->status &= ~(KBDS_AUX_BUFFER_FULL |
						KBDS_KBD_BUFFER_FULL);
			else
				base->status &= ~(KBDS_AUX_BUFFER_FULL);
			base->outport &= ~KBDO_AUX_OUTFULL;
		}

		atkbdc_poll(base);
		return;
	}

	if (base->kbd.bcnt > 0) {
		*buf = base->kbd.buffer[base->kbd.brd];
		base->kbd.brd = (base->kbd.brd + 1) % FIFOSZ;
		base->kbd.bcnt--;
		if (base->kbd.bcnt == 0) {
			base->status &= ~KBDS_KBD_BUFFER_FULL;
			base->outport &= ~KBDO_KBD_OUTFULL;
		}

		atkbdc_poll(base);
	}

	if (ps2mouse_fifocnt(base->ps2mouse) == 0 && base->kbd.bcnt == 0)
		base->status &= ~(KBDS_AUX_BUFFER_FULL | KBDS_KBD_BUFFER_FULL);
}

static int
atkbdc_data_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		    uint32_t *eax, void *arg)
{
	struct atkbdc_base *base;
	uint8_t buf;
	int retval;

	if (bytes != 1)
		return -1;
	base = arg;
	retval = 0;

	pthread_mutex_lock(&base->mtx);
	if (in) {
		base->curcmd = 0;
		if (base->ctrlbyte != 0) {
			*eax = base->ctrlbyte & 0xff;
			base->ctrlbyte = 0;
		} else {
			/* read device buffer; includes kbd cmd responses */
			atkbdc_dequeue_data(base, &buf);
			*eax = buf;
		}

		base->status &= ~KBDS_CTRL_FLAG;
		pthread_mutex_unlock(&base->mtx);
		return retval;
	}

	if (base->status & KBDS_CTRL_FLAG) {
		/*
		 * Command byte for the controller.
		 */
		switch (base->curcmd) {
		case KBDC_SET_COMMAND_BYTE:
			base->ram[0] = *eax;
			if (base->ram[0] & KBD_SYS_FLAG_BIT)
				base->status |= KBDS_SYS_FLAG;
			else
				base->status &= ~KBDS_SYS_FLAG;
			break;
		case KBDC_WRITE_OUTPORT:
			base->outport = *eax;
			break;
		case KBDC_WRITE_TO_AUX:
			ps2mouse_write(base->ps2mouse, *eax, 0);
			atkbdc_poll(base);
			break;
		case KBDC_WRITE_KBD_OUTBUF:
			atkbdc_kbd_queue_data(base, *eax);
			break;
		case KBDC_WRITE_AUX_OUTBUF:
			ps2mouse_write(base->ps2mouse, *eax, 1);
			base->status |= (KBDS_AUX_BUFFER_FULL |
					KBDS_KBD_BUFFER_FULL);
			atkbdc_aux_poll(base);
			break;
		default:
			/* write to particular RAM byte */
			if (base->curcmd >= 0x61 && base->curcmd <= 0x7f) {
				int byten;

				byten = (base->curcmd - 0x60) & 0x1f;
				base->ram[byten] = *eax & 0xff;
			}
			break;
		}

		base->curcmd = 0;
		base->status &= ~KBDS_CTRL_FLAG;

		pthread_mutex_unlock(&base->mtx);
		return retval;
	}

	/*
	 * Data byte for the device.
	 */
	ps2kbd_write(base->ps2kbd, *eax);
	atkbdc_poll(base);

	pthread_mutex_unlock(&base->mtx);

	return retval;
}

static int
atkbdc_sts_ctl_handler(struct vmctx *ctx, int vcpu, int in, int port,
		       int bytes, uint32_t *eax, void *arg)
{
	struct atkbdc_base *base;
	int retval;

	if (bytes != 1)
		return -1;

	base = arg;
	retval = 0;

	pthread_mutex_lock(&base->mtx);

	if (in) {
		/* read status register */
		*eax = base->status;
		pthread_mutex_unlock(&base->mtx);
		return retval;
	}

	base->curcmd = 0;
	base->status |= KBDS_CTRL_FLAG;
	base->ctrlbyte = 0;

	switch (*eax) {
	case KBDC_GET_COMMAND_BYTE:
		base->ctrlbyte = CTRL_CMD_FLAG | base->ram[0];
		break;
	case KBDC_TEST_CTRL:
		base->ctrlbyte = CTRL_CMD_FLAG | 0x55;
		break;
	case KBDC_TEST_AUX_PORT:
	case KBDC_TEST_KBD_PORT:
		base->ctrlbyte = CTRL_CMD_FLAG | 0;
		break;
	case KBDC_READ_INPORT:
		base->ctrlbyte = CTRL_CMD_FLAG | 0;
		break;
	case KBDC_READ_OUTPORT:
		base->ctrlbyte = CTRL_CMD_FLAG | base->outport;
		break;
	case KBDC_SET_COMMAND_BYTE:
	case KBDC_WRITE_OUTPORT:
	case KBDC_WRITE_KBD_OUTBUF:
	case KBDC_WRITE_AUX_OUTBUF:
		base->curcmd = *eax;
		break;
	case KBDC_DISABLE_KBD_PORT:
		base->ram[0] |= KBD_DISABLE_KBD_PORT;
		break;
	case KBDC_ENABLE_KBD_PORT:
		base->ram[0] &= ~KBD_DISABLE_KBD_PORT;
		if (base->kbd.bcnt > 0)
			base->status |= KBDS_KBD_BUFFER_FULL;
		atkbdc_poll(base);
		break;
	case KBDC_WRITE_TO_AUX:
		base->curcmd = *eax;
		break;
	case KBDC_DISABLE_AUX_PORT:
		base->ram[0] |= KBD_DISABLE_AUX_PORT;
		ps2mouse_toggle(base->ps2mouse, 0);
		base->status &= ~(KBDS_AUX_BUFFER_FULL | KBDS_KBD_BUFFER_FULL);
		base->outport &= ~KBDS_AUX_BUFFER_FULL;
		break;
	case KBDC_ENABLE_AUX_PORT:
		base->ram[0] &= ~KBD_DISABLE_AUX_PORT;
		ps2mouse_toggle(base->ps2mouse, 1);
		if (ps2mouse_fifocnt(base->ps2mouse) > 0)
			base->status |= KBDS_AUX_BUFFER_FULL |
					KBDS_KBD_BUFFER_FULL;
		break;
	case KBDC_RESET:		/* Pulse "cold reset" line */
		vm_suspend(ctx, VM_SUSPEND_FULL_RESET);
		mevent_notify();
		break;
	default:
		if (*eax >= 0x21 && *eax <= 0x3f) {
			/* read "byte N" from RAM */
			int	byten;

			byten = (*eax - 0x20) & 0x1f;
			base->ctrlbyte = CTRL_CMD_FLAG | base->ram[byten];
		}
		break;
	}

	pthread_mutex_unlock(&base->mtx);

	if (base->ctrlbyte != 0) {
		base->status |= KBDS_KBD_BUFFER_FULL;
		base->status &= ~KBDS_AUX_BUFFER_FULL;
		atkbdc_assert_kbd_intr(base);
	} else if (ps2mouse_fifocnt(base->ps2mouse) > 0 &&
			(base->ram[0] & KBD_DISABLE_AUX_PORT) == 0) {
		base->status |= KBDS_AUX_BUFFER_FULL | KBDS_KBD_BUFFER_FULL;
		atkbdc_assert_aux_intr(base);
	} else if (base->kbd.bcnt > 0 && (base->ram[0] &
		KBD_DISABLE_KBD_PORT) == 0) {
		base->status |= KBDS_KBD_BUFFER_FULL;
		atkbdc_assert_kbd_intr(base);
	}

	return retval;
}

void
atkbdc_event(struct atkbdc_base *base, int iskbd)
{
	pthread_mutex_lock(&base->mtx);

	if (iskbd)
		atkbdc_kbd_poll(base);
	else
		atkbdc_aux_poll(base);
	pthread_mutex_unlock(&base->mtx);
}

void
atkbdc_init(struct vmctx *ctx)
{
	struct inout_port iop;
	struct atkbdc_base *base;
	int error;

	base = calloc(1, sizeof(struct atkbdc_base));
	if (!base) {
		pr_err("%s: alloc memory fail!\n", __func__);
		return;
	}

	base->ctx = ctx;
	ctx->atkbdc_base = base;

	pthread_mutex_init(&base->mtx, NULL);

	bzero(&iop, sizeof(struct inout_port));
	iop.name = "atkdbc";
	iop.port = KBD_STS_CTL_PORT;
	iop.size = 1;
	iop.flags = IOPORT_F_INOUT;
	iop.handler = atkbdc_sts_ctl_handler;
	iop.arg = base;

	error = register_inout(&iop);
	if (error < 0)
		goto fail;

	bzero(&iop, sizeof(struct inout_port));
	iop.name = "atkdbc";
	iop.port = KBD_DATA_PORT;
	iop.size = 1;
	iop.flags = IOPORT_F_INOUT;
	iop.handler = atkbdc_data_handler;
	iop.arg = base;

	error = register_inout(&iop);
	if (error < 0)
		goto fail;

	pci_irq_reserve(KBD_DEV_IRQ);
	base->kbd.irq = KBD_DEV_IRQ;

	pci_irq_reserve(AUX_DEV_IRQ);
	base->aux.irq = AUX_DEV_IRQ;

	base->ps2kbd = ps2kbd_init(base);
	base->ps2mouse = ps2mouse_init(base);

	return;
fail:
	pr_err("%s: fail to init!\n", __func__);
	free(base);
}

void
atkbdc_deinit(struct vmctx *ctx)
{
	struct inout_port iop;
	struct atkbdc_base *base = ctx->atkbdc_base;

	ps2kbd_deinit(base);
	base->ps2kbd = NULL;
	ps2mouse_deinit(base);
	base->ps2mouse = NULL;

	bzero(&iop, sizeof(struct inout_port));
	iop.name = "atkdbc";
	iop.port = KBD_DATA_PORT;
	iop.size = 1;
	unregister_inout(&iop);

	bzero(&iop, sizeof(struct inout_port));
	iop.name = "atkdbc";
	iop.port = KBD_STS_CTL_PORT;
	iop.size = 1;
	unregister_inout(&iop);

	free(base);
	ctx->atkbdc_base = NULL;
}

static void
atkbdc_dsdt(void)
{
	dsdt_line("");
	dsdt_line("Device (KBD)");
	dsdt_line("{");
	dsdt_line("  Name (_HID, EisaId (\"PNP0303\"))");
	dsdt_line("  Name (_CRS, ResourceTemplate ()");
	dsdt_line("  {");
	dsdt_indent(2);
	dsdt_fixed_ioport(KBD_DATA_PORT, 1);
	dsdt_fixed_ioport(KBD_STS_CTL_PORT, 1);
	dsdt_fixed_irq(1);
	dsdt_unindent(2);
	dsdt_line("  })");
	dsdt_line("}");

	dsdt_line("");
	dsdt_line("Device (MOU)");
	dsdt_line("{");
	dsdt_line("  Name (_HID, EisaId (\"PNP0F13\"))");
	dsdt_line("  Name (_CRS, ResourceTemplate ()");
	dsdt_line("  {");
	dsdt_indent(2);
	dsdt_fixed_ioport(KBD_DATA_PORT, 1);
	dsdt_fixed_ioport(KBD_STS_CTL_PORT, 1);
	dsdt_fixed_irq(12);
	dsdt_unindent(2);
	dsdt_line("  })");
	dsdt_line("}");
}
LPC_DSDT(atkbdc_dsdt);
