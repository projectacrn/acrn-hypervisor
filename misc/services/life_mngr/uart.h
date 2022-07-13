/*
 * Copyright (C)2021-2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _UART_H_
#define _UART_H_
#include <sys/types.h>
#include <stdint.h>
#include <sys/queue.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/un.h>

#define TTY_PATH_MAX 32U

#define SECOND_TO_US 1000000
#define WAIT_RECV (SECOND_TO_US>>2)
#define RETRY_RECV_TIMES 100U

struct uart_dev {
	char tty_path[TTY_PATH_MAX]; /**< UART device name */
	int tty_fd; /**< the FD of opened UART device */
};
/**
 * @brief Allocate UART device instance and initialize UART
 * device according to device name
 *
 * @param path UART device name
 * @return struct uart_dev* Ponit to UART device instance
 */
struct uart_dev *init_uart_dev(char *path);
/**
 * @brief Close UART devcie and free UART device instance
 *
 * @param dev Poin to UART device instance
 */
void deinit_uart_dev(struct uart_dev *dev);
/**
 * @brief Set handler to handle received message
 */
ssize_t send_message_by_uart(struct uart_dev *dev, const void *buf, size_t len);
/**
 * @brief Receive message and retry RETRY_RECV_TIMES time to
 * avoid miss message in some cases.
 */
ssize_t receive_message_by_uart(struct uart_dev *dev, void *buf, size_t len);
/**
 * @brief Get the file descriptor of a UART device
 */
static inline int get_uart_dev_fd(struct uart_dev *dev)
{
	return dev->tty_fd;
}
/**
 * @brief Get the name of a UART device
 */
static inline char *get_uart_dev_path(struct uart_dev *dev)
{
	return dev->tty_path;
}
#endif

