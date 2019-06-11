/*-
 * Copyright (c) 2016 iXsystems Inc.
 * All rights reserved.
 *
 * This software was developed by Jakub Klama <jceel@FreeBSD.org>
 * under sponsorship from iXsystems Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <termios.h>
#include <limits.h>

#include "dm.h"
#include "pci_core.h"
#include "virtio.h"
#include "mevent.h"

#define	VIRTIO_CONSOLE_RINGSZ	64
#define	VIRTIO_CONSOLE_MAXPORTS	16
#define	VIRTIO_CONSOLE_MAXQ	(VIRTIO_CONSOLE_MAXPORTS * 2 + 2)

#define	VIRTIO_CONSOLE_DEVICE_READY	0
#define	VIRTIO_CONSOLE_DEVICE_ADD	1
#define	VIRTIO_CONSOLE_DEVICE_REMOVE	2
#define	VIRTIO_CONSOLE_PORT_READY	3
#define	VIRTIO_CONSOLE_CONSOLE_PORT	4
#define	VIRTIO_CONSOLE_CONSOLE_RESIZE	5
#define	VIRTIO_CONSOLE_PORT_OPEN	6
#define	VIRTIO_CONSOLE_PORT_NAME	7

#define	VIRTIO_CONSOLE_F_SIZE		0
#define	VIRTIO_CONSOLE_F_MULTIPORT	1
#define	VIRTIO_CONSOLE_F_EMERG_WRITE	2
#define	VIRTIO_CONSOLE_S_HOSTCAPS	\
	(VIRTIO_CONSOLE_F_SIZE |	\
	VIRTIO_CONSOLE_F_MULTIPORT |	\
	VIRTIO_CONSOLE_F_EMERG_WRITE)

static int virtio_console_debug;
#define DPRINTF(params) do {		\
	if (virtio_console_debug)	\
		printf params;		\
} while (0)
#define WPRINTF(params) (printf params)

struct virtio_console;
struct virtio_console_port;
struct virtio_console_config;
typedef void (virtio_console_cb_t)(struct virtio_console_port *, void *,
				   struct iovec *, int);

enum virtio_console_be_type {
	VIRTIO_CONSOLE_BE_STDIO = 0,
	VIRTIO_CONSOLE_BE_TTY,
	VIRTIO_CONSOLE_BE_PTY,
	VIRTIO_CONSOLE_BE_FILE,
	VIRTIO_CONSOLE_BE_SOCKET,
	VIRTIO_CONSOLE_BE_MAX,
	VIRTIO_CONSOLE_BE_INVALID = VIRTIO_CONSOLE_BE_MAX
};

struct virtio_console_port {
	struct virtio_console	*console;
	int			id;
	const char		*name;
	bool			enabled;
	bool			is_console;
	bool			rx_ready;
	bool			open;
	int			rxq;
	int			txq;
	void			*arg;
	virtio_console_cb_t	*cb;
};

struct virtio_console_backend {
	struct virtio_console_port	*port;
	struct mevent			*evp;
	int				fd;
	bool				open;
	enum virtio_console_be_type	be_type;
	int				pts_fd;	/* only valid for PTY */
	const char 			*portpath;
	const char 			*socket_type;
};

struct virtio_console {
	struct virtio_base		base;
	struct virtio_vq_info		queues[VIRTIO_CONSOLE_MAXQ];
	pthread_mutex_t			mtx;
	uint64_t			cfg;
	uint64_t			features;
	int				nports;
	bool				ready;
	struct virtio_console_port	control_port;
	struct virtio_console_port	ports[VIRTIO_CONSOLE_MAXPORTS];
	struct virtio_console_config	*config;
	int				ref_count;
};

struct virtio_console_config {
	uint16_t cols;
	uint16_t rows;
	uint32_t max_nr_ports;
	uint32_t emerg_wr;
} __attribute__((packed));

struct virtio_console_control {
	uint32_t id;
	uint16_t event;
	uint16_t value;
} __attribute__((packed));

struct virtio_console_console_resize {
	uint16_t cols;
	uint16_t rows;
} __attribute__((packed));

static void virtio_console_reset(void *);
static void virtio_console_notify_rx(void *, struct virtio_vq_info *);
static void virtio_console_notify_tx(void *, struct virtio_vq_info *);
static int virtio_console_cfgread(void *, int, int, uint32_t *);
static void virtio_console_neg_features(void *, uint64_t);
static void virtio_console_control_send(struct virtio_console *,
	struct virtio_console_control *, const void *, size_t);
static void virtio_console_announce_port(struct virtio_console_port *);
static void virtio_console_open_port(struct virtio_console_port *, bool);
static void virtio_console_teardown_backend(void *);

static struct virtio_ops virtio_console_ops = {
	"vtcon",			/* our name */
	VIRTIO_CONSOLE_MAXQ,		/* we support VTCON_MAXQ virtqueues */
	sizeof(struct virtio_console_config), /* config reg size */
	virtio_console_reset,		/* reset */
	NULL,				/* device-wide qnotify */
	virtio_console_cfgread,		/* read virtio config */
	NULL,				/* write virtio config */
	virtio_console_neg_features,	/* apply negotiated features */
	NULL,				/* called on guest set status */
};

static const char *virtio_console_be_table[VIRTIO_CONSOLE_BE_MAX] = {
	[VIRTIO_CONSOLE_BE_STDIO]	= "stdio",
	[VIRTIO_CONSOLE_BE_TTY]		= "tty",
	[VIRTIO_CONSOLE_BE_PTY]		= "pty",
	[VIRTIO_CONSOLE_BE_FILE]	= "file",
	[VIRTIO_CONSOLE_BE_SOCKET]	= "socket"
};

static struct termios virtio_console_saved_tio;
static int virtio_console_saved_flags;

static void
virtio_console_reset(void *vdev)
{
	struct virtio_console *console;

	console = vdev;

	DPRINTF(("vtcon: device reset requested!\n"));
	virtio_reset_dev(&console->base);
}

static void
virtio_console_neg_features(void *vdev, uint64_t negotiated_features)
{
	struct virtio_console *console = vdev;

	console->features = negotiated_features;
}

static int
virtio_console_cfgread(void *vdev, int offset, int size, uint32_t *retval)
{
	struct virtio_console *console = vdev;
	void *ptr;

	ptr = (uint8_t *)console->config + offset;
	memcpy(retval, ptr, size);
	return 0;
}

static inline struct virtio_console_port *
virtio_console_vq_to_port(struct virtio_console *console,
			  struct virtio_vq_info *vq)
{
	uint16_t num = vq->num;

	if (num == 0 || num == 1)
		return &console->ports[0];

	if (num == 2 || num == 3)
		return &console->control_port;

	return &console->ports[(num / 2) - 1];
}

static inline struct virtio_vq_info *
virtio_console_port_to_vq(struct virtio_console_port *port, bool tx_queue)
{
	int qnum;

	qnum = tx_queue ? port->txq : port->rxq;
	return &port->console->queues[qnum];
}

static struct virtio_console_port *
virtio_console_add_port(struct virtio_console *console, const char *name,
			virtio_console_cb_t *cb, void *arg, bool is_console)
{
	struct virtio_console_port *port;

	if (console->nports == VIRTIO_CONSOLE_MAXPORTS) {
		errno = EBUSY;
		return NULL;
	}

	port = &console->ports[console->nports++];
	port->id = console->nports - 1;
	port->console = console;
	port->name = name;
	port->cb = cb;
	port->arg = arg;
	port->is_console = is_console;

	if (port->id == 0) {
		/* port0 */
		port->txq = 0;
		port->rxq = 1;
	} else {
		port->txq = console->nports * 2;
		port->rxq = port->txq + 1;
	}

	port->enabled = true;
	return port;
}

static void
virtio_console_control_tx(struct virtio_console_port *port, void *arg,
			  struct iovec *iov, int niov)
{
	struct virtio_console *console;
	struct virtio_console_port *tmp;
	struct virtio_console_control resp, *ctrl;
	int i;

	assert(niov == 1);

	console = port->console;
	ctrl = (struct virtio_console_control *)iov->iov_base;

	switch (ctrl->event) {
	case VIRTIO_CONSOLE_DEVICE_READY:
		console->ready = true;
		/* set port ready events for registered ports */
		for (i = 0; i < VIRTIO_CONSOLE_MAXPORTS; i++) {
			tmp = &console->ports[i];
			if (tmp->enabled)
				virtio_console_announce_port(tmp);

			if (tmp->open)
				virtio_console_open_port(tmp, true);
		}
		break;

	case VIRTIO_CONSOLE_PORT_READY:
		if (ctrl->id >= console->nports) {
			WPRINTF(("VTCONSOLE_PORT_READY for unknown port %d\n",
			    ctrl->id));
			return;
		}

		tmp = &console->ports[ctrl->id];
		if (tmp->is_console) {
			resp.event = VIRTIO_CONSOLE_CONSOLE_PORT;
			resp.id = ctrl->id;
			resp.value = 1;
			virtio_console_control_send(console, &resp, NULL, 0);
		}
		break;
	}
}

static void
virtio_console_announce_port(struct virtio_console_port *port)
{
	struct virtio_console_control event;

	event.id = port->id;
	event.event = VIRTIO_CONSOLE_DEVICE_ADD;
	event.value = 1;
	virtio_console_control_send(port->console, &event, NULL, 0);

	event.event = VIRTIO_CONSOLE_PORT_NAME;
	virtio_console_control_send(port->console, &event, port->name,
	    strnlen(port->name, NAME_MAX));
}

static void
virtio_console_open_port(struct virtio_console_port *port, bool open)
{
	struct virtio_console_control event;

	if (!port->console->ready) {
		port->open = true;
		return;
	}

	event.id = port->id;
	event.event = VIRTIO_CONSOLE_PORT_OPEN;
	event.value = (int)open;
	virtio_console_control_send(port->console, &event, NULL, 0);
}

static void
virtio_console_control_send(struct virtio_console *console,
			    struct virtio_console_control *ctrl,
			    const void *payload, size_t len)
{
	struct virtio_vq_info *vq;
	struct iovec iov;
	uint16_t idx;
	int n;

	vq = virtio_console_port_to_vq(&console->control_port, true);

	if (!vq_has_descs(vq))
		return;

	n = vq_getchain(vq, &idx, &iov, 1, NULL);

	assert(n == 1);

	memcpy(iov.iov_base, ctrl, sizeof(struct virtio_console_control));
	if (payload != NULL && len > 0)
		memcpy(iov.iov_base + sizeof(struct virtio_console_control),
		     payload, len);

	vq_relchain(vq, idx, sizeof(struct virtio_console_control) + len);
	vq_endchains(vq, 1);
}

static void
virtio_console_notify_tx(void *vdev, struct virtio_vq_info *vq)
{
	struct virtio_console *console;
	struct virtio_console_port *port;
	struct iovec iov[1];
	uint16_t idx;
	uint16_t flags[8];

	console = vdev;
	port = virtio_console_vq_to_port(console, vq);

	while (vq_has_descs(vq)) {
		vq_getchain(vq, &idx, iov, 1, flags);
		if (port != NULL)
			port->cb(port, port->arg, iov, 1);

		/*
		 * Release this chain and handle more
		 */
		vq_relchain(vq, idx, 0);
	}
	vq_endchains(vq, 1);	/* Generate interrupt if appropriate. */
}

static void
virtio_console_notify_rx(void *vdev, struct virtio_vq_info *vq)
{
	struct virtio_console *console;
	struct virtio_console_port *port;

	console = vdev;
	port = virtio_console_vq_to_port(console, vq);

	if (!port->rx_ready) {
		port->rx_ready = 1;
		vq->used->flags |= VRING_USED_F_NO_NOTIFY;
	}
}

static void
virtio_console_reset_backend(struct virtio_console_backend *be)
{
	if (!be)
		return;

	if (be->evp)
		mevent_disable(be->evp);
	if (be->fd != STDIN_FILENO)
		close(be->fd);
	be->fd = -1;
	be->open = false;
}

static void
virtio_console_backend_read(int fd __attribute__((unused)),
			    enum ev_type t __attribute__((unused)),
			    void *arg)
{
	struct virtio_console_port *port;
	struct virtio_console_backend *be = arg;
	struct virtio_vq_info *vq;
	struct iovec iov;
	static char dummybuf[2048];
	int len, n;
	uint16_t idx;

	port = be->port;
	vq = virtio_console_port_to_vq(port, true);

	if (!be->open || !port->rx_ready) {
		len = read(be->fd, dummybuf, sizeof(dummybuf));
		if (len == 0)
			goto close;
		return;
	}

	if (!vq_has_descs(vq)) {
		len = read(be->fd, dummybuf, sizeof(dummybuf));
		vq_endchains(vq, 1);
		if (len == 0)
			goto close;
		return;
	}

	do {
		n = vq_getchain(vq, &idx, &iov, 1, NULL);
		len = readv(be->fd, &iov, n);
		if (len <= 0) {
			vq_retchain(vq);
			vq_endchains(vq, 0);

			/* no data available */
			if (len == -1 && errno == EAGAIN)
				return;

			/* any other errors */
			goto close;
		}

		vq_relchain(vq, idx, len);
	} while (vq_has_descs(vq));

	vq_endchains(vq, 1);

close:
	virtio_console_reset_backend(be);
	WPRINTF(("vtcon: be read failed and close! len = %d, errno = %d\n",
		len, errno));
}

static void
virtio_console_backend_write(struct virtio_console_port *port, void *arg,
			     struct iovec *iov, int niov)
{
	struct virtio_console_backend *be;
	int ret;

	be = arg;

	if (be->fd == -1)
		return;

	ret = writev(be->fd, iov, niov);
	if (ret <= 0) {
		/* Case 1:backend cannot receive more data. For example when pts is
		 * not connected to any client, its tty buffer will become full.
		 * In this case we just drop data from guest hvc console.
		 *
		 * Case 2: Backend connection not yet setup. For example, when
		 * virtio-console is used as console port with socket backend, guest
		 * kernel tries to hook it up with hvc console and sets it up. It
		 * doesn't check if a client is connected and can result in ENOTCONN
		 * with virtio-console backend being reset. This will prevent
		 * client connection at a later point. To avoid this, ignore
		 * ENOTCONN error.
		 *
		 * PS: For Kata, the runtime first launches VM and then proxy which
		 * acts as a client connects to this socket.
		 */
		if (ret == -1 && (errno == EAGAIN || errno == ENOTCONN))
			return;

		virtio_console_reset_backend(be);
		WPRINTF(("vtcon: be write failed! errno = %d\n", errno));
	}
}

static void
virtio_console_restore_stdio(void)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &virtio_console_saved_tio);
	fcntl(STDIN_FILENO, F_SETFL, virtio_console_saved_flags);
	stdio_in_use = false;
}

static bool
virtio_console_backend_can_read(enum virtio_console_be_type be_type)
{
	return (be_type == VIRTIO_CONSOLE_BE_FILE) ? false : true;
}

static int
virtio_console_open_backend(const char *path,
			    enum virtio_console_be_type be_type)
{
	int fd = -1;

	switch (be_type) {
	case VIRTIO_CONSOLE_BE_PTY:
		fd = posix_openpt(O_RDWR | O_NOCTTY);
		if (fd == -1)
			WPRINTF(("vtcon: posix_openpt failed, errno = %d\n",
				errno));
		else if (grantpt(fd) == -1 || unlockpt(fd) == -1) {
			WPRINTF(("vtcon: grant/unlock failed, errno = %d\n",
				errno));
			close(fd);
			fd = -1;
		}
		break;
	case VIRTIO_CONSOLE_BE_STDIO:
		if (stdio_in_use) {
			WPRINTF(("vtcon: stdio is used by other device\n"));
			break;
		}
		fd = STDIN_FILENO;
		stdio_in_use = true;
		break;
	case VIRTIO_CONSOLE_BE_TTY:
		fd = open(path, O_RDWR | O_NONBLOCK);
		if (fd < 0)
			WPRINTF(("vtcon: open failed: %s\n", path));
		else if (!isatty(fd)) {
			WPRINTF(("vtcon: not a tty: %s\n", path));
			close(fd);
			fd = -1;
		}
		break;
	case VIRTIO_CONSOLE_BE_FILE:
		fd = open(path, O_WRONLY|O_CREAT|O_APPEND|O_NONBLOCK, 0666);
		if (fd < 0)
			WPRINTF(("vtcon: open failed: %s\n", path));
		break;
	case VIRTIO_CONSOLE_BE_SOCKET:
		fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (fd < 0)
			WPRINTF(("vtcon: socket open failed: %s\n", path));
		break;
	default:
		WPRINTF(("not supported backend %d!\n", be_type));
	}

	return fd;
}

static int
make_socket_non_blocking(int fd)
{
	int flags, s;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		WPRINTF(("fcntl get failed =%d\n", errno));
		return -1;
	}

	flags |= O_NONBLOCK;
	s = fcntl(fd, F_SETFL, flags);
	if (s == -1) {
		WPRINTF(("fcntl set failed =%d\n", errno));
		return -1;
	}

	return 0;
}

static void
virtio_console_accept_new_connection(int fd __attribute__((unused)),
					enum ev_type t __attribute__((unused)), void *arg)
{

	int accepted_fd;
	int close_true;
	uint32_t len;
	struct sockaddr_un addr;
	struct virtio_console_backend *be = arg;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, be->portpath, sizeof(addr.sun_path));
	addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

	len = sizeof(addr);
	accepted_fd = accept(be->fd, (struct sockaddr *)&addr, &len);
	if (accepted_fd == -1) {
		WPRINTF(("accept error= %d, addr.sun_path=%s\n", errno, addr.sun_path));
		return;
	} else {
		/* close the fd associated with listening socket
		 * and reuse it for accepted socket.
		 */
		close_true = 1;
		setsockopt(be->fd, SOL_SOCKET, SO_REUSEADDR, &close_true, sizeof(int));
		close(be->fd);
		be->fd = accepted_fd;
	}

	if (be->evp) {
		/* close the event associated with listening socket
		 * and reuse it for accepted socket.
		 */
		mevent_delete(be->evp);
	}

	be->evp = mevent_add(be->fd, EVF_READ, virtio_console_backend_read, be,
				virtio_console_teardown_backend, be);
	if (be->evp == NULL) {
		WPRINTF(("accepted fd mevent_add failed\n"));
		return;
	}

	if (make_socket_non_blocking(be->fd) == -1) {
		WPRINTF(("accepted fd non-blocking failed\n"));
		return;
	}
}

static int
virtio_console_config_backend(struct virtio_console_backend *be)
{
	int fd, flags;
	char *pts_name = NULL;
	int slave_fd = -1;
	struct termios tio, saved_tio;
	struct sockaddr_un addr;

	if (!be || be->fd == -1)
		return -1;

	fd = be->fd;
	switch (be->be_type) {
	case VIRTIO_CONSOLE_BE_PTY:
		pts_name = ptsname(fd);
		if (pts_name == NULL) {
			WPRINTF(("vtcon: ptsname return NULL, errno = %d\n",
				errno));
			return -1;
		}

		slave_fd = open(pts_name, O_RDWR);
		if (slave_fd == -1) {
			WPRINTF(("vtcon: slave_fd open failed, errno = %d\n",
				errno));
			return -1;
		}

		tcgetattr(slave_fd, &tio);
		cfmakeraw(&tio);
		tcsetattr(slave_fd, TCSAFLUSH, &tio);
		be->pts_fd = slave_fd;

		WPRINTF(("***********************************************\n"));
		WPRINTF(("virt-console backend redirected to %s\n", pts_name));
		WPRINTF(("***********************************************\n"));

		flags = fcntl(fd, F_GETFL);
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
		break;
	case VIRTIO_CONSOLE_BE_TTY:
	case VIRTIO_CONSOLE_BE_STDIO:
		tcgetattr(fd, &tio);
		saved_tio = tio;
		cfmakeraw(&tio);
		tio.c_cflag |= CLOCAL;
		tcsetattr(fd, TCSANOW, &tio);

		if (be->be_type == VIRTIO_CONSOLE_BE_STDIO) {
			flags = fcntl(fd, F_GETFL);
			fcntl(fd, F_SETFL, flags | O_NONBLOCK);

			virtio_console_saved_flags = flags;
			virtio_console_saved_tio = saved_tio;
			atexit(virtio_console_restore_stdio);
		}
		break;
	case VIRTIO_CONSOLE_BE_SOCKET:
		if (be->portpath == NULL) {
			WPRINTF(("vtcon: portpath is NULL\n"));
			return -1;
		}

		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, be->portpath, sizeof(addr.sun_path));
		addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

		if (be->socket_type == NULL || !strcmp(be->socket_type,"server")) {
			unlink(be->portpath);
			if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
				WPRINTF(("Bind Error = %d\n", errno));
				return -1;
			}
			if (listen(fd, 64) == -1) {
				WPRINTF(("Listen Error= %d\n", errno));
				return -1;
			}
			if (make_socket_non_blocking(fd) == -1) {
				WPRINTF(("Backend config: fcntl Error\n"));
				return -1;
			}
			be->evp = mevent_add(fd, EVF_READ, virtio_console_accept_new_connection, be, NULL, NULL);
			if (be->evp == NULL) {
				WPRINTF(("Socket Accept mevent_add failed\n"));
				return -1;
			}
		} else if (!strcmp(be->socket_type,"client")) {
			if (access(be->portpath,0)) {
				WPRINTF(("%s not exist\n", be->portpath));
				return -1;
			}
			/*
			 * When the VM reset, client will not able to connect to server.
			 * But here only show some warning.
			 * TODO: implement re-connect function
			 */
			if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
				WPRINTF(("vtcon: connect error[%d] \n", errno));
			} else {
				if (make_socket_non_blocking(fd) == -1) {
					WPRINTF(("Backend config: fcntl Error\n"));
				}
			}
		} else {
			WPRINTF(("Socket type not exist\n"));
			return -1;
		}

	default:
		break; /* nothing to do */
	}

	return 0;
}

static enum virtio_console_be_type
virtio_console_get_be_type(const char *backend)
{
	int i;

	for (i = 0; i < VIRTIO_CONSOLE_BE_MAX; i++)
		if (strcasecmp(backend, virtio_console_be_table[i]) == 0)
			return i;

	return VIRTIO_CONSOLE_BE_INVALID;
}

static int
virtio_console_add_backend(struct virtio_console *console, char *opts)
{
	struct virtio_console_backend *be;
	int error = 0, fd = -1;
	bool is_console = false;
	char *backend = NULL;
	char *portname = NULL;
	char *portpath = NULL;
	char *socket_type = NULL;
	char *opt;
	enum virtio_console_be_type be_type = VIRTIO_CONSOLE_BE_INVALID;

	/* virtio-console,[@]stdio|tty|pty|file:portname[=portpath]
	 * [,[@]stdio|tty|pty|file:portname[=portpath][:socket_type]]
	 */
	while ((opt = strsep(&opts, ",")) != NULL) {
		backend = strsep(&opt, ":");

		if (backend == NULL) {
			WPRINTF(("vtcon: no backend is specified!\n"));
			error = -1;
			goto parse_fail;
		}

		if (backend[0] == '@') {
			is_console = true;
			backend++;
		} else
			is_console = false;

		be_type = virtio_console_get_be_type(backend);
		if (be_type == VIRTIO_CONSOLE_BE_INVALID) {
			WPRINTF(("vtcon: invalid backend %s!\n",
				backend));
			error = -1;
			goto parse_fail;
		}

		if (opt != NULL) {
			if (be_type == VIRTIO_CONSOLE_BE_SOCKET) {
				portname = strsep(&opt, "=");
				portpath = strsep(&opt, ":");
				socket_type = opt;
			} else {
				portname = strsep(&opt, "=");
				portpath = opt;
			}
			if (portpath == NULL
				&& be_type != VIRTIO_CONSOLE_BE_STDIO
				&& be_type != VIRTIO_CONSOLE_BE_PTY
				&& be_type != VIRTIO_CONSOLE_BE_SOCKET) {
				WPRINTF(("vtcon: portpath missing for %s\n",
					portname));
				error = -1;
				goto parse_fail;
			}
		}
	}

	be = calloc(1, sizeof(struct virtio_console_backend));
	if (be == NULL) {
		error = -1;
		goto out;
	}

	fd = virtio_console_open_backend(portpath, be_type);
	if (fd < 0) {
		error = -1;
		goto out;
	}

	be->fd = fd;
	be->be_type = be_type;
	be->portpath = portpath;
	be->socket_type = socket_type;

	if (virtio_console_config_backend(be) < 0) {
		WPRINTF(("vtcon: virtio_console_config_backend failed\n"));
		error = -1;
		goto out;
	}

	be->port = virtio_console_add_port(console, portname,
		virtio_console_backend_write, be, is_console);
	if (be->port == NULL) {
		WPRINTF(("vtcon: virtio_console_add_port failed\n"));
		error = -1;
		goto out;
	}

	if (virtio_console_backend_can_read(be_type)) {
		if (isatty(fd)) {
			be->evp = mevent_add(fd, EVF_READ,
					virtio_console_backend_read, be,
					virtio_console_teardown_backend, be);
			if (be->evp == NULL) {
				WPRINTF(("vtcon: mevent_add failed\n"));
				error = -1;
				goto out;
			}
			console->ref_count++;
		}
	}

	virtio_console_open_port(be->port, true);
	be->open = true;

out:
	if (error != 0) {
		if (be) {
			if (be->port) {
				be->port->enabled = false;
				be->port->arg = NULL;
			}
			if (be->be_type == VIRTIO_CONSOLE_BE_PTY &&
				be->pts_fd > 0)
				close(be->pts_fd);
			free(be);
		}
		if (fd != -1 && fd != STDIN_FILENO)
			close(fd);
	}

parse_fail:

	WPRINTF(("vtcon: add port failed %s\n", portname));
	return error;
}

static void
virtio_console_close_backend(struct virtio_console_backend *be)
{
	if (!be)
		return;

	switch (be->be_type) {
	case VIRTIO_CONSOLE_BE_PTY:
		if (be->pts_fd > 0) {
			close(be->pts_fd);
			be->pts_fd = -1;
		}
		break;
	case VIRTIO_CONSOLE_BE_STDIO:
		virtio_console_restore_stdio();
		break;
	default:
		break;
	}

	if (be->be_type != VIRTIO_CONSOLE_BE_STDIO && be->fd > 0) {
		close(be->fd);
		be->fd = -1;
	}

	memset(be->port, 0, sizeof(*be->port));
	free(be);
}

static void
virtio_console_destroy(struct virtio_console *console)
{
	if (console) {
		if (console->config)
			free(console->config);
		free(console);
		console = NULL;
	}
}

static void
virtio_console_teardown_backend(void *param)
{
	struct virtio_console *console = NULL;
	struct virtio_console_backend *be;

	be = (struct virtio_console_backend *)param;
	if (!be)
		return;

	if (be->port)
		console = be->port->console;

	virtio_console_close_backend(be);

	if (console) {
		console->ref_count--;
		/* free virtio_console if this is the last backend */
		if (console->ref_count == 0)
			virtio_console_destroy(console);
	}
}

static int
virtio_console_close_all(struct virtio_console *console)
{
	int i, rc = 0;
	struct virtio_console_port *port;
	struct virtio_console_backend *be;

	/*
	 * we should close ports without mevent first.
	 * port->enabled is reset to false when a backend is closed.
	 */
	for (i = 0; i < console->nports; i++) {
		port = &console->ports[i];

		if (!port->enabled)
			continue;

		be = (struct virtio_console_backend *)port->arg;
		if (be && !be->evp)
			virtio_console_close_backend(be);
	}

	for (i = 0; i < console->nports; i++) {
		port = &console->ports[i];

		if (!port->enabled)
			continue;

		be = (struct virtio_console_backend *)port->arg;
		if (be && be->evp) {
			/* resources will be freed in the teardown callback */
			mevent_delete(be->evp);
			rc = 1;
		}
	}

	return rc;
}

static int
virtio_console_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_console *console;
	int i;
	pthread_mutexattr_t attr;
	int rc;

	if (!opts) {
		WPRINTF(("vtcon: invalid opts\n"));
		return -1;
	}

	console = calloc(1, sizeof(struct virtio_console));
	if (!console) {
		WPRINTF(("vtcon: calloc returns NULL\n"));
		return -1;
	}

	console->config = calloc(1, sizeof(struct virtio_console_config));
	if (!console->config) {
		WPRINTF(("vtcon->config: calloc returns NULL\n"));
		free(console);
		return -1;
	}

	console->config->max_nr_ports = VIRTIO_CONSOLE_MAXPORTS;
	console->config->cols = 80;
	console->config->rows = 25;

	/* init mutex attribute properly to avoid deadlock */
	rc = pthread_mutexattr_init(&attr);
	if (rc)
		DPRINTF(("mutexattr init failed with erro %d!\n", rc));
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (rc)
		DPRINTF(("virtio_console: mutexattr_settype failed with "
					"error %d!\n", rc));

	rc = pthread_mutex_init(&console->mtx, &attr);
	if (rc)
		DPRINTF(("virtio_console: pthread_mutex_init failed with "
					"error %d!\n", rc));

	virtio_linkup(&console->base, &virtio_console_ops, console, dev,
		console->queues, BACKEND_VBSU);
	console->base.mtx = &console->mtx;
	console->base.device_caps = VIRTIO_CONSOLE_S_HOSTCAPS;

	for (i = 0; i < VIRTIO_CONSOLE_MAXQ; i++) {
		console->queues[i].qsize = VIRTIO_CONSOLE_RINGSZ;
		console->queues[i].notify = i % 2 == 0
		    ? virtio_console_notify_rx
		    : virtio_console_notify_tx;
	}

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_DEVICE, VIRTIO_DEV_CONSOLE);
	pci_set_cfgdata16(dev, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_SIMPLECOMM);
	pci_set_cfgdata16(dev, PCIR_SUBDEV_0, VIRTIO_TYPE_CONSOLE);
	pci_set_cfgdata16(dev, PCIR_SUBVEND_0, VIRTIO_VENDOR);

	if (virtio_interrupt_init(&console->base, virtio_uses_msix())) {
		if (console) {
			if (console->config)
				free(console->config);
			free(console);
		}
		return -1;
	}
	virtio_set_io_bar(&console->base, 0);

	/* create control port */
	console->control_port.console = console;
	console->control_port.txq = 2;
	console->control_port.rxq = 3;
	console->control_port.cb = virtio_console_control_tx;
	console->control_port.enabled = true;
	if (virtio_console_add_backend(console, opts) < 0) {
		return -1;
	}

	return 0;
}

static void
virtio_console_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	struct virtio_console *console;
	int rc;

	console = (struct virtio_console *)dev->arg;
	if (console) {
		rc = virtio_console_close_all(console);
		/*
		 * if all the ports are without mevent attached,
		 * no teardown will be called, we should destroy
		 * console here explicitly.
		 */
		if (!rc)
			virtio_console_destroy(console);
	}
}

struct pci_vdev_ops pci_ops_virtio_console = {
	.class_name	= "virtio-console",
	.vdev_init	= virtio_console_init,
	.vdev_deinit	= virtio_console_deinit,
	.vdev_barwrite	= virtio_pci_write,
	.vdev_barread	= virtio_pci_read
};
DEFINE_PCI_DEVTYPE(pci_ops_virtio_console);
