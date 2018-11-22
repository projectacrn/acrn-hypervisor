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

#ifndef	_MEVENT_H_
#define	_MEVENT_H_

enum ev_type {
	EVF_READ,
	EVF_WRITE,
	EVF_READ_ET,
	EVF_WRITE_ET,
	EVF_TIMER,		/* Not supported yet */
	EVF_SIGNAL		/* Not supported yet */
};

struct mevent;

struct mevent *mevent_add(int fd, enum ev_type type,
			  void (*run)(int, enum ev_type, void *), void *param,
			  void (*teardown)(void *), void *teardown_param);
int	mevent_enable(struct mevent *evp);
int	mevent_disable(struct mevent *evp);
int	mevent_delete(struct mevent *evp);
int	mevent_delete_close(struct mevent *evp);
int	mevent_notify(void);

void	mevent_dispatch(void);
int	mevent_init(void);
void	mevent_deinit(void);

#define list_foreach_safe(var, head, field, tvar)	\
for ((var) = LIST_FIRST((head));			\
	(var) && ((tvar) = LIST_NEXT((var), field), 1);\
	(var) = (tvar))

#endif	/* _MEVENT_H_ */
