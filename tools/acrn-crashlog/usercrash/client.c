/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * Usercrash works as C/S model: usercrash_c works as usercrash client to
 * collect crash logs and information once crash event occurs. For each time,
 * usercrash_c receives 3 params from core_dump and sends connect request event
 * to usercrash_s, then it receives file fd from server to fill crash info into
 * the file. After this work is done, it will notify server that dump work is
 * completed.
 */

int main(void)
{
	//TO BE DONE
	//This empty function is to satisfy the dependency of Makefile.
	//This is the entry of usercrash_c, the implementation will be
	//filled by following patches.
	return 0;
}
