/*
* Copyright (C) 2022 Intel Corporation.
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "ivshmemlib.h"

//The shared memory region that is used for inter-vm shared memory
char *ivshmem_ptr = NULL;

/*
int setup_ivshmem_region(const char *f_path)
input: char *f_path - A string containing the pci resource2 filepath
output: int - Whether the setup succeeded or not

This function attempts to open the file whose path is f_path
On success it returns 0
On failure it returns -1
*/
int setup_ivshmem_region(const char *f_path)
{

	//Open the file so we can map it into memory
	int pci_file = open(f_path, O_RDWR | O_SYNC);
	if (pci_file == failure) {

		perror("Failed to open the resource2 file\n");
		return failure;

	}

	//Map the file into memory
	ivshmem_ptr = (char *)mmap(0, REGION_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pci_file , 0);
	close(pci_file);
	if (!ivshmem_ptr) {

		perror("Failed to map the shared memory region into our address space\n");
		return failure;

	}

	return success;
}

/*
int close_ivshmem_region(void)
output: int - Whether unmapping the region succeeded or not.

This function closes the ivshmem region
On success it returns 0
On failure it returns -1
*/
int close_ivshmem_region(void)
{

	int ret_val = failure;

	//Determine if ivshmem region is set up
	if (ivshmem_ptr) {

		ret_val =  munmap(ivshmem_ptr, REGION_SIZE);
		ivshmem_ptr = NULL;

	}

	else

		printf("Ivshmem region is not set up.");

	return ret_val;

}

/*
size_t read_ivshmem_region(char *ivshmemPtr, char *user_ptr, size_t size)
input: char *user_ptr - The memory to write to
input: size_t size - the number of bytes to read from the memory
output: size_t - The number of bytes that were successfully read or -1 on failure

This function reads from the ivshmem region and copies size - 1 bytes
from the shared memory region to the user_ptr
and null-terminates the string
*/
size_t read_ivshmem_region(char *user_ptr, size_t size)
{
	//Number of bytes copied
	size_t ret = failure;

	//Make sure that we actually need to read something
	if (size == 0)
		return ret;

	//Determine if ivshmem region is set up
	if ((ivshmem_ptr) && (size < REGION_SIZE - 1)) {

		//Do the copy and zero out the ivshmem region
		bzero(user_ptr, size);
		strncpy(user_ptr, ivshmem_ptr, size);
		ivshmem_ptr[size] = '\0';
		user_ptr[size] = '\0';
		ret = strlen(user_ptr);
		bzero(ivshmem_ptr, size);

	}

	else

		printf("Ivshmem region is not set up.");

	return ret;
}

/*
size_t write_ivshmem_region(char *ivshmemPtr, char *user_ptr, size_t size)
input: char *user_ptr - The memory to read from
input: int size - the number of bytes to write from the memory
output: int - The number of bytes that were successfully written or -1 on failure

This function reads from the user_ptr and copies size - 1 bytes
to the shared memory region
and null-terminates the string
*/
size_t write_ivshmem_region(char *user_ptr, size_t size)
{
	//Return value that holds the amount of bytes that were copied from the user_ptr
	size_t ret = failure;

	//Make sure that we need to actually write something
	if (size == 0)

		return ret;

	//Determine if ivshmem region is set up
	if ((ivshmem_ptr) && (size < REGION_SIZE - 1)){

		//Do the copy and zero out the user_ptr
		bzero(ivshmem_ptr, size);
		strncpy(ivshmem_ptr, user_ptr, size);
		user_ptr[size] = '\0';
		ivshmem_ptr[size] = '\0';
		ret = strlen(ivshmem_ptr);
		bzero(user_ptr, size);

	}

	else

		printf("Ivshmem region is not set up.");

	return ret;
}
