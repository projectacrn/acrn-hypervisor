/*
 * Copyright (c) 2019, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ATT_KEYBOX_H
#define _ATT_KEYBOX_H

typedef struct _HECI_MESSAGE_HEADER {
	uint32_t  groupid : 8;
	uint32_t  command : 7;
	uint32_t  is_response : 1;
	uint32_t  reserved : 8;
	uint32_t  result : 8;
} HECI_MESSAGE_HEADER;

typedef struct
{
	HECI_MESSAGE_HEADER    header;
	/* Size of the whole attkb file, including the added encryption header. */
	uint16_t               total_file_size;
	uint16_t               read_offset;
	/* Size in bytes actually read */
	uint16_t               read_size;
	uint16_t               reserved;
	uint8_t                file_data[];
} HECI_READ_ATTKB_EX_Response;

typedef struct
{
	HECI_MESSAGE_HEADER    header;
	uint16_t               offset; // Offset in file in bytes.
	uint16_t               size;   // Size in bytes to read
	struct
	{
		uint32_t encryption : 1;
		uint32_t reserved   : 31;
	} flags;
} HECI_READ_ATTKB_EX_Request;

#pragma pack (1)
typedef struct rpmb_block {
	uint8_t  signature[4];
	uint32_t length;
	uint32_t revision;
	uint32_t flag;
	uint16_t attkb_addr;
	uint32_t attkb_size;
	uint16_t attkb_svn;
	uint16_t uos_rpmb_size;
	uint8_t  reserved[230];
} rpmb_block_t;
#pragma pack ()

uint16_t read_attkb(void *data, uint16_t size);
uint16_t get_attkb_size(void);

#endif    // _ATT_KEYBOX_H
