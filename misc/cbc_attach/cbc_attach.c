/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file
 *
 * The cbc_attach tool is a tool similar to ldattach or slcand.
 * It configures the given serial line for cbc and loads the cbc
 * line discipline. On exit it switches the line discipline to n_tty.
 */

#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <sys/types.h>
#include <unistd.h>

#ifndef ANDROID_BUILD
#define APP_INFO "v" VERSION_STRING
#else
#define APP_INFO ""
#endif

#include <linux/tty.h>
#ifndef N_CBCCORE
#define N_CBCCORE 27
#endif

#define VERSION_MAJOR 4
#define VERSION_MINOR 2
#define VERSION_REVISON 2

#define VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION}"

char const *const cDEFAULT_DEVICE_NAME = "/dev/ttyS1";

static struct option longOpts[] = {
	{"help", no_argument, 0, 'h'},
	{"baudrate", required_argument, 0, 'b'},
	{"hardwareFlowControl", no_argument, 0, 'f'},
	{"min-receive-bytes", required_argument, 0, 'm'},
	{0, 0, 0, 0}
};
/* granularity is currently a module parameter -> not set from this tool. */

void printMainUsage(void)
{
	printf("cbc_attach tool %s - attach cbc line discpline ", APP_INFO);
	printf("to serial line\n\n");
	printf("Usage: cbc_attach [OPTION]... [TTY-DEVICE]...\n\n");
	printf("-h , --help                 Help\n");
	printf("-b , --baudrate=<Baudrate>  Baudrate e.g.:4000000\n");
	printf("-f , --hardwareFlowControl  Use hardware flow control\n");
	printf("-m , --min-receive-bytes    Minimum number of bytes to ");
	printf("receive from the serial device ");
	printf("(range:0-255, default:255)\n");
	printf("<tty-device>                Name of the serial line. ");
	printf("default: %s\n", cDEFAULT_DEVICE_NAME);
}


bool convertBaudRate(const uint32_t baudRateInt, speed_t *baudRate)
{
	switch (baudRateInt) {
	case 50:
		*baudRate = B50;
		break;
	case 75:
		*baudRate = B75;
		break;
	case 110:
		*baudRate = B110;
		break;
	case 134:
		*baudRate = B134;
		break;
	case 150:
		*baudRate = B150;
		break;
	case 200:
		*baudRate = B200;
		break;
	case 300:
		*baudRate = B300;
		break;
	case 600:
		*baudRate = B600;
		break;
	case 1200:
		*baudRate = B1200;
		break;
	case 1800:
		*baudRate = B1800;
		break;
	case 2400:
		*baudRate = B2400;
		break;
	case 4800:
		*baudRate = B4800;
		break;
	case 9600:
		*baudRate = B9600;
		break;
	case 19200:
		*baudRate = B19200;
		break;
	case 38400:
		*baudRate = B38400;
		break;
	case 57600:
		*baudRate = B57600;
		break;
	case 115200:
		*baudRate = B115200;
		break;
	case 230400:
		*baudRate = B230400;
		break;
	case 460800:
		*baudRate = B460800;
		break;
	case 500000:
		*baudRate = B500000;
		break;
	case 576000:
		*baudRate = B576000;
		break;
	case 921600:
		*baudRate = B921600;
		break;
	case 1000000:
		*baudRate = B1000000;
		break;
	case 1152000:
		*baudRate = B1152000;
		break;
	case 1500000:
		*baudRate = B1500000;
		break;
	case 2000000:
		*baudRate = B2000000;
		break;
	case 2500000:
		*baudRate = B2500000;
		break;
	case 3000000:
		*baudRate = B3000000;
		break;
	case 3500000:
		*baudRate = B3500000;
		break;
	case 4000000:
		*baudRate = B4000000;
		break;
	default:
		return false;
	}
	return true;
}


bool initTerminal(int deviceFd, const uint32_t baudRateInt,
				const bool useHardwareFlowControl,
				const uint8_t minReceiveBytes)
{
	bool success = true;
	struct termios terminalSettings;

	speed_t baudRate = B0;

	if (!convertBaudRate(baudRateInt, &baudRate)) {
		printf("Invalid baud rate given %i\n", baudRateInt);
		success = false;
	}

	if (success) {
		int res = tcgetattr(deviceFd, &terminalSettings);

		if (res < 0) {
			printf("Failed to get terminal settings (error: %s)\n",
						strerror(errno));
			success = false;
		}
	}

	if (success) {
		terminalSettings.c_cflag = 0;
		terminalSettings.c_iflag = 0;

		/* set 8n1 */
		terminalSettings.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
		terminalSettings.c_cflag |= CS8 | CLOCAL | CREAD;
		terminalSettings.c_iflag |= IGNPAR;

		if (useHardwareFlowControl)
			/* Enable hardware flow control. */
			terminalSettings.c_cflag |= CRTSCTS;
		else
			/* Disable hardware flow control.*/
			terminalSettings.c_cflag &= ~CRTSCTS;

		/* Disable software flow control. */
		terminalSettings.c_iflag  &= ~(IXON | IXOFF | IXANY);

		/* Set raw mode. */
		cfmakeraw(&terminalSettings);

		/* Set VTIME to 1 to get a read() timeout of 100ms. */
		terminalSettings.c_cc[VTIME] = 1u;

		/* Set VMIN. */
		terminalSettings.c_cc[VMIN] = minReceiveBytes;

		/* Set baudrate. */
		int res = cfsetspeed(&terminalSettings, baudRate);

		if (res != 0) {
			printf("Failed to set serial speed (error: %s)\n",
					strerror(errno));
			success = false;
		}
	}

	if (success) {
		int res = cfsetospeed(&terminalSettings, baudRate);

		if (res != 0) {
			printf("Failed to set o/p serial speed (error: %s)\n",
							strerror(errno));
			success = false;
		}
	}

	if (success) {
		(void)tcflush(deviceFd, TCIFLUSH);
		int res = tcsetattr(deviceFd, TCSANOW, &terminalSettings);

		if (res != 0) {
			printf("Failed to set terminal settings (error: %s)\n",
						strerror(errno));
			success = false;
		}
	}

	return success;
}


void cbc_attach_shutdown(int *const deviceFd)
{
	if (*deviceFd > 0) {
		int disc = N_TTY;
		int res = ioctl(*deviceFd, TIOCSETD, &disc);

		if (res != 0)
			printf("Failed to set line disc tty (error: %s)\n",
					strerror(errno));

		res = close(*deviceFd);
		if (res != 0)
			printf("Failed to close serial device (error: %s)\n",
						strerror(errno));
		else
			*deviceFd = -1;
	}
}


bool openDevice(int *const deviceFd, char const *const deviceName)
{
	bool success = true;

	*deviceFd = open(deviceName, O_RDONLY | O_NOCTTY);
	if (*deviceFd < 0) {
		printf("Failed to open serial device %s, error: %s\n",
					deviceName, strerror(errno));
		success = false;
	}
	return success;
}


bool init(int *const deviceFd,
	  char const *const deviceName,
	  uint32_t const baudRateInt,
	  bool const useHardwareFlowControl,
	  uint8_t const minReceiveBytes)
{
	/* TODO check whether VMIN/VTIME handling is necessary */
	bool success = true;

	success = openDevice(deviceFd, deviceName);

	if (success) {
		success = initTerminal(*deviceFd, baudRateInt,
					useHardwareFlowControl,
					minReceiveBytes);
	}

	if (success) {
		/* Set line discipline. N_CBCCORE is unknown on host.
		 * Use magic number instead of define.
		 */
		int disc = N_CBCCORE;
		int res = ioctl(*deviceFd, TIOCSETD, &disc);

		if (res != 0) {
			printf("Failed to set line disc cbc (error: %s)\n",
							strerror(errno));
			success = false;
		}
	}

	/* Close the device if initialization has failed. */
	if (!success)
		cbc_attach_shutdown(deviceFd);

	return success;
}


int main(int argc, char **argv)
{
	int optionIndex;
	int c;
	bool success;

	char const *deviceName = cDEFAULT_DEVICE_NAME;
	int baudrate = 4000000;
	bool useHwFlowControl = false;
	uint8_t minReceiveBytes = 255;
	int deviceFd = 0;

	/* Retry times and uint */
	int retry_time = 30;
	int retry_uint = 2;

	/* Try to get the CBC device name from an environment variable. */
	char const *envDeviceName = getenv("CBC_TTY");

	if (envDeviceName)
		deviceName = envDeviceName;

	/* Parse command line options. */
	if (argc == 0)
		return -1;

	while (1) {
		c = getopt_long(argc, argv, "hb:f", longOpts, &optionIndex);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			printMainUsage();
			return 0;

		case 'b':
			if (optarg != NULL) {
				baudrate = atoi(optarg);
				if (baudrate == 0) {
					printf("Unknown baudrate %s,exiting\n",
								optarg);
					return -1;
				}
			}
			break;

		case 'f':
			useHwFlowControl = true;
			break;

		default:
			return -1;
		}
	}

	if (optind < argc)
		deviceName = argv[optind];

	printf("%s " APP_INFO "Started (pid: %i, CBC device: %s,", argv[0],
					getpid(), deviceName);
	printf("baudrate: %i, hw flow control: %s)\n", baudrate,
					useHwFlowControl ? "on" : "off");
	do {
		/* set up serial line */
		success = init(&deviceFd, deviceName, baudrate,
				useHwFlowControl, minReceiveBytes);
		if (success)
			break;

		sleep(retry_uint);
		retry_time -= retry_uint;
		printf("Init failed, retry time is %d seconds\n", retry_time);

	} while (retry_time > 0);

	if (success) {
		pause();
		cbc_attach_shutdown(&deviceFd);
	}

	return 0;
}
