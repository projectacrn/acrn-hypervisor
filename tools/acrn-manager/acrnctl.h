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

extern const char *state_str[];

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
	unsigned long update;   /* update count, remove a vm if no update for it */
	LIST_ENTRY(vmmngr_struct) list;
};

int shell_cmd(const char *cmd, char *outbuf, int len);

/* update names and states of VMs in SOS
 * before you stop, start, pause, resume, suspend continue a VM
 * use a name, it is better to run vmmngr_update() first
 * and use vmngr_find() to check is this VM is still available
 */
void vmmngr_update(void);

extern LIST_HEAD(vmmngr_list_struct, vmmngr_struct) vmmngr_head;

/* vm life cycle ops */
int list_vm(void);
int stop_vm(const char *vmname);
int start_vm(const char *vmname);
int pause_vm(const char *vmname);
int continue_vm(const char *vmname);
int suspend_vm(const char *vmname);
int resume_vm(const char *vmname, unsigned reason);

#endif				/* _ACRNCTL_H_ */
