/*
 * Copyright (C) 2018 Intel Corporation
 * Copyright (C) 2014, 2015 IBM Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <sys/un.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "vmmapi.h"
#include "tpm_internal.h"

/* According to definition in TPM2 spec */
#define TPM_ORD_ContinueSelfTest	0x53
#define TPM_TAG_RSP_COMMAND		0xc4
#define TPM_FAIL		9
#define PTM_INIT_FLAG_DELETE_VOLATILE	(1 << 0)

/* To align with definition in SWTPM */
typedef uint32_t ptm_res;

struct ptm_est {
	union {
		struct {
			ptm_res tpm_result;
			unsigned char bit; /* TPM established bit */
		} resp; /* response */
	} u;
};

struct ptm_reset_est {
	union {
		struct {
			uint8_t loc; /* locality to use */
		} req; /* request */
		struct {
			ptm_res tpm_result;
		} resp; /* response */
	} u;
};

struct ptm_init {
	union {
		struct {
			uint32_t init_flags; /* see definitions below */
		} req; /* request */
		struct {
			ptm_res tpm_result;
		} resp; /* response */
	} u;
};

struct ptm_loc {
	union {
		struct {
			uint8_t loc; /* locality to set */
		} req; /* request */
		struct {
			ptm_res tpm_result;
		} resp; /* response */
	} u;
};

struct ptm_getconfig {
	union {
		struct {
			ptm_res tpm_result;
			uint32_t flags;
		} resp; /* response */
	} u;
};

struct ptm_setbuffersize {
	union {
		struct {
			uint32_t buffersize; /* 0 to query for current buffer size */
		} req; /* request */
		struct {
			ptm_res tpm_result;
			uint32_t buffersize; /* buffer size in use */
			uint32_t minsize; /* min. supported buffer size */
			uint32_t maxsize; /* max. supported buffer size */
		} resp; /* response */
	} u;
};

typedef struct ptm_est ptm_est;
typedef struct ptm_reset_est ptm_reset_est;
typedef struct ptm_loc ptm_loc;
typedef struct ptm_init ptm_init;
typedef struct ptm_getconfig ptm_getconfig;
typedef struct ptm_setbuffersize ptm_setbuffersize;

/* For TPM CRB definition */
#pragma pack(push, 1)
typedef struct  {
	uint16_t  tag;
	uint32_t  length;
	uint32_t  ordinal;
} tpm_input_header;

typedef struct  {
	uint16_t  tag;
	uint32_t  length;
	uint32_t  return_code;
} tpm_output_header;
#pragma pack(pop)

/* This is the main data structure for tpm emulator,
 * it will work with one SWTPM instance to
 * provide TPM functionlity to UOS.
 *
 * ctrl_chan_fd: fd to communicate with SWTPM ctrl channel
 * cmd_chan_fd: fd to communicate with SWTPM cmd channel
 * cur_locty_number: to store the last set locality
 * established_flag & established_flag_cached: used in
 *    swtpm_get_tpm_established_flag, to store tpm establish flag.
 */
typedef struct swtpm_context {
	int ctrl_chan_fd;
	int cmd_chan_fd;
	uint8_t cur_locty_number; /* last set locality */
	unsigned int established_flag:1;
	unsigned int established_flag_cached:1;
} swtpm_context;

/* Align with definition in SWTPM */
enum {
	CMD_GET_CAPABILITY = 1,		/* 0x01 */
	CMD_INIT,			/* 0x02 */
	CMD_SHUTDOWN,			/* 0x03 */
	CMD_GET_TPMESTABLISHED,		/* 0x04 */
	CMD_SET_LOCALITY,		/* 0x05 */
	CMD_HASH_START,			/* 0x06 */
	CMD_HASH_DATA,			/* 0x07 */
	CMD_HASH_END,			/* 0x08 */
	CMD_CANCEL_TPM_CMD,		/* 0x09 */
	CMD_STORE_VOLATILE,		/* 0x0a */
	CMD_RESET_TPMESTABLISHED,	/* 0x0b */
	CMD_GET_STATEBLOB,		/* 0x0c */
	CMD_SET_STATEBLOB,		/* 0x0d */
	CMD_STOP,			/* 0x0e */
	CMD_GET_CONFIG,			/* 0x0f */
	CMD_SET_DATAFD,			/* 0x10 */
	CMD_SET_BUFFERSIZE,		/* 0x11 */
	CMD_GET_INFO,			/* 0x12 */
};

static swtpm_context tpm_context;


static inline uint16_t tpm_cmd_get_tag(const void *b)
{
	return __builtin_bswap16(*(uint16_t*)(b));
}

static inline uint32_t tpm_cmd_get_size(const void *b)
{
	return __builtin_bswap32(*(uint32_t*)(b + 2));
}

static inline uint32_t tpm_cmd_get_ordinal(const void *b)
{
	return __builtin_bswap32(*(uint32_t*)(b + 6));
}

static inline uint32_t tpm_cmd_get_errcode(const void *b)
{
	return __builtin_bswap32(*(uint32_t*)(b + 6));
}

static inline void tpm_cmd_set_tag(void *b, uint16_t tag)
{
	*(uint16_t*)(b) = __builtin_bswap16(tag);
}

static inline void tpm_cmd_set_size(void *b, uint32_t size)
{
	*(uint32_t*)(b + 2) = __builtin_bswap32(size);
}

static inline void tpm_cmd_set_error(void *b, uint32_t error)
{
	*(uint32_t*)(b + 6) = __builtin_bswap32(error);
}

static bool tpm_is_selftest(const uint8_t *in, uint32_t in_len)
{
	if (in_len >= sizeof(tpm_input_header))
		return tpm_cmd_get_ordinal(in) == TPM_ORD_ContinueSelfTest;

	return false;
}

static int ctrl_chan_conn(const char *servername)
{
	int clifd;
	struct sockaddr_un servaddr;
	int ret;

	if (!servername) {
		printf("%s error, invalid input\n", __func__);
		return -1;
	}

	if (strnlen(servername, sizeof(servaddr.sun_path)) == (sizeof(servaddr.sun_path))) {
		printf("%s error, length of servername is too long\n", __func__);
		return -1;
	}

	clifd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (clifd < 0) {
		printf("socket failed.\n");
		return -1;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;

	strncpy(servaddr.sun_path, servername, sizeof(servaddr.sun_path));

	ret = connect(clifd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	if (ret < 0) {
		printf("connect failed.\n");
		close(clifd);
		return -1;
	}

	return clifd;
}

static int ctrl_chan_write(int ctrl_chan_fd, const uint8_t *buf, int len,
			int *pdatafd, int fd_num)
{
	int ret;
	struct msghdr msg;
	struct iovec iov[1];
	union {
		struct cmsghdr cm;
		char control[CMSG_SPACE(sizeof(int))];
	} control_un;
	struct cmsghdr *pcmsg;

	if (!buf || (!pdatafd && fd_num)) {
		printf("%s error, invalid input\n", __func__);
		return -1;
	}

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	iov[0].iov_base = (void*)buf;
	iov[0].iov_len = len;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	if (fd_num == 0) {
		if (pdatafd)
			return -1;

		msg.msg_control = NULL;
		msg.msg_controllen = 0;
	} else if (fd_num == 1) {
		msg.msg_control = control_un.control;
		msg.msg_controllen = sizeof(control_un.control);

		pcmsg = CMSG_FIRSTHDR(&msg);
		pcmsg->cmsg_len = CMSG_LEN(sizeof(int));
		pcmsg->cmsg_level = SOL_SOCKET;
		pcmsg->cmsg_type = SCM_RIGHTS;
		*((int *)CMSG_DATA(pcmsg)) = *pdatafd;
	} else {
		printf("fd_num failed.\n");
		return -1;
	}

	do {
		ret = sendmsg(ctrl_chan_fd, &msg, 0);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		fprintf(stderr, "Failed to send msg, reason: %s\n", strerror(errno));
	}

	return ret;
}

static int ctrl_chan_read(int ctrl_chan_fd, uint8_t *buf, int len)
{
	struct msghdr msg;
	struct iovec iov[1];
	int recvd = 0;
	int n;

	if (!buf) {
		printf("%s error, invalid input\n", __func__);
		return -1;
	}

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	iov[0].iov_base = buf;
	iov[0].iov_len = len;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	/* No need to recv fd */
	msg.msg_control = NULL;
	msg.msg_controllen = 0;

	while (recvd < len) {
		if (0 == recvd)
			n = recvmsg(ctrl_chan_fd, &msg, 0);
		else
			n = read(ctrl_chan_fd, msg.msg_iov[0].iov_base + recvd, len - recvd);
		if (n <= 0)
			return n;
		recvd += n;
	}

	return recvd;
}

static int cmd_chan_write(int cmd_chan_fd, const uint8_t *buf, int len)
{
	ssize_t	 nwritten = 0;
	int buffer_length = len;

	if (!buf) {
		printf("%s error, invalid input\n", __func__);
		return -1;
	}

	while (buffer_length > 0) {
		nwritten = write(cmd_chan_fd, buf, buffer_length);
		if (nwritten >= 0) {
			buffer_length -= nwritten;
			buf += nwritten;
		}
		else {
			fprintf(stderr, "cmd_chan_write: Error, write() %d %s\n",
					  errno, strerror(errno));
			return -1;
		}
	}

	return (len - buffer_length);
}

static int cmd_chan_read(int cmd_chan_fd, uint8_t *buf, int len)
{
	ssize_t nread = 0;
	size_t nleft = len;

	if (!buf) {
		printf("%s error, invalid input\n", __func__);
		return -1;
	}

	while (nleft > 0) {
		nread = read(cmd_chan_fd, buf, nleft);
		if (nread > 0) {
			nleft -= nread;
			buf += nread;
		}
		else if (nread < 0) {/* error */
			fprintf(stderr, "cmd_chan_read: Error, read() error %d %s\n",
				   errno, strerror(errno));
			return -1;
		}
		else if (nread == 0) {/* EOF */
			fprintf(stderr, "cmd_chan_read: Error, read EOF, read %lu bytes\n",
				   (unsigned long)(len - nleft));
			return -1;
		}
	}

	return (len - nleft);
}

/*
 * Send command to swtpm ctrl channel.
 * Note: Both msg_len_in & msg_len_out are valid and needed.
 * It has 2 cases as below:
 * 1. msg_len_in is equal to msg_len_out, all are valid and bigger than 0.
 * 2. msg_len_in is 0, while msg_len_out is bigger than 0.
 * msg_len_out should always >0 because it need to return "ptm_res"
 * as the return value(which to indicate pass or fail) at least.
 */
static int swtpm_ctrlcmd(int ctrl_chan_fd, unsigned long cmd, void *msg,
			size_t msg_len_in, size_t msg_len_out,
			int *pdatafd, int fd_num)
{
	uint32_t cmd_no = __builtin_bswap32(cmd);
	ssize_t n = sizeof(uint32_t) + msg_len_in;
	uint8_t *buf = NULL;
	int ret = -1;
	int send_num;
	int recv_num;

	if (!msg || (!pdatafd && fd_num)) {
		printf("%s error, invalid input\n", __func__);
		return -1;
	}

	buf = calloc(n, sizeof(char));
	if (!buf)
		return -1;

	memcpy(buf, &cmd_no, sizeof(cmd_no));
	memcpy(buf + sizeof(cmd_no), msg, msg_len_in);

	send_num = ctrl_chan_write(ctrl_chan_fd, buf, n, pdatafd, fd_num);
	if ((send_num <= 0) || (send_num != n) ) {
		printf("%s failed to write %d != %ld\n", __func__, send_num, n);
		goto end;
	}

	if (msg_len_out != 0) {
		recv_num = ctrl_chan_read(ctrl_chan_fd, msg, msg_len_out);
		if ((recv_num <= 0) || (recv_num != msg_len_out)) {
			printf("%s failed to read %d != %ld\n", __func__, recv_num, msg_len_out);
			goto end;
		}
	}

	ret = 0;

end:
	free(buf);
	return ret;
}

/*
 * Send command to swtpm cmd channel.
 * Note: out_len should be needed.
 * Currently swtpm_cmdcmd will only be called by swtpm_handle_request
 * to deliver the real tpm2 commands. And in crb_reg_write, we can
 * find that out_len is set as (4096-0x80) which is the maximum size
 * according to TPM2 spec. So inside function swtpm_cmdcmd,
 * it need to Check" tpm_cmd_get_size(out)>out_len".
 */
static int swtpm_cmdcmd(int cmd_chan_fd,
			const uint8_t *in, uint32_t in_len,
			uint8_t *out, uint32_t out_len, bool *selftest_done)
{
	ssize_t ret;
	bool is_selftest = false;

	if (!in || !out) {
		printf("%s error, invalid input\n", __func__);
		return -1;
	}

	if (selftest_done) {
		*selftest_done = false;
		is_selftest = tpm_is_selftest(in, in_len);
	}

	ret = cmd_chan_write(cmd_chan_fd, (uint8_t *)in, in_len);
	if ((ret == -1) || (ret != in_len)) {
		printf("%s failed to write %ld != %d\n", __func__, ret, in_len);
		return -1;
	}

	ret = cmd_chan_read(cmd_chan_fd, (uint8_t *)out,
			  sizeof(tpm_output_header));
	if (ret == -1) {
		printf("%s failed to read size\n", __func__);
		return -1;
	}

	if (tpm_cmd_get_size(out) > out_len) {
		printf("%s error, get out size is larger than out_len\n", __func__);
		return -1;
	}

	ret = cmd_chan_read(cmd_chan_fd,
				(uint8_t *)out + sizeof(tpm_output_header),
				tpm_cmd_get_size(out) - sizeof(tpm_output_header));
	if (ret == -1) {
		printf("%s failed to read data\n", __func__);
		return -1;
	}

	if (is_selftest) {
		*selftest_done = tpm_cmd_get_errcode(out) == 0;
	}

	return 0;
}

/*
 * Create ctrl channel.
 */
static int swtpm_ctrlchan_create(const char *arg_path)
{
	int connfd;

	if (!arg_path) {
		printf("%s error, invalid input\n", __func__);
		return -1;
	}

	connfd = ctrl_chan_conn(arg_path);
	if(connfd < 0)
	{
		printf("Error[%d] when connecting...",errno);
		return -1;
	}

	tpm_context.ctrl_chan_fd = connfd;

	return connfd;
}

/*
 * Create cmd channel.
 */
static int swtpm_cmdchan_create(void)
{
	ptm_res res = 0;
	int sv[2] = {-1, -1};

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0)
	{
		printf("socketpair failed!\n");
		return -1;
	}
	if (swtpm_ctrlcmd(tpm_context.ctrl_chan_fd, CMD_SET_DATAFD, &res, 0,
				 sizeof(res), &sv[1], 1) < 0 || res != 0) {
		printf("swtpm: Failed to send CMD_SET_DATAFD: %s", strerror(errno));
		goto err_exit;
	}
	tpm_context.cmd_chan_fd = sv[0];
	close(sv[1]);

	return 0;

err_exit:
	close(sv[0]);
	close(sv[1]);
	return -1;
}

static int swtpm_start(ptm_init *init)
{
	ptm_res res;

	if (!init) {
		printf("%s error, invalid input\n", __func__);
		return -1;
	}

	if (swtpm_ctrlcmd(tpm_context.ctrl_chan_fd, CMD_INIT,
				init, sizeof(*init), sizeof(*init), NULL, 0) < 0) {
		printf("swtpm: could not send INIT: %s", strerror(errno));
		goto err_exit;
	}

	res = __builtin_bswap32(init->u.resp.tpm_result);
	if (res) {
		printf("swtpm: TPM result for CMD_INIT: 0x%x", res);
		goto err_exit;
	}

	return 0;

err_exit:
	return -1;
}

static int swtpm_stop(void)
{
	ptm_res res = 0;

	if (swtpm_ctrlcmd(tpm_context.ctrl_chan_fd, CMD_STOP, &res, 0, sizeof(res), NULL, 0) < 0) {
		printf("swtpm: Could not stop TPM: %s", strerror(errno));
		return -1;
	}

	res = __builtin_bswap32(res);
	if (res) {
		printf("swtpm: TPM result for CMD_STOP: 0x%x", res);
		return -1;
	}

	return 0;
}

/* wanted_size: input, the buffer size which we want to setup.
 * actual_size: output, the actual buffer size returned after setup.
 *
 * Note: To meet swtpm logic, swtpm_stop() must be called before
 *    swtpm_set_buffer_size()
 */
static int swtpm_set_buffer_size(size_t wanted_size,
					size_t *actual_size)
{
	ptm_setbuffersize psbs;

	if (wanted_size == 0) {
		printf("%s error, wanted_size is 0\n", __func__);
		return -1;
	}

	psbs.u.req.buffersize = __builtin_bswap32(wanted_size);

	if (swtpm_ctrlcmd(tpm_context.ctrl_chan_fd, CMD_SET_BUFFERSIZE, &psbs,
			 sizeof(psbs.u.req), sizeof(psbs.u.resp), NULL, 0) < 0) {
		printf("swtpm: Could not set buffer size: %s", strerror(errno));
		return -1;
	}

	psbs.u.resp.tpm_result = __builtin_bswap32(psbs.u.resp.tpm_result);
	if (psbs.u.resp.tpm_result != 0) {
		printf("swtpm: TPM result for set buffer size : 0x%x", psbs.u.resp.tpm_result);
		return -1;
	}

	if (actual_size) {
		*actual_size = __builtin_bswap32(psbs.u.resp.buffersize);
	}

	return 0;
}

static int swtpm_startup_tpm(size_t buffersize,
				bool is_resume)
{
	ptm_init init = {
		.u.req.init_flags = 0,
	};

	if (swtpm_stop() < 0) {
		printf("swtpm_stop() failed!\n");
		return -1;
	}

	if (buffersize != 0 &&
		swtpm_set_buffer_size(buffersize, NULL) < 0) {
		return -1;
	}

	if (is_resume) {
		init.u.req.init_flags |= __builtin_bswap32(PTM_INIT_FLAG_DELETE_VOLATILE);
	}

	return swtpm_start(&init);
}

static void swtpm_shutdown(void)
{
	ptm_res res = 0;

	if (swtpm_ctrlcmd(tpm_context.ctrl_chan_fd, CMD_SHUTDOWN,
				&res, 0, sizeof(res), NULL, 0) < 0) {
		printf("swtpm: Could not cleanly shutdown the TPM: %s", strerror(errno));
	} else if (res != 0) {
		printf("swtpm: TPM result for sutdown: 0x%x", __builtin_bswap32(res));
	}
}

static int swtpm_set_locality(uint8_t locty_number)
{
	ptm_loc loc;

	if (tpm_context.cur_locty_number == locty_number)
		return 0;

	loc.u.req.loc = locty_number;
	if (swtpm_ctrlcmd(tpm_context.ctrl_chan_fd, CMD_SET_LOCALITY, &loc,
							 sizeof(loc), sizeof(loc), NULL, 0) < 0) {
		printf("swtpm: could not set locality : %s", strerror(errno));
		return -1;
	}

	loc.u.resp.tpm_result = __builtin_bswap32(loc.u.resp.tpm_result);
	if (loc.u.resp.tpm_result != 0) {
		printf("swtpm: TPM result for set locality : 0x%x", loc.u.resp.tpm_result);
		return -1;
	}

	tpm_context.cur_locty_number = locty_number;

	return 0;
}

static void swtpm_write_fatal_error_response(uint8_t *out, uint32_t out_len)
{
	if (!out) {
		printf("%s error, invalid input.\n", __func__);
		return;
	}

	if (out_len >= sizeof(tpm_output_header)) {
		tpm_cmd_set_tag(out, TPM_TAG_RSP_COMMAND);
		tpm_cmd_set_size(out, sizeof(tpm_output_header));
		tpm_cmd_set_error(out, TPM_FAIL);
	}
}

static void swtpm_cleanup(void)
{
	swtpm_shutdown();
	close(tpm_context.cmd_chan_fd);
	close(tpm_context.ctrl_chan_fd);
}

int swtpm_startup(size_t buffersize)
{
	return swtpm_startup_tpm(buffersize, false);
}

int swtpm_handle_request(TPMCommBuffer *cmd)
{
	if (!cmd) {
		printf("%s error, invalid input.\n", __func__);
		return -1;
	}

	if (swtpm_set_locality(cmd->locty) < 0 ||
		swtpm_cmdcmd(tpm_context.cmd_chan_fd, cmd->in, cmd->in_len,
				cmd->out, cmd->out_len,
				&cmd->selftest_done) < 0) {
		swtpm_write_fatal_error_response(cmd->out, cmd->out_len);
		return -1;
	}

	return 0;
}

bool swtpm_get_tpm_established_flag(void)
{
	ptm_est est;

	if (tpm_context.established_flag_cached) {
		return tpm_context.established_flag;
	}

	if (swtpm_ctrlcmd(tpm_context.ctrl_chan_fd, CMD_GET_TPMESTABLISHED, &est,
				0, sizeof(est), NULL, 0) < 0) {
		printf("swtpm: Could not get the TPM established flag: %s", strerror(errno));
		return false;
	}

	tpm_context.established_flag_cached = 1;
	tpm_context.established_flag = (est.u.resp.bit != 0);

	return tpm_context.established_flag;
}

int swtpm_reset_tpm_established_flag(void)
{
	ptm_reset_est reset_est;
	ptm_res res;

	reset_est.u.req.loc = tpm_context.cur_locty_number;
	if (swtpm_ctrlcmd(tpm_context.ctrl_chan_fd, CMD_RESET_TPMESTABLISHED,
				&reset_est, sizeof(reset_est),
				sizeof(reset_est), NULL, 0) < 0) {
		printf("swtpm: Could not reset the establishment bit: %s",
		strerror(errno));
		return -1;
	}

	res = __builtin_bswap32(reset_est.u.resp.tpm_result);
	if (res) {
		printf("swtpm: TPM result for rest establixhed flag: 0x%x", res);
		return -1;
	}

	tpm_context.established_flag_cached = 0;

	return 0;
}

void swtpm_cancel_cmd(void)
{
	ptm_res res = 0;

	if (swtpm_ctrlcmd(tpm_context.ctrl_chan_fd, CMD_CANCEL_TPM_CMD, &res, 0,
				sizeof(res), NULL, 0) < 0) {
		printf("swtpm: Could not cancel command: %s", strerror(errno));
	} else if (res != 0) {
		printf("swtpm: Failed to cancel TPM: 0x%x", __builtin_bswap32(res));
	}
}

int init_tpm_emulator(const char *sock_path)
{
	if (swtpm_ctrlchan_create(sock_path) < 0) {
		printf("error, failed to create ctrl channel.\n");
		return -1;
	}

	if (swtpm_cmdchan_create() < 0) {
		printf("error, failed to create cmd channel.\n");
		return -1;
	}

	return 0;
}

void deinit_tpm_emulator(void)
{
	swtpm_cleanup();
}
