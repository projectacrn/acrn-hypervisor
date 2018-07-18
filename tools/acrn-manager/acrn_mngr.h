/*
 * Copyright (C)2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ACRN_MANAGER_H
#define ACRN_MANAGER_H

#ifdef MNGR_DEBUG
#define pdebug()        fprintf(stderr, "%s %d\n", __FUNCTION__, __LINE__)
#else
#define pdebug()        while(0){}
#endif

#include <stdlib.h>

/* Basic message format */

#define MNGR_MSG_MAGIC   0x67736d206d6d76	/* that is char[8] "mngr msg", on X86 */
#define VMNAME_LEN	16

struct mngr_msg {
	unsigned long long magic;	/* Make sure you get a mngr_msg */
	unsigned int msgid;
	unsigned long timestamp;
	size_t len;		/* mngr_msg + payload size */
	char payload[0];
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
	DM_MAX,
};

/* DM handled message req/ack pairs */

struct req_dm_stop {
	struct mngr_msg msg;	/* req DM_STOP */
};

struct ack_dm_stop {
	struct mngr_msg msg;	/* ack DM_STOP */
	int err;
};

struct req_dm_suspend {
	struct mngr_msg msg;	/* req DM_SUSPEND */
};

struct ack_dm_suspend {
	struct mngr_msg msg;	/* ack DM_SUSPEND */
	int err;
};

struct req_dm_resume {
	struct mngr_msg msg;	/* req DM_RESUME */
	int reason;
};

struct ack_dm_resume {
	struct mngr_msg msg;	/* ack DM_RESUME */
	int err;
};

struct req_dm_pause {
	struct mngr_msg msg;	/* req DM_PAUSE */
};

struct ack_dm_pause {
	struct mngr_msg msg;	/* ack DM_PAUSE */
	int err;
};

struct req_dm_continue {
	struct mngr_msg msg;	/* req DM_CONTINUE */
};

struct ack_dm_continue {
	struct mngr_msg msg;	/* ack DM_CONTINUE */
	int err;
};

struct req_dm_query {
	struct mngr_msg msg;	/* req DM_QUERY */
};

struct ack_dm_query {
	struct mngr_msg msg;	/* ack DM_QUERY */
	int state;
};

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

struct req_acrnd_timer {
	struct mngr_msg msg;	/* req ACRND_TIMER */
	char name[VMNAME_LEN];
	time_t t;
};

struct ack_acrnd_timer {
	struct mngr_msg msg;	/* ack ACRND_TIMER */
	int err;
};

struct req_acrnd_reason {
	struct mngr_msg msg;	/* req ACRND_REASON */
};

struct ack_acrnd_reason {
	struct mngr_msg msg;	/* ack ACRND_REASON */
	int reason;
};

struct req_dm_notify {
	struct mngr_msg msg;	/* req DM_NOTIFY */
	int state;
};

struct ack_dm_notify {
	struct mngr_msg msg;	/* ack DM_NOTIFY */
	int err;
};

struct req_acrnd_stop {
	struct mngr_msg msg;	/* req ACRND_STOP */
	int force;
	unsigned timeout;
};

struct ack_acrnd_stop {
	struct mngr_msg msg;	/* ack ACRND_STOP */
	int err;
};

struct req_acrnd_suspend {
	struct mngr_msg msg;	/* req ACRND_SUSPEND */
	int force;
	unsigned timeout;
};

struct ack_acrnd_suspend {
	struct mngr_msg msg;	/* ack ACRND_SUSPEND */
	int err;
};

struct req_acrnd_resume {
	struct mngr_msg msg;	/* req ACRND_RESUME */
};

struct ack_acrnd_resume {
	struct mngr_msg msg;	/* ack ACRND_RESUME */
	int err;
};

/* SOS-LCS handled message event types */
enum sos_lcs_msgid {
	WAKEUP_REASON = ACRND_MAX + 1,	/* Acrnd/Acrnctl request wakeup reason */
	RTC_TIMER,		/* Acrnd request to setup RTC timer */
	SUSPEND,
	SHUTDOWN,
	REBOOT,
};

/* SOS-LCS handled message req/ack pairs */

struct req_wakeup_reason {
	struct mngr_msg msg;
};

struct ack_wakeup_reason {
	struct mngr_msg msg;
	int reason;
};

struct req_rtc_timer {
	struct mngr_msg msg;
	char vmname[VMNAME_LEN];
	time_t t;
};

struct ack_rtc_timer {
	struct mngr_msg msg;
	int err;
};

struct req_power_state {
	struct mngr_msg msg;
};

struct ack_power_state {
	struct mngr_msg msg;
	int err;
};

/* helper functions */
#define MNGR_SERVER	1	/* create a server fd, which you can add handlers onto it */
#define MNGR_CLIENT	0	/* create a client, just send req and read ack */

/**
 * @brief create a descripter for vm management IPC
 *
 * @param name: refer to a sock file under /run/acrn/mngr/[name].[pid].socket
 * @param flags: MNGR_SERVER to create a server, MNGR_CLIENT to create a client
 *
 * @return descripter ID (> 1024) on success, errno (< 0) on error.
 */
int mngr_open_un(const char *name, int flags);

/**
 * @brief close descripter and release the resouces
 *
 * @param desc: descripter to be closed
 */
void mngr_close(int desc);

/**
 * @brief add a handler for message specified by msg
 *
 * @param desc: descripter to register handler to
 * @param id: id of message to handle
 * @param cb: handler callback
 * @param param: param for the callback
 * @return 0 on success, errno on error
 */
int mngr_add_handler(int desc, unsigned id,
		     void (*cb) (struct mngr_msg * msg, int client_fd,
				 void *param), void *param);

/**
 * @brief send a message and wait for ack
 *
 * @param desc: descripter created using mngr_open_un
 * @param req: pointer to message to send
 * @param ack: pointer to ack struct, NULL if no ack required
 * @param ack_len: size in byte of the expected ack message
 * @param timeout: time to wait for ack, zero to blocking waiting
 * @return len of ack messsage (0 if ack is NULL) on succes, errno on error
 */
int mngr_send_msg(int desc, struct mngr_msg *req, struct mngr_msg *ack,
		  size_t ack_len, unsigned timeout);

#endif				/* ACRN_MANAGER_H */
