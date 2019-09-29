/*
 * Copyright (C)2019 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <windows.h>
#include <stdio.h>


#define SOS_REQ         "shutdown"
#define UOS_ACK         "acked"
#define BUFF_SIZE       16U
#define MSG_SIZE        8U

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
		printf("open %s failed!\n", szStr);
		return hCom;
	}

	printf("open %s successed!\n", szStr);
	SetupComm(hCom, 1024, 1024);

	COMMTIMEOUTS TimeOuts;
	TimeOuts.ReadIntervalTimeout = 100;		/* Maximum time between read chars. */
	TimeOuts.ReadTotalTimeoutMultiplier = 5000;	/* Multiplier of characters.        */
	TimeOuts.ReadTotalTimeoutConstant = 5000;	/* Constant in milliseconds.        */
	TimeOuts.WriteTotalTimeoutMultiplier = 500;	/* Multiplier of characters.        */
	TimeOuts.WriteTotalTimeoutConstant = 2000;	/* Constant in milliseconds.        */
	SetCommTimeouts(hCom, &TimeOuts);

	DCB dcb;
	GetCommState(hCom, &dcb);
	dcb.BaudRate = 115200;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	SetCommState(hCom, &dcb);

	return hCom;
}

int main()
{
	DWORD recvsize = 0;
	char recvbuf[BUFF_SIZE];

	HANDLE hCom = initCom("COM2");
	if (hCom == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	do {
		memset(recvbuf, 0, sizeof(recvbuf));
		ReadFile(hCom, recvbuf, sizeof(recvbuf), &recvsize, NULL);
		if (recvsize < MSG_SIZE)
		{
			continue;
		}

		if (strncmp(recvbuf, SOS_REQ, MSG_SIZE) == 0)
		{
			WriteFile(hCom, UOS_ACK, sizeof(UOS_ACK), NULL, NULL);
			system("shutdown -s -t 0");
			break;
		}
	} while (1);

	return 0;
}

