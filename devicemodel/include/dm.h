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

#include <stdbool.h>
#include "types.h"
#include "vmm.h"
#include "dm_string.h"

#define MAX_VMNAME_LEN	128U

struct vmctx;
extern char *guest_uuid_str;
extern uint8_t trusty_enabled;
extern char *vsbl_file_name;
extern char *ovmf_file_name;
extern char *kernel_file_name;
extern char *elf_file_name;
extern char *vmname;
extern bool stdio_in_use;
extern char *mac_seed;
extern bool lapic_pt;
extern bool is_rtvm;
extern bool pt_tpm2;
extern bool is_winvm;

int vmexit_task_switch(struct vmctx *ctx, struct vhm_request *vhm_req,
		       int *vcpu);

/**
 * @brief Convert guest physical address to host virtual address
 *
 * @param ctx Pointer to to struct vmctx representing VM context.
 * @param gaddr Guest physical address base.
 * @param len Guest physical address length.
 *
 * @return NULL on convert failed and host virtual address on successful.
 */
void *paddr_guest2host(struct vmctx *ctx, uintptr_t gaddr, size_t len);
int  virtio_uses_msix(void);
size_t high_bios_size(void);
void init_debugexit(void);
void deinit_debugexit(void);
#endif
