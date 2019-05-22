/*-
 * Copyright (c) 2014 Neel Natu (neel@freebsd.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _RTC_H_
#define	_RTC_H_

#include "types.h"

#define IO_RTC	0x070	/* RTC */

#define	RTC_LMEM_LSB	0x34
#define	RTC_LMEM_MSB	0x35
#define	RTC_HMEM_LSB	0x5b
#define	RTC_HMEM_SB	0x5c
#define	RTC_HMEM_MSB	0x5d

struct vrtc;
struct vmctx;

int vrtc_init(struct vmctx *ctx);
void vrtc_enable_localtime(int l_time);
void vrtc_deinit(struct vmctx *ctx);
void vrtc_reset(struct vrtc *vrtc);
int vrtc_set_time(struct vrtc *vrtc, time_t secs);
int vrtc_nvram_write(struct vrtc *vrtc, int offset, uint8_t value);
int vrtc_addr_handler(struct vmctx *ctx, int vcpu, int in, int port,
		      int bytes, uint32_t *eax, void *arg);
int vrtc_data_handler(struct vmctx *ctx, int vcpu, int in, int port,
		      int bytes, uint32_t *eax, void *arg);

#endif
