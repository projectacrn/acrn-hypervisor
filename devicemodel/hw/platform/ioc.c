/*
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
 */
/*
 * Automotive IO Controller mediator virtualization.
 *
 * IOC mediator block diagram.
 * +------------+     +------------------+
 * |    IOC     |<--->|Native CBC cdevs  |
 * |            |     |                  |
 * |  mediator  |     |/dev/cbc-lifecycle|
 * |            |     |/dev/cbc-signals  |
 * |            |     |...               |
 * |            |     +------------------+
 * |            |     +------------+
 * |            |     |Virtual UART|
 * |            |     |            |
 * |            |<--->|            |
 * |            |     |            |
 * +------------+     +------------+
 *
 * IOC mediator data flow diagram.
 * +------+       +----------------+
 * |Core  |<------+Native CBC cdevs|
 * |      |       +----------------+
 * |thread|       +----------------+
 * |      |<------+Virtual UART    |
 * |      |       +----------------+
 * |      |       +----------------+
 * |      +------>|Tx/Rx queues    |
 * +------+       +----------------+
 *
 * +------+       +----------------+
 * |Rx    |<------+Rx queue        |
 * |      |       +----------------+
 * |thread|
 * |      |       +----------------+
 * |      +------>|Native CBC cdevs|
 * +------+       +----------------+
 *
 * +------+       +----------------+
 * |Tx    |<------+Tx queue        |
 * |      |       +----------------+
 * |thread|       |
 * |      |       +----------------+
 * |      +------>|Virtual UART    |
 * +------+       +----------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <string.h>
#include <stdbool.h>
#include <types.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "ioc.h"

/*
 * Debug printf
 */
static int ioc_debug;
#define	DPRINTF(fmt, args...) \
	do { if (ioc_debug) printf(fmt, ##args); } while (0)
#define	WPRINTF(fmt, args...) printf(fmt, ##args)


/*
 * Type definition for thread function.
 */
typedef void* (*ioc_work)(void *arg);

/*
 * IOC mediator and virtual UART communication channel path,
 * comes from DM command line parameters.
 */
static char virtual_uart_path[32];

/* IOC boot reason(for S5)
 * comes from DM command line parameters.
 */
static uint32_t ioc_boot_reason;

/*
 * IOC channels definition.
 */
static struct ioc_ch_info ioc_ch_tbl[] = {
	{IOC_INIT_FD, IOC_NP_PMT,   IOC_NATIVE_PMT,	IOC_CH_OFF},
	{IOC_INIT_FD, IOC_NP_LF,    IOC_NATIVE_LFCC,	IOC_CH_ON },
	{IOC_INIT_FD, IOC_NP_SIG,   IOC_NATIVE_SIGNAL,	IOC_CH_ON },
	{IOC_INIT_FD, IOC_NP_ESIG,  IOC_NATIVE_ESIG,	IOC_CH_OFF},
	{IOC_INIT_FD, IOC_NP_DIAG,  IOC_NATIVE_DIAG,	IOC_CH_OFF},
	{IOC_INIT_FD, IOC_NP_DLT,   IOC_NATIVE_DLT,	IOC_CH_OFF},
	{IOC_INIT_FD, IOC_NP_LIND,  IOC_NATIVE_LINDA,	IOC_CH_OFF},
	{IOC_INIT_FD, IOC_NP_RAW0,  IOC_NATIVE_RAW0,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW1,  IOC_NATIVE_RAW1,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW2,  IOC_NATIVE_RAW2,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW3,  IOC_NATIVE_RAW3,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW4,  IOC_NATIVE_RAW4,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW5,  IOC_NATIVE_RAW5,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW6,  IOC_NATIVE_RAW6,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW7,  IOC_NATIVE_RAW7,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW8,  IOC_NATIVE_RAW8,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW9,  IOC_NATIVE_RAW9,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW10, IOC_NATIVE_RAW10,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW11, IOC_NATIVE_RAW11,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_DP_NONE,  IOC_VIRTUAL_UART,	IOC_CH_ON},
};

/* TODO: Fill the tables by folow-up patches */
static struct cbc_signal cbc_tx_signal_table[] = {
};

static struct cbc_signal cbc_rx_signal_table[] = {
};

static struct cbc_group cbc_rx_group_table[] = {
};

static struct cbc_group cbc_tx_group_table[] = {
};

static struct wlist_signal wlist_rx_signal_table[] = {
};

static struct wlist_signal wlist_tx_signal_table[] = {
};

static struct wlist_group wlist_rx_group_table[] = {
};

static struct wlist_group wlist_tx_group_table[] = {
};

/*
 * Open native CBC cdevs and virtual UART.
 */
static int
ioc_ch_init(struct ioc_dev *ioc)
{
	/* TODO: implementation */
	return 0;
}

/*
 * Close native CBC cdevs and virtual UART.
 */
static void
ioc_ch_deinit(void)
{
	/* TODO: implementation */
}

/*
 * Rx processing of the epoll kicks.
 */
static int
ioc_process_rx(struct ioc_dev *ioc, enum ioc_ch_id id)
{
	/* TODO: implementation */
	return 0;
}

/*
 * Tx processing of the epoll kicks.
 */
static int
ioc_process_tx(struct ioc_dev *ioc, enum ioc_ch_id id)
{
	/* TODO: implementation */
	return 0;
}

/*
 * Core thread monitors epoll events of Rx and Tx directions
 * based on the channel id for different process.
 */
static void
ioc_dispatch(struct ioc_dev *ioc, struct ioc_ch_info *chl)
{
	switch (chl->id) {
	case IOC_NATIVE_LFCC:
	case IOC_NATIVE_SIGNAL:
	case IOC_NATIVE_RAW0 ... IOC_NATIVE_RAW11:
		ioc_process_tx(ioc, chl->id);
		break;
	case IOC_VIRTUAL_UART:
		ioc_process_rx(ioc, chl->id);
		break;
	default:
		DPRINTF("ioc dispatch got wrong channel:%d\r\n", chl->id);
		break;
	}
}

/*
 * Handle EPOLLIN events for native CBC cdevs and virtual UART.
 */
static void *
ioc_core_thread(void *arg)
{
	int n, i;
	struct ioc_ch_info *chl;
	int chl_size = ARRAY_SIZE(ioc_ch_tbl);
	struct ioc_dev *ioc = (struct ioc_dev *) arg;
	struct epoll_event eventlist[IOC_MAX_EVENTS];

	/*
	 * Initialize epoll events.
	 * NOTE: channel size always same with events number, so that acess
	 * event by channel index.
	 */
	for (i = 0, chl = ioc_ch_tbl; i < chl_size; i++, chl++) {
		if (chl->fd > 0) {
			ioc->evts[i].events = EPOLLIN;
			ioc->evts[i].data.ptr = chl;
			if (epoll_ctl(ioc->epfd, EPOLL_CTL_ADD, chl->fd,
						&ioc->evts[i]) < 0)
				DPRINTF("ioc epoll ctl %s failed, error:%s\r\n",
						chl->name, strerror(errno));
		}
	}

	/* Start to epoll wait loop */
	for (;;) {
		n = epoll_wait(ioc->epfd, eventlist, IOC_MAX_EVENTS, -1);
		if (n < 0 && errno != EINTR) {
			DPRINTF("ioc epoll wait error:%s, exit ioc core\r\n",
					strerror(errno));
			goto exit;
		}
		for (i = 0; i < n; i++)
			ioc_dispatch(ioc, (struct ioc_ch_info *)
					eventlist[i].data.ptr);
	}
exit:
	return NULL;
}

/*
 * Rx thread waits for CBC requests of rx queue, if rx queue is not empty,
 * it wll get a cbc_request from rx queue and invokes cbc_rx_handler to process.
 */
static void *
ioc_rx_thread(void *arg)
{
	/* TODO: implementation */
	return NULL;
}

/*
 * Tx thread waits for CBC requests of tx queue, if tx queue is not empty,
 * it wll get a CBC request from tx queue and invokes cbc_tx_handler to process.
 */
static void *
ioc_tx_thread(void *arg)
{
	/* TODO: implementation */
	return NULL;
}

/*
 * Stop all threads(core/rx/tx)
 */
static void
ioc_kill_workers(struct ioc_dev *ioc)
{
	/* TODO: implementation */
}

static int
ioc_create_thread(const char *name, pthread_t *tid,
		ioc_work func, void *arg)
{
	if (pthread_create(tid, NULL, func, arg) != 0) {
		DPRINTF("ioc can not create thread\r\n");
		return -1;
	}
	pthread_setname_np(*tid, name);
	return 0;
}

/*
 * Process rx direction data flow, the rx direction is that
 * virtual UART -> native CBC cdevs.
 */
static void
cbc_rx_handler(struct cbc_pkt *pkt)
{
	/* TODO: implementation */
}

/*
 * Process tx direction data flow, the tx direction is that
 * native CBC cdevs -> virtual UART.
 */
static void
cbc_tx_handler(struct cbc_pkt *pkt)
{
	/* TODO: implementation */
}

/*
 * Check if current platform supports IOC or not.
 */
static int
ioc_is_platform_supported(void)
{
	struct stat st;

	/* The early signal channel will be created after cbc attached,
	 * if not, the current platform does not support IOC, exit IOC mediator.
	 */
	return stat(IOC_NP_ESIG, &st);
}

/*
 * To get IOC bootup reason and virtual UART path for communication
 * between IOC mediator and virtual UART.
 */
int
ioc_parse(const char *opts)
{
	char *tmp;
	char *param = strdup(opts);

	tmp = strtok(param, ",");
	snprintf(virtual_uart_path, sizeof(virtual_uart_path), "%s", param);
	if (tmp != NULL) {
		tmp = strtok(NULL, ",");
		ioc_boot_reason = strtoul(tmp, 0, 0);
	}
	free(param);
	return 0;
}

/*
 * IOC mediator main entry.
 */
struct ioc_dev *
ioc_init(void)
{
	int i;
	struct ioc_dev *ioc;

	if (ioc_is_platform_supported() != 0)
		goto ioc_err;

	ioc = (struct ioc_dev *)calloc(1, sizeof(struct ioc_dev));
	if (!ioc)
		goto ioc_err;
	ioc->pool = (struct cbc_request *)calloc(IOC_MAX_REQUESTS,
			sizeof(struct cbc_request));
	ioc->evts = (struct epoll_event *)calloc(ARRAY_SIZE(ioc_ch_tbl),
			sizeof(struct epoll_event));
	if (!ioc->pool || !ioc->evts)
		goto alloc_err;

	/*
	 * IOC mediator needs to manage more than 15 channels with mass data
	 * transfer, to avoid blocking other mevent users, IOC mediator
	 * creates its own epoll in one separated thread.
	 */
	ioc->epfd = epoll_create1(0);
	if (ioc->epfd < 0)
		goto alloc_err;

	/*
	 * Put all buffered CBC requests on the free queue, the free queue is
	 * used to be a cbc_request buffer.
	 */
	SIMPLEQ_INIT(&ioc->free_qhead);
	pthread_mutex_init(&ioc->free_mtx, NULL);
	for (i = 0; i < IOC_MAX_REQUESTS; i++)
		SIMPLEQ_INSERT_TAIL(&ioc->free_qhead, ioc->pool + i, me_queue);

	/*
	 * Initialize native CBC cdev and virtual UART.
	 */
	if (ioc_ch_init(ioc) != 0)
		goto chl_err;

	/* Setup IOC rx members */
	snprintf(ioc->rx_name, sizeof(ioc->rx_name), "ioc_rx");
	ioc->ioc_dev_rx = cbc_rx_handler;
	pthread_cond_init(&ioc->rx_cond, NULL);
	pthread_mutex_init(&ioc->rx_mtx, NULL);
	SIMPLEQ_INIT(&ioc->rx_qhead);
	ioc->rx_config.cbc_sig_num = ARRAY_SIZE(cbc_rx_signal_table);
	ioc->rx_config.cbc_grp_num = ARRAY_SIZE(cbc_rx_group_table);
	ioc->rx_config.wlist_sig_num = ARRAY_SIZE(wlist_rx_signal_table);
	ioc->rx_config.wlist_grp_num = ARRAY_SIZE(wlist_rx_group_table);
	ioc->rx_config.cbc_sig_tbl = cbc_rx_signal_table;
	ioc->rx_config.cbc_grp_tbl = cbc_rx_group_table;
	ioc->rx_config.wlist_sig_tbl = wlist_rx_signal_table;
	ioc->rx_config.wlist_grp_tbl = wlist_rx_group_table;

	/* Setup IOC tx members */
	snprintf(ioc->tx_name, sizeof(ioc->tx_name), "ioc_tx");
	ioc->ioc_dev_tx = cbc_tx_handler;
	pthread_cond_init(&ioc->tx_cond, NULL);
	pthread_mutex_init(&ioc->tx_mtx, NULL);
	SIMPLEQ_INIT(&ioc->tx_qhead);
	ioc->tx_config.cbc_sig_num = ARRAY_SIZE(cbc_tx_signal_table);
	ioc->tx_config.cbc_grp_num = ARRAY_SIZE(cbc_tx_group_table);
	ioc->tx_config.wlist_sig_num = ARRAY_SIZE(wlist_tx_signal_table);
	ioc->tx_config.wlist_grp_num = ARRAY_SIZE(wlist_tx_group_table);
	ioc->tx_config.cbc_sig_tbl = cbc_tx_signal_table;
	ioc->tx_config.cbc_grp_tbl = cbc_tx_group_table;
	ioc->tx_config.wlist_sig_tbl = wlist_tx_signal_table;
	ioc->tx_config.wlist_grp_tbl = wlist_tx_group_table;

	/*
	 * Three threads are created for IOC work flow.
	 * Rx thread is responsible for writing data to native CBC cdevs.
	 * Tx thread is responsible for writing data to virtual UART.
	 * Core thread is responsible for reading data from native CBC cdevs
	 * and virtual UART.
	 */
	if (ioc_create_thread(ioc->rx_name, &ioc->rx_tid, ioc_rx_thread,
			(void *)ioc) < 0)
		goto work_err;
	if (ioc_create_thread(ioc->tx_name, &ioc->tx_tid, ioc_tx_thread,
			(void *)ioc) < 0)
		goto work_err;
	snprintf(ioc->name, sizeof(ioc->name), "ioc_core");
	if (ioc_create_thread(ioc->name, &ioc->tid, ioc_core_thread,
			(void *)ioc) < 0)
		goto work_err;

	return ioc;
work_err:
	pthread_mutex_destroy(&ioc->rx_mtx);
	pthread_cond_destroy(&ioc->rx_cond);
	pthread_mutex_destroy(&ioc->tx_mtx);
	pthread_cond_destroy(&ioc->tx_cond);
	ioc_kill_workers(ioc);
chl_err:
	ioc_ch_deinit();
	pthread_mutex_destroy(&ioc->free_mtx);
	close(ioc->epfd);
alloc_err:
	free(ioc->evts);
	free(ioc->pool);
	free(ioc);
ioc_err:
	DPRINTF("%s", "ioc mediator startup failed!!\r\n");
	return NULL;
}

/*
 * Called by DM in main entry.
 */
void
ioc_deinit(struct ioc_dev *ioc)
{
	if (!ioc) {
		DPRINTF("%s", "ioc deinit parameter is NULL\r\n");
		return;
	}
	ioc_kill_workers(ioc);
	ioc_ch_deinit();
	close(ioc->epfd);
	free(ioc->evts);
	free(ioc->pool);
	free(ioc);
}
