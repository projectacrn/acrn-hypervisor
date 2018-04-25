/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * Usercrash works as C/S model: usercrash_s works as usercrash server, which
 * is to handle events from client in endless loop. Once server receives events
 * from client, it will create usercrash_0x file under /var/log/usercrashes/
 * and send file fd to client. Then server will wait for client filling the
 * event info completely to the crash file. After client's work has been done,
 * server will be responsiable to free the crash node and process other events.
 */

int main(void)
{
	//TO BE DONE
	//This empty function is to satisfy the dependency of Makefile.
	//This is the entry of usercrash_s, the implementation will be filled
	//by following patches.
	return 0;
}
