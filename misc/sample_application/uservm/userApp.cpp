/*
* Copyright (C) 2022 Intel Corporation.
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "userApp.h"

int main(void)
{

	//First make sure the user is root
	if (geteuid() != 0) {

		printf("You need to run this program as root!!!\n");
		return failure;

	}

	//Set up the interprocess communication system between userapp and webapp
	shm_addr = setup_ipcomms();

	//Open the shared memory region
	string pci_fname = "/sys/class/uio/uio0/device/resource2";
	if (setup_ivshmem_region(pci_fname.c_str()) == failure) {

		perror("Failed to open the shared memory region");
		return failure;

	}

	//Set up signal handler for when we get interrupt
	signal(SIGINT, sig_handler);

	//Keep track of the latency data
	LatencyCounter latencies;

	//Loop forever, reading the data and sending it to the UI
	while (1) {

		//Process a data point
		if (process_data(latencies))

			latencies.latenciesCount++;

		//Dump the data if we have enough data points
		if ((latencies.latenciesCount > 0) && (latencies.latenciesCount % 100 == 0)) {

			dump_data(latencies, shm_addr);

			//Determine if we need to clear the Latency Counter
			if (latencies.latenciesCount >= 2000000) 

				latencies.clear();

		}

	}

	//Close the shared memory region now that we don't need it
	close_ivshmem_region();
	remove_shm_region(shm_addr);
	shm_addr = NULL;

	return success;
}

/*
int process_data(LatencyCounter& latencies)
input: LatencyCounter& latencies - The class that holds the current latency data
output: int - 0 on success, -1 on failure

This function processes a data point by reading the ivshmem region, parsing the data,
and incrementing the latency count in the latencies hash table
*/
int process_data(LatencyCounter& latencies)
{
	//Clear the data
	bzero(data_buffer, BUFFERSIZE);

	//Used to hold the latency value
	int actual_latency;

	//Used to determine if the latency exists in the map
	int has_latency;

	//Read the data from the RT vm
	if (read_ivshmem_region(data_buffer, BUFFERSIZE)) {

		actual_latency = failure;

		//Deteremine if we have a data point
		search_str = strstr(data_buffer, "Act:");

		//Scan the data point if we have one
		if (search_str)
			sscanf(search_str, "Act: %d", &actual_latency);

		//Update the latency count
		if (actual_latency != failure) {

			//Determine if we have the latency value or create it if we do not
			has_latency = (*latencies.latencyValues).count(actual_latency);	
			if (has_latency)

				(*latencies.latencyValues)[actual_latency] = (*latencies.latencyValues)[actual_latency] + 1;


			else

				(*latencies.latencyValues)[actual_latency] = 1;

			return 1;

		}

	}

	return success;

}
/*
char *setup_ipcomms(void)
output: char * - A pointer to the shared memory region or NULL on failure

This function sets up the shared memory and synchronization between the userapp and the webapp
*/
char *setup_ipcomms(void)
{

	//Set up the shm region
	char *shm_region = setup_shm_region();
	if (!shm_region)

		return shm_region;

	//Set up the semaphore for synchronization with initial value of 1
	web_sem  = sem_open(SEM_KEY, O_CREAT | O_RDWR | O_SYNC, 0666, 1);
	if (web_sem == SEM_FAILED) {

		perror("Failed to create the semaphore");
		remove_shm_region(shm_region);
		shm_region = NULL;

	}

	return shm_region;

}

/*
char *setup_shm_region(void)
output: char * - A pointer to the shared memory region or NULL on failure

This function sets up a shared memory region to be used between the userapp and the
webapp
*/
char *setup_shm_region(void)
{
	void *shared_mem_region = NULL;

	//ID for the memory region
	int shm_id;

	shm_unlink(SHM_KEY);

	//Set up the shared memory region
	shm_id = shm_open(SHM_KEY, O_CREAT | O_RDWR, 0);
	if (shm_id == failure) {

		perror("Failed to get the shared memory region");
		return (char *)shared_mem_region;

	}

	//Set the size of the shared memory region so we avoid bus error
	ftruncate(shm_id, SHM_SIZE);

	//Map the shared memory region
	shared_mem_region = mmap(0, SHM_SIZE, O_RDWR, MAP_SHARED, shm_id, 0);
	if (shared_mem_region == (void *)failure) {

		perror("SHMAT ERROR");
		shmctl(shm_id, IPC_RMID, NULL);
		shared_mem_region = NULL;
		return (char *)shared_mem_region;

	}

	return (char *)shared_mem_region;

}

/*
void remove_shm_region(void)
output: int - 0 on success, -1 on failure

This function tears down the shared memory region
*/
int remove_shm_region(void *shm_region)
{

	//Detach the shared memory region from the process
	return shmdt(shm_region);

}

/*
int dump_data(LatencyCounter& latencies, char *region)
input: LatencyCounter& latencies - The class that holds the current latency data
input: char *region - The region to copy the data to.
output: int - 0 on success, -1 on failure

This function will dump the latency counts and values to the shared memory region
that the python web server will use
*/
int dump_data(LatencyCounter& latencies, char *region)
{

	//Get the shared memory region
	char *current_place = region;
	if (current_place == (char *)NULL) {
	
		printf("Shared memory region is not setup\n");
		return failure;

	}

	//Holds the total count of the latencies so we can send percentages over
	unsigned int total_count = latencies.latenciesCount;

	//Latency value percentage
	int percentage;

	//First lock the semaphore so we can write
	sem_wait(web_sem);

	//Dump the current count of the values before the percentages
	current_place += sprintf(current_place, "%u ", latencies.latenciesCount);

	//Iterate over each value in the latencies map, placing the value in the shared memory region
	for(unordered_map<int,int>::iterator i = (*latencies.latencyValues).begin(); i != (*latencies.latencyValues).end(); i++) {

		percentage = (i->second * 100 / total_count);

		//Only copy the latency value if it is at least 1 percentage point to filter out outliers
		if (percentage > 0)

			current_place += sprintf(current_place, "%d %d ", i->first, percentage);

	}

	sprintf(current_place, "\n");

	sem_post(web_sem);

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

	//Close the shared memory region now that we don't need it
	close_ivshmem_region();
	remove_shm_region(shm_addr);
	shm_addr = NULL;

	exit(-1);

}
