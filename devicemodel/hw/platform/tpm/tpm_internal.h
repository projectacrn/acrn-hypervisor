/*
 * Copyright (C) 2018 Intel Corporation
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _TPM_INTERNAL_H_
#define _TPM_INTERNAL_H_

/* TPMCommBuffer will package TPM2 command and
 * response which are handled by TPM emulator
 *
 * locty: the locality TPM emulator used
 * in & in_len: To indicate the buffer and the
 *    size for TPM command
 * out & out_len: To indicate the buffer and
 *    the size for TPM response
 */
typedef struct TPMCommBuffer {
	uint8_t locty;
	const uint8_t *in;
	uint32_t in_len;
	uint8_t *out;
	uint32_t out_len;
	bool selftest_done;
} TPMCommBuffer;

/* APIs by tpm_emulator.c */
/* Create Ctrl channel and Cmd channel so as to communicate with SWTPM */
int init_tpm_emulator(const char *sock_path);

/* Shutdown of SWTPM and close Ctrl channel and Cmd channel */
void deinit_tpm_emulator(void);

/* Send Ctrl channel command CMD_GET_TPMESTABLISHED to SWTPM */
bool swtpm_get_tpm_established_flag(void);

/* Send Ctrl channel command CMD_RESET_TPMESTABLISHED to SWTPM */
int swtpm_reset_tpm_established_flag(void);

/* Send TPM2 command request to SWTPM by using Cmd channel */
int swtpm_handle_request(TPMCommBuffer *cmd);

/* Initialization for SWTPM */
int swtpm_startup(size_t buffersize);

/* Cancellation of the current TPM2 command */
void swtpm_cancel_cmd(void);

#endif
