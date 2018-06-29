/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#ifndef	_DM_H_
#define	_DM_H_

#define	VMEXIT_CONTINUE		(0)
#define	VMEXIT_ABORT		(-1)
#include <stdbool.h>
#include "types.h"
#include "vmm.h"

struct vmctx;
extern int guest_ncpus;
extern char *guest_uuid_str;
extern uint8_t trusty_enabled;
extern char *vsbl_file_name;
extern char *vmname;
extern bool stdio_in_use;

int vmexit_task_switch(struct vmctx *ctx, struct vhm_request *vhm_req,
		       int *vcpu);
void *paddr_guest2host(struct vmctx *ctx, uintptr_t addr, size_t len);
void *dm_gpa2hva(uint64_t gpa, size_t size);
int  virtio_uses_msix(void);
void ptdev_prefer_msi(bool enable);
void ptdev_no_reset(bool enable);
#endif
