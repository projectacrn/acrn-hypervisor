/*
* Copyright (C) 2022 Intel Corporation.
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <iostream>
#include <unordered_map>
#include <string>

#include <semaphore.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ivshmemlib.h"

using namespace std;

class LatencyCounter
{
public:

	//A total count of the latencies
	unsigned int latenciesCount;
	
	/*Used for sending the data needed for the histogram to python
	First value is the latency data point, second value is the count*/
	unordered_map<int,int> *latencyValues;

	LatencyCounter()
	{
		latenciesCount = 0;
		latencyValues = new unordered_map<int,int>();
	}

	~LatencyCounter()
	{
	
		latenciesCount = 0;
		delete latencyValues;
	
	}

	/*
	void clear(void)

	This method clears the latency counter
	*/
	void clear(void)
	{
	
		latenciesCount = 0;	
		latencyValues->clear();
	
	}

};

#define SHM_KEY "/pyservershm"
#define SHM_ID 1337
#define SHM_SIZE 1048576

#define SEM_KEY "/pyserversem"

#define BUFFERSIZE 256

//Used for synchronizing between the webserver and the data
sem_t *web_sem;

//Shared memory region between userapp and webapp
char *shm_addr;

//Used for holding the data from cyclictest
char data_buffer[BUFFERSIZE] = {0};
char *search_str;

char *setup_ipcomms(void);
char *setup_shm_region(void);
int remove_shm_region(void *);

int process_data(LatencyCounter&);
int dump_data(LatencyCounter&, char *);

void sig_handler(int);
