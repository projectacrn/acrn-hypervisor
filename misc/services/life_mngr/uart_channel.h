/*
 * Copyright (C)2021-2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _UART_CHANNEL_H_
#define _UART_CHANNEL_H_
#include <sys/queue.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/un.h>
#include "uart.h"

#define WAIT_USER_VM_POWEROFF (10*SECOND_TO_US)

#define CHANNEL_DEV_NAME_MAX 128U
#define CHANNEL_DEV_BUF_LEN 256U

#define MIN_RESEND_TIME 3U
#define LISTEN_INTERVAL (5 * SECOND_TO_US)

typedef void data_handler_f(const char *cmd_name, int fd);

struct channel_dev {
	struct uart_dev *uart_device;
	char name[CHANNEL_DEV_NAME_MAX]; /**< channel device name */
	bool listening; /**< listen thread loop flag */
	bool polling; /**< message polling thread loop flag */
	pthread_t listen_thread;
	pthread_t pool_thread;

	char buf[CHANNEL_DEV_BUF_LEN]; /**< store received message */

	LIST_ENTRY(channel_dev) list; /**< list node used in UART connection list */
	LIST_ENTRY(channel_dev) open_list; /**< list node used UART opening list */

	struct uart_channel *channel; /**< point to UART server */
	sem_t dev_sem; /**< semaphore used to start polling message */
	char resend_buf[CHANNEL_DEV_BUF_LEN]; /**< store the message that will be sent */
	unsigned int resend_time; /**< the time which the message will be resent */
};
struct channel_config {
	char identifier[CHANNEL_DEV_NAME_MAX]; /**< the user VM name which is configured by user */
};
struct uart_channel {
	data_handler_f *data_handler;
	LIST_HEAD(tty_head, channel_dev) tty_conn_head; /* UART connection list */
	LIST_HEAD(tty_open_head, channel_dev) tty_open_head; /* UART opening list */
	pthread_mutex_t tty_conn_list_lock;

	struct channel_config conf;
};
/**
 * @brief Initialize each field of uart channel instance, such as
 * a lock and configuration of uart channel
 */
struct uart_channel *init_uart_channel(char *id);
/**
 * @brief Create one uart channel device according to device name
 *
 * Create one channel device instance to store information about
 * one uart channel device which will be opened.
 * For master channel, create two threads, one thread
 * is to listen and wait sync messaage from slave channel, another thread
 * is to poll message from slave channel.
 * For slave channel, create one thread to send sync message
 * to master channel every 5 second until acked sync
 * message is received from master channel and poll meessage from master channel.
 *
 * @param uart  point to uart server
 * @param path	start address of the name of the device which will
 *              be opened
 * @param fn	the handler of handling message
 */
struct channel_dev *create_uart_channel_dev(struct uart_channel *c, char *path, data_handler_f *fn);

/**
 * @brief Wait uart channel devices threads to exit
 */
void wait_uart_channel_devs_threads(struct uart_channel *c);
/**
 * @brief Destroy uart channel and release channel device instance
 */
void deinit_uart_channel(struct uart_channel *c);
/**
 * @brief Wait to connect device in uart channel
 *
 * Wait sync message from slave channel device, parse slave channel device
 * indentifier from sync message, then add channel device into uart channel
 * device connection list.
 */
void *listen_uart_channel_dev(void *arg);
/**
 * @brief Wait to connect device in the uart channel
 *
 * Send sync message every 5 second and wait acked sync message from master
 * channel device, add uart channel device instance into uart connection list.
 */
void *connect_uart_channel_dev(void *arg);
/**
 * @brief Poll and dispatch message received from uart channel
 *
 * If resend time is set, this interface will resend message unit the ACK message
 * is received.
 */
void *poll_and_dispatch_uart_channel_events(void *arg);
/**
 * @brief Find uart channel device instance according to fd
 */
struct channel_dev *find_uart_channel_dev(struct uart_channel *c, int fd);
/**
 * @brief Find uart channel device instance according to device name
 */
struct channel_dev *find_uart_channel_dev_by_name(struct uart_channel *c, char *name);
/**
 * @brief Disconnect uart channel device instance
 */
void disconnect_uart_channel_dev(struct channel_dev *c_dev, struct uart_channel *c);
/**
 * @brief Stop to listen uart channel device
 */
void stop_listen_uart_channel_dev(struct uart_channel *c);
/**
 * @brief Set the uart channel device resending buffer and resending time
 *
 * If ACK message is not received during specified time, resend
 * message.
 */
void start_uart_channel_dev_resend(struct channel_dev *c_dev, char *resend_buf, unsigned int resend_time);
/**
 * @brief Start to resend for all connected uart channel devices
 */
void start_all_uart_channel_dev_resend(struct uart_channel *c, char *msg, unsigned int resend_time);
/**
 * @brief Stop the uart channel device resending buffer and resending time
 */
void stop_uart_channel_dev_resend(struct channel_dev *c_dev);
/**
 * @brief Broadcast message to each connected uart channel device
 */
void notify_all_connected_uart_channel_dev(struct uart_channel *c, char *msg);
/**
 * @brief Check whether uart channel connection list is empty or not
 */
bool is_uart_channel_connection_list_empty(struct uart_channel *c);
#endif

