/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CLIENT_H
#define CLIENT_H

#define COMM_NAME_LEN 64
#define SOCKET_NAME "user_crash"

#include <stdio.h>

enum CrashPacketType {
	/* Initial request from crash_dump */
	kDumpRequest = 0,

	/* Notification of a completed crash dump */
	kCompletedDump,

	/* Responses to kRequest */
	kPerformDump
};

struct crash_packet {
	enum CrashPacketType packet_type;
	int pid;
	char name[COMM_NAME_LEN];
};

#endif
