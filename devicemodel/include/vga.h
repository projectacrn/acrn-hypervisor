/*
 * Copyright (C) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2015 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
 * All rights reserved.
 */

#ifndef _VGA_H_
#define	_VGA_H_

#include "gc.h"
#include "vdisplay.h"

#define	VGA_IOPORT_START		0x3c0
#define	VGA_IOPORT_END			0x3df

/* General registers */
#define	GEN_INPUT_STS0_PORT		0x3c2
#define	GEN_FEATURE_CTRL_PORT		0x3ca
#define	GEN_MISC_OUTPUT_PORT		0x3cc
#define	GEN_INPUT_STS1_MONO_PORT	0x3ba
#define	GEN_INPUT_STS1_COLOR_PORT	0x3da
#define	GEN_IS1_VR			0x08	/* Vertical retrace */
#define	GEN_IS1_DE			0x01	/* Display enable not */

/* Attribute controller registers. */
#define	ATC_IDX_PORT			0x3c0
#define	ATC_DATA_PORT			0x3c1

#define	ATC_IDX_MASK			0x1f
#define	ATC_PALETTE0			0
#define	ATC_PALETTE15			15
#define	ATC_MODE_CONTROL		16
#define	ATC_MC_IPS			0x80	/* Internal palette size */
#define	ATC_MC_GA			0x01	/* Graphics/alphanumeric */
#define	ATC_OVERSCAN_COLOR		17
#define	ATC_COLOR_PLANE_ENABLE		18
#define	ATC_HORIZ_PIXEL_PANNING		19
#define	ATC_COLOR_SELECT		20
#define	ATC_CS_C67			0x0c	/* Color select bits 6+7 */
#define	ATC_CS_C45			0x03	/* Color select bits 4+5 */

/* Sequencer registers. */
#define	SEQ_IDX_PORT			0x3c4
#define	SEQ_DATA_PORT			0x3c5

#define	SEQ_RESET			0
#define	SEQ_RESET_ASYNC			0x1
#define	SEQ_RESET_SYNC			0x2
#define	SEQ_CLOCKING_MODE		1
#define	SEQ_CM_SO			0x20	/* Screen off */
#define	SEQ_CM_89			0x01	/* 8/9 dot clock */
#define	SEQ_MAP_MASK			2
#define	SEQ_CHAR_MAP_SELECT		3
#define	SEQ_CMS_SAH			0x20	/* Char map A bit 2 */
#define	SEQ_CMS_SAH_SHIFT		5
#define	SEQ_CMS_SA			0x0c	/* Char map A bits 0+1 */
#define	SEQ_CMS_SA_SHIFT		2
#define	SEQ_CMS_SBH			0x10	/* Char map B bit 2 */
#define	SEQ_CMS_SBH_SHIFT		4
#define	SEQ_CMS_SB			0x03	/* Char map B bits 0+1 */
#define	SEQ_CMS_SB_SHIFT		0
#define	SEQ_MEMORY_MODE			4
#define	SEQ_MM_C4			0x08	/* Chain 4 */
#define	SEQ_MM_OE			0x04	/* Odd/even */
#define	SEQ_MM_EM			0x02	/* Extended memory */

/* Graphics controller registers. */
#define	GC_IDX_PORT			0x3ce
#define	GC_DATA_PORT			0x3cf

#define	GC_SET_RESET			0
#define	GC_ENABLE_SET_RESET		1
#define	GC_COLOR_COMPARE		2
#define	GC_DATA_ROTATE			3
#define	GC_READ_MAP_SELECT		4
#define	GC_MODE				5
#define	GC_MODE_OE			0x10	/* Odd/even */
#define	GC_MODE_C4			0x04	/* Chain 4 */

#define	GC_MISCELLANEOUS		6
#define	GC_MISC_GM			0x01	/* Graphics/alphanumeric */
#define	GC_MISC_MM			0x0c	/* memory map */
#define	GC_MISC_MM_SHIFT		2
#define	GC_COLOR_DONT_CARE		7
#define	GC_BIT_MASK			8

/* CRT controller registers. */
#define	CRTC_IDX_MONO_PORT		0x3b4
#define	CRTC_DATA_MONO_PORT		0x3b5
#define	CRTC_IDX_COLOR_PORT		0x3d4
#define	CRTC_DATA_COLOR_PORT		0x3d5

#define	CRTC_HORIZ_TOTAL		0
#define	CRTC_HORIZ_DISP_END		1
#define	CRTC_START_HORIZ_BLANK		2
#define	CRTC_END_HORIZ_BLANK		3
#define	CRTC_START_HORIZ_RETRACE	4
#define	CRTC_END_HORIZ_RETRACE		5
#define	CRTC_VERT_TOTAL			6
#define	CRTC_OVERFLOW			7
#define	CRTC_OF_VRS9			0x80	/* VRS bit 9 */
#define	CRTC_OF_VRS9_SHIFT		7
#define	CRTC_OF_VDE9			0x40	/* VDE bit 9 */
#define	CRTC_OF_VDE9_SHIFT		6
#define	CRTC_OF_VRS8			0x04	/* VRS bit 8 */
#define	CRTC_OF_VRS8_SHIFT		2
#define	CRTC_OF_VDE8			0x02	/* VDE bit 8 */
#define	CRTC_OF_VDE8_SHIFT		1
#define	CRTC_PRESET_ROW_SCAN		8
#define	CRTC_MAX_SCAN_LINE		9
#define	CRTC_MSL_MSL			0x1f
#define	CRTC_CURSOR_START		10
#define	CRTC_CS_CO			0x20	/* Cursor off */
#define	CRTC_CS_CS			0x1f	/* Cursor start */
#define	CRTC_CURSOR_END			11
#define	CRTC_CE_CE			0x1f	/* Cursor end */
#define	CRTC_START_ADDR_HIGH		12
#define	CRTC_START_ADDR_LOW		13
#define	CRTC_CURSOR_LOC_HIGH		14
#define	CRTC_CURSOR_LOC_LOW		15
#define	CRTC_VERT_RETRACE_START		16
#define	CRTC_VERT_RETRACE_END		17
#define	CRTC_VRE_MASK			0xf
#define	CRTC_VERT_DISP_END		18
#define	CRTC_OFFSET			19
#define	CRTC_UNDERLINE_LOC		20
#define	CRTC_START_VERT_BLANK		21
#define	CRTC_END_VERT_BLANK		22
#define	CRTC_MODE_CONTROL		23
#define	CRTC_MC_TE			0x80	/* Timing enable */
#define	CRTC_LINE_COMPARE		24

/* DAC registers */
#define	DAC_MASK			0x3c6
#define	DAC_IDX_RD_PORT			0x3c7
#define	DAC_IDX_WR_PORT			0x3c8
#define	DAC_DATA_PORT			0x3c9

#define VBE_DISPI_INDEX_ID		0x0
#define VBE_DISPI_INDEX_XRES		0x1
#define VBE_DISPI_INDEX_YRES		0x2
#define VBE_DISPI_INDEX_BPP		0x3
#define VBE_DISPI_INDEX_ENABLE		0x4
#define VBE_DISPI_INDEX_BANK		0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH	0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT	0x7
#define VBE_DISPI_INDEX_X_OFFSET	0x8
#define VBE_DISPI_INDEX_Y_OFFSET	0x9
#define VBE_DISPI_INDEX_VIDEO_MEM_64K	0xa
#define VBE_DISPI_DISABLED		0x00
#define VBE_DISPI_ENABLED		0x01
#define VBE_DISPI_GETCAPS		0x02
#define VBE_DISPI_8BIT_DAC		0x20
#define VBE_DISPI_LFB_ENABLED		0x40
#define VBE_DISPI_NOCLEARMEM		0x80
#define VBE_DISPI_ID0			0xB0C0
#define VBE_DISPI_ID1			0xB0C1
#define VBE_DISPI_ID2			0xB0C2
#define VBE_DISPI_ID3			0xB0C3
#define VBE_DISPI_ID4			0xB0C4
#define VBE_DISPI_ID5			0xB0C5

struct vga {
	bool enable;
	void *dev;
	struct gfx_ctx *gc;
	struct surface surf;
	pthread_t tid;
	struct {
		uint16_t  id;
		uint16_t  xres;
		uint16_t  yres;
		uint16_t  bpp;
		uint16_t  enable;
		uint16_t  bank;
		uint16_t  virt_width;
		uint16_t  virt_height;
		uint16_t  x_offset;
		uint16_t  y_offset;
		uint16_t  video_memory_64k;
	} __attribute__((packed)) vberegs;
};

void *vga_init(struct gfx_ctx *gc, int io_only);
void vga_render(struct gfx_ctx *gc, void *arg);
int vga_port_in_handler(struct vmctx *ctx, int in, int port, int bytes,
		     uint8_t *val, void *arg);
int vga_port_out_handler(struct vmctx *ctx, int in, int port, int bytes,
		     uint8_t val, void *arg);
void vga_ioport_write(struct vmctx *ctx, int vcpu, struct vga *vga,
		uint64_t offset, int size, uint64_t value);
uint64_t vga_ioport_read(struct vmctx *ctx, int vcpu, struct vga *vga,
		uint64_t offset, int size);
void vga_vbe_write(struct vmctx *ctx, int vcpu, struct vga *vga,
		uint64_t offset, int size, uint64_t value);
uint64_t vga_vbe_read(struct vmctx *ctx, int vcpu, struct vga *vga,
		uint64_t offset, int size);

void vga_deinit(struct vga *vga);
#endif /* _VGA_H_ */
