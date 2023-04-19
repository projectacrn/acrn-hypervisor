/*
 * Copyright (C)2021-2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <pthread.h>
#include <limits.h>
#include <stdint.h>
#include "uart_channel.h"
#include "log.h"
#include "list.h"
#include "config.h"
#include "command.h"

#define SYNC_FMT "sync:%s"

static void parse_channel_dev_id(struct channel_dev *c_dev)
{
	char *saveptr;

	(void) strtok_r(c_dev->buf, ":", &saveptr);
	if (strlen(saveptr) > 0) {
		strncpy(c_dev->name, saveptr, CHANNEL_DEV_NAME_MAX - 1U);
		LOG_PRINTF("Device fd:%d, VM name:%s\n",
					get_uart_dev_fd(c_dev->uart_device), c_dev->name);
	}
}
static void add_uart_channel_dev_connection_list(struct channel_dev *c_dev)
{
	struct uart_channel *c = c_dev->channel;

	pthread_mutex_lock(&c->tty_conn_list_lock);
	LIST_INSERT_HEAD(&c->tty_conn_head, c_dev, list);
	pthread_mutex_unlock(&c->tty_conn_list_lock);
}
/**
 * @brief Wait to connect device in uart channel
 *
 * Wait sync message from slave channel device, parse slave channel device
 * indentifier from sync message, then add channel device into uart channel
 * device connection list.
 */
void *listen_uart_channel_dev(void *arg)
{
	ssize_t num;
	struct channel_dev *c_dev = (struct channel_dev *)arg;

	LOG_PRINTF("Lifecycle manager in service VM fd=%d tty node=%s\n",
				get_uart_dev_fd(c_dev->uart_device), get_uart_dev_path(c_dev->uart_device));
	memset(c_dev->buf, 0, sizeof(c_dev->buf));
	while (c_dev->listening) {
		num = receive_message_by_uart(c_dev->uart_device, (void *)c_dev->buf,
							sizeof(c_dev->buf));
		if (num == 0) {
			usleep(LISTEN_INTERVAL);
			continue;
		}
		parse_channel_dev_id(c_dev);
		if (strncmp(SYNC_CMD, c_dev->buf, sizeof(SYNC_CMD)) == 0) {
			/** Add channel device instance into UART connection list */
			add_uart_channel_dev_connection_list(c_dev);

			c_dev->listening = false;
			LOG_PRINTF("Receive sync message from user VM (%s), start to talk.\n",
					c_dev->name);
			usleep(2 * WAIT_RECV);
			(void)send_message_by_uart(c_dev->uart_device, ACK_SYNC, strlen(ACK_SYNC));
			sem_post(&c_dev->dev_sem);
		}
	}

	LOG_PRINTF("Lifecycle manager stops to listen device:%s\n",
				get_uart_dev_path(c_dev->uart_device));
	return NULL;
}
/**
 * @brief Wait to connect device in the uart channel
 * and poll message
 *
 * Send sync message every 5 second and wait acked sync message,
 * if acked sync message is received, add uart channel device instance
 * into uart connection list. It acked sync message is not received, the lifecycle
 * manager will exit.
 */
void *connect_uart_channel_dev(void *arg)
{
	ssize_t ret;
	struct channel_dev *c_dev = (struct channel_dev *)arg;
	struct uart_channel *c = c_dev->channel;
	char buf[CHANNEL_DEV_NAME_MAX + SYNC_LEN];

	snprintf(buf, sizeof(buf), SYNC_FMT, c->conf.identifier);
	/* TODO: will add SYNC resending */
	LOG_PRINTF("Send sync command:%s identifier=%s\n", buf, c->conf.identifier);
	ret = send_message_by_uart(c_dev->uart_device, (void *)buf, strlen(buf));
	if (ret < 0) {
		LOG_WRITE("Send sync command to service VM fail\n");
	} else {
		usleep(LISTEN_INTERVAL);
		memset(c_dev->buf, 0, sizeof(c_dev->buf));
		(void) receive_message_by_uart(c_dev->uart_device, (void *)c_dev->buf, sizeof(c_dev->buf));
		if (strncmp(ACK_SYNC, c_dev->buf, sizeof(ACK_SYNC)) == 0) {
			/** Add channel device instance into UART connection list */
			add_uart_channel_dev_connection_list(c_dev);
			LOG_WRITE("Lifecycle manager: connected\n");
		} else {
			ret = -1;
			LOG_PRINTF("Device in the (%s): failed to connect\n", c->conf.identifier);
		}
	}
	if (ret < 0)
		c_dev->polling = false;
	c_dev->listening = false;
	sem_post(&c_dev->dev_sem);
	return NULL;
}

void *poll_and_dispatch_uart_channel_events(void *arg)
{
	struct channel_dev *c_dev = (struct channel_dev *)arg;
	ssize_t num, ret;
	struct uart_channel *c;

	c = c_dev->channel;
	sem_wait(&c_dev->dev_sem);
	LOG_PRINTF("UART polling fd=%d...\n", get_uart_dev_fd(c_dev->uart_device));
	while (c_dev->polling) {
		memset(c_dev->buf, 0, sizeof(c_dev->buf));
		num = receive_message_by_uart(c_dev->uart_device, (void *)c_dev->buf,
								sizeof(c_dev->buf));
		/**
		 * Resend message if resend_time is set.
		 */
		if (num == 0) {
			if (c_dev->resend_time > 1) {
				usleep(LISTEN_INTERVAL + SECOND_TO_US);
				LOG_PRINTF("Resend (%s) to (%s)\n", c_dev->resend_buf, c_dev->name);
				ret = send_message_by_uart(c_dev->uart_device, (void *)c_dev->resend_buf,
								strlen(c_dev->resend_buf));
				if (ret < 0)
					LOG_WRITE("Send poweroff message to user VM fail\n");
				c_dev->resend_time--;
			} else if (c_dev->resend_time == 1) {
				memcpy(c_dev->buf, ACK_TIMEOUT, strlen(ACK_TIMEOUT));
				num = strlen(ACK_TIMEOUT);
			} else {
				/* No action if resend_time is 0 */
			}
		}
		if (num > 0) {
			parse_channel_dev_id(c_dev);
			c->data_handler((const char *)c_dev->buf, get_uart_dev_fd(c_dev->uart_device));
		}
	}
	LOG_PRINTF("Lifecycle manager stops to poll device:%s\n",
			get_uart_dev_path(c_dev->uart_device));
	return NULL;
}
struct channel_dev *find_uart_channel_dev(struct uart_channel *c, int fd)
{
	struct channel_dev *c_dev = NULL;

	LIST_FOREACH(c_dev, &c->tty_conn_head, list) {
		if (get_uart_dev_fd(c_dev->uart_device) == fd)
			break;
	}
	return c_dev;
}
struct channel_dev *find_uart_channel_dev_by_name(struct uart_channel *c, char *name)
{
	struct channel_dev *c_dev = NULL;

	LIST_FOREACH(c_dev, &c->tty_conn_head, list) {
		if (strncmp(name, c_dev->name, sizeof(c_dev->name)) == 0)
			break;
	}
	return c_dev;
}
/**
 * @brief Set message polling loop flag as flase and remove channel device instance from
 * the connection list
 *
 * @param c_dev point to uart channel device instance
 * @param c point to uart channel instance
 */
void disconnect_uart_channel_dev(struct channel_dev *c_dev, struct uart_channel *c)
{
	c_dev->listening = false;
	c_dev->polling = false;

	pthread_mutex_lock(&c->tty_conn_list_lock);
	LIST_REMOVE(c_dev, list);
	pthread_mutex_unlock(&c->tty_conn_list_lock);
}
/**
 * @brief Traverse uart channel open list to set listening loop flag and polling loop flag
 * to false for each channel device which is in listening state.
 *
 * @param c point to uart channel instance
 */
void stop_listen_uart_channel_dev(struct uart_channel *c)
{
	struct channel_dev *c_dev;

	LIST_FOREACH(c_dev, &c->tty_open_head, open_list) {
		if (c_dev->listening) {
			LOG_PRINTF("Stop to listen uart device (%s)\n",
					get_uart_dev_path(c_dev->uart_device));
			c_dev->listening = false;
			c_dev->polling = false;
			sem_post(&c_dev->dev_sem);
		}
	}
}
void start_uart_channel_dev_resend(struct channel_dev *c_dev, char *resend_buf, unsigned int resend_time)
{
	if (resend_time < MIN_RESEND_TIME)
		resend_time = MIN_RESEND_TIME;
	strncpy(c_dev->resend_buf, resend_buf, CHANNEL_DEV_BUF_LEN - 1);
	c_dev->resend_time = resend_time + 1;
}
void start_all_uart_channel_dev_resend(struct uart_channel *c, char *msg, unsigned int resend_time)
{
	struct channel_dev *c_dev;

	/* Enable resend for all connected uart channel devices */
	pthread_mutex_lock(&c->tty_conn_list_lock);
	LIST_FOREACH(c_dev, &c->tty_conn_head, list) {
		start_uart_channel_dev_resend(c_dev, msg, resend_time);
	}
	pthread_mutex_unlock(&c->tty_conn_list_lock);
}
void stop_uart_channel_dev_resend(struct channel_dev *c_dev)
{
	if (c_dev->resend_time == 1U)
		LOG_PRINTF("Timeout of receiving ACK message from (%s)\n", c_dev->name);
	c_dev->resend_time = 0U;
	memset(c_dev->resend_buf, 0x0, CHANNEL_DEV_BUF_LEN);
}
/**
 * @brief Send message to each connected uart channel device
 *
 * @param c uart channel instance
 * @param msg pointer which points to the message to be sent
 */
void notify_all_connected_uart_channel_dev(struct uart_channel *c, char *msg)
{
	struct channel_dev *c_dev;

	/* Send message to all tty connected devices*/
	pthread_mutex_lock(&c->tty_conn_list_lock);
	LIST_FOREACH(c_dev, &c->tty_conn_head, list) {
		LOG_PRINTF("Send (%s) to (%s)\n", msg, c_dev->name);
		(void) send_message_by_uart(c_dev->uart_device, msg, strlen(msg));
	}
	pthread_mutex_unlock(&c->tty_conn_list_lock);
}
bool is_uart_channel_connection_list_empty(struct uart_channel *c)
{
	bool ret = false;

	pthread_mutex_lock(&c->tty_conn_list_lock);
	if (LIST_EMPTY(&c->tty_conn_head))
		ret = true;
	pthread_mutex_unlock(&c->tty_conn_list_lock);
	return ret;
}
struct channel_dev *create_uart_channel_dev(struct uart_channel *c, char *path, data_handler_f *fn)
{
	struct uart_dev *dev;
	struct channel_dev *c_dev;

	if (c == NULL || path == NULL || fn == NULL)
		return NULL;
	c->data_handler = fn;
	dev = init_uart_dev(path);
	if (dev == NULL) {
		LOG_PRINTF("Failed to initialize UART device %s\n", path);
		return NULL;
	}

	c_dev = calloc(1, sizeof(*c_dev));
	if (!c_dev) {
		LOG_PRINTF("%s: Failed to allocate memory for UART channel device\n", __func__);
		deinit_uart_dev(dev);
		return NULL;
	}
	memset(c_dev, 0x0, sizeof(*c_dev));
	c_dev->uart_device = dev;
	c_dev->channel = c;
	c_dev->listening = true;
	c_dev->polling = true;
	sem_init(&c_dev->dev_sem, 0, 0);
	/** Add channel device instance into open list */
	pthread_mutex_lock(&c->tty_conn_list_lock);
	LIST_INSERT_HEAD(&c->tty_open_head, c_dev, open_list);
	pthread_mutex_unlock(&c->tty_conn_list_lock);
	return c_dev;
}
static void destroy_uart_channel_devs(struct uart_channel *c)
{
	struct channel_dev *c_dev, *tc_dev;

	list_foreach_safe(c_dev, &c->tty_open_head, open_list, tc_dev) {
		pthread_mutex_lock(&c->tty_conn_list_lock);
		LIST_REMOVE(c_dev, open_list);
		pthread_mutex_unlock(&c->tty_conn_list_lock);

		deinit_uart_dev(c_dev->uart_device);
		free(c_dev);
	}
}
void wait_uart_channel_devs_threads(struct uart_channel *c)
{
	struct channel_dev *c_dev;

	LIST_FOREACH(c_dev, &c->tty_open_head, open_list) {
		pthread_join(c_dev->listen_thread, NULL);
		pthread_join(c_dev->pool_thread, NULL);
	}
}
struct uart_channel *init_uart_channel(char *id)
{
	struct uart_channel *c;

	if (id == NULL) {
		LOG_PRINTF("%s:invlid parameter\n", __func__);
		return NULL;
	}
	c = calloc(1, sizeof(*c));
	if (!c) {
		LOG_PRINTF("%s: Failed to allocate memory for UART channel\n", __func__);
		return NULL;
	}
	c->data_handler = NULL;
	LIST_INIT(&c->tty_conn_head);
	LIST_INIT(&c->tty_open_head);
	pthread_mutex_init(&c->tty_conn_list_lock, NULL);
	memcpy(c->conf.identifier, id, strlen(id));
	return c;
}
void deinit_uart_channel(struct uart_channel *c)
{
	if (c != NULL) {
		destroy_uart_channel_devs(c);
		free(c);
	}
}
