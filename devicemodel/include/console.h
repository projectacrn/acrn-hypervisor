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

#ifndef _CONSOLE_H_
#define	_CONSOLE_H_

struct gfx_ctx;

typedef void (*fb_render_func_t)(struct gfx_ctx *gc, void *arg);
typedef void (*kbd_event_func_t)(int down, uint32_t keysym, void *arg);
typedef void (*ptr_event_func_t)(uint8_t mask, int x, int y, void *arg);

struct gfx_ctx_image *console_get_image(void);

void	console_kbd_register(kbd_event_func_t event_cb, void *arg, int pri);
void	console_kbd_unregister(void);

void	console_ptr_register(ptr_event_func_t event_cb, void *arg, int pri);
void	console_ptr_unregister(void);

#endif /* _CONSOLE_H_ */
