/*
 * Copyright (C)2019 Intel Corporation
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
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SHUTDOWN_CMD		"shutdown"
#define ACK_CMD			"acked"
#define FAIL_CMD		"fail"
#define BUFF_SIZE		16U
#define MSG_SIZE		8U
#define NODE_SIZE		3U
#define TRY_SEND_CNT		3U
#define SERVICE_VM_SOCKET_PORT	(0x2000U)
#define USER_VM_SOCKET_PORT	(SERVICE_VM_SOCKET_PORT + 1U)

/* life_mngr process run in Service VM or User VM */
enum process_env {
	PROCESS_UNKNOWN = 0,
	PROCESS_RUN_IN_SERVICE_VM,
	PROCESS_RUN_IN_USER_VM,
};

/* Enumerated shutdown state machine only for User VM thread */
enum shutdown_state {
	SHUTDOWN_REQ_WAITING = 0,	     /* Can receive shutdown cmd in this state */
	SHUTDOWN_ACK_WAITING,                /* Wait acked message from Service VM */
	SHUTDOWN_REQ_FROM_SERVICE_VM,        /* Trigger shutdown by Service VM */
	SHUTDOWN_REQ_FROM_USER_VM,             /* Trigger shutdown by User VM */

};

/* the file description for dev/ttyS1 */
static int tty_dev_fd = 0;
static enum shutdown_state shutdown_state = SHUTDOWN_REQ_WAITING;
static FILE *log_fd;

#define LOG_PRINTF(format, args...) \
do { fprintf(log_fd, format, args); \
		fflush(log_fd); } while (0)

#define LOG_WRITE(args) \
do { fwrite(args, 1, sizeof(args), log_fd); \
		fflush(log_fd); } while (0)

/* it read from vuart, and if end is '\0' or '\n' or len = buff-len it will return */
static int receive_message(int fd, uint8_t *buffer, int buf_len)
{
	int rc = 0, count = 0;

	do {
		rc = read(fd, buffer + count, buf_len - count);
		if (rc > 0) {
			count += rc;
			if ((buffer[count - 1] == '\0') || (buffer[count - 1] == '\n')
						|| (count == buf_len)) {
				break;
			}
		}
	} while (rc > 0);

	return count;
}

static int send_message(int fd, char *buf, int len)
{
	int ret = 0;

	ret = write(fd, buf, len);

	return (ret == len) ? (0) : (-1);
}

static int set_tty_attr(int fd, int speed)
{
	struct termios tty;

	if (tcgetattr(fd, &tty) < 0) {
		LOG_PRINTF("error from tcgetattr: %s\n", strerror(errno));
		return errno;
	}

	cfsetospeed(&tty, (speed_t)speed);
	cfsetispeed(&tty, (speed_t)speed);

	/* set input-mode */
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK |
			ISTRIP | INLCR | IGNCR | ICRNL | IXON);

	/* set output-mode */
	tty.c_oflag &= ~OPOST;

	/* set control-mode */
	tty.c_cflag |= (CLOCAL | CREAD | CS8);
	tty.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);

	/* set local-mode */
	tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

	/* block until one char read, set next char's timeout */
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 1;

	tcflush(fd, TCIOFLUSH);

	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		LOG_PRINTF("error from tcsetattr: %s\n", strerror(errno));
		return errno;
	}
	return 0;
}

static int setup_socket_listen(unsigned short port)
{
	int listen_fd;
	struct sockaddr_in socket_addr;
	int opt = SO_REUSEADDR;

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd == -1) {
		LOG_WRITE("create an socket endpoint error!\n");
		exit(1);
	}

	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	memset(&socket_addr,  0, sizeof(socket_addr));
	socket_addr.sin_family = AF_INET;
	socket_addr.sin_port = htons(port);
	socket_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listen_fd, (struct sockaddr *)&socket_addr, sizeof(socket_addr)) == -1) {
		LOG_PRINTF("Bind to a socket(fd = 0x%x) error\n", listen_fd);
		close(listen_fd);
		exit(1);
	}

	if (listen(listen_fd, 5) == -1) {
		LOG_PRINTF("listen for connection error on a socket(fd=0x%x)\n", listen_fd);
		close(listen_fd);
		exit(1);
	}

	return listen_fd;
}

/* this thread runs on Service VM:
 * communiate between lifecycle-mngr and acrn-dm process in Service VM side
 */
static void *service_vm_socket_thread(void *arg)
{
	int listen_fd, connect_fd, connect_fd2;
	struct sockaddr_in client;
	socklen_t len = sizeof(struct sockaddr_in);
	fd_set fds;
	int flags, nfds, num, ret;
	char buf[BUFF_SIZE];

	listen_fd = setup_socket_listen(SERVICE_VM_SOCKET_PORT);
	LOG_PRINTF("life_mngr:listen_fd=0x%x socket port is 0x%x\r\n", listen_fd, SERVICE_VM_SOCKET_PORT);

	connect_fd = accept(listen_fd, (struct sockaddr *)&client, &len);
	if (connect_fd == -1) {
		LOG_PRINTF("%s: accept error: %s\r\n", __func__, strerror(errno));
		close(listen_fd);
		return NULL;
	}

	LOG_WRITE("socket connection accepted\n");

	/* make the socket non-blocking for compatibility with select */
	if ((flags = fcntl(listen_fd, F_GETFL, 0)) == -1 ||
		fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		LOG_PRINTF("%s: fcntl error: %s\r\n", __func__, strerror(errno));
		goto out;
	}

	nfds = (connect_fd > listen_fd) ? (connect_fd + 1) : (listen_fd + 1);

	while (1) {
		FD_ZERO(&fds);
		FD_SET(listen_fd, &fds);
		FD_SET(connect_fd, &fds);

		ret = select(nfds, &fds, NULL, NULL, NULL);

		if (ret < 0) {
			LOG_PRINTF("%s: select error: %s\r\n", __func__, strerror(errno));
			break;
		}

		if (FD_ISSET(connect_fd, &fds)) {
			memset(buf, 0, sizeof(buf));

			/* Here assume the socket communication is reliable, no need to try */
			num = read(connect_fd, buf, sizeof(buf));

			if (num == -1) {
				LOG_PRINTF("read error on a socket(fd = 0x%x)\n", connect_fd);
				break;
			} else if (num == 0) {
				LOG_PRINTF("socket(fd = 0x%x) closed\n", connect_fd);
				break;
			}

			LOG_PRINTF("received msg [%s]\r\n", buf);
			if (strncmp(SHUTDOWN_CMD, (const char *)buf, strlen(SHUTDOWN_CMD)) == 0) {
				ret = send_message(connect_fd, ACK_CMD, sizeof(ACK_CMD));
				if (ret != 0) {
					LOG_WRITE("Send acked message to acrn-dm VM fail\n");
				}
				LOG_WRITE("Receive shutdown command from User VM\r\n");
				ret = system("~/s5_trigger.sh sos");
				LOG_PRINTF("call s5_trigger.sh ret=0x%x\r\n", ret);
				break;
			}
		}
		if (FD_ISSET(listen_fd, &fds)) {
			connect_fd2 = accept(listen_fd, (struct sockaddr *)&client, &len);

			if (connect_fd2 == -1) {
				LOG_PRINTF("%s: accept for second socket connection error: %s\r\n",
						__func__, strerror(errno));
			} else {
				close(connect_fd2);
				LOG_WRITE("second socket connection detected - closing\n");
			}
		}

	}

out:
	close(listen_fd);
	close(connect_fd);
	return NULL;
}

/* this thread runs on User VM:
 * User VM wait for message from Service VM
 */
static void *listener_fn_to_service_vm(void *arg)
{

	int ret;
	int retry = TRY_SEND_CNT;
	bool shutdown_self = false;
	unsigned char buf[BUFF_SIZE];

	/* User VM server wait for message from Service VM */
	do {
		memset(buf, 0, sizeof(buf));
		ret = receive_message(tty_dev_fd, buf, sizeof(buf));
		if (ret > 0) {
			LOG_PRINTF("receive buf :%s ret=0x%x shutdown_state=0x%x\r\n", buf, ret, shutdown_state);
		}

		switch (shutdown_state) {
		/* it can receive shutdown command from Service VM */
		case SHUTDOWN_REQ_WAITING:
		case SHUTDOWN_REQ_FROM_SERVICE_VM:
			if ((ret > 0) && (strncmp(SHUTDOWN_CMD, (const char *)buf, strlen(SHUTDOWN_CMD)) == 0)) {
				shutdown_state = SHUTDOWN_REQ_FROM_SERVICE_VM;
				ret = send_message(tty_dev_fd, ACK_CMD, sizeof(ACK_CMD));
				if (ret != 0) {
					LOG_WRITE("User VM send acked message failed!\n");
				} else {
					shutdown_self = true;
				}
				LOG_WRITE("User VM start shutdown\n");
			}
			break;

		/* it will try to resend shutdown cmd to Service VM if there is no acked message */
		case SHUTDOWN_ACK_WAITING:
			if ((ret > 0) && (strncmp(ACK_CMD, (const char *)buf, strlen(ACK_CMD)) == 0)) {
				LOG_WRITE("received acked message from Service VM\n");
				shutdown_self = true;
			} else {
				if (retry  > 0) {
					if (send_message(tty_dev_fd, SHUTDOWN_CMD, sizeof(SHUTDOWN_CMD)) != 0) {
						LOG_PRINTF("Try resend shutdown cmd failed cnt = %d\n", retry);
					}
					retry--;
				} else {
					LOG_PRINTF("Cann't not receive acked message from Service VM, have try %d times\r\n",
							TRY_SEND_CNT);
					shutdown_state = SHUTDOWN_REQ_WAITING;
					retry = TRY_SEND_CNT;
				}
			}
			break;

		default:
			LOG_PRINTF("Invalid shutdown_state=0x%x\r\n",  shutdown_state);
			break;

		}

		/* will poweroff self*/
		if (shutdown_self) {
			break;
		}

		sleep(1);

	} while (1);

	ret = system("poweroff");
	return NULL;
}

/* this thread runs on User VM:
 * User VM wait for shutdown from other process (e.g. s5_trigger.sh)
 */
static void *listener_fn_to_operator(void *arg)
{
	int listen_fd, connect_fd;
	struct sockaddr_in client;
	socklen_t len = sizeof(struct sockaddr_in);
	int num, ret;
	char buf[BUFF_SIZE];

	listen_fd = setup_socket_listen(USER_VM_SOCKET_PORT);
	LOG_PRINTF("listen_fd=0x%x socket port is 0x%x\r\n",
			listen_fd, USER_VM_SOCKET_PORT);

	while (1) {
		connect_fd = accept(listen_fd, (struct sockaddr *)&client, &len);
		if (connect_fd == -1) {
			LOG_WRITE("accept a connection error on a socket\n");
			close(listen_fd);
			exit(1);
		}

		/* Here assume the socket communication is reliable, no need to try */
		num = read(connect_fd, buf, sizeof(SHUTDOWN_CMD));
		if (num == -1) {
			LOG_PRINTF("read error on a socket(fd=0x%x)\n", connect_fd);
			close(connect_fd);
			close(listen_fd);
			exit(1);
		}

		LOG_PRINTF("Receive msg is %s by the socket\r\n", buf);
		if (strncmp(SHUTDOWN_CMD, (const char *)buf, strlen(SHUTDOWN_CMD)) == 0) {
			if (shutdown_state != SHUTDOWN_REQ_WAITING) {
				LOG_PRINTF("Cann't handle shutdown cmd during shutdowning...\
					    shutdown_state=0x%x\n", shutdown_state);
				if (send_message(connect_fd, FAIL_CMD, sizeof(FAIL_CMD)) != 0) {
					LOG_WRITE("Send fail message to the initiator failed\n");
				}
				continue;
			}
			shutdown_state = SHUTDOWN_REQ_FROM_USER_VM;
			/* send acked message to the caller */
			LOG_WRITE("Send acked message to the caller\r\n");
			ret = send_message(connect_fd, ACK_CMD, sizeof(ACK_CMD));
			if (ret != 0) {
				LOG_WRITE("Send acked message fail\n");
			}

			LOG_WRITE("send shutdown message to Service VM\r\n");
			/* send shutdown command to the Servcie VM  */
			ret = send_message(tty_dev_fd, SHUTDOWN_CMD, sizeof(SHUTDOWN_CMD));
			if (ret != 0) {
				LOG_WRITE("Send shutdown command to Service VM fail\n");
				shutdown_state = SHUTDOWN_REQ_WAITING;
			} else {
				shutdown_state = SHUTDOWN_ACK_WAITING;
			}
			LOG_PRINTF("Write shutdown msg ret=0x%x\r\n", ret);
		}
	}

	close(connect_fd);
	close(listen_fd);
	return NULL;
}

int main(int argc, char *argv[])
{

	int ret = 0;
	char *devname_user_vm = "";
	enum process_env env = PROCESS_UNKNOWN;
	pthread_t service_vm_socket_pid;
	/* User VM wait for shutdown from Service VM */
	pthread_t user_vm_thread_pid_1;
	/* User VM wait for shutdown from other process */
	pthread_t user_vm_thread_pid_2;

	log_fd = fopen("/var/log/life_mngr.log", "w+");
	if (log_fd == NULL) {
		printf("open log file failed\r\n");
		return -EINVAL;
	}

	if (argc <= 2) {
		LOG_WRITE("Too few options. Example: [./life_mngr uos /dev/ttyS1] or ./life_mngr sos /dev/ttyS1]\n");
		fclose(log_fd);
		return -EINVAL;
	}

	if (strncmp("uos", argv[1], NODE_SIZE) == 0) {
		env = PROCESS_RUN_IN_USER_VM;
		devname_user_vm = argv[2];
		tty_dev_fd = open(devname_user_vm, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
		if (tty_dev_fd < 0) {
			LOG_PRINTF("Error opening %s: %s\n", devname_user_vm, strerror(errno));
			fclose(log_fd);
			return errno;
		}
		if (set_tty_attr(tty_dev_fd, B115200) != 0) {
			return -EINVAL;
		}

		ret = pthread_create(&user_vm_thread_pid_1, NULL, listener_fn_to_service_vm, NULL);
		ret = pthread_create(&uer_vm_thread_pid_2, NULL, listener_fn_to_operator, NULL);

	} else if (strncmp("sos", argv[1], NODE_SIZE) == 0) {
		env = PROCESS_RUN_IN_SERVICE_VM;
		ret = pthread_create(&service_vm_socket_pid, NULL, service_vm_socket_thread, NULL);
	} else {
		LOG_WRITE("Invalid param. Example: [./life_mngr uos /dev/ttyS1] or ./life_mngr sos /dev/ttyS1]\n");
		fclose(log_fd);
		return -EINVAL;
	}

	if (env == PROCESS_RUN_IN_SERVICE_VM) {
		pthread_join(service_vm_socket_pid, NULL);
	} else if (env == PROCESS_RUN_IN_USER_VM) {
		pthread_join(user_vm_thread_pid_1, NULL);
		pthread_join(user_vm_thread_pid_2, NULL);
		close(tty_dev_fd);
	}
	fclose(log_fd);

	return ret;
}
