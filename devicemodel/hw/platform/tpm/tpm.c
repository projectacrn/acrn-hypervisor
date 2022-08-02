/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include "acpi.h"
#include "vmmapi.h"
#include "tpm.h"
#include "tpm_internal.h"
#include "log.h"
#include "mmio_dev.h"
#include "dm.h"

static int tpm_debug;
#define LOG_TAG "tpm: "
#define DPRINTF(fmt, args...) \
	do { if (tpm_debug) pr_dbg(LOG_TAG "%s:" fmt, __func__, ##args); } while (0)
#define WPRINTF(fmt, args...) \
	do { pr_err(LOG_TAG "%s:" fmt, __func__, ##args); } while (0)

#define STR_MAX_LEN 1024U
static char *sock_path = NULL;
static uint32_t vtpm_crb_mmio_addr = 0U;
struct acpi_dev_pt_ops pt_acpi_dev;

#define TPM2_TABLE_SYSFS_PATH	"/sys/firmware/acpi/tables/TPM2"

static inline int checksum(uint8_t *buf, size_t size)
{
	size_t i;
	uint8_t sum = 0;
	for (i = 0; i < size; i++)
		sum += buf[i];
	return sum == 0;
}

static int read_sysfs_tpm2_table(struct acpi_table_tpm2 *tpm2_out)
{
	FILE *fp;
	size_t ch_read;
	struct acpi_table_tpm2 tpm2 = { 0 };

	fp = fopen(TPM2_TABLE_SYSFS_PATH, "r");
	if (!fp)
		return -errno;

	ch_read = fread(&tpm2, 1, sizeof(tpm2), fp);
	fclose(fp);

	/* we may read less than sizeof(tpm2) as laml and lasa are optional */
	if (!ch_read)
		return -errno;

	if (strncmp(tpm2.header.signature, "TPM2", 4) ||
		!checksum((uint8_t *)&tpm2, tpm2.header.length))
		return -EINVAL;

	memcpy(tpm2_out, &tpm2, ch_read);

	return 0;
}

static int is_tpm2_eventlog_supported(struct acpi_table_tpm2 *tpm2)
{
	/* Per TCG ACPI spec ver 1.2 rev 8, if LAML and LASA field are present, length is 76 */
	return ((tpm2->header.length == 76) && tpm2->lasa && tpm2->laml);
}

static int is_hid_tpm2_device(char *opts)
{
	int ret;
	struct acpi_dev_pt_ops *ops = &pt_acpi_dev;
	uint32_t uid = 0;
	char *devopts, *hid, *vtopts;

	if (!opts || !*opts)
		return false;

	devopts = vtopts = strdup(opts);
	hid = strsep(&vtopts, ",");
	uid = (vtopts != NULL) ? atoi(vtopts) : 0;

	ret = get_more_acpi_dev_info(hid, uid, ops);
	free(devopts);
	if (ret)
		return false;
	return (strstr(ops->modalias, "MSFT0101") != NULL);
}

/**
 * Add TPM2 device with HID hid to tpm2dev to be passed-through.
 * If eventlog is also supported, it will be added to the second resource of the
 * tpm2dev->dev.
 *
 * @pre: hid should be a valid HID of the TPM2 device being passed-through.
 * @pre: tpm2dev != NULL
 */
static int init_tpm2_pt(char *opts, struct mmio_dev *tpm2dev)
{
	int err = 0;
	uint64_t tpm2_buffer_hpa, tpm2_buffer_size;
	uint32_t base = 0;
	struct acpi_table_tpm2 tpm2 = { 0 };
	char *devopts, *vtopts = NULL;

	/* TODO: Currently we did not validate if the opts is a valid one.
	 * We trust it to be valid as specifying --acpidev_pt is regarded
	 * as root user operation.
	 */
	if (!opts || !*opts) {
		return -EINVAL;
	}

	devopts = strdup(opts);
	if (devopts == NULL) {
		return -EINVAL;
	}
	vtopts = strstr(devopts,",");

	/* Check whether user set the uid to identify same hid devices for
	 * several instances.
	 */
	if (vtopts != NULL ){
		vtopts[0] = ':';
	}

	/* parse /proc/iomem to find the address and size of tpm buffer */
	if (!get_mmio_hpa_resource(devopts, &tpm2_buffer_hpa, &tpm2_buffer_size)) {
		free(devopts);
		return -ENODEV;
	}

	free(devopts);

	err = read_sysfs_tpm2_table(&tpm2);
	if (err)
		return err;

	if ((tpm2.start_method != 6) && (tpm2.start_method != 7)) {
		pr_err("TPM2 start method %d not supported.\n", tpm2.start_method);
		return -EINVAL;
	}

	if ((tpm2.control_address < tpm2_buffer_hpa) ||
		(tpm2.control_address > tpm2_buffer_hpa + tpm2_buffer_size)) {
		pr_err("TPM2 control area address 0x%016lX outside of the \
			requested address region 0x%016lX-0x%016lX\n", tpm2.control_address,
			tpm2_buffer_hpa, tpm2_buffer_hpa + tpm2_buffer_size);
		return -EINVAL;
	}

	if ((tpm2.control_address >= MMIO_DEV_BASE) && (tpm2.control_address <= MMIO_DEV_LIMIT)) {
		/* we don't support if tpm2 buffer falls in MMIO region */
		return -EINVAL;
	}

	strncpy(tpm2dev->name, "MSFT0101", 8);
	strncpy(tpm2dev->dev.name, "tpm2", 4);
	tpm2dev->dev.res[0].host_pa = tpm2_buffer_hpa;
	tpm2dev->dev.res[0].user_vm_pa = tpm2_buffer_hpa;
	tpm2dev->dev.res[0].size = tpm2_buffer_size;

	/* Search for eventlog */
	if (is_tpm2_eventlog_supported(&tpm2) &&
		!mmio_dev_alloc_gpa_resource32(&base, tpm2.laml)) {
		tpm2dev->dev.res[1].host_pa = tpm2.lasa;
		tpm2dev->dev.res[1].user_vm_pa = base;
		tpm2dev->dev.res[1].size = tpm2.laml;
	}

	pt_tpm2 = true;

	return 0;
}

uint32_t get_vtpm_crb_mmio_addr(void) {
	return vtpm_crb_mmio_addr;
}

static struct acrn_mmiodev *get_tpm2_mmio_dev()
{
	struct mmio_dev *dev = get_mmiodev("MSFT0101");
	return dev ? &dev->dev : NULL;
}

uint32_t get_tpm_crb_mmio_addr(void)
{
	uint32_t base;

	if (pt_tpm2) {
		struct acrn_mmiodev *d = get_tpm2_mmio_dev();
		base = d ? (uint32_t)d->res[0].host_pa : 0U;
	} else {
		base = get_vtpm_crb_mmio_addr();
	}

	return base;
}

static uint32_t get_tpm_crb_mmio_size(void)
{
	struct acrn_mmiodev *d = get_tpm2_mmio_dev();
	return (pt_tpm2 && d) ? d->res[0].size : TPM_CRB_MMIO_SIZE;
}

static uint32_t get_tpm2_table_minimal_log_length(void)
{
	struct acrn_mmiodev *d = get_tpm2_mmio_dev();
	return (pt_tpm2 && d && d->res[1].host_pa) ? d->res[1].size : 0U;
}

static uint64_t get_tpm2_table_log_address(void)
{
	struct acrn_mmiodev *d = get_tpm2_mmio_dev();
	return (pt_tpm2 && d && d->res[1].host_pa) ? d->res[1].user_vm_pa : 0UL;
}

enum {
	SOCK_PATH_OPT = 0
};

char *const token[] = {
	[SOCK_PATH_OPT] = "sock_path",
	NULL
};

int acrn_parse_vtpm2(char *arg)
{
	char *value;
	size_t len = strnlen(arg, STR_MAX_LEN);

	if (len == STR_MAX_LEN)
		return -1;

	if (SOCK_PATH_OPT == getsubopt(&arg, token, &value)) {
		if (value == NULL) {
			DPRINTF("Invalid vtpm socket path\n");
			return -1;
		}
		sock_path = calloc(len + 1, 1);
		if (!sock_path)
			return -1;
		strncpy(sock_path, value, len + 1);
	}
	vtpm2 = true;

	return 0;
}

void init_vtpm2(struct vmctx *ctx)
{
	if (!sock_path) {
		WPRINTF("Invalid socket path!\n");
		return;
	}

	if (init_tpm_emulator(sock_path) < 0) {
		WPRINTF("Failed init tpm emulator!\n");
		return;
	}

	if (mmio_dev_alloc_gpa_resource32(&vtpm_crb_mmio_addr, TPM_CRB_MMIO_SIZE) < 0) {
		WPRINTF("Failed allocate gpa resorce!\n");
		return;
	}

	if (init_tpm_crb(ctx) < 0) {
		WPRINTF("Failed init tpm emulator!\n");
	}
}

void deinit_vtpm2(struct vmctx *ctx)
{
	if (ctx->tpm_dev) {
		deinit_tpm_crb(ctx);

		deinit_tpm_emulator();
	}
}

static void tpm_write_dsdt(struct vmctx *ctx)
{
	if (ctx->tpm_dev || pt_tpm2) {
		dsdt_line("  Scope (\\_SB)");
		dsdt_line("  {");
		dsdt_line("    Device (TPM)");
		dsdt_line("    {");
		dsdt_line("      Name (_HID, \"MSFT0101\" /* TPM 2.0 Security Device */)  // _HID: Hardware ID");
		dsdt_line("      Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings");
		dsdt_line("      {");
		dsdt_indent(4);
		dsdt_fixed_mem32(get_tpm_crb_mmio_addr(), get_tpm_crb_mmio_size());
		dsdt_unindent(4);
		dsdt_line("      })");
		dsdt_line("      Method (_STA, 0, NotSerialized)  // _STA: Status");
		dsdt_line("      {");
		dsdt_line("        Return (0x0F)");
		dsdt_line("      }");
		dsdt_line("    }");
		dsdt_line("  }");
	}
}

int basl_fwrite_tpm2(FILE *fp, struct vmctx *ctx)
{
	EFPRINTF(fp, "/*\n");
	EFPRINTF(fp, " * dm TPM2 template\n");
	EFPRINTF(fp, " */\n");

	EFPRINTF(fp, "[0004]\t\tSignature : \"TPM2\"\n");
	EFPRINTF(fp, "[0004]\t\tTable Length : 0000004C\n");
	EFPRINTF(fp, "[0001]\t\tRevision : 00\n");
	EFPRINTF(fp, "[0001]\t\tChecksum : 00\n");
	EFPRINTF(fp, "[0006]\t\tOem ID : \"ACRNDM\"\n");
	EFPRINTF(fp, "[0008]\t\tOem Table ID : \"DMTPM2  \"\n");
	EFPRINTF(fp, "[0004]\t\tOem Revision : 00000000\n");

	/* iasl will fill the compiler ID/revision fields */
	EFPRINTF(fp, "[0004]\t\tAsl Compiler ID : \"xxxx\"\n");
	EFPRINTF(fp, "[0004]\t\tAsl Compiler Revision : 00000000\n");

	EFPRINTF(fp, "[0002]\t\tPlatform Class : 0000\n");
	EFPRINTF(fp, "[0002]\t\tReserved : 0000\n");
	EFPRINTF(fp, "[0008]\t\tControl Address : %016lX\n", get_tpm_crb_mmio_addr() + (long)CRB_REGS_CTRL_REQ);
	EFPRINTF(fp, "[0004]\t\tStart Method : 00000007\n");

	EFPRINTF(fp, "[0012]\t\tMethod Parameters : 00 00 00 00 00 00 00 00 00 00 00 00\n");
	EFPRINTF(fp, "[0004]\t\tMinimum Log Length : %08X\n", get_tpm2_table_minimal_log_length());
	EFPRINTF(fp, "[0008]\t\tLog Address : %016lX\n", get_tpm2_table_log_address());

	EFFLUSH(fp);

	return 0;
}

struct acpi_dev_pt_ops pt_tpm2_dev_ops = {
	.match = is_hid_tpm2_device,
	.init = init_tpm2_pt,
	.write_dsdt = tpm_write_dsdt,
};
DEFINE_ACPI_PT_DEV(pt_tpm2_dev_ops);
