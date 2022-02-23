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
} vdpy = {
	.s.is_ui_realized = false,
	.s.is_active = false,
	.s.is_wayland = false,
	.s.is_x11 = false,
	.s.is_fullscreen = false,
	.s.updates = 0,
	.s.n_connect = 0
};

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

void
vdpy_surface_set(int handle, struct surface *surf)
{
	pixman_image_t *src_img;
	int format;

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
	} else {
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
	}
	if (vdpy.dpy_texture) {
		SDL_DestroyTexture(vdpy.dpy_texture);
	}
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
	vdpy.dpy_texture = SDL_CreateTexture(vdpy.dpy_renderer,
			format, SDL_TEXTUREACCESS_STREAMING,
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
vdpy_surface_update(int handle, struct surface *surf)
{
	if (handle != vdpy.s.n_connect) {
		return;
	}

	if (!surf) {
		pr_err("Incorrect order of submitting Virtio-GPU cmd.\n");
	        return;
	}

	SDL_UpdateTexture(vdpy.dpy_texture, NULL,
			  surf->pixel,
			  surf->stride);

	SDL_RenderClear(vdpy.dpy_renderer);
	SDL_RenderCopy(vdpy.dpy_renderer, vdpy.dpy_texture, NULL, NULL);
	SDL_RenderPresent(vdpy.dpy_renderer);

	/* update the rendering time */
	clock_gettime(CLOCK_MONOTONIC, &vdpy.last_time);
}

static void
vdpy_sdl_ui_refresh(void *data)
{
	struct display *ui_vdpy;
	struct timespec cur_time;
	uint64_t elapsed_time;

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
