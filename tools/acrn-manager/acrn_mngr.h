/*
 * Copyright (C)2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ACRN_MANAGER_H
#define ACRN_MANAGER_H

#include <stdlib.h>
#include <acrn_common.h>

#define MNGR_MSG_MAGIC	0x67736d206d6d76	/* that is char[8] "mngr msg", on X86 */
#define PATH_LEN		128

#define ACRN_CONF_PATH			"/usr/share/acrn/conf"
#define ACRN_CONF_PATH_ADD		ACRN_CONF_PATH "/add"
#define ACRN_CONF_TIMER_LIST	ACRN_CONF_PATH "/timer_list"

#define ACRN_DM_BASE_PATH	"/run/acrn"
#define ACRN_DM_SOCK_PATH	"/run/acrn/mngr"

/* TODO: Revisit PARAM_LEN and see if size can be reduced */
#define PARAM_LEN	256

struct mngr_msg {
	unsigned long long magic;	/* Make sure you get a mngr_msg */
	unsigned int msgid;
	unsigned long timestamp;
	union {

		/* Arguments to rescan virtio-blk device */
		char devargs[PARAM_LEN];

		/* ack of DM_STOP, DM_SUSPEND, DM_RESUME, DM_PAUSE, DM_CONTINUE,
		   ACRND_TIMER, ACRND_STOP, ACRND_RESUME, RTC_TIMER */
		int err;

		/* ack of WAKEUP_REASON */
		unsigned reason;

		/* ack of DM_QUERY */
		int state;

		/* req of ACRND_TIMER */
		struct req_acrnd_timer {
			char name[MAX_VM_OS_NAME_LEN];
			time_t t;
		} acrnd_timer;

		/* req of ACRND_STOP */
		struct req_acrnd_stop {
			int force;
			unsigned timeout;
		} acrnd_stop;

		/* req of ACRND_SUSPEND */
		struct req_acrnd_suspend {
			int force;
			unsigned timeout;
		} acrnd_suspend;

		/* req of ACRND_RESUME */
		struct req_acrnd_resume {
			int force;
			unsigned timeout;
		} acrnd_resume;

		/* req of RTC_TIMER */
		struct req_rtc_timer {
			char vmname[MAX_VM_OS_NAME_LEN];
			time_t t;
		} rtc_timer;

	} data;
};

/* mngr_msg event types */
enum msgid {
	MSG_MIN = 0,
	MSG_STR,		/* The message payload is a string, terminated with '\0' */
	MSG_MAX,
};

/* DM handled message event types */
enum dm_msgid {
	DM_STOP = MSG_MAX + 1,	/* Stop this UOS */
	DM_SUSPEND,		/* Suspend this UOS from running state */
	DM_RESUME,		/* Resume this UOS from suspend state */
	DM_PAUSE,		/* Freeze this virtual machine */
	DM_CONTINUE,		/* Unfreeze this virtual machine */
	DM_QUERY,		/* Ask power state of this UOS */
	DM_BLKRESCAN,		/* Rescan virtio-blk device for any changes in UOS */
	DM_MAX,
};

/* DM handled message req/ack pairs */

/* Acrnd handled message event types */
enum acrnd_msgid {
	/* DM -> Acrnd */
	ACRND_TIMER = DM_MAX + 1,	/* DM request to setup a launch timer */
	ACRND_REASON,		/* DM ask for updating wakeup reason */
	DM_NOTIFY,		/* DM notify Acrnd that state is changed */

	/* SOS-LCS ->Acrnd */
	ACRND_STOP,		/* SOS-LCS request to Stop all UOS */
	ACRND_RESUME,		/* SOS-LCS request to Resume UOS */
	ACRND_SUSPEND,		/* SOS-LCS request to Suspend all UOS */

	ACRND_MAX,
};

/* Acrnd handled message req/ack pairs */

/* SOS-LCS handled message event types */
enum sos_lcs_msgid {
	WAKEUP_REASON = ACRND_MAX + 1,	/* Acrnd/Acrnctl request wakeup reason */
	RTC_TIMER,		/* Acrnd request to setup RTC timer */
	SUSPEND,
	SHUTDOWN,
	REBOOT,
};

/* helper functions */
#define MNGR_SERVER	1	/* create a server fd, which you can add handlers onto it */
#define MNGR_CLIENT	0	/* create a client, just send req and read ack */

#define CHK_CREAT 1		/* create a directory, if not exist */
#define CHK_ONLY  0		/* check if the directory exist only */

/**
 * @brief create a descripter for vm management IPC
 *
 * @param name refer to a sock file under /run/acrn/mngr/[name].[pid].socket
 * @param flags MNGR_SERVER to create a server, MNGR_CLIENT to create a client
 *
 * @return descripter ID (> 1024) on success, errno (< 0) on error.
 */
int mngr_open_un(const char *name, int flags);

/**
 * @brief close descripter and release the resouces
 *
 * @param desc descripter to be closed
 */
void mngr_close(int desc);

/**
 * @brief add a handler for message specified by msg
 *
 * @param desc descripter to register handler to
 * @param id id of message to handle
 * @param cb handler callback
 * @param param param for the callback
 * @return 0 on success, errno on error
 */
int mngr_add_handler(int desc, unsigned id,
		     void (*cb) (struct mngr_msg * msg, int client_fd,
				 void *param), void *param);

/**
 * @brief send a message and wait for ack
 *
 * @param desc descripter created using mngr_open_un
 * @param req pointer to message to send
 * @param ack pointer to ack struct, NULL if no ack required
 * @param timeout time to wait for ack, zero to blocking waiting
 * @return len of ack messsage (0 if ack is NULL) on succes, errno on error
 */
int mngr_send_msg(int desc, struct mngr_msg *req, struct mngr_msg *ack,
		  unsigned timeout);

/**
 * @brief check @path existence and create or report error accoding to the flag
 *
 * @param path path to check
 * @param flags CHK_CREAT to create directory, CHK_ONLY check directory only
 * @return 0 on success, -1 on error
 */
int check_dir(const char *path, int flags);

#endif				/* ACRN_MANAGER_H */
