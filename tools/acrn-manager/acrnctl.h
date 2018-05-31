/**
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _ACRNCTL_H_
#define _ACRNCTL_H_

#include <sys/queue.h>

#define ACRNCTL_OPT_ROOT        "/opt/acrn/conf"
#define ACRN_DM_SOCK_ROOT       "/run/acrn/mngr"

#define MAX_NAME_LEN            (16)

enum vm_state {
	VM_STATE_UNKNOWN = 0,
	VM_CREATED,		/* VM created / awaiting start (boot) */
	VM_STARTED,		/* VM started (booted) */
	VM_PAUSED,		/* VM paused */
	VM_UNTRACKED,		/* VM not created by acrnctl, or its launch script can change vm name */
};

static const char *state_str[] = {
	[VM_STATE_UNKNOWN] = "unknown",
	[VM_CREATED] = "stopped",
	[VM_STARTED] = "started",
	[VM_PAUSED] = "paused",
	[VM_UNTRACKED] = "untracked",
};

/**
 * @brief search all vm and store it in vmmngr_head
 */
void get_vm_list(void);

/**
 * @brief free all vmmngr_struct allocated by get_vm_list
 */
void put_vm_list(void);

/**
 * @brief search vm indentified by vm from vmmngr_head
 *
 * @return vmmngr_struct * if find, NULL not find
 */
struct vmmngr_struct *vmmngr_find(char *vmname);

/* Per-vm vm managerment struct */
struct vmmngr_struct {
	char name[MAX_NAME_LEN];
	unsigned long state;
	LIST_ENTRY(vmmngr_struct) list;
};

int shell_cmd(const char *cmd, char *outbuf, int len);

/* vm life cycle ops */
int list_vm(void);
int stop_vm(char *vmname);
int start_vm(char *vmname);
int pause_vm(char *vmname);
int continue_vm(char *vmname);
int suspend_vm(char *vmname);
int resume_vm(char *vmname);

#endif				/* _ACRNCTL_H_ */
