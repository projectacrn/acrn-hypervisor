/*-
 * Copyright (c) 2015 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
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
 *
 * $FreeBSD$
 */

#ifndef _ATKBDC_H_
#define _ATKBDC_H_

#define	KBD_DATA_PORT		0x60

#define	KBD_STS_CTL_PORT	0x64

#define	KBDC_RESET		0xfe

#define	KBD_DEV_IRQ		1
#define	AUX_DEV_IRQ		12

/* controller commands */
#define	KBDC_SET_COMMAND_BYTE	0x60
#define	KBDC_GET_COMMAND_BYTE	0x20
#define	KBDC_DISABLE_AUX_PORT	0xa7
#define	KBDC_ENABLE_AUX_PORT	0xa8
#define	KBDC_TEST_AUX_PORT	0xa9
#define	KBDC_TEST_CTRL		0xaa
#define	KBDC_TEST_KBD_PORT	0xab
#define	KBDC_DISABLE_KBD_PORT	0xad
#define	KBDC_ENABLE_KBD_PORT	0xae
#define	KBDC_READ_INPORT	0xc0
#define	KBDC_READ_OUTPORT	0xd0
#define	KBDC_WRITE_OUTPORT	0xd1
#define	KBDC_WRITE_KBD_OUTBUF	0xd2
#define	KBDC_WRITE_AUX_OUTBUF	0xd3
#define	KBDC_WRITE_TO_AUX	0xd4

/* controller command byte (set by KBDC_SET_COMMAND_BYTE) */
#define	KBD_TRANSLATION		0x40
#define	KBD_SYS_FLAG_BIT	0x04
#define	KBD_DISABLE_KBD_PORT	0x10
#define	KBD_DISABLE_AUX_PORT	0x20
#define	KBD_ENABLE_AUX_INT	0x02
#define	KBD_ENABLE_KBD_INT	0x01
#define	KBD_KBD_CONTROL_BITS	(KBD_DISABLE_KBD_PORT | KBD_ENABLE_KBD_INT)
#define	KBD_AUX_CONTROL_BITS	(KBD_DISABLE_AUX_PORT | KBD_ENABLE_AUX_INT)

/* controller status bits */
#define	KBDS_KBD_BUFFER_FULL	0x01
#define KBDS_SYS_FLAG		0x04
#define KBDS_CTRL_FLAG		0x08
#define	KBDS_AUX_BUFFER_FULL	0x20

/* controller output port */
#define	KBDO_KBD_OUTFULL	0x10
#define	KBDO_AUX_OUTFULL	0x20

#define	RAMSZ			32
#define	FIFOSZ			15
#define	CTRL_CMD_FLAG		0x8000

struct vmctx;

struct kbd_dev {
	bool	irq_active;
	int	irq;

	uint8_t	buffer[FIFOSZ];
	int	brd, bwr;
	int	bcnt;
};

struct aux_dev {
	bool	irq_active;
	int	irq;
};

struct atkbdc_base {
	struct vmctx *ctx;
	pthread_mutex_t mtx;

	struct ps2kbd_info	*ps2kbd;
	struct ps2mouse_info	*ps2mouse;

	uint8_t	status;		/* status register */
	uint8_t	outport;	/* controller output port */
	uint8_t	ram[RAMSZ];	/* byte0 = controller config */

	uint32_t curcmd;	/* current command for next byte */
	uint32_t  ctrlbyte;

	struct kbd_dev kbd;
	struct aux_dev aux;
};

void atkbdc_init(struct vmctx *ctx);
void atkbdc_deinit(struct vmctx *ctx);
void atkbdc_event(struct atkbdc_base *base, int iskbd);

#endif /* _ATKBDC_H_ */
