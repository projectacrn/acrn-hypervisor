/*
 * Copyright (C) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Virtual Display for VMs
 *
 */
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <egl.h>
#include <pixman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "log.h"
#include "vdisplay.h"
#include "atomic.h"
#include "timer.h"
#include <egl.h>
#include <eglext.h>
#include <gl2.h>
#include <gl2ext.h>

#define VDPY_MAX_WIDTH 1920
#define VDPY_MAX_HEIGHT 1080
#define VDPY_DEFAULT_WIDTH 1280
#define VDPY_DEFAULT_HEIGHT 720
#define VDPY_MIN_WIDTH 640
#define VDPY_MIN_HEIGHT 480
#define transto_10bits(color) (uint16_t)(color * 1024 + 0.5)

static unsigned char default_raw_argb[640 * 480 * 4];

struct state {
	bool is_ui_realized;
	bool is_active;
	bool is_wayland;
	bool is_x11;
	bool is_fullscreen;
	uint64_t updates;
	int n_connect;
};

struct egl_display_ops {
	PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
	PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
	PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
};

static struct display {
	struct display_info info;
	struct state s;
	SDL_Texture *dpy_texture;
	SDL_Window *dpy_win;
	SDL_Renderer *dpy_renderer;
	pixman_image_t *dpy_img;
	pthread_t tid;
	int width, height; // Width/height of dpy_win
	int org_x, org_y;
	int guest_width, guest_height;
	int screen;
	struct surface surf;
	struct cursor cur;
	SDL_Texture *cursor_tex;
	/* Add one UI_timer(33ms) to render the buffers from guest_vm */
	struct acrn_timer ui_timer;
	struct vdpy_display_bh ui_timer_bh;
	/* Record the update_time that is activated from guest_vm */
	struct timespec last_time;
	// protect the request_list
	pthread_mutex_t vdisplay_mutex;
	// receive the signal that request is submitted
	pthread_cond_t  vdisplay_signal;
	TAILQ_HEAD(display_list, vdpy_display_bh) request_list;
	/* add the below two fields for calling eglAPI directly */
	bool egl_dmabuf_supported;
	SDL_GLContext eglContext;
	EGLDisplay eglDisplay;
	struct egl_display_ops gl_ops;
	EGLImage cur_egl_img;
} vdpy = {
	.s.is_ui_realized = false,
	.s.is_active = false,
	.s.is_wayland = false,
	.s.is_x11 = false,
	.s.is_fullscreen = false,
	.s.updates = 0,
	.s.n_connect = 0
};

typedef enum {
	ESTT = 1, // Established Timings I & II
	STDT,    // Standard Timings
	ESTT3,   // Established Timings III
} TIMING_MODE;

static const struct timing_entry {
	uint32_t hpixel;// Horizontal pixels
	uint32_t vpixel;// Vertical pixels
	uint32_t byte;  // byte idx in the Established Timings I & II
	uint32_t byte_t3;// byte idx in the Established Timings III Descriptor
	uint32_t bit;   // bit idx
	uint8_t hz;     // frequency
} timings[] = {
	/* Established Timings I & II (all @ 60Hz) */
	{ .hpixel = 1024, .vpixel =  768, .byte  = 36, .bit = 3, .hz = 60},
	{ .hpixel =  800, .vpixel =  600, .byte  = 35, .bit = 0, .hz = 60 },
	{ .hpixel =  640, .vpixel =  480, .byte  = 35, .bit = 5, .hz = 60 },

	/* Standard Timings */
	{ .hpixel = 1920, .vpixel = 1080, .hz = 60 },
	{ .hpixel = 1280, .vpixel =  720, .hz = 60 },
};

typedef struct frame_param{
	uint32_t hav_pixel;     // Horizontal Addressable Video in pixels
	uint32_t hb_pixel;      // Horizontal Blanking in pixels
	uint32_t hfp_pixel;     // Horizontal Front Porch in pixels
	uint32_t hsp_pixel;     // Horizontal Sync Pulse Width in pixels
	uint32_t lhb_pixel;     // Left Horizontal Border or Right Horizontal
	                        // Border in pixels

	uint32_t vav_line;      // Vertical Addressable Video in lines
	uint32_t vb_line;       // Vertical Blanking in lines
	uint32_t vfp_line;      // Vertical Front Porch in Lines
	uint32_t vsp_line;      // Vertical Sync Pulse Width in Lines
	uint32_t tvb_line;      // Top Vertical Border or Bottom Vertical
	                        // Border in Lines

	uint64_t pixel_clock;   // Hz
	uint32_t width;         // mm
	uint32_t height;        // mm
}frame_param;

typedef struct base_param{
	uint32_t h_pixel;       // pixels
	uint32_t v_pixel;       // lines
	uint32_t h_pixelmax;
	uint32_t v_pixelmax;
	uint32_t rate;          // Hz
	uint32_t width;         // mm
	uint32_t height;        // mm

	const char *id_manuf;   // ID Manufacturer Name, ISA 3-character ID Code
	uint16_t id_product;    // ID Product Code
	uint32_t id_sn;         // ID Serial Number and it is a number only.

	const char *sn;         // Serial number.
	const char *product_name;// Product name.
}base_param;

static void
vdpy_edid_set_baseparam(base_param *b_param, uint32_t width, uint32_t height)
{
	b_param->h_pixel = width;
	b_param->v_pixel = height;
	b_param->h_pixelmax = 0;
	b_param->v_pixelmax = 0;
	b_param->rate = 60;
	b_param->width = width;
	b_param->height = height;

	b_param->id_manuf = "ACRN";
	b_param->id_product = 4321;
	b_param->id_sn = 12345678;

	b_param->sn = "A0123456789";
	b_param->product_name = "ACRN_Monitor";
}

static void
vdpy_edid_set_frame(frame_param *frame, const base_param *b_param)
{
	frame->hav_pixel = b_param->h_pixel;
	frame->hb_pixel = b_param->h_pixel * 35 / 100;
	frame->hfp_pixel = b_param->h_pixel * 25 / 100;
	frame->hsp_pixel = b_param->h_pixel * 3 / 100;
	frame->lhb_pixel = 0;
	frame->vav_line = b_param->v_pixel;
	frame->vb_line = b_param->v_pixel * 35 / 1000;
	frame->vfp_line = b_param->v_pixel * 5 / 1000;
	frame->vsp_line = b_param->v_pixel * 5 / 1000;
	frame->tvb_line = 0;
	frame->pixel_clock = b_param->rate *
			(frame->hav_pixel + frame->hb_pixel + frame->lhb_pixel * 2) *
			(frame->vav_line + frame->vb_line + frame->tvb_line * 2);
	frame->width = b_param->width;
	frame->height = b_param->height;
}

static void
vdpy_edid_set_color(uint8_t *edid, float red_x, float red_y,
				   float green_x, float green_y,
				   float blue_x, float blue_y,
				   float white_x, float white_y)
{
	uint8_t *color;
	uint16_t rx, ry, gx, gy, bx, by, wx, wy;

	rx = transto_10bits(red_x);
	ry = transto_10bits(red_y);
	gx = transto_10bits(green_x);
	gy = transto_10bits(green_y);
	bx = transto_10bits(blue_x);
	by = transto_10bits(blue_y);
	wx = transto_10bits(white_x);
	wy = transto_10bits(white_y);

	color = edid + 25;
	color[0] = ((rx & 0x03) << 6) |
		   ((ry & 0x03) << 4) |
		   ((gx & 0x03) << 2) |
		    (gy & 0x03);
	color[1] = ((bx & 0x03) << 6) |
		   ((by & 0x03) << 4) |
		   ((wx & 0x03) << 2) |
		    (wy & 0x03);
	color[2] = rx >> 2;
	color[3] = ry >> 2;
	color[4] = gx >> 2;
	color[5] = gy >> 2;
	color[6] = bx >> 2;
	color[7] = by >> 2;
	color[8] = wx >> 2;
	color[9] = wy >> 2;
}

static void
vdpy_edid_set_timing(uint8_t *addr, const base_param *b_param, TIMING_MODE mode)
{
	static uint16_t idx;
	static uint16_t size;
	const struct timing_entry *timing;
	uint8_t stdcnt;
	uint16_t hpixel;
	int16_t AR;

	stdcnt = 0;

	if(mode == STDT) {
		addr += 38;
	}

	idx = 0;
	size = sizeof(timings) / sizeof(timings[0]);
	for(; idx < size; idx++){
		timing = timings + idx;
		if ((b_param->h_pixelmax && b_param->h_pixelmax < timing->hpixel) ||
		    (b_param->v_pixelmax && b_param->v_pixelmax < timing->vpixel)) {
			continue;
		}
		switch(mode){
		case ESTT: // Established Timings I & II
			if(timing->byte) {
				addr[timing->byte] |= (1 << timing->bit);
				break;
			} else {
				return;
			}
		case ESTT3: // Established Timings III
			if(timing->byte_t3){
				addr[timing->byte_t3] |= (1 << timing->bit);
				break;
			} else {
				return;
			}
		case STDT: // Standard Timings
			if(stdcnt < 8 && (timing->hpixel == b_param->h_pixel)) {
				hpixel = (timing->hpixel >> 3) - 31;
				if (timing->hpixel == 0 ||
				    timing->vpixel == 0) {
					AR = -1;
				} else if (hpixel & 0xff00) {
					AR = -2;
				} else if (timing->hpixel * 10 ==
				    timing->vpixel * 16) {
					AR = 0;
				} else if (timing->hpixel * 3 ==
				    timing->vpixel * 4) {
					AR = 1;
				} else if (timing->hpixel * 4 ==
				    timing->vpixel * 5) {
					AR = 2;
				} else if (timing->hpixel * 9 ==
				    timing->vpixel * 16) {
					AR = 3;
				} else {
					AR = -2;
				}
				if (AR >= 0) {
					addr[0] = hpixel & 0xff;
					addr[1] = (AR << 6) | ((timing->hz - 60) & 0x3f);
					addr += 2;
					stdcnt++;
				} else if (AR == -1){
					addr[0] = 0x01;
					addr[1] = 0x01;
					addr += 2;
					stdcnt++;
				}
				break;
			} else {
				return;
			}

		default:
			return;
		}
	}
	while(mode == STDT && stdcnt < 8){
		addr[0] = 0x01;
		addr[1] = 0x01;
		addr += 2;
		stdcnt++;
	}
}

static void
vdpy_edid_set_dtd(uint8_t *dtd, const frame_param *frame)
{
	uint16_t pixel_clk;

	// Range: 10 kHz to 655.35 MHz in 10 kHz steps
	pixel_clk = frame->pixel_clock / 10000;
	memcpy(dtd, &pixel_clk, sizeof(pixel_clk));
	dtd[2] = frame->hav_pixel & 0xff;
	dtd[3] = frame->hb_pixel & 0xff;
	dtd[4] = ((frame->hav_pixel & 0xf00) >> 4) |
		 ((frame->hb_pixel & 0xf00) >> 8);
	dtd[5] = frame->vav_line & 0xff;
	dtd[6] = frame->vb_line & 0xff;
	dtd[7] = ((frame->vav_line & 0xf00) >> 4) |
		 ((frame->vb_line & 0xf00) >> 8);
	dtd[8] = frame->hfp_pixel & 0xff;
	dtd[9] = frame->hsp_pixel & 0xff;
	dtd[10] = ((frame->vfp_line & 0xf) << 4) |
		   (frame->vsp_line & 0xf);
	dtd[11] = ((frame->hfp_pixel & 0x300) >> 2) |
		  ((frame->hsp_pixel & 0x300) >> 4) |
		  ((frame->vfp_line & 0x030) >> 6) |
		  ((frame->vsp_line & 0x030) >> 8);
	dtd[12] = frame->width & 0xff;
	dtd[13] = frame->height & 0xff;
	dtd[14] = ((frame->width & 0xf00) >> 4) |
		  ((frame->height & 0xf00) >> 8);
	dtd[15] = frame->lhb_pixel & 0xff;
	dtd[16] = frame->tvb_line & 0xff;
	dtd[17] = 0x18;
}

static void
vdpy_edid_set_descripter(uint8_t *edid, uint8_t is_dtd,
		uint8_t tag, const base_param *b_param)
{
	static uint8_t offset;
	uint8_t *desc;
	frame_param frame;
	const char* text;
	uint16_t len;

	offset = 54;
	desc = edid + offset;

	if (is_dtd) {
		vdpy_edid_set_frame(&frame, b_param);
		vdpy_edid_set_dtd(desc, &frame);
		offset += 18;
		return;
	}
	desc[3] = tag;
	text = NULL;
	switch(tag){
	// Established Timings III Descriptor (tag #F7h)
	case 0xf7:
		desc[5] = 0x0a; // Revision Number
		vdpy_edid_set_timing(desc, b_param, ESTT3);
		break;
	// Display Range Limits & Additional Timing Descriptor (tag #FDh)
	case 0xfd:
		desc[5] =  50; // Minimum Vertical Rate. (50 -> 125 Hz)
		desc[6] = 125; // Maximum Vertical Rate.
		desc[7] =  30; // Minimum Horizontal Rate.(30 -> 160 kHz)
		desc[8] = 160; // Maximum Horizontal Rate.
		desc[9] = 2550 / 10; // Max Pixel Clock. (2550 MHz)
		desc[10] = 0x01; // no extended timing information
		desc[11] = '\n'; // padding
		break;
	// Display Product Name (ASCII) String Descriptor (tag #FCh)
	case 0xfc:
	// Display Product Serial Number Descriptor (tag #FFh)
	case 0xff:
		text = (tag == 0xff) ? b_param->sn : b_param->product_name;
		memset(desc + 5, ' ', 13);
		if (text == NULL)
			break;
		len = strlen(text);
		if (len > 12)
			len = 12;
		memcpy(desc + 5, text, len);
		desc[len + 5] = '\n';
		break;
	// Dummy Descriptor (Tag #10h)
	case 0x10:
	default:
		break;
	}
	offset += 18;
}

static uint8_t
vdpy_edid_get_checksum(uint8_t *edid)
{
	uint8_t sum;
	int i;

	sum = 0;
	for (i = 0; i < 127; i++) {
		sum += edid[i];
	}

	return 0x100 - sum;
}

static void
vdpy_edid_generate(uint8_t *edid, size_t size, struct edid_info *info)
{
	uint16_t id_manuf;
	uint16_t id_product;
	uint32_t serial;
	base_param b_param, c_param;

	vdpy_edid_set_baseparam(&b_param, info->prefx, info->prefy);

	memset(edid, 0, size);
	/* edid[7:0], fixed header information, (00 FF FF FF FF FF FF 00)h */
	memset(edid + 1, 0xff, 6);

	/* edid[17:8], Vendor & Product Identification */
	// Manufacturer ID is a big-endian 16-bit value.
	id_manuf = ((((b_param.id_manuf[0] - '@') & 0x1f) << 10) |
		    (((b_param.id_manuf[1] - '@') & 0x1f) << 5) |
		    (((b_param.id_manuf[2] - '@') & 0x1f) << 0));
	edid[8] = id_manuf >> 8;
	edid[9] = id_manuf & 0xff;

	// Manufacturer product code is a little-endian 16-bit number.
	id_product = b_param.id_product;
	memcpy(edid+10, &id_product, sizeof(id_product));

	// Serial number is a little-endian 32-bit value.
	serial = b_param.id_sn;
	memcpy(edid+12, &serial, sizeof(serial));

	edid[16] = 0;           // Week of Manufacture
	edid[17] = 2018 - 1990; // Year of Manufacture or Model Year.
				// Acrn is released in 2018.

	edid[18] = 1;   // Version Number
	edid[19] = 4;   // Revision Number

	/* edid[24:20], Basic Display Parameters & Features */
	// Video Input Definition: 1 Byte
	edid[20] = 0xa5; // Digital input;
			 // 8 Bits per Primary Color;
			 // DisplayPort is supported

	// Horizontal and Vertical Screen Size or Aspect Ratio: 2 Bytes
	// screen size, in centimetres
	edid[21] = info->prefx / 10;
	edid[22] = info->prefy / 10;

	// Display Transfer Characteristics (GAMMA): 1 Byte
	// Stored Value = (GAMMA x 100) - 100
	edid[23] = 120; // display gamma: 2.2

	// Feature Support: 1 Byte
	edid[24] = 0x06; // sRGB Standard is the default color space;
			 // Preferred Timing Mode includes the native
			 // pixel format and preferred.

	/* edid[34:25], Display x, y Chromaticity Coordinates */
	vdpy_edid_set_color(edid, 0.6400, 0.3300,
				  0.3000, 0.6000,
				  0.1500, 0.0600,
				  0.3127, 0.3290);

	/* edid[37:35], Established Timings */
	vdpy_edid_set_timing(edid, &b_param, ESTT);

	/* edid[53:38], Standard Timings: Identification 1 -> 8 */
	vdpy_edid_set_timing(edid, &b_param, STDT);

	/* edid[125:54], Detailed Timing Descriptor - 18 bytes x 4 */
	// Detailed Timing Descriptor 1
	vdpy_edid_set_baseparam(&c_param, VDPY_MAX_WIDTH, VDPY_MAX_HEIGHT);
	vdpy_edid_set_descripter(edid, 0x1, 0, &c_param);
	// Detailed Timing Descriptor 2
	vdpy_edid_set_baseparam(&c_param, VDPY_DEFAULT_WIDTH, VDPY_DEFAULT_HEIGHT);
	vdpy_edid_set_descripter(edid, 0x1, 0, &c_param);
	// Display Product Name (ASCII) String Descriptor (tag #FCh)
	vdpy_edid_set_descripter(edid, 0, 0xfc, &b_param);
	// Display Product Serial Number Descriptor (tag #FFh)
	vdpy_edid_set_descripter(edid, 0, 0xff, &b_param);

	/* EDID[126], Extension Block Count */
	edid[126] = 0;  // no Extension Block

	/* Checksum */
	edid[127] = vdpy_edid_get_checksum(edid);
}

void
vdpy_get_edid(int handle, uint8_t *edid, size_t size)
{
	struct edid_info edid_info;

	if (handle == vdpy.s.n_connect) {
		edid_info.prefx = vdpy.info.width;
		edid_info.prefy = vdpy.info.height;
		edid_info.maxx = VDPY_MAX_WIDTH;
		edid_info.maxy = VDPY_MAX_HEIGHT;
	} else {
		edid_info.prefx = 0;
		edid_info.prefy = 0;
		edid_info.maxx = 0;
		edid_info.maxy = 0;
	}
	edid_info.refresh_rate = 0;
	edid_info.vendor = NULL;
	edid_info.name = NULL;
	edid_info.sn = NULL;

	vdpy_edid_generate(edid, size, &edid_info);
}

void
vdpy_get_display_info(int handle, struct display_info *info)
{
	if (handle == vdpy.s.n_connect) {
		info->xoff = vdpy.info.xoff;
		info->yoff = vdpy.info.yoff;
		info->width = vdpy.info.width;
		info->height = vdpy.info.height;
	} else {
		info->xoff = 0;
		info->yoff = 0;
		info->width = 0;
		info->height = 0;
	}
}

static void
sdl_gl_display_init(void)
{
	struct egl_display_ops *gl_ops = &vdpy.gl_ops;

	/* obtain the eglDisplay/eglContext */
	vdpy.eglDisplay = eglGetCurrentDisplay();
	vdpy.eglContext = SDL_GL_GetCurrentContext();

	/* Try to use the eglGetProcaddress to obtain callback API for
	 * eglCreateImageKHR/eglDestroyImageKHR
	 * glEGLImageTargetTexture2DOES
	 */
	gl_ops->eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)
				eglGetProcAddress("eglCreateImageKHR");
	gl_ops->eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)
				eglGetProcAddress("eglDestroyImageKHR");
	gl_ops->glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
				eglGetProcAddress("glEGLImageTargetTexture2DOES");

	vdpy.cur_egl_img = EGL_NO_IMAGE_KHR;
	if ((gl_ops->eglCreateImageKHR == NULL) ||
		(gl_ops->eglDestroyImageKHR == NULL) ||
		(gl_ops->glEGLImageTargetTexture2DOES == NULL)) {
		pr_info("DMABuf is not supported.\n");
		vdpy.egl_dmabuf_supported = false;
	} else
		vdpy.egl_dmabuf_supported = true;

	return;
}

void
vdpy_surface_set(int handle, struct surface *surf)
{
	pixman_image_t *src_img;
	int format;
	int access, i;

	if (handle != vdpy.s.n_connect) {
		return;
	}

	if (surf == NULL ) {
		vdpy.surf.width = 0;
		vdpy.surf.height = 0;
		/* Need to use the default 640x480 for the SDL_Texture */
		src_img = pixman_image_create_bits(PIXMAN_a8r8g8b8,
			VDPY_MIN_WIDTH, VDPY_MIN_HEIGHT,
			(uint32_t *)default_raw_argb,
			VDPY_MIN_WIDTH * 4);
		if (src_img == NULL) {
			pr_err("failed to create pixman_image\n");
			return;
		}
		vdpy.guest_width = VDPY_MIN_WIDTH;
		vdpy.guest_height = VDPY_MIN_HEIGHT;
	} else if (surf->surf_type == SURFACE_PIXMAN) {
		src_img = pixman_image_create_bits(surf->surf_format,
			surf->width, surf->height, surf->pixel,
			surf->stride);
		if (src_img == NULL) {
			pr_err("failed to create pixman_image\n");
			return;
		}
		vdpy.surf = *surf;
		vdpy.guest_width = surf->width;
		vdpy.guest_height = surf->height;
	} else if (surf->surf_type == SURFACE_DMABUF) {
		src_img = NULL;
		vdpy.surf = *surf;
		vdpy.guest_width = surf->width;
		vdpy.guest_height = surf->height;
	} else {
		/* Unsupported type */
		return;
	}

	if (vdpy.dpy_texture) {
		SDL_DestroyTexture(vdpy.dpy_texture);
	}
	if (surf && (surf->surf_type == SURFACE_DMABUF)) {
		access = SDL_TEXTUREACCESS_STATIC;
		format = SDL_PIXELFORMAT_EXTERNAL_OES;
	} else {
		access = SDL_TEXTUREACCESS_STREAMING;
		format = SDL_PIXELFORMAT_ARGB8888;
		switch (pixman_image_get_format(src_img)) {
		case PIXMAN_a8r8g8b8:
		case PIXMAN_x8r8g8b8:
			format = SDL_PIXELFORMAT_ARGB8888;
			break;
		case PIXMAN_a8b8g8r8:
		case PIXMAN_x8b8g8r8:
			format = SDL_PIXELFORMAT_ABGR8888;
			break;
		case PIXMAN_r8g8b8a8:
			format = SDL_PIXELFORMAT_RGBA8888;
		case PIXMAN_r8g8b8x8:
			format = SDL_PIXELFORMAT_RGBX8888;
			break;
		case PIXMAN_b8g8r8a8:
		case PIXMAN_b8g8r8x8:
			format = SDL_PIXELFORMAT_BGRA8888;
			break;
		default:
			pr_err("Unsupported format. %x\n",
					pixman_image_get_format(src_img));
		}
	}
	vdpy.dpy_texture = SDL_CreateTexture(vdpy.dpy_renderer,
			format, access,
			vdpy.guest_width, vdpy.guest_height);

	if (vdpy.dpy_texture == NULL) {
		pr_err("Failed to create SDL_texture for surface.\n");
	}

	/* For the surf_switch, it will be updated in surface_update */
	if (!surf) {
		SDL_UpdateTexture(vdpy.dpy_texture, NULL,
				  pixman_image_get_data(src_img),
				  pixman_image_get_stride(src_img));

		SDL_RenderClear(vdpy.dpy_renderer);
		SDL_RenderCopy(vdpy.dpy_renderer, vdpy.dpy_texture, NULL, NULL);
		SDL_RenderPresent(vdpy.dpy_renderer);
	} else if (surf->surf_type == SURFACE_DMABUF) {
		EGLImageKHR egl_img = EGL_NO_IMAGE_KHR;
		EGLint attrs[64];
		struct egl_display_ops *gl_ops;

		gl_ops = &vdpy.gl_ops;
		i = 0;
		attrs[i++] = EGL_WIDTH;
		attrs[i++] = surf->width;
		attrs[i++] = EGL_HEIGHT;
		attrs[i++] = surf->height;
		attrs[i++] = EGL_LINUX_DRM_FOURCC_EXT;
		attrs[i++] = surf->dma_info.surf_fourcc;
		attrs[i++] = EGL_DMA_BUF_PLANE0_FD_EXT;
		attrs[i++] = surf->dma_info.dmabuf_fd;
		attrs[i++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
		attrs[i++] = surf->stride;
		attrs[i++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
		attrs[i++] = 0;
		attrs[i++] = EGL_NONE;

		egl_img = gl_ops->eglCreateImageKHR(vdpy.eglDisplay,
				EGL_NO_CONTEXT,
				EGL_LINUX_DMA_BUF_EXT,
				NULL, attrs);
		if (egl_img == EGL_NO_IMAGE_KHR) {
			pr_err("Failed in eglCreateImageKHR.\n");
			return;
		}

		SDL_GL_BindTexture(vdpy.dpy_texture, NULL, NULL);
		gl_ops->glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, egl_img);
		if (vdpy.cur_egl_img != EGL_NO_IMAGE_KHR)
			gl_ops->eglDestroyImageKHR(vdpy.eglDisplay,
					vdpy.cur_egl_img);

		/* In theory the created egl_img can be released after it is bound
		 * to texture.
		 * Now it is released next time so that it is controlled correctly
		 */
		vdpy.cur_egl_img = egl_img;
	}

	if (vdpy.dpy_img)
		pixman_image_unref(vdpy.dpy_img);

	if (surf == NULL) {
		SDL_SetWindowTitle(vdpy.dpy_win,
				"Not activate display yet!");
	} else {
		SDL_SetWindowTitle(vdpy.dpy_win,
				"ACRN Virtual Monitor");
	}
	/* Replace the cur_img with the created_img */
	vdpy.dpy_img = src_img;
}

void
vdpy_cursor_position_transformation(struct display *vdpy, SDL_Rect *rect)
{
	rect->x = (vdpy->cur.x * vdpy->width) / vdpy->guest_width;
	rect->y = (vdpy->cur.y * vdpy->height) / vdpy->guest_height;
	rect->w = (vdpy->cur.width * vdpy->width) / vdpy->guest_width;
	rect->h = (vdpy->cur.height * vdpy->height) / vdpy->guest_height;
}

void
vdpy_surface_update(int handle, struct surface *surf)
{
	SDL_Rect cursor_rect;

	if (handle != vdpy.s.n_connect) {
		return;
	}

	if (!surf) {
		pr_err("Incorrect order of submitting Virtio-GPU cmd.\n");
	        return;
	}

	if (surf->surf_type == SURFACE_PIXMAN)
		SDL_UpdateTexture(vdpy.dpy_texture, NULL,
			  surf->pixel,
			  surf->stride);

	SDL_RenderClear(vdpy.dpy_renderer);
	SDL_RenderCopy(vdpy.dpy_renderer, vdpy.dpy_texture, NULL, NULL);

	/* This should be handled after rendering the surface_texture.
	 * Otherwise it will be hidden
	 */
	if (vdpy.cursor_tex) {
		vdpy_cursor_position_transformation(&vdpy, &cursor_rect);
		SDL_RenderCopy(vdpy.dpy_renderer, vdpy.cursor_tex,
				NULL, &cursor_rect);
	}

	SDL_RenderPresent(vdpy.dpy_renderer);

	/* update the rendering time */
	clock_gettime(CLOCK_MONOTONIC, &vdpy.last_time);
}

void
vdpy_cursor_define(int handle, struct cursor *cur)
{
	if (handle != vdpy.s.n_connect) {
		return;
	}

	if (cur->data == NULL)
		return;

	if (vdpy.cursor_tex)
		SDL_DestroyTexture(vdpy.cursor_tex);

	vdpy.cursor_tex = SDL_CreateTexture(
			vdpy.dpy_renderer,
			SDL_PIXELFORMAT_ARGB8888,
			SDL_TEXTUREACCESS_STREAMING,
			cur->width, cur->height);
	if (vdpy.cursor_tex == NULL) {
		pr_err("Failed to create sdl_cursor surface for %p.\n", cur);
		return;
	}

	SDL_SetTextureBlendMode(vdpy.cursor_tex, SDL_BLENDMODE_BLEND);
	vdpy.cur = *cur;
	SDL_UpdateTexture(vdpy.cursor_tex, NULL, cur->data, cur->width * 4);
}

void
vdpy_cursor_move(int handle, uint32_t x, uint32_t y)
{
	if (handle != vdpy.s.n_connect) {
		return;
	}

	/* Only move the position of the cursor. The cursor_texture
	 * will be handled in surface_update
	 */
	vdpy.cur.x = x;
	vdpy.cur.y = y;
}

static void
vdpy_sdl_ui_refresh(void *data)
{
	struct display *ui_vdpy;
	struct timespec cur_time;
	uint64_t elapsed_time;
	SDL_Rect cursor_rect;

	ui_vdpy = (struct display *)data;

	/* Skip it if no surface needs to be rendered */
	if (ui_vdpy->dpy_texture == NULL)
		return;

	clock_gettime(CLOCK_MONOTONIC, &cur_time);

	elapsed_time = (cur_time.tv_sec - ui_vdpy->last_time.tv_sec) * 1000000000 +
			cur_time.tv_nsec - ui_vdpy->last_time.tv_nsec;

	/* the time interval is less than 10ms. Skip it */
	if (elapsed_time < 10000000)
		return;

	SDL_RenderClear(ui_vdpy->dpy_renderer);
	SDL_RenderCopy(ui_vdpy->dpy_renderer, ui_vdpy->dpy_texture, NULL, NULL);

	/* This should be handled after rendering the surface_texture.
	 * Otherwise it will be hidden
	 */
	if (ui_vdpy->cursor_tex) {
		vdpy_cursor_position_transformation(ui_vdpy, &cursor_rect);
		SDL_RenderCopy(ui_vdpy->dpy_renderer, ui_vdpy->cursor_tex,
				NULL, &cursor_rect);
	}

	SDL_RenderPresent(ui_vdpy->dpy_renderer);
}

static void
vdpy_sdl_ui_timer(void *data, uint64_t nexp)
{
	struct display *ui_vdpy;
	struct vdpy_display_bh *bh_task;

	ui_vdpy = (struct display *)data;

	/* Don't submit the display_request if another func already
	 * acquires the mutex.
	 * This is to optimize the mevent thread otherwise it needs
	 * to wait for some time.
	 */
	if (pthread_mutex_trylock(&ui_vdpy->vdisplay_mutex))
		return;

	bh_task = &ui_vdpy->ui_timer_bh;
	if ((bh_task->bh_flag & ACRN_BH_PENDING) == 0) {
		bh_task->bh_flag |= ACRN_BH_PENDING;
		TAILQ_INSERT_TAIL(&ui_vdpy->request_list, bh_task, link);
	}
	pthread_cond_signal(&ui_vdpy->vdisplay_signal);
	pthread_mutex_unlock(&ui_vdpy->vdisplay_mutex);
}

static void *
vdpy_sdl_display_thread(void *data)
{
	uint32_t win_flags;
	struct vdpy_display_bh *bh;
	struct itimerspec ui_timer_spec;

	if (vdpy.width && vdpy.height) {
		/* clip the region between (640x480) and (1920x1080) */
		if (vdpy.width < VDPY_MIN_WIDTH)
			vdpy.width = VDPY_MIN_WIDTH;
		if (vdpy.width > VDPY_MAX_WIDTH)
			vdpy.width = VDPY_MAX_WIDTH;
		if (vdpy.height < VDPY_MIN_HEIGHT)
			vdpy.height = VDPY_MIN_HEIGHT;
		if (vdpy.height > VDPY_MAX_HEIGHT)
			vdpy.height = VDPY_MAX_HEIGHT;
	} else {
		/* the default window(1280x720) is created with undefined pos
		 * when no geometry info is passed
		 */
		vdpy.org_x = 0xFFFF;
		vdpy.org_y = 0xFFFF;
		vdpy.width = VDPY_DEFAULT_WIDTH;
		vdpy.height = VDPY_DEFAULT_HEIGHT;
	}

	win_flags = SDL_WINDOW_OPENGL |
		    SDL_WINDOW_ALWAYS_ON_TOP |
		    SDL_WINDOW_SHOWN;
	if (vdpy.s.is_fullscreen) {
		win_flags |= SDL_WINDOW_FULLSCREEN;
	}
	vdpy.dpy_win = NULL;
	vdpy.dpy_renderer = NULL;
	vdpy.dpy_win = SDL_CreateWindow("ACRN_DM",
					vdpy.org_x, vdpy.org_y,
					vdpy.width, vdpy.height,
					win_flags);
	if (vdpy.dpy_win == NULL) {
		pr_err("Failed to Create SDL_Window\n");
		goto sdl_fail;
	}
	vdpy.dpy_renderer = SDL_CreateRenderer(vdpy.dpy_win, -1, 0);
	if (vdpy.dpy_renderer == NULL) {
		pr_err("Failed to Create GL_Renderer \n");
		goto sdl_fail;
	}
	sdl_gl_display_init();
	pthread_mutex_init(&vdpy.vdisplay_mutex, NULL);
	pthread_cond_init(&vdpy.vdisplay_signal, NULL);
	TAILQ_INIT(&vdpy.request_list);
	vdpy.s.is_active = 1;

	vdpy.ui_timer_bh.task_cb = vdpy_sdl_ui_refresh;
	vdpy.ui_timer_bh.data = &vdpy;
	clock_gettime(CLOCK_MONOTONIC, &vdpy.last_time);
	vdpy.ui_timer.clockid = CLOCK_MONOTONIC;
	acrn_timer_init(&vdpy.ui_timer, vdpy_sdl_ui_timer, &vdpy);
	ui_timer_spec.it_interval.tv_sec = 0;
	ui_timer_spec.it_interval.tv_nsec = 33000000;
	/* Wait for 5s to start the timer */
	ui_timer_spec.it_value.tv_sec = 5;
	ui_timer_spec.it_value.tv_nsec = 0;
	/* Start one periodic timer to refresh UI based on 30fps */
	acrn_timer_settime(&vdpy.ui_timer, &ui_timer_spec);

	pr_info("SDL display thread is created\n");
	/* Begin to process the display_cmd after initialization */
	do {
		if (!vdpy.s.is_active) {
			pr_info("display is exiting\n");
			break;
		}
		pthread_mutex_lock(&vdpy.vdisplay_mutex);

		if (TAILQ_EMPTY(&vdpy.request_list))
			pthread_cond_wait(&vdpy.vdisplay_signal,
					  &vdpy.vdisplay_mutex);

		/* the bh_task is handled in vdisplay_mutex lock */
		while (!TAILQ_EMPTY(&vdpy.request_list)) {
			bh = TAILQ_FIRST(&vdpy.request_list);

			TAILQ_REMOVE(&vdpy.request_list, bh, link);

			bh->task_cb(bh->data);

			if (atomic_load(&bh->bh_flag) & ACRN_BH_FREE) {
				free(bh);
				bh = NULL;
			} else {
				/* free is owned by the submitter */
				atomic_store(&bh->bh_flag, ACRN_BH_DONE);
			}
		}

		pthread_mutex_unlock(&vdpy.vdisplay_mutex);
	} while (1);

	acrn_timer_deinit(&vdpy.ui_timer);
	/* SDL display_thread will exit because of DM request */
	pthread_mutex_destroy(&vdpy.vdisplay_mutex);
	pthread_cond_destroy(&vdpy.vdisplay_signal);
	if (vdpy.dpy_img)
		pixman_image_unref(vdpy.dpy_img);
	/* Continue to thread cleanup */

	if (vdpy.dpy_texture) {
		SDL_DestroyTexture(vdpy.dpy_texture);
		vdpy.dpy_texture = NULL;
	}
	if (vdpy.cursor_tex) {
		SDL_DestroyTexture(vdpy.cursor_tex);
		vdpy.cursor_tex = NULL;
	}

	if (vdpy.egl_dmabuf_supported && (vdpy.cur_egl_img != EGL_NO_IMAGE_KHR))
		vdpy.gl_ops.eglDestroyImageKHR(vdpy.eglDisplay,
					vdpy.cur_egl_img);

sdl_fail:

	if (vdpy.dpy_renderer) {
		SDL_DestroyRenderer(vdpy.dpy_renderer);
		vdpy.dpy_renderer = NULL;
	}
	if (vdpy.dpy_win) {
		SDL_DestroyWindow(vdpy.dpy_win);
		vdpy.dpy_win = NULL;
	}

	/* This is used to workaround the TLS issue of libEGL + libGLdispatch
	 * after unloading library.
	 */
	eglReleaseThread();
	return NULL;
}

bool vdpy_submit_bh(int handle, struct vdpy_display_bh *bh_task)
{
	bool bh_ok = false;

	if (handle != vdpy.s.n_connect) {
		return bh_ok;
	}

	if (!vdpy.s.is_active)
		return bh_ok;

	pthread_mutex_lock(&vdpy.vdisplay_mutex);

	if ((bh_task->bh_flag & ACRN_BH_PENDING) == 0) {
		bh_task->bh_flag |= ACRN_BH_PENDING;
		TAILQ_INSERT_TAIL(&vdpy.request_list, bh_task, link);
		bh_ok = true;
	}
	pthread_cond_signal(&vdpy.vdisplay_signal);
	pthread_mutex_unlock(&vdpy.vdisplay_mutex);

	return bh_ok;
}

int
vdpy_init()
{
	int err, count;

	if (vdpy.s.n_connect) {
		return 0;
	}

	/* start one vdpy_sdl_display_thread to handle the 3D request
	 * in this dedicated thread. Otherwise the libSDL + 3D doesn't
	 * work.
	 */
	err = pthread_create(&vdpy.tid, NULL, vdpy_sdl_display_thread, &vdpy);
	if (err) {
		pr_err("Failed to create the sdl_display_thread.\n");
		return 0;
	}

	count = 0;
	/* Wait up to 200ms so that the vdpy_sdl_display_thread is ready to
	 * handle the 3D request
	 */
	while (!vdpy.s.is_active && count < 20) {
		usleep(10000);
		count++;
	}
	if (!vdpy.s.is_active) {
		pr_err("display_thread is not ready.\n");
	}

	vdpy.s.n_connect++;
	return vdpy.s.n_connect;
}

int
vdpy_deinit(int handle)
{
	if (handle != vdpy.s.n_connect) {
		return -1;
	}

	vdpy.s.n_connect--;

	if (!vdpy.s.is_active) {
		return -1;
	}

	pthread_mutex_lock(&vdpy.vdisplay_mutex);
	vdpy.s.is_active = 0;
	/* Wakeup the vdpy_sdl_display_thread if it is waiting for signal */
	pthread_cond_signal(&vdpy.vdisplay_signal);
	pthread_mutex_unlock(&vdpy.vdisplay_mutex);

	pthread_join(vdpy.tid, NULL);
	pr_info("Exit SDL display thread\n");

	return 0;
}

void
gfx_ui_init()
{
	SDL_SysWMinfo info;
	SDL_Rect disp_rect;

	setenv("SDL_VIDEO_X11_FORCE_EGL", "1", 1);
	setenv("SDL_OPENGL_ES_DRIVER", "1", 1);
	setenv("SDL_RENDER_DRIVER", "opengles2", 1);
	setenv("SDL_RENDER_SCALE_QUALITY", "linear", 1);

	if (SDL_Init(SDL_INIT_VIDEO)) {
		pr_err("Failed to Init SDL2 system");
	}

	SDL_GetDisplayBounds(0, &disp_rect);

	if (disp_rect.w < VDPY_MIN_WIDTH ||
	    disp_rect.h < VDPY_MIN_HEIGHT) {
		pr_err("Too small resolutions. Please check the "
		       " graphics system\n");
		SDL_Quit();
	}

	SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");
	memset(&info, 0, sizeof(info));
	SDL_VERSION(&info.version);

	/* Set the GL_parameter for Window/Renderer */
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
			    SDL_GL_CONTEXT_PROFILE_ES);
	/* GLES2.0 is used */
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	/* GL target surface selects A8/R8/G8/B8 */
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	vdpy.s.is_ui_realized = true;
}

void
gfx_ui_deinit()
{
	if (!vdpy.s.is_ui_realized) {
		return;
	}

	SDL_Quit();
	pr_info("SDL_Quit\r\n");
}

int vdpy_parse_cmd_option(const char *opts)
{
	char *str;
	int snum, error;

	error = 0;

	str = strcasestr(opts, "geometry=");
	if (opts && strcasestr(opts, "geometry=fullscreen")) {
		snum = sscanf(str, "geometry=fullscreen:%d", &vdpy.screen);
		if (snum != 1) {
			vdpy.screen = 0;
		}
		vdpy.width = VDPY_MAX_WIDTH;
		vdpy.height = VDPY_MAX_HEIGHT;
		vdpy.s.is_fullscreen = true;
		pr_info("virtual display: fullscreen.\n");
	} else if (opts && strcasestr(opts, "geometry=")) {
		snum = sscanf(str, "geometry=%dx%d+%d+%d",
				&vdpy.width, &vdpy.height,
				&vdpy.org_x, &vdpy.org_y);
		if (snum != 4) {
			pr_err("incorrect geometry option. Should be"
					" WxH+x+y\n");
			error = -1;
		}
		vdpy.s.is_fullscreen = false;
		pr_info("virtual display: windowed.\n");
	}

	return error;
}
