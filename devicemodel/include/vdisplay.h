/*
 * Copyright (C) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Vistual Display for VMs
 *
 */
#ifndef _VDISPLAY_H_
#define _VDISPLAY_H_

#include <sys/queue.h>
#include <pixman.h>
#include "dm.h"

typedef void (*bh_task_func)(void *data);

/* bh task is still pending */
#define ACRN_BH_PENDING (1 << 0)
/* bh task is done */
#define ACRN_BH_DONE	(1 << 1)
/* free vdpy_display_bh after executing bh_cb */
#define ACRN_BH_FREE    (1 << 2)

struct vdpy_display_bh {
	TAILQ_ENTRY(vdpy_display_bh) link;
	bh_task_func task_cb;
	void *data;
	uint32_t bh_flag;
};

struct edid_info {
	char *vendor;
	char *name;
	char *sn;
	uint32_t prefx;
	uint32_t prefy;
	uint32_t maxx;
	uint32_t maxy;
	uint32_t refresh_rate;
};

struct display_info {
	/* geometry */
	int xoff;
	int yoff;
	uint32_t width;
	uint32_t height;
};

enum surface_type {
	SURFACE_PIXMAN = 1,
	SURFACE_DMABUF,
};

struct surface {
	enum surface_type surf_type;
	/* use pixman_format as the intermediate-format */
	pixman_format_code_t surf_format;
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
	uint32_t bpp;
	uint32_t stride;
	void *pixel;
};

struct cursor {
	enum surface_type surf_type;
	/* use pixman_format as the intermediate-format */
	pixman_format_code_t surf_format;
	uint32_t x;
	uint32_t y;
	uint32_t hot_x;
	uint32_t hot_y;
	uint32_t width;
	uint32_t height;
	void *data;
};

int vdpy_parse_cmd_option(const char *opts);
void gfx_ui_init();
int vdpy_init();
void vdpy_get_display_info(int handle, struct display_info *info);
void vdpy_surface_set(int handle, struct surface *surf);
void vdpy_surface_update(int handle, struct surface *surf);
bool vdpy_submit_bh(int handle, struct vdpy_display_bh *bh);
void vdpy_get_edid(int handle, uint8_t *edid, size_t size);
void vdpy_cursor_define(int handle, struct cursor *cur);
void vdpy_cursor_move(int handle, uint32_t x, uint32_t y);
int vdpy_deinit(int handle);
void gfx_ui_deinit();

#endif /* _VDISPLAY_H_ */
