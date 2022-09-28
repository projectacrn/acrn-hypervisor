/*
 * Copyright (C)2019-2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdbool.h>
#include <winsock2.h>
#include <windows.h>

#define WIN_VM_NAME		"windows"
#define REQ_SYS_SHUTDOWN	"req_sys_shutdown"

#define ACK_REQ_SYS_SHUTDOWN "ack_req_sys_shutdown"
#define SYNC_CMD		"sync"
#define ACK_SYNC      "ack_sync"
#define POWEROFF_CMD    "poweroff_cmd"
#define ACK_POWEROFF	"ack_poweroff"
#define USER_VM_SHUTDOWN  "user_vm_shutdown"
#define ACK_USER_VM_SHUTDOWN "ack_user_vm_shutdown"
#define USER_VM_REBOOT  "user_vm_reboot"
#define ACK_USER_VM_REBOOT "ack_user_vm_reboot"
#define REQ_SYS_REBOOT "req_sys_reboot"
#define ACK_REQ_SYS_REBOOT "ack_req_sys_reboot"
#define SYNC_FMT	"sync:%s"
#define S5_REJECTED	"system shutdown request is rejected"

#define BUFF_SIZE	(32U)
#define MSG_SIZE	(8U)
#define UVM_SOCKET_PORT (0x2001U)
#define READ_INTERVAL	(100U) /* The time unit is microsecond */
#define MIN_RESEND_TIME (3U)
#define SECOND_TO_MS	(1000U)
#define RETRY_RECV_TIMES	(100U)

HANDLE hCom2;
unsigned int resend_time;
char resend_buf[BUFF_SIZE];

void send_message_by_uart(HANDLE hCom, char *buf, unsigned int len)
{
	int i;
	DWORD written;

	for (i = 0; i < len; i++)
		WriteFile(hCom, &buf[i], 1, &written, NULL);

	WriteFile(hCom, "\n", 1, &written, NULL);
}
void start_uart_resend(char *buf, unsigned int time)
{
	if (resend_time < MIN_RESEND_TIME)
		resend_time = MIN_RESEND_TIME;
	strncpy(resend_buf, buf, BUFF_SIZE - 1);
	resend_time = time + 1U;
}
void stop_uart_resend(void)
{
	memset(resend_buf, 0x0, BUFF_SIZE);
	resend_time = 0U;
}
void handle_socket_request(SOCKET sClient, char *req_message)
{
	char ack_message[BUFF_SIZE];

	snprintf(ack_message, sizeof(ack_message), "ack_%s", req_message);
	/**
	 * The lifecycle manager in Service VM checks sync message every 5 seconds
	 * during listening phase, delay 6 seconds to wait Service VM to receive the
	 * sync message, then start to send message to Service VM.
	 */
	Sleep(6U * SECOND_TO_MS);
	send(sClient, ack_message, sizeof(ack_message), 0);
	start_uart_resend(req_message, MIN_RESEND_TIME);
	send_message_by_uart(hCom2, req_message, strnlen(req_message, BUFF_SIZE));
	Sleep(2U * READ_INTERVAL);
	return;
}
DWORD WINAPI open_socket_server(LPVOID lpParam)
{
	WORD sockVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	char revData[BUFF_SIZE];
	struct sockaddr_in sin;
	SOCKET sClient;
	struct sockaddr_in remoteAddr;
	int nAddrlen = sizeof(remoteAddr);
	int ret;

	ret = WSAStartup(sockVersion, &wsaData);
	if (ret != 0) {
		printf("Failed to initiate Windows Socket, error: %d\n", ret);
		return -1;
	}
	SOCKET slisten = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (slisten == INVALID_SOCKET) {
		printf("Socket error !\n");
		goto start_error;
	}
	sin.sin_family = AF_INET;
	sin.sin_port = htons(UVM_SOCKET_PORT);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(slisten, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR) {
		printf("Bind error !\n");
		goto sock_exit;
	}
	if (listen(slisten, 5) == SOCKET_ERROR) {
		printf("Listen error !\n");
		goto sock_exit;
	}
	printf("Wait for connect ...\n");
	sClient = accept(slisten, (SOCKADDR *)&remoteAddr, &nAddrlen);
	if (sClient == INVALID_SOCKET) {
		printf("Accept error\n");
		goto sock_exit;
	}
	printf("Accept one connect %s\n", inet_ntoa(remoteAddr.sin_addr));
	do {
		memset(revData, 0, sizeof(revData));
		int ret = recv(sClient, revData, BUFF_SIZE, 0);
		if (ret > 0) {
			revData[ret] = 0x00;
			printf(revData);
		}
		Sleep(READ_INTERVAL);
		if (strncmp(revData, REQ_SYS_SHUTDOWN, sizeof(REQ_SYS_SHUTDOWN)) == 0) {
			handle_socket_request(sClient, REQ_SYS_SHUTDOWN);
			break;
		}
		if (strncmp(revData, REQ_SYS_REBOOT, sizeof(REQ_SYS_REBOOT)) == 0) {
			handle_socket_request(sClient, REQ_SYS_REBOOT);
			break;
		}
	} while (1);
	closesocket(sClient);
sock_exit:
	closesocket(slisten);
start_error:
	WSACleanup();
	return 0;
}

HANDLE initCom(const char *szStr)
{
	HANDLE hCom = CreateFile(szStr,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (hCom == INVALID_HANDLE_VALUE)
	{
		printf("Opening %s failed!\n", szStr);
		return hCom;
	}

	printf("%s opened succesfully!", szStr);
	SetupComm(hCom, 1024, 1024);

	COMMTIMEOUTS TimeOuts;
	TimeOuts.ReadIntervalTimeout = MAXDWORD; /* Maximum time between read chars. */
	TimeOuts.ReadTotalTimeoutMultiplier = 0; /* Multiplier of characters. */
	TimeOuts.ReadTotalTimeoutConstant = 0;	/* Constant in milliseconds. */
	TimeOuts.WriteTotalTimeoutMultiplier = 500; /* Multiplier of characters. */
	TimeOuts.WriteTotalTimeoutConstant = 100; /* Constant in milliseconds. */
	SetCommTimeouts(hCom, &TimeOuts);

	DCB dcb = {0};
	dcb.BaudRate = 115200;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;

	SetCommState(hCom, &dcb);
	PurgeComm(hCom, PURGE_TXCLEAR | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_RXABORT);
	return hCom;
}

int main(int argc, char **argv)
{
	DWORD recvsize = 0;
	char recvbuf[BUFF_SIZE];
	char buf[BUFF_SIZE];
	DWORD dwError;
	DWORD threadId;
	bool poweroff = false;
	bool reboot = false;
	unsigned int retry_times;

	if (argc > 2)
		return -1;

	if ((argc == 2) && (sizeof(argv[1]) + 5 > BUFF_SIZE)) {
		printf("VM name (%s) is too long\n", argv[1]);
		return -1;
	}

	hCom2 = initCom("COM2");
	if (hCom2 == INVALID_HANDLE_VALUE)
		return -1;
	memset(buf, 0, sizeof(buf));
	memset(resend_buf, 0, sizeof(resend_buf));
	CreateThread(NULL, 0, open_socket_server, NULL, 0, &threadId);
	if (ClearCommError(hCom2, &dwError, NULL)) {
		PurgeComm(hCom2, PURGE_TXABORT | PURGE_TXCLEAR);
	}

	snprintf(buf, sizeof(buf), SYNC_FMT, (argc == 1) ? WIN_VM_NAME : argv[1]);
	start_uart_resend(buf, MIN_RESEND_TIME);
	send_message_by_uart(hCom2, buf, strlen(buf));
	/**
	 * The lifecycle manager in Service VM checks sync message every 5 seconds
	 * during listening phase, delay 5 seconds to wait Service VM to receive the
	 * sync message, then start to read ack message from Service VM.
	 */
	Sleep(5U * SECOND_TO_MS);
	do {
		do {
			retry_times = RETRY_RECV_TIMES;
			memset(recvbuf, 0, sizeof(recvbuf));
			do {
				ReadFile(hCom2, recvbuf, sizeof(recvbuf), &recvsize, NULL);
				Sleep(READ_INTERVAL);
				retry_times--;
			} while ((recvsize < MSG_SIZE) && (retry_times > 0));
			if (recvsize < MSG_SIZE) {
				if (resend_time > 1U) {
					Sleep(6U * SECOND_TO_MS);
					printf("Resend command (%s) service VM\n", resend_buf);
					send_message_by_uart(hCom2, resend_buf, strlen(resend_buf));
					resend_time--;
				} else if (resend_time == 1U) {
					printf("Failed to resend command (%s)\n", resend_buf);
					break;
				} else {
					/* No action if resend_time is 0 */
				}
			}
		} while (recvsize < MSG_SIZE);

		if (resend_time == 1U)
			break;
		if (recvbuf[recvsize - 1] == '\n')
			recvbuf[recvsize - 1] = '\0';

		if (strncmp(recvbuf, ACK_SYNC, sizeof(ACK_SYNC)) == 0)
		{
			stop_uart_resend();
			printf("Received acked sync message from service VM\n");
		} else if (strncmp(recvbuf, ACK_REQ_SYS_SHUTDOWN, sizeof(ACK_REQ_SYS_SHUTDOWN)) == 0) {
			stop_uart_resend();
			printf("Received acked system shutdown request from service VM\n");
		} else if (strncmp(recvbuf, ACK_REQ_SYS_REBOOT, sizeof(ACK_REQ_SYS_REBOOT)) == 0) {
			stop_uart_resend();
			printf("Received acked system reboot request from service VM\n");
		} else if (strncmp(recvbuf, POWEROFF_CMD, sizeof(POWEROFF_CMD)) == 0) {
			printf("Received system shutdown message from service VM\n");
			send_message_by_uart(hCom2, ACK_POWEROFF, sizeof(ACK_POWEROFF));
			Sleep(2 * READ_INTERVAL);
			printf("Windows VM will shutdown.\n");
			poweroff = true;
			break;
		} else if (strncmp(recvbuf, USER_VM_SHUTDOWN, sizeof(USER_VM_SHUTDOWN)) == 0) {
			printf("Received guest shutdown message from service VM\n");
			send_message_by_uart(hCom2, ACK_USER_VM_SHUTDOWN, sizeof(ACK_USER_VM_SHUTDOWN));
			Sleep(2 * READ_INTERVAL);
			printf("Windows VM will shutdown.\n");
			poweroff = true;
			break;
		} else if (strncmp(recvbuf, USER_VM_REBOOT, sizeof(USER_VM_REBOOT)) == 0) {
			printf("Received guest reboot message from service VM\n");
			send_message_by_uart(hCom2, ACK_USER_VM_REBOOT, sizeof(ACK_USER_VM_REBOOT));
			Sleep(2 * READ_INTERVAL);
			printf("Windows VM will reboot.\n");
			reboot = true;
			break;
		} else {
			printf("Received invalid message (%s) from service VM.\n", recvbuf);
		}
	} while (1);
	CloseHandle(hCom2);
	if (poweroff)
		system("shutdown -s -t 0");
	if (reboot)
		system("shutdown -r -t 0");
	return 0;
}
