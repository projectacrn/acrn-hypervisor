/*
* Copyright (C) 2022 Intel Corporation.
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/mman.h>

//The size of the shared memory region
#define REGION_SIZE 32768

enum return_vals {success = 0, failure = -1};

int setup_ivshmem_region(const char*);
int close_ivshmem_region(void);
size_t read_ivshmem_region(char *, size_t);
size_t write_ivshmem_region(char *, size_t);
