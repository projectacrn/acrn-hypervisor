/*
 * Project Acrn
 * Acrn-dm-monitor
 *
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Author: TaoYuhong <yuhong.tao@intel.com>
 */

/* this file will be shared, by anyone who want talk to acrn-dm */
#ifndef MONITOR_MSG_H
#define MONITOR_MSG_H

enum msgid {
	REQ_STARTALL = 0,	/* SOS lifecycle service -> VM Mngr */
	REQ_PAUSEALL,		/* SOS lifecycle service -> VM Mngr */
	REQ_START,		/* VM Mngr -> ACRN-DM(vm) */
	REQ_STOP,		/* VM Mngr -> ACRN-DM(vm) */
	REQ_PAUSE,
	REQ_RESUME,		/* VM Mngr -> ACRN-DM(vm) */
	REQ_RESET,
	REQ_QUERY,
	NTF_ALLSTOPPED,		/* VM Mngr -> SOS lifecycle service */
	NTF_ALLPAUSED,		/* VM Mngr -> SOS lifecycle service */
	NTF_STARTED,		/* ACRN-DM -> VM Mngr */
	NTF_STOPPED,		/* ACRN-DM -> VM Mngr */
	NTF_PAUSED,		/* ACRN-DM -> VM Mngr */
	NTF_RESUMED,		/* ACRN-DM -> VM Mngr */

	MSG_STR,
	MSG_HANDSHAKE,		/* handshake */

	MSGID_MAX
};

#define VMM_MSG_MAGIC	0x67736d206d6d76	/* that is char[8] "vmm msg", on X86 */
#define VMM_MSG_MAX_LEN		4096

struct vmm_msg {
	unsigned long long magic;	/* Make sure you get a vmm_msg */
	unsigned int msgid;
	unsigned long timestamp;
	size_t len;		/* vmm_msg + payload size */
	char payload[0];
};

/* practical messages, and helpers
 *  each message defined in msgid should have its own data structure,
 *  and shared by acrn-dm and vm-mngr. So that such that message data 
 *  structure can be recognized by both sides of the communication
 */

/* For test, generate a message who carry a string
 * eg., VMM_MSG_STR(hello_msg, "Hello\n") will create hello_msg,
 * then you can write(sock_fd, hello_msg, sizeof(hello_msg))
 */
#define VMM_MSG_STR(var, str)	\
struct vmm_msg_##var {		\
struct vmm_msg vmsg;		\
        char raw[sizeof(str)];		\
} var = {				\
        .vmsg = {			\
                .magic = VMM_MSG_MAGIC,	\
                .msgid = MSG_STR,	\
                .len = sizeof(struct vmm_msg_##var),	\
        },						\
        .raw = str,				\
}

#define CLIENT_NAME_LEN	16
struct vmm_msg_handshake {
	struct vmm_msg vmsg;
	char name[CLIENT_NAME_LEN];	/* name should be a string, end with '\0' */
	/* can be "acrnd", "vm-mngr" or "acrnctl" */
	int broadcast;		/* if set, allow acrn-dm send broadcast */
	/*   message to such client */
};

#endif
