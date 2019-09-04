/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
/*
 * Automotive IO Controller mediator virtualization.
 *
 * IOC mediator block diagram.
 * +------------+     +------------------+
 * |    IOC     |<--->|Native CBC cdevs  |
 * |            |     |                  |
 * |  mediator  |     |/dev/cbc-lifecycle|
 * |            |     |/dev/cbc-signals  |
 * |            |     |...               |
 * |            |     +------------------+
 * |            |     +------------+
 * |            |     |Virtual UART|
 * |            |     |            |
 * |            |<--->|            |
 * |            |     |            |
 * +------------+     +------------+
 *
 * IOC mediator data flow diagram.
 * +------+       +----------------+
 * |Core  |<------+Native CBC cdevs|
 * |      |       +----------------+
 * |thread|       +----------------+
 * |      |<------+Virtual UART    |
 * |      |       +----------------+
 * |      |       +----------------+
 * |      +------>|Tx/Rx queues    |
 * +------+       +----------------+
 *
 * +------+       +----------------+
 * |Rx    |<------+Rx queue        |
 * |      |       +----------------+
 * |thread|
 * |      |       +----------------+
 * |      +------>|Native CBC cdevs|
 * +------+       +----------------+
 *
 * +------+       +----------------+
 * |Tx    |<------+Tx queue        |
 * |      |       +----------------+
 * |thread|       |
 * |      |       +----------------+
 * |      +------>|Virtual UART    |
 * +------+       +----------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <types.h>
#include <libgen.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "pty_vuart.h"

#include "dm.h"
#include "ioc.h"
#include "vmmapi.h"
#include "monitor.h"

/* For debugging log to a file */
static int ioc_debug;
static FILE *dbg_file;
#define IOC_LOG_INIT do { if (ioc_debug) {\
	dbg_file = fopen("/tmp/ioc_log", "w+");\
if (!dbg_file)\
	printf("ioc log open failed\r\n"); else cbc_set_log_file(dbg_file);\
} } while (0)
#define IOC_LOG_DEINIT do { if (dbg_file) fclose(dbg_file); dbg_file = NULL;\
cbc_set_log_file(dbg_file);\
} while (0)
#define DPRINTF(format, arg...) \
do { if (ioc_debug && dbg_file) { fprintf(dbg_file, format, arg);\
	fflush(dbg_file); } } while (0)
#define	WPRINTF(format, arg...) printf(format, ##arg)

/*
 * For debugging only, to generate lifecycle, signal and oem-raw data
 * from PTY devices instead of native CBC cdevs.
 */
static bool ioc_debug_enable;

/*
 * Type definition for thread function.
 */
typedef void* (*ioc_work)(void *arg);

/*
 * IOC mediator and virtual UART communication channel path,
 * comes from DM command line parameters.
 */
static char virtual_uart_path[32 + MAX_VMNAME_LEN];

/*
 * To activate CBC signal channel(/dev/cbc-signals).
 * Need to send open channel command to CBC signal char device before receive
 * signal data.
 * NOTE: Only send open channel command, no need to send close channel since
 * close channel command would deactivate the signal channel for all UOS, so
 * there will be a SOS service to deactivate signal channel in the future.
 */
static uint8_t cbc_open_channel_command[] = {0xFD, 0x00, 0x00, 0x00};

/* IOC boot reason(for S5)
 * comes from DM command line parameters.
 */
static uint32_t ioc_boot_reason;

/*
 * Dummy pty slave fd is to maintain the pty active,
 * to avoid EIO error when close the slave pty.
 */
static int dummy0_sfd = -1;
static int dummy1_sfd = -1;
static int dummy2_sfd = -1;

/*
 * VM Manager interfaces description.
 *
 * +---------+                 +---------+                 +---------+
 * |IOC      | VM stop         |VM       |                 |SOS      |
 * |Mediator |<----------------+Manager  |                 |Lifecycle|
 * |         |                 |         |                 |         |
 * |         | VM suspend      |         |                 |         |
 * |         |<----------------+         |                 |         |
 * |         |                 |         |                 |         |
 * |         | VM resume       |         |                 |         |
 * |         |<----------------+         |                 |         |
 * |         |get_wakeup_reason|         |get wakeup reason|         |
 * |         |for resume flow  |         |via unix socket  |         |
 * |         +---------------->|         +---------------->|         |
 * +---------+                 +---------+                 +---------+
 *
 * Only support stop/resume/suspend in IOC mediator currently.
 * For resume request, IOC mediator will get the wakeup reason from SOS
 * lifecycle service, then pass to UOS once received HB INIT from UOS.
 * For stop and suspend requests, they are implemented as wakeup reason of
 * ignition button.
 */
static int vm_stop_handler(void *arg);
static int vm_resume_handler(void *arg);
static int vm_suspend_handler(void *arg);
static struct monitor_vm_ops vm_ops = {
	.stop = vm_stop_handler,
	.resume = vm_resume_handler,
	.suspend = vm_suspend_handler,
};

/*
 * IOC State Transfer
 *
 * +-----------+     RESUME EVENT           +-----------+
 * |   INIT    +<---------------------------+ SUSPENDED |
 * +-----+-----+                            +-----------+
 *       |                                        ^
 *       |                                        |
 *       |HB ACTIVE                               |SHUTDOWN
 *       |EVENT                                   |EVENT
 *       |                                        |
 *       |                                        |
 *       v           RAM REFRESH EVENT/           |
 * +-----+-----+     HB INACTIVE EVENT      +-----+-----+
 * |   ACTIVE  +--------------------------->+SUSPENDING |
 * +-----------+                            +-----------+
 *
 * INIT state:   The state is IOC mediator initialized IOC state, all of CBC
 *               protocol packats are handler normally. In this state, UOS has
 *               not yet sent active heartbeat.
 *
 * ACTIVE state: Enter this state if HB ACTIVE event is triggered that indicates
 *               UOS state has been active and need to set the bit 23(SoC active
 *               bit) in the wakeup reason.
 *
 * SUSPENDING state: Enter this state if RAM REFRESH event or HB INACTIVE event
 *		     is triggered, the related event handler needs to set the
 *		     suspend or shutdown to PM DM and begin to drop the queued
 *                   CBC protocol packets.
 *
 * SUSPENDED state: Enter this state if SHUTDOWN event is triggered to close all
 *                  native CBC char devices. The IOC mediator will be enter to
 *                  sleeping until RESUME event is triggered that re-opens
 *                  closed native CBC char devices and transfer to INIT state.
 */
static int process_hb_active_event(struct ioc_dev *ioc);
static int process_ram_refresh_event(struct ioc_dev *ioc);
static int process_hb_inactive_event(struct ioc_dev *ioc);
static int process_shutdown_event(struct ioc_dev *ioc);
static int process_resume_event(struct ioc_dev *ioc);
static struct ioc_state_info ioc_state_tbl[] = {
	{IOC_S_INIT,		IOC_S_ACTIVE,		IOC_E_HB_ACTIVE,
		process_hb_active_event},
	{IOC_S_ACTIVE,		IOC_S_SUSPENDING,	IOC_E_RAM_REFRESH,
		process_ram_refresh_event},
	{IOC_S_ACTIVE,		IOC_S_SUSPENDING,	IOC_E_HB_INACTIVE,
		process_hb_inactive_event},
	{IOC_S_SUSPENDING,	IOC_S_SUSPENDED,	IOC_E_SHUTDOWN,
		process_shutdown_event},
	{IOC_S_SUSPENDED,	IOC_S_INIT,		IOC_E_RESUME,
		process_resume_event},
};

/*
 * IOC channels definition.
 */
static struct ioc_ch_info ioc_ch_tbl[] = {
	{IOC_INIT_FD, IOC_NP_PMT,   IOC_NATIVE_PMT,	IOC_CH_OFF},
	{IOC_INIT_FD, IOC_NP_LF,    IOC_NATIVE_LFCC,	IOC_CH_ON },
	{IOC_INIT_FD, IOC_NP_SIG,   IOC_NATIVE_SIGNAL,	IOC_CH_ON },
	{IOC_INIT_FD, IOC_NP_ESIG,  IOC_NATIVE_ESIG,	IOC_CH_OFF},
	{IOC_INIT_FD, IOC_NP_DIAG,  IOC_NATIVE_DIAG,	IOC_CH_OFF},
	{IOC_INIT_FD, IOC_NP_DLT,   IOC_NATIVE_DLT,	IOC_CH_OFF},
	{IOC_INIT_FD, IOC_NP_LIND,  IOC_NATIVE_LINDA,	IOC_CH_OFF},
	{IOC_INIT_FD, IOC_NP_RAW0,  IOC_NATIVE_RAW0,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW1,  IOC_NATIVE_RAW1,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW2,  IOC_NATIVE_RAW2,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW3,  IOC_NATIVE_RAW3,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW4,  IOC_NATIVE_RAW4,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW5,  IOC_NATIVE_RAW5,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW6,  IOC_NATIVE_RAW6,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW7,  IOC_NATIVE_RAW7,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW8,  IOC_NATIVE_RAW8,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW9,  IOC_NATIVE_RAW9,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW10, IOC_NATIVE_RAW10,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_RAW11, IOC_NATIVE_RAW11,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_DP_NONE,  IOC_VIRTUAL_UART,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_DP_NONE,  IOC_LOCAL_EVENT,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_FLF,   IOC_NATIVE_DUMMY0,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_FSIG,  IOC_NATIVE_DUMMY1,	IOC_CH_ON},
	{IOC_INIT_FD, IOC_NP_FRAW,  IOC_NATIVE_DUMMY2,	IOC_CH_ON}
};

static struct cbc_signal cbc_tx_signal_table[] = {
	{(uint16_t)CBC_SIG_ID_VSWA,	16,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VSPD,	16,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VESP,	16,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VECT,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VRGR,	2,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VPS,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VPM,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VMD,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VIS,	2,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VGP,	4,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VAG,	4,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VFS,	2,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VFL,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VDTE,	16,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWUB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWRB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWPB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWNB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWLB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWLB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWDB,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWVA,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWSCB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWPLB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWPCB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWPSB,    3, 	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWHB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWEB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWECB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWCB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWCLB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_SWAMB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSUB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSRB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSPB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSP9B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSP8B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSP7B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSP6B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSP5B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSP4B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSP3B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSP2B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSP1B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSP0B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSNB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSLB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSDB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSVA,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RSSSB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSSCB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSSB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSRDB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSPLB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSPSB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSOMB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSHB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSHDB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSENB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSEJB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSCB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSCLB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRSAMB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RVCS,	2,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PSS,	2,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PUB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PRB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PPB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PP9B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PP8B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PP7B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PP6B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PP5B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PP4B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PP3B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PP2B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PP1B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PP0B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PNB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PLB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PDB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PVA,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PSB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PSCB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PSRB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PRDB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PPLB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PPSB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_POMB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PHMB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PHDB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PENB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PEJB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PCFB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PCLB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PAMB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSUB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSRB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSPB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSP9B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSP8B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSP7B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSP6B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSP5B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSP4B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSP3B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSP2B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSP1B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSP0B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSNB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSLB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSDB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSVA,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSAMB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSSB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSSCB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSSRB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSRDB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSPLB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSPSB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSOMB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSHMB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSHDB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSENB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSEJB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSCFB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LRSCLB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DVA,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DECSP,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DECST,	2,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DAMB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DNB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DLB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DDB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DUB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DRB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DPB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DP9B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DP8B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DP7B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DP6B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DP5B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DP4B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DP3B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DP2B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DP1B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DP0B,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DSCB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DSRB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DRDB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DSTB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DPLB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DPSB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DOMB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DHMB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DHHB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DENB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DEJB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DCFB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DCLB,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DSTG,	2,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DSRR,	2,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DSRF,	2,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DSLR,	2,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DSLF,	2,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_DSEH,	2,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_CSSRRW,	2,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_CSSRR,	2,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_CSSLRW,	2,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_CSSLR,	2,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_ATEMP,	16,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_ANSL,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_ALTI,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VSA,	16,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LLAT,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LLON,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LALT,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LACC,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LHED,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LSPD,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LSRC,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_LSCT,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PDFB,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PDFL1,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PDFL2,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PDFL3,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PDFR1,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PDFR2,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PDFR3,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PDRC,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PDRL1,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PDRL2,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PDRL3,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PDRR1,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PDRR2,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PDRR3,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VXA,	16,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VYA,	16,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VZA,	16,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_MBV,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_TSA,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_TSE,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_IACR,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_IWCR,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_IFCR,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_GYROX,	16,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_GYROY,	16,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_IAVB,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_IAVMJ,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RAV,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RMAX,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RMIN,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_ACCX,	16,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_ACCY,	16,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_ACCZ,	16,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_MDS,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_FCP,	10,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_GYROZ,	16,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_IAVMN,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RTST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PKBK,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PKBKST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PKBKAT,	32,     CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PKBKAS,	32,     CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HFSPD,	32,     CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HFSST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HFDIR,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HFDSTT,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HVACA,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HVASTT,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HAMAX,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HVMST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HAUTO,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HATSTT,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HVDEF,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HDEFSTT,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HDFMAX,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HDMXSTT,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HDUAL,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HDSTT,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HHSMR,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HHSMST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HHSWL,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HHSWST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HPOWR,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HPWSTT,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HRECC,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HRECST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HTEMCB,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HTCSTT,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HTMPST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HTSSTT,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HTMPU,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HTUSTT,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HVTST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HVSSTT,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HRCAT,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HRASTT,	32,	CBC_ACTIVE},
};

static struct cbc_signal cbc_rx_signal_table[] = {
	{(uint16_t)CBC_SIG_ID_STFR,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_EGYO,	1,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_WACS,	3,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RIFC,	1,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RIWC,	1,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RIAC,	1,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RIVS,	1,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_RRMS,	8,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_MTAM,	1,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PBST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_PBAT,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HFSS,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HFDST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HVAST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HAMS,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HATST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HDEFST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HDMXST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HDST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HHSMS,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HHSWS,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HPWST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HRCST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HTCST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HTSST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HTUST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HVSST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_HRAST,	32,	CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_USBVBUS,  1,      CBC_ACTIVE},
	{(uint16_t)CBC_SIG_ID_VICL,	8,	CBC_ACTIVE},
};

static struct cbc_group cbc_rx_group_table[] = {
	{(uint16_t)CBC_GRP_ID_0,	CBC_ACTIVE},
};

static struct cbc_group cbc_tx_group_table[] = {
	{(uint16_t)CBC_GRP_ID_LOC,	CBC_ACTIVE},
	{(uint16_t)CBC_GRP_ID_PDF,	CBC_ACTIVE},
	{(uint16_t)CBC_GRP_ID_PDR,	CBC_ACTIVE},
	{(uint16_t)CBC_GRP_ID_VAC,	CBC_ACTIVE},
	{(uint16_t)CBC_GRP_ID_GAS,	CBC_ACTIVE},
	{(uint16_t)CBC_GRP_ID_IVR,	CBC_ACTIVE},
	{(uint16_t)CBC_GRP_ID_IRM,	CBC_ACTIVE},
	{(uint16_t)CBC_GRP_ID_GAC,	CBC_ACTIVE}
};

static struct wlist_signal wlist_rx_signal_table[] = {
	{(uint16_t)CBC_SIG_ID_HRASTT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_PBST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_PBAT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HFSS,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HFDST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HVAST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HAMS,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HATST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HDEFST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HDMXST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HDST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HHSMS,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HHSWS,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HPWST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HRCST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HTCST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HTSST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HTUST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HVSST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HRAST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_USBVBUS,  DEFAULT_WLIST_NODE},
};

static struct wlist_signal wlist_tx_signal_table[] = {
	{(uint16_t)CBC_SIG_ID_TSA,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_VSPD,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_VESP,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_ATEMP,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_VSPD,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_VESP,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_VECT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_VRGR,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_VGP,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_VAG,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_VFS,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_SWUB,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_SWSCB,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_SWPCB,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_SWAMB,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_SWDB,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_ALTI,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_PKBK,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_PKBKST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_PKBKAT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_PKBKAS,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HFSPD,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HFSST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HFDIR,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HFDSTT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HVACA,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HVASTT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HAMAX,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HVMST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HAUTO,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HATSTT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HVDEF,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HDEFSTT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HDFMAX,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HDMXSTT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HDUAL,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HDSTT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HHSMR,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HHSMST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HHSWL,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HHSWST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HPOWR,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HPWSTT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HRECC,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HRECST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HTEMCB,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HTCSTT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HTMPST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HTSSTT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HTMPU,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HTUSTT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HVTST,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HVSSTT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HRCAT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_HRASTT,	DEFAULT_WLIST_NODE},
	{(uint16_t)CBC_SIG_ID_VSWA,	DEFAULT_WLIST_NODE},
};

static struct wlist_group wlist_rx_group_table[] = {
};

static struct wlist_group wlist_tx_group_table[] = {
};

/*
 * Read data from the native CBC cdevs and virtual UART based on
 * IOC channel ID.
 */
static int
ioc_ch_recv(enum ioc_ch_id id, uint8_t *buf, size_t size)
{
	int fd;
	int count;

	fd = ioc_ch_tbl[id].fd;
	if (fd < 0 || !buf || size == 0)
		return -1;
	count = read(fd, buf, size);

	/*
	 * Currently epoll work mode is LT, so ignore EAGAIN error.
	 * If change epoll work mode to ET, need to handle EAGAIN.
	 */
	if (count < 0) {
		DPRINTF("ioc read bytes error:%s\r\n", strerror(errno));
		return -1;
	}
	return count;
}

/*
 * Write data to the native CBC cdevs and virtual UART based on
 * IOC channel ID.
 */
int
ioc_ch_xmit(enum ioc_ch_id id, const uint8_t *buf, size_t size)
{
	int count = 0;
	int fd, rc;

	fd = ioc_ch_tbl[id].fd;
	if (fd < 0 || !buf || size == 0)
		return -1;
	while (count < size) {
		rc = write(fd, (buf + count), (size - count));

		/*
		 * Currently epoll work mode is LT, so ignore EAGAIN error.
		 * If change epoll work mode to ET, need to handle EAGAIN.
		 */
		if (rc < 0) {
			DPRINTF("ioc write error:%s\r\n", strerror(errno));
			break;
		}
		count += rc;
	}
	return count;
}

/*
 * Open native CBC cdevs.
 */
static int
ioc_open_native_ch(const char *dev_name)
{
	int fd;

	if (!dev_name)
		return -1;
	fd = open(dev_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0)
		DPRINTF("ioc open %s failed:%s\r\n", dev_name, strerror(errno));
	return fd;
}

/*
 * Open native CBC cdevs and virtual UART.
 */
static int
ioc_ch_init(struct ioc_dev *ioc)
{
	int i, fd;
	struct ioc_ch_info *chl;
	int pipe_fds[2];

	for (i = 0, chl = ioc_ch_tbl; i < ARRAY_SIZE(ioc_ch_tbl); i++, chl++) {
		if (chl->stat == IOC_CH_OFF)
			continue;

		switch (i) {
		case IOC_NATIVE_LFCC:
		case IOC_NATIVE_SIGNAL:
		case IOC_NATIVE_RAW0 ... IOC_NATIVE_RAW11:
			fd = ioc_open_native_ch(chl->name);
			break;
		case IOC_VIRTUAL_UART:
			fd = pty_open_virtual_uart(virtual_uart_path);
			break;
		case IOC_LOCAL_EVENT:
			if (!pipe(pipe_fds)) {
				fd = pipe_fds[0];
				ioc->evt_fd = pipe_fds[1];
			} else {
				fd = IOC_INIT_FD;
				DPRINTF("%s", "ioc open event fd failed\r\n");
			}
			break;
		/*
		 * TODO: check open if success for dummy fd
		 */
		case IOC_NATIVE_DUMMY0:
			if (ioc_debug_enable) {
				fd = pty_open_virtual_uart(chl->name);
				dummy0_sfd = open(chl->name, O_RDWR | O_NOCTTY |
						O_NONBLOCK);
			} else
				fd = -1;
			break;
		case IOC_NATIVE_DUMMY1:
			if (ioc_debug_enable) {
				fd = pty_open_virtual_uart(chl->name);
				dummy1_sfd = open(chl->name, O_RDWR | O_NOCTTY |
						O_NONBLOCK);
			} else
				fd = -1;
			break;
		case IOC_NATIVE_DUMMY2:
			if (ioc_debug_enable) {
				fd = pty_open_virtual_uart(chl->name);
				dummy2_sfd = open(chl->name, O_RDWR | O_NOCTTY |
						O_NONBLOCK);
			} else
				fd = -1;
			break;
		default:
			fd = -1;
			break;
		}

		/*
		 * Critical channels must open successfully
		 * if can not open lifecycle or virtual UART
		 * ioc needs to exit initilization with failure
		 */
		if (fd < 0 && (i == IOC_NATIVE_LFCC || i == IOC_VIRTUAL_UART ||
					i == IOC_LOCAL_EVENT))
			return -1;

		chl->fd = fd;
	}
	return 0;
}

/*
 * Close native CBC cdevs and virtual UART.
 */
static void
ioc_ch_deinit(void)
{
	int i;
	struct ioc_ch_info *chl = NULL;

	for (i = 0, chl = ioc_ch_tbl; i < ARRAY_SIZE(ioc_ch_tbl); i++, chl++) {
		if (chl->fd < 0)
			continue;

		/*
		 * No need to call EPOLL_CTL_DEL before close the fd, since
		 * epoll_wait thread should exit before all channels release.
		 */
		close(chl->fd);
		chl->fd = IOC_INIT_FD;
	}

	if (dummy0_sfd > 0) {
		close(dummy0_sfd);
		dummy0_sfd = -1;
	}
	if (dummy1_sfd > 0) {
		close(dummy1_sfd);
		dummy1_sfd = -1;
	}
	if (dummy2_sfd > 0) {
		close(dummy2_sfd);
		dummy2_sfd = -1;
	}
}

/*
 * Called to put a cbc_request to a specific queue
 */
static void
cbc_request_enqueue(struct ioc_dev *ioc, struct cbc_request *req,
		enum cbc_queue_type qtype, bool to_head)
{
	pthread_cond_t *cond;
	pthread_mutex_t *mtx;
	struct cbc_qhead *qhead;

	if (!req)
		return;

	if (qtype == CBC_QUEUE_T_RX) {
		cond = &ioc->rx_cond;
		mtx = &ioc->rx_mtx;
		qhead = &ioc->rx_qhead;
	} else if (qtype == CBC_QUEUE_T_TX) {
		cond = &ioc->tx_cond;
		mtx = &ioc->tx_mtx;
		qhead = &ioc->tx_qhead;
	} else {
		cond = NULL;
		mtx = &ioc->free_mtx;
		qhead = &ioc->free_qhead;
	}

	pthread_mutex_lock(mtx);
	if (to_head)
		SIMPLEQ_INSERT_HEAD(qhead, req, me_queue);
	else
		SIMPLEQ_INSERT_TAIL(qhead, req, me_queue);
	if (cond != NULL)
		pthread_cond_signal(cond);
	pthread_mutex_unlock(mtx);
}

/*
 * Called to get a cbc_request from a specific queue,
 * due to rx and tx threads have implemented getting a cbc_request from
 * related queue and only core thread needs to dequque, so only supports
 * dequeue from the free queue.
 */
static struct cbc_request*
cbc_request_dequeue(struct ioc_dev *ioc, enum cbc_queue_type qtype)
{
	struct cbc_request *free = NULL;

	if (qtype == CBC_QUEUE_T_FREE) {
		pthread_mutex_lock(&ioc->free_mtx);
		if (!SIMPLEQ_EMPTY(&ioc->free_qhead)) {
			free = SIMPLEQ_FIRST(&ioc->free_qhead);
			SIMPLEQ_REMOVE_HEAD(&ioc->free_qhead, me_queue);
		}
		pthread_mutex_unlock(&ioc->free_mtx);
	}
	return free;
}

/*
 * Send a cbc_request to TX handler
 */
static int
send_tx_request(struct ioc_dev *ioc, enum cbc_request_type type)
{
	struct cbc_request *req;

	req = cbc_request_dequeue(ioc, CBC_QUEUE_T_FREE);
	if (!req) {
		DPRINTF("%s", "ioc sends a tx request failed\r\n");
		return -1;
	}

	req->rtype = type;
	cbc_request_enqueue(ioc, req, CBC_QUEUE_T_TX, true);
	return 0;
}

/*
 * Process hb active event before transfer to next state
 */
static int
process_hb_active_event(struct ioc_dev *ioc)
{
	/* Enable wakeup reason bit 23 that indicating UOS is active */
	return send_tx_request(ioc, CBC_REQ_T_UOS_ACTIVE);
}

/*
 * Process ram refresh event before transfer to next state
 */
static int
process_ram_refresh_event(struct ioc_dev *ioc)
{
	int rc;

	/* Rx and Tx threads discard all CBC protocol packets */
	ioc->cbc_enable = false;

	/*
	 * Tx handler sents shutdown wakeup reason,
	 * Then enter suspended state.
	 */
	rc = send_tx_request(ioc, CBC_REQ_T_UOS_INACTIVE);

	/*
	 * TODO: set suspend to PM DM
	 */

	return rc;
}

/*
 * Process hb inactive event before transfer to next state
 */
static int
process_hb_inactive_event(struct ioc_dev *ioc)
{
	int rc;

	/* Rx and Tx threads discard all CBC protocol packets */
	ioc->cbc_enable = false;

	/*
	 * Tx sents shutdown wakeup reason,
	 * Then enter shutdown state.
	 */
	rc = send_tx_request(ioc, CBC_REQ_T_UOS_INACTIVE);

	/*
	 * TODO: set shutdown to PM DM
	 */

	return rc;
}

/*
 * Process shutdown reason event before transfer to next state
 */
static int
process_shutdown_event(struct ioc_dev *ioc)
{
	int i;
	struct ioc_ch_info *chl;

	/*
	 * Due to native CBC driver buffer will be full if the native CBC char
	 * devices are opened, but not keep reading. So close the native devices
	 * when removing them from epoll.
	 */
	for (i = 0, chl = ioc_ch_tbl; i < ARRAY_SIZE(ioc_ch_tbl); i++, chl++) {
		switch (i) {
		case IOC_NATIVE_PMT ... IOC_NATIVE_RAW11:
			if (chl->stat == IOC_CH_OFF || chl->fd < 0)
				continue;
			epoll_ctl(ioc->epfd, EPOLL_CTL_DEL, chl->fd, NULL);
			close(chl->fd);
			chl->fd = IOC_INIT_FD;
			break;
		}
	}

	return 0;
}

/*
 * Process resume event before transfer to next state
 */
static int
process_resume_event(struct ioc_dev *ioc)
{
	int i;
	struct ioc_ch_info *chl;

	/* Rx and Tx threads begin to process CBC protocol packets */
	ioc->cbc_enable = true;

	/* re-open native CBC char devices and add them into epoll */
	for (i = 0, chl = ioc_ch_tbl; i < ARRAY_SIZE(ioc_ch_tbl); i++, chl++) {
		switch (i) {
		case IOC_NATIVE_PMT ... IOC_NATIVE_RAW11:
			if (chl->stat == IOC_CH_OFF || chl->fd != IOC_INIT_FD)
				continue;

			chl->fd = ioc_open_native_ch(chl->name);
			if (chl->fd > 0)
				epoll_ctl(ioc->epfd, EPOLL_CTL_ADD, chl->fd,
						&ioc->evts[i]);
			else
				DPRINTF("ioc open failed, channel:%s\r\n",
						chl->name);
			break;
		}
	}

	/*
	 * The signal channel is inactive after SOS resumed, need to send
	 * open channel command again to activate the signal channel.
	 * And it would not impact to UOS itself enter/exit S3.
	 */
	if (ioc_ch_xmit(IOC_NATIVE_SIGNAL, cbc_open_channel_command,
				sizeof(cbc_open_channel_command)) <= 0)
		DPRINTF("%s", "ioc reopen signal channel failed\r\n");

	return 0;
}

/*
 * Process IOC local events
 */
static void
ioc_process_events(struct ioc_dev *ioc, enum ioc_ch_id id)
{
	int i;
	uint8_t evt;

	/* Get one event */
	if (ioc_ch_recv(id, &evt, sizeof(evt)) < 0) {
		DPRINTF("%s", "ioc state gets event failed\r\n");
		return;
	}

	/* IOC_E_KNOCK event is only used to wakeup core thread */
	if (evt == IOC_E_KNOCK)
		return;

	for (i = 0; i < ARRAY_SIZE(ioc_state_tbl); i++) {
		if (evt == ioc_state_tbl[i].evt &&
				ioc->state == ioc_state_tbl[i].cur_stat) {
			if (ioc_state_tbl[i].handler &&
				ioc_state_tbl[i].handler(ioc) == 0)
				ioc->state = ioc_state_tbl[i].next_stat;
			else
				DPRINTF("ioc state switching failed,%d->%d\r\n",
						ioc_state_tbl[i].cur_stat,
						ioc_state_tbl[i].next_stat);
		}
	}
}

/*
 * For a new event update.
 * The interface is for mult-threads, currently write one byte to core thread,
 * So if write more types, needs to add protection for thread safety.
 */
void
ioc_update_event(int fd, enum ioc_event_type evt)
{
	uint8_t val = evt;

	if (write(fd, &val, sizeof(val)) < 0)
		DPRINTF("ioc update event failed, error:%s\r\n",
				strerror(errno));
}

/*
 * Build a cbc_request with CBC link frame and add the cbc_request to
 * the rx queue tail.
 */
void
ioc_build_request(struct ioc_dev *ioc, int32_t link_len, int32_t srv_len)
{
	int i, pos;
	struct cbc_ring *ring = &ioc->ring;
	struct cbc_request *req;

	req = cbc_request_dequeue(ioc, CBC_QUEUE_T_FREE);
	if (!req) {
		WPRINTF(("ioc queue is full!!, drop the data\n\r"));
		return;
	}
	for (i = 0; i < link_len; i++) {
		pos = (ring->head + i) & (CBC_RING_BUFFER_SIZE - 1);

		req->buf[i] = ring->buf[pos];
	}
	req->srv_len = srv_len;
	req->link_len = link_len;
	req->rtype = CBC_REQ_T_PROT;
	cbc_request_enqueue(ioc, req, CBC_QUEUE_T_RX, false);
}

/*
 * Rx processing of the epoll kicks.
 */
static int
ioc_process_rx(struct ioc_dev *ioc, enum ioc_ch_id id)
{
	uint8_t c;

	/*
	 * Read virtual UART data byte one by one
	 * FIXME: if IOC DM can get several bytes one time
	 * then need to improve this
	 */
	if (ioc_ch_recv(id, &c, sizeof(c)) < 0)
		return -1;
	if (cbc_copy_to_ring(&c, 1, &ioc->ring) == 0)
		cbc_unpack_link(ioc);
	return 0;
}

/*
 * Tx processing of the epoll kicks.
 */
static int
ioc_process_tx(struct ioc_dev *ioc, enum ioc_ch_id id)
{
	int count;
	struct cbc_request *req;

	req = cbc_request_dequeue(ioc, CBC_QUEUE_T_FREE);
	if (!req) {
		WPRINTF("ioc free queue is full!!, drop the data\r\n");
		return -1;
	}

	/*
	 * The data from native CBC cdevs and each receiving can read a complete
	 * CBC service frame, so copy the bytes to the CBC service start
	 * position.
	 */
	count = ioc_ch_recv(id, req->buf + CBC_SRV_POS, CBC_MAX_SERVICE_SIZE);
	if (count <= 0) {
		cbc_request_enqueue(ioc, req, CBC_QUEUE_T_FREE, false);
		DPRINTF("ioc channel=%d,recv error\r\n", id);
		return -1;
	}

	/* Build a cbc_request and send it to Tx queue */
	req->srv_len = count;
	req->link_len = 0;
	req->rtype = CBC_REQ_T_PROT;
	if (id == IOC_NATIVE_DUMMY0)
		req->id = IOC_NATIVE_LFCC;
	else if (id == IOC_NATIVE_DUMMY1)
		req->id = IOC_NATIVE_SIGNAL;
	else if (id == IOC_NATIVE_DUMMY2)
		req->id = IOC_NATIVE_RAW11;
	else
		req->id = id;

	cbc_request_enqueue(ioc, req, CBC_QUEUE_T_TX, false);
	return 0;
}

/*
 * Core thread monitors epoll events of Rx and Tx directions
 * based on the channel id for different process.
 */
static void
ioc_dispatch(struct ioc_dev *ioc, struct ioc_ch_info *chl)
{
	switch (chl->id) {
	case IOC_NATIVE_LFCC:
	case IOC_NATIVE_SIGNAL:
	case IOC_NATIVE_RAW0 ... IOC_NATIVE_RAW11:
	case IOC_NATIVE_DUMMY0:
	case IOC_NATIVE_DUMMY1:
	case IOC_NATIVE_DUMMY2:
		ioc_process_tx(ioc, chl->id);
		break;
	case IOC_VIRTUAL_UART:
		ioc_process_rx(ioc, chl->id);
		break;
	case IOC_LOCAL_EVENT:
		ioc_process_events(ioc, chl->id);
		break;
	default:
		DPRINTF("ioc dispatch got wrong channel:%d\r\n", chl->id);
		break;
	}
}

/*
 * Handle EPOLLIN events for native CBC cdevs and virtual UART.
 */
static void *
ioc_core_thread(void *arg)
{
	int n, i;
	struct ioc_ch_info *chl;
	int chl_size = ARRAY_SIZE(ioc_ch_tbl);
	struct ioc_dev *ioc = (struct ioc_dev *) arg;
	struct epoll_event eventlist[IOC_MAX_EVENTS];

	/*
	 * Initialize epoll events.
	 * NOTE: channel size always same with events number, so that acess
	 * event by channel index.
	 */
	for (i = 0, chl = ioc_ch_tbl; i < chl_size; i++, chl++) {
		if (chl->fd > 0) {
			ioc->evts[i].events = EPOLLIN;
			ioc->evts[i].data.ptr = chl;
			if (epoll_ctl(ioc->epfd, EPOLL_CTL_ADD, chl->fd,
						&ioc->evts[i]) < 0)
				DPRINTF("ioc epoll ctl %s failed, error:%s\r\n",
						chl->name, strerror(errno));
		}
	}

	/* Start to epoll wait loop */
	while (!ioc->closing) {
		n = epoll_wait(ioc->epfd, eventlist, IOC_MAX_EVENTS, -1);
		if (n < 0 && errno != EINTR) {
			DPRINTF("ioc epoll wait error:%s, exit ioc core\r\n",
					strerror(errno));
			goto exit;
		}
		for (i = 0; i < n; i++)
			ioc_dispatch(ioc, (struct ioc_ch_info *)
					eventlist[i].data.ptr);
	}
exit:
	return NULL;
}

/*
 * Rx thread waits for CBC requests of rx queue, if rx queue is not empty,
 * it wll get a cbc_request from rx queue and invokes cbc_rx_handler to process.
 */
static void *
ioc_rx_thread(void *arg)
{
	struct ioc_dev *ioc = (struct ioc_dev *) arg;
	struct cbc_request *req = NULL;
	struct cbc_pkt packet;

	memset(&packet, 0, sizeof(packet));
	packet.cfg = &ioc->rx_config;
	packet.ioc = ioc;

	for (;;) {
		pthread_mutex_lock(&ioc->rx_mtx);
		while (SIMPLEQ_EMPTY(&ioc->rx_qhead)) {
			pthread_cond_wait(&ioc->rx_cond, &ioc->rx_mtx);
			if (ioc->closing)
				goto exit;
		}
		if (ioc->closing)
			goto exit;

		/* Get a cbc request from the queue head */
		req = SIMPLEQ_FIRST(&ioc->rx_qhead);
		SIMPLEQ_REMOVE_HEAD(&ioc->rx_qhead, me_queue);
		pthread_mutex_unlock(&ioc->rx_mtx);
		packet.req = req;

		/*
		 * Reset the queue type to free queue
		 * prepare for routing after main process
		 */
		packet.qtype = CBC_QUEUE_T_FREE;

		/* rx main process */
		ioc->ioc_dev_rx(&packet);

		/* Route the cbc_request */
		if (packet.qtype == CBC_QUEUE_T_TX)
			cbc_request_enqueue(ioc, req, CBC_QUEUE_T_TX, true);
		else
			cbc_request_enqueue(ioc, req, CBC_QUEUE_T_FREE, false);
	}
exit:
	pthread_mutex_unlock(&ioc->rx_mtx);
	return NULL;
}

/*
 * Tx thread waits for CBC requests of tx queue, if tx queue is not empty,
 * it wll get a CBC request from tx queue and invokes cbc_tx_handler to process.
 */
static void *
ioc_tx_thread(void *arg)
{
	struct ioc_dev *ioc = (struct ioc_dev *) arg;
	struct cbc_request *req = NULL;
	struct cbc_pkt packet;

	memset(&packet, 0, sizeof(packet));
	packet.cfg = &ioc->tx_config;
	packet.ioc = ioc;

	for (;;) {
		pthread_mutex_lock(&ioc->tx_mtx);
		while (SIMPLEQ_EMPTY(&ioc->tx_qhead)) {
			pthread_cond_wait(&ioc->tx_cond, &ioc->tx_mtx);
			if (ioc->closing)
				goto exit;
		}
		if (ioc->closing)
			goto exit;

		/* Get a cbc request from the queue head */
		req = SIMPLEQ_FIRST(&ioc->tx_qhead);
		SIMPLEQ_REMOVE_HEAD(&ioc->tx_qhead, me_queue);
		pthread_mutex_unlock(&ioc->tx_mtx);
		packet.req = req;

		/*
		 * Reset the queue type to free queue
		 * prepare for routing after main process
		 */
		packet.qtype = CBC_QUEUE_T_FREE;

		/* tx main process */
		ioc->ioc_dev_tx(&packet);

		/* Route the cbc_request */
		if (packet.qtype == CBC_QUEUE_T_RX)
			cbc_request_enqueue(ioc, req, CBC_QUEUE_T_RX, true);
		else
			cbc_request_enqueue(ioc, req, CBC_QUEUE_T_FREE, false);
	}
exit:
	pthread_mutex_unlock(&ioc->tx_mtx);
	return NULL;
}

/*
 * Stop all threads(core/rx/tx)
 */
static void
ioc_kill_workers(struct ioc_dev *ioc)
{
	ioc->closing = 1;

	/* Stop IOC core thread */
	ioc_update_event(ioc->evt_fd, IOC_E_KNOCK);
	close(ioc->epfd);
	ioc->epfd = IOC_INIT_FD;
	pthread_join(ioc->tid, NULL);

	/* Stop IOC rx thread */
	pthread_mutex_lock(&ioc->rx_mtx);
	pthread_cond_signal(&ioc->rx_cond);
	pthread_mutex_unlock(&ioc->rx_mtx);
	pthread_join(ioc->rx_tid, NULL);

	/* Stop IOC tx thread */
	pthread_mutex_lock(&ioc->tx_mtx);
	pthread_cond_signal(&ioc->tx_cond);
	pthread_mutex_unlock(&ioc->tx_mtx);
	pthread_join(ioc->tx_tid, NULL);

	/* Release the cond and mutex */
	pthread_mutex_destroy(&ioc->rx_mtx);
	pthread_cond_destroy(&ioc->rx_cond);
	pthread_mutex_destroy(&ioc->tx_mtx);
	pthread_cond_destroy(&ioc->tx_cond);
	pthread_mutex_destroy(&ioc->free_mtx);
}

static int
ioc_create_thread(const char *name, pthread_t *tid,
		ioc_work func, void *arg)
{
	if (pthread_create(tid, NULL, func, arg) != 0) {
		DPRINTF("%s", "ioc can not create thread\r\n");
		return -1;
	}
	pthread_setname_np(*tid, name);
	return 0;
}

/*
 * Check if current platform supports IOC or not.
 */
static int
ioc_is_platform_supported(void)
{
	struct stat st;

	/* The early signal channel will be created after cbc attached,
	 * if not, the current platform does not support IOC, exit IOC mediator.
	 */
	return stat(IOC_NP_ESIG, &st);
}

/*
 * The callback to handle with VM stop request.
 * To emulate ignition off wakeup reason including set force S5 bit.
 */
static int
vm_stop_handler(void *arg)
{
	struct ioc_dev *ioc = arg;

	if (!ioc) {
		DPRINTF("%s", "ioc vm stop gets NULL pointer\r\n");
		return -1;
	}
	ioc->vm_req = VM_REQ_STOP;
	return 0;
}

/*
 * The callback to handle with VM suspend.
 * To emulate ignition off wakeup reason.
 */
static int
vm_suspend_handler(void *arg)
{
	struct ioc_dev *ioc = arg;

	if (!ioc) {
		DPRINTF("%s", "ioc vm suspend gets NULL pointer\r\n");
		return -1;
	}
	ioc->vm_req = VM_REQ_SUSPEND;
	return 0;
}

/*
 * The callback to handle with VM resume.
 * To get wakeup reason and trigger IOC_E_RESUME event.
 */
static int
vm_resume_handler(void *arg)
{
	struct ioc_dev *ioc = arg;
	uint32_t reason;

	if (!ioc) {
		DPRINTF("%s", "ioc vm resume gets NULL pointer\r\n");
		return -1;
	}

	reason = get_wakeup_reason();
	if (!reason) {
		DPRINTF("%s", "ioc vm resume gets invalid wakeup reason \r\n");
		return -1;
	}

	/*
	 * Change VM request to resume for stopping the emulation of suspend
	 * and shutdown wakeup reasons.
	 */
	ioc->vm_req = VM_REQ_RESUME;

	ioc->boot_reason = reason;
	ioc_update_event(ioc->evt_fd, IOC_E_RESUME);
	return 0;
}

/*
 * To get IOC bootup reason and virtual UART path for communication
 * between IOC mediator and virtual UART.
 */
int
ioc_parse(const char *opts)
{
	char *tmp, *str, *cpy;
	int rc;

	cpy = str = strdup(opts);
	if (!cpy)
		return -ENOMEM;

	/*
	 * IOC mediator parameters format as below:
	 * <virtual_uart_path>[,<wakeup_reason>]
	 * For e.g. "/run/acrn/ioc_vm1,0x20"
	 */
	tmp = strsep(&str, ",");
	if (!tmp)
		goto exit;

	rc = snprintf(virtual_uart_path, sizeof(virtual_uart_path), "%s", tmp);
	if (rc < 0 || rc >= sizeof(virtual_uart_path))
		WPRINTF("ioc gets incomplete virtual uart path:%s\r\n",
				virtual_uart_path);

	if (!str)
		goto exit;

	ioc_boot_reason = strtoul(str, 0, 0);

	/*
	 * According to the CBC protocol, the wakeup reason only occupies 0-23
	 * bits, so 24-31 bits are used for customized functions, and bit 24 is
	 * used to check whether to enable the ioc mediator debug function.
	 */
	ioc_debug_enable = ioc_boot_reason & CBC_WK_RSN_DGB ? true : false;

	/* Mask invalid bits of wakeup reason for IOC mediator */
	ioc_boot_reason &= CBC_WK_RSN_ALL;

exit:
	free(cpy);
	return 0;
}

/*
 * IOC mediator main entry.
 */
int
ioc_init(struct vmctx *ctx)
{
	int i;
	int rc;
	struct ioc_dev *ioc;

	IOC_LOG_INIT;

	if (ioc_is_platform_supported() != 0)
		goto ioc_err;

	/*
	 * Set default boot wakeup reason as ignition button active if met
	 * invalid parameter.
	 */
	if (ioc_boot_reason == 0)
		ioc_boot_reason = CBC_WK_RSN_BTN;
	ioc = (struct ioc_dev *)calloc(1, sizeof(struct ioc_dev));
	if (!ioc)
		goto ioc_err;
	ioc->pool = (struct cbc_request *)calloc(IOC_MAX_REQUESTS,
			sizeof(struct cbc_request));
	ioc->evts = (struct epoll_event *)calloc(ARRAY_SIZE(ioc_ch_tbl),
			sizeof(struct epoll_event));
	if (!ioc->pool || !ioc->evts)
		goto alloc_err;

	/*
	 * IOC mediator needs to manage more than 15 channels with mass data
	 * transfer, to avoid blocking other mevent users, IOC mediator
	 * creates its own epoll in one separated thread.
	 */
	ioc->epfd = epoll_create1(0);
	if (ioc->epfd < 0)
		goto alloc_err;

	/*
	 * Register IOC mediator VM ops for stop/suspend/resume.
	 */
	if (monitor_register_vm_ops(&vm_ops, ioc, "ioc_dm") < 0) {
		DPRINTF("%s", "ioc register to VM monitor failed\r\n");
		goto alloc_err;
	}

	/*
	 * Put all buffered CBC requests on the free queue, the free queue is
	 * used to be a cbc_request buffer.
	 */
	SIMPLEQ_INIT(&ioc->free_qhead);
	pthread_mutex_init(&ioc->free_mtx, NULL);
	for (i = 0; i < IOC_MAX_REQUESTS; i++)
		SIMPLEQ_INSERT_TAIL(&ioc->free_qhead, ioc->pool + i, me_queue);

	/* Initialize IOC state */
	ioc->state = IOC_S_INIT;

	/* Set event fd to default value */
	ioc->evt_fd = IOC_INIT_FD;

	/* Enable CBC packet processing by default */
	ioc->cbc_enable = true;

	/* Set boot reason from IOC mediator boot command line */
	ioc->boot_reason = ioc_boot_reason;

	/*
	 * Initialize native CBC cdev and virtual UART.
	 */
	if (ioc_ch_init(ioc) != 0)
		goto chl_err;

	/*
	 * Make sure the CBC signal channel is activated after channel
	 * initialization is successful.
	 * TODO: Check firmware version before sending open channel command
	 * since the old IOC firmware needs not use open channel command to
	 * activate signal channel that is activated by default.
	 */
	if (ioc_ch_xmit(IOC_NATIVE_SIGNAL, cbc_open_channel_command,
				sizeof(cbc_open_channel_command)) <= 0)
		DPRINTF("%s", "ioc sends CBC open channel command failed\r\n");

	/* Initlialize CBC rx/tx signal and group whitelists */
	wlist_init_signal(cbc_rx_signal_table, ARRAY_SIZE(cbc_rx_signal_table),
			wlist_rx_signal_table,
			ARRAY_SIZE(wlist_rx_signal_table));
	wlist_init_group(cbc_rx_group_table, ARRAY_SIZE(cbc_rx_group_table),
			wlist_rx_group_table,
			ARRAY_SIZE(wlist_rx_group_table));
	wlist_init_signal(cbc_tx_signal_table, ARRAY_SIZE(cbc_tx_signal_table),
			wlist_tx_signal_table,
			ARRAY_SIZE(wlist_tx_signal_table));
	wlist_init_group(cbc_tx_group_table, ARRAY_SIZE(cbc_tx_group_table),
			wlist_tx_group_table,
			ARRAY_SIZE(wlist_tx_group_table));

	/* Setup IOC rx members */
	rc = snprintf(ioc->rx_name, sizeof(ioc->rx_name), "ioc_rx");
	if (rc < 0)
		WPRINTF("%s", "ioc fails to set ioc_rx thread name\r\n");

	ioc->ioc_dev_rx = cbc_rx_handler;
	pthread_cond_init(&ioc->rx_cond, NULL);
	pthread_mutex_init(&ioc->rx_mtx, NULL);
	SIMPLEQ_INIT(&ioc->rx_qhead);
	ioc->rx_config.cbc_sig_num = ARRAY_SIZE(cbc_rx_signal_table);
	ioc->rx_config.cbc_grp_num = ARRAY_SIZE(cbc_rx_group_table);
	ioc->rx_config.wlist_sig_num = ARRAY_SIZE(wlist_rx_signal_table);
	ioc->rx_config.wlist_grp_num = ARRAY_SIZE(wlist_rx_group_table);
	ioc->rx_config.cbc_sig_tbl = cbc_rx_signal_table;
	ioc->rx_config.cbc_grp_tbl = cbc_rx_group_table;
	ioc->rx_config.wlist_sig_tbl = wlist_rx_signal_table;
	ioc->rx_config.wlist_grp_tbl = wlist_rx_group_table;

	/* Setup IOC tx members */
	rc = snprintf(ioc->tx_name, sizeof(ioc->tx_name), "ioc_tx");
	if (rc < 0)
		WPRINTF("%s", "ioc fails to set ioc_tx thread name\r\n");

	ioc->ioc_dev_tx = cbc_tx_handler;
	pthread_cond_init(&ioc->tx_cond, NULL);
	pthread_mutex_init(&ioc->tx_mtx, NULL);
	SIMPLEQ_INIT(&ioc->tx_qhead);
	ioc->tx_config.cbc_sig_num = ARRAY_SIZE(cbc_tx_signal_table);
	ioc->tx_config.cbc_grp_num = ARRAY_SIZE(cbc_tx_group_table);
	ioc->tx_config.wlist_sig_num = ARRAY_SIZE(wlist_tx_signal_table);
	ioc->tx_config.wlist_grp_num = ARRAY_SIZE(wlist_tx_group_table);
	ioc->tx_config.cbc_sig_tbl = cbc_tx_signal_table;
	ioc->tx_config.cbc_grp_tbl = cbc_tx_group_table;
	ioc->tx_config.wlist_sig_tbl = wlist_tx_signal_table;
	ioc->tx_config.wlist_grp_tbl = wlist_tx_group_table;

	/*
	 * Three threads are created for IOC work flow.
	 * Rx thread is responsible for writing data to native CBC cdevs.
	 * Tx thread is responsible for writing data to virtual UART.
	 * Core thread is responsible for reading data from native CBC cdevs
	 * and virtual UART.
	 */
	if (ioc_create_thread(ioc->rx_name, &ioc->rx_tid, ioc_rx_thread,
			(void *)ioc) < 0)
		goto work_err;
	if (ioc_create_thread(ioc->tx_name, &ioc->tx_tid, ioc_tx_thread,
			(void *)ioc) < 0)
		goto work_err;
	rc = snprintf(ioc->name, sizeof(ioc->name), "ioc_core");
	if (rc < 0)
		WPRINTF("%s", "ioc fails to set ioc_core thread name\r\n");

	if (ioc_create_thread(ioc->name, &ioc->tid, ioc_core_thread,
			(void *)ioc) < 0)
		goto work_err;

	ctx->ioc_dev = ioc;
	return 0;

work_err:
	pthread_mutex_destroy(&ioc->rx_mtx);
	pthread_cond_destroy(&ioc->rx_cond);
	pthread_mutex_destroy(&ioc->tx_mtx);
	pthread_cond_destroy(&ioc->tx_cond);
	ioc_kill_workers(ioc);
chl_err:
	ioc_ch_deinit();
	pthread_mutex_destroy(&ioc->free_mtx);
	if (ioc->evt_fd >= 0)
		close(ioc->evt_fd);
	close(ioc->epfd);
alloc_err:
	free(ioc->evts);
	free(ioc->pool);
	free(ioc);
ioc_err:
	IOC_LOG_DEINIT;
	DPRINTF("%s", "ioc mediator startup failed!!\r\n");
	return -1;
}

/*
 * Called by DM in main entry.
 */
void
ioc_deinit(struct vmctx *ctx)
{
	struct ioc_dev *ioc = ctx->ioc_dev;

	if (!ioc) {
		DPRINTF("%s", "ioc deinit parameter is NULL\r\n");
		return;
	}
	ioc_kill_workers(ioc);
	ioc_ch_deinit();
	if (ioc->evt_fd >= 0)
		close(ioc->evt_fd);
	close(ioc->epfd);
	free(ioc->evts);
	free(ioc->pool);
	free(ioc);
	IOC_LOG_DEINIT;

	ctx->ioc_dev = NULL;
}
