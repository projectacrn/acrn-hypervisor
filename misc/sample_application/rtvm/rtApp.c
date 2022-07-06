/*
* Copyright (C) 2022 Intel Corporation.
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "ivshmemlib.h"

#define BUFFERSIZE 256

//Used for reading output from cyclictest
int data_pipe = -1;

//Used for handling signals
void sig_handler(int);

int main(void)
{

	//First make sure the user is root
	if (geteuid() != 0) {

		printf("You need to run this program as root!!!\n");
		return failure;

	}

	//Used for holding the data from cyclictest
	char data_buffer[BUFFERSIZE] = {0};

	//Used for sanitizing the data
	char *start_stat = NULL;

	//Set up signal handler for when we get interrupt
	signal(SIGINT, sig_handler);

	//Used for reading output from cyclictest
	data_pipe = open("data_pipe", O_RDWR);
	if (data_pipe == failure) {

		perror("Failed to open a fifo pipe");
		return failure;

	}

	//Open the shared memory region
	if (setup_ivshmem_region("/sys/class/uio/uio0/device/resource2") == failure) {

		perror("Failed to open the shared memory region");
		close(data_pipe);
		return failure;

	}

	//Loop forever, reading and writing the data from cyclictest to the uservm
	while (1) {

		//Read the data
		bzero(data_buffer, BUFFERSIZE);
		read(data_pipe, data_buffer, BUFFERSIZE - 1);

		//Get the sample stat
		start_stat = strstr(data_buffer, "T:");
		if (start_stat != NULL)

			//Send the data
			write_ivshmem_region(start_stat, BUFFERSIZE);

	}

	//Close the shared memory region and the data pipe now that we don't need them
	close_ivshmem_region();
	close(data_pipe);

	return success;
}

/*
void sig_handler(int signum)
input: int - the signal value

This function will get run when a signal is sent to the process and will gracefully
shut down the program
*/
void sig_handler(int signum)
{

	fprintf(stderr, "Received signal: %d\n", signum);

	//Close the shared memory region and the data pipe now that we don't need them
	close_ivshmem_region();
	if (data_pipe != -1)
		close(data_pipe);

	exit(-1);

}
