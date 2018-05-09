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

/* acrn-dm monitor APIS */

#ifndef MONITOR_H
#define MONITOR_H

#include "monitor_msg.h"

int monitor_init(struct vmctx *ctx);
void monitor_close(void);

/**
 * monitor broadcast()
 * Developer can use monitor_broadcast() inside acrn-dm, send vmm_msg to all client.
 * @arguements:
 * @msg: any valid vmm_msg data structure, which you want send
 */

int monitor_broadcast(struct vmm_msg *msg);

/* msg_sender will be seen/modify by msg handler */
struct msg_sender {
	int fd;			/* msg handler can replay to this fd */
	char name[CLIENT_NAME_LEN];	/* client have a chance to name itsself */
	int broadcast;
};

/**
 * monitor_register_handler()
 * Developer can add vmm_msg handler inside acrn-dm, that means, when a client send a
 * vmm_msg to acrn-dm, the correspoding callback will be called
 * @arguements:
 * @msg: msg->msgid must be set, for which handler will be add.
 * @callback: when a received message match msg->msgid, callback will be envoked.
 *   And these data are pass in to help developer: (a)msg, the received message, from
 *   socket. (b)sender, tell you who send this message, anything wite to sender->fd
 *   will be able to read out by client socket. Developer should only write valid
 *   vmm_msg. (c)priv, that is what you pass to monitor_add_msg_handler();
 * @priv, callback will see this value.
*/

int monitor_register_handler(struct vmm_msg *msg,
			     void (*callback) (struct vmm_msg * msg,
					       struct msg_sender * sender,
					       void *priv), void *priv);
#endif
