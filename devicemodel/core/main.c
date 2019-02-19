/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <sysexits.h>
#include <stdbool.h>
#include <getopt.h>

#include "vmmapi.h"
#include "sw_load.h"
#include "cpuset.h"
#include "dm.h"
#include "acpi.h"
#include "atkbdc.h"
#include "inout.h"
#include "ioapic.h"
#include "mem.h"
#include "mevent.h"
#include "mptbl.h"
#include "pci_core.h"
#include "irq.h"
#include "lpc.h"
#include "smbiostbl.h"
#include "rtc.h"
#include "pit.h"
#include "version.h"
#include "sw_load.h"
#include "monitor.h"
#include "ioc.h"
#include "pm.h"
#include "atomic.h"
#include "vmcfg_config.h"
#include "vmcfg.h"
#include "virtio.h"

#define GUEST_NIO_PORT		0x488	/* guest upcalls via i/o port */

/* Values returned for reads on invalid I/O requests. */
#define VHM_REQ_PIO_INVAL	(~0U)
#define VHM_REQ_MMIO_INVAL	(~0UL)

typedef void (*vmexit_handler_t)(struct vmctx *,
		struct vhm_request *, int *vcpu);

char *vmname;

int guest_ncpus;
char *guest_uuid_str;
char *vsbl_file_name;
char *ovmf_file_name;
char *kernel_file_name;
char *elf_file_name;
uint8_t trusty_enabled;
char *mac_seed;
bool stdio_in_use;

static int virtio_msix = 1;
static bool debugexit_enabled;
static char mac_seed_str[50];

static int acpi;

static char *progname;
static const int BSP;

static cpuset_t cpumask;

static void vm_loop(struct vmctx *ctx);

static char vhm_request_page[4096] __aligned(4096);

static struct vhm_request *vhm_req_buf =
				(struct vhm_request *)&vhm_request_page;

struct dmstats {
	uint64_t	vmexit_bogus;
	uint64_t	vmexit_reqidle;
	uint64_t	vmexit_hlt;
	uint64_t	vmexit_pause;
	uint64_t	vmexit_mtrap;
	uint64_t	cpu_switch_rotate;
	uint64_t	cpu_switch_direct;
	uint64_t	vmexit_mmio_emul;
} stats;

struct mt_vmm_info {
	pthread_t	mt_thr;
	struct vmctx	*mt_ctx;
	int		mt_vcpu;
} mt_vmm_info[VM_MAXCPU];

static cpuset_t *vcpumap[VM_MAXCPU] = { NULL };

static struct vmctx *_ctx;

static void
usage(int code)
{
	fprintf(stderr,
		"Usage: %s [-hAEWY] [-c vcpus] [-l <lpc>]\n"
		"       %*s [-m mem] [-p vcpu:hostcpu] [-s <pci>] [-U uuid] \n"
		"       %*s [--vsbl vsbl_file_name] [--part_info part_info_name]\n"
		"       %*s [--enable_trusty] [--debugexit] <vm>\n"
		"       -A: create ACPI tables\n"
		"       -c: # cpus (default 1)\n"
		"       -E: elf image path\n"
		"       -h: help\n"
		"       -l: LPC device configuration\n"
		"       -m: memory size in MB\n"
		"       -p: pin 'vcpu' to 'hostcpu'\n"
		"       -s: <slot,driver,configinfo> PCI slot config\n"
		"       -U: uuid\n"
		"       -W: force virtio to use single-vector MSI\n"
		"       -Y: disable MPtable generation\n"
		"       -k: kernel image path\n"
		"       -r: ramdisk image path\n"
		"       -B: bootargs for kernel\n"
		"       -G: GVT args: low_gm_size, high_gm_size, fence_sz\n"
		"       -v: version\n"
		"       -i: ioc boot parameters\n"
#ifdef CONFIG_VM_CFG
		"       --vmcfg: build-in VM configurations\n"
		"       --dump: show build-in VM configurations\n"
#endif
		"       --vsbl: vsbl file path\n"
		"       --ovmf: ovmf file path\n"
		"       --part_info: guest partition info file path\n"
		"       --enable_trusty: enable trusty for guest\n"
		"       --ptdev_no_reset: disable reset check for ptdev\n"
		"       --debugexit: enable debug exit function\n"
		"       --intr_monitor: enable interrupt storm monitor\n"
		"............its params: threshold/s,probe-period(s),delay_time(ms),delay_duration(ms)\n",
		progname, (int)strnlen(progname, PATH_MAX), "", (int)strnlen(progname, PATH_MAX), "",
		(int)strnlen(progname, PATH_MAX), "");

	exit(code);
}

static void
print_version(void)
{
	fprintf(stdout, "DM version is: %s-%s (daily tag:%s), build by %s@%s\n",
			DM_FULL_VERSION,
			DM_BUILD_VERSION, DM_DAILY_TAG, DM_BUILD_USER, DM_BUILD_TIME);

	exit(0);
}

static int
pincpu_parse(const char *opt)
{
	int vcpu, pcpu;
	char *cp;

	if (dm_strtoi(opt, &cp, 10, &vcpu) || *cp != ':' ||
		dm_strtoi(cp + 1, &cp, 10, &pcpu)) {
		fprintf(stderr, "invalid format: %s\n", opt);
		return -1;
	}

	if (vcpu < 0 || vcpu >= VM_MAXCPU) {
		fprintf(stderr, "vcpu '%d' outside valid range from 0 to %d\n",
		    vcpu, VM_MAXCPU - 1);
		return -1;
	}

	if (pcpu < 0 || pcpu >= CPU_SETSIZE) {
		fprintf(stderr,
			"hostcpu '%d' outside valid range from 0 to %d\n",
			pcpu, CPU_SETSIZE - 1);
		return -1;
	}

	if (vcpumap[vcpu] == NULL) {
		vcpumap[vcpu] = malloc(sizeof(cpuset_t));
		if (vcpumap[vcpu] == NULL) {
			perror("malloc");
			return -1;
		}
		CPU_ZERO(vcpumap[vcpu]);
	}
	CPU_SET(pcpu, vcpumap[vcpu]);
	return 0;
}

/**
 * @brief Convert guest physical address to host virtual address
 *
 * @param ctx Pointer to to struct vmctx representing VM context.
 * @param gaddr Guest physical address base.
 * @param len Guest physical address length.
 *
 * @return NULL on convert failed and host virtual address on successful.
 */
void *
paddr_guest2host(struct vmctx *ctx, uintptr_t gaddr, size_t len)
{
	return vm_map_gpa(ctx, gaddr, len);
}

int
virtio_uses_msix(void)
{
	return virtio_msix;
}

size_t
high_bios_size(void)
{
	size_t size = 0;

	if (ovmf_file_name)
		size = ovmf_image_size();

	return roundup2(size, 2 * MB);
}

static void *
start_thread(void *param)
{
	char tname[MAXCOMLEN + 1];
	struct mt_vmm_info *mtp;
	int vcpu;

	mtp = param;
	vcpu = mtp->mt_vcpu;

	snprintf(tname, sizeof(tname), "vcpu %d", vcpu);
	pthread_setname_np(mtp->mt_thr, tname);

	vm_loop(mtp->mt_ctx);

	/* reset or halt */
	return NULL;
}

static int
add_cpu(struct vmctx *ctx, int guest_ncpus)
{
	int i;
	int error;

	for (i = 0; i < guest_ncpus; i++) {
		error = vm_create_vcpu(ctx, (uint16_t)i);
		if (error != 0) {
			fprintf(stderr, "ERROR: could not create VCPU %d\n", i);
			return error;
		}

		CPU_SET_ATOMIC(i, &cpumask);

		mt_vmm_info[i].mt_ctx = ctx;
		mt_vmm_info[i].mt_vcpu = i;
	}

	vm_set_vcpu_regs(ctx, &ctx->bsp_regs);

	error = pthread_create(&mt_vmm_info[0].mt_thr, NULL,
	    start_thread, &mt_vmm_info[0]);
	return error;
}

static int
delete_cpu(struct vmctx *ctx, int vcpu)
{
	if (!CPU_ISSET(vcpu, &cpumask)) {
		fprintf(stderr, "Attempting to delete unknown cpu %d\n", vcpu);
		exit(1);
	}

	vm_destroy_ioreq_client(ctx);
	pthread_join(mt_vmm_info[0].mt_thr, NULL);

	CPU_CLR_ATOMIC(vcpu, &cpumask);
	return CPU_EMPTY(&cpumask);
}

void
notify_vmloop_thread(void)
{
	pthread_kill(mt_vmm_info[0].mt_thr, SIGCONT);
	return;
}

static void
vmexit_inout(struct vmctx *ctx, struct vhm_request *vhm_req, int *pvcpu)
{
	int error;
	int bytes, port, in;

	port = vhm_req->reqs.pio_request.address;
	bytes = vhm_req->reqs.pio_request.size;
	in = (vhm_req->reqs.pio_request.direction == REQUEST_READ);

	error = emulate_inout(ctx, pvcpu, &vhm_req->reqs.pio_request);
	if (error) {
		fprintf(stderr, "Unhandled %s%c 0x%04x\n",
				in ? "in" : "out",
				bytes == 1 ? 'b' : (bytes == 2 ? 'w' : 'l'),
				port);

		if (in) {
			vhm_req->reqs.pio_request.value = VHM_REQ_PIO_INVAL;
		}
	}
}

static void
vmexit_mmio_emul(struct vmctx *ctx, struct vhm_request *vhm_req, int *pvcpu)
{
	int err;

	stats.vmexit_mmio_emul++;
	err = emulate_mem(ctx, &vhm_req->reqs.mmio_request);

	if (err) {
		if (err == -ESRCH)
			fprintf(stderr, "Unhandled memory access to 0x%lx\n",
				vhm_req->reqs.mmio_request.address);

		fprintf(stderr, "Failed to emulate instruction [");
		fprintf(stderr, "mmio address 0x%lx, size %ld",
				vhm_req->reqs.mmio_request.address,
				vhm_req->reqs.mmio_request.size);

		if (vhm_req->reqs.mmio_request.direction == REQUEST_READ) {
			vhm_req->reqs.mmio_request.value = VHM_REQ_MMIO_INVAL;
		}
	}
}

static void
vmexit_pci_emul(struct vmctx *ctx, struct vhm_request *vhm_req, int *pvcpu)
{
	int err, in = (vhm_req->reqs.pci_request.direction == REQUEST_READ);

	err = emulate_pci_cfgrw(ctx, *pvcpu, in,
			vhm_req->reqs.pci_request.bus,
			vhm_req->reqs.pci_request.dev,
			vhm_req->reqs.pci_request.func,
			vhm_req->reqs.pci_request.reg,
			vhm_req->reqs.pci_request.size,
			&vhm_req->reqs.pci_request.value);
	if (err) {
		fprintf(stderr, "Unhandled pci cfg rw at %x:%x.%x reg 0x%x\n",
			vhm_req->reqs.pci_request.bus,
			vhm_req->reqs.pci_request.dev,
			vhm_req->reqs.pci_request.func,
			vhm_req->reqs.pci_request.reg);

		if (in) {
			vhm_req->reqs.pio_request.value = VHM_REQ_PIO_INVAL;
		}
	}
}

#define	DEBUG_EPT_MISCONFIG

#ifdef DEBUG_EPT_MISCONFIG

#define	EXIT_REASON_EPT_MISCONFIG	49
#define	VMCS_GUEST_PHYSICAL_ADDRESS	0x00002400
#define	VMCS_IDENT(x)			((x) | 0x80000000)

#endif	/* #ifdef DEBUG_EPT_MISCONFIG */

static vmexit_handler_t handler[VM_EXITCODE_MAX] = {
	[VM_EXITCODE_INOUT]  = vmexit_inout,
	[VM_EXITCODE_MMIO_EMUL] = vmexit_mmio_emul,
	[VM_EXITCODE_PCI_CFG] = vmexit_pci_emul,
};

static void
handle_vmexit(struct vmctx *ctx, struct vhm_request *vhm_req, int vcpu)
{
	enum vm_exitcode exitcode;

	exitcode = vhm_req->type;
	if (exitcode >= VM_EXITCODE_MAX || handler[exitcode] == NULL) {
		fprintf(stderr, "handle vmexit: unexpected exitcode 0x%x\n",
				exitcode);
		exit(1);
	}

	(*handler[exitcode])(ctx, vhm_req, &vcpu);

	/* We cannot notify the VHM/hypervisor on the request completion at this
	 * point if the UOS is in suspend or system reset mode, as the VM is
	 * still not paused and a notification can kick off the vcpu to run
	 * again. Postpone the notification till vm_system_reset() or
	 * vm_suspend_resume() for resetting the ioreq states in the VHM and
	 * hypervisor.
	 */
	if ((VM_SUSPEND_SYSTEM_RESET == vm_get_suspend_mode()) ||
		(VM_SUSPEND_SUSPEND == vm_get_suspend_mode()))
		return;

	vm_notify_request_done(ctx, vcpu);
}

static int
vm_init_vdevs(struct vmctx *ctx)
{
	int ret;

	init_mem();
	init_inout();
	pci_irq_init(ctx);
	atkbdc_init(ctx);
	ioapic_init(ctx);

	/*
	 * We don't care ioc_init return value so far.
	 * Will add return value check once ioc is full function.
	 */
	ret = ioc_init(ctx);

	ret = vrtc_init(ctx);
	if (ret < 0)
		goto vrtc_fail;

	ret = vpit_init(ctx);
	if (ret < 0)
		goto vpit_fail;

	sci_init(ctx);

	if (debugexit_enabled)
		init_debugexit();

	ret = monitor_init(ctx);
	if (ret < 0)
		goto monitor_fail;

	ret = init_pci(ctx);
	if (ret < 0)
		goto pci_fail;

	return 0;

pci_fail:
	monitor_close();
monitor_fail:
	if (debugexit_enabled)
		deinit_debugexit();

	vpit_deinit(ctx);
vpit_fail:
	vrtc_deinit(ctx);
vrtc_fail:
	ioc_deinit(ctx);
	atkbdc_deinit(ctx);
	pci_irq_deinit(ctx);
	ioapic_deinit();
	return -1;
}

static void
vm_deinit_vdevs(struct vmctx *ctx)
{
	deinit_pci(ctx);
	monitor_close();

	if (debugexit_enabled)
		deinit_debugexit();

	vpit_deinit(ctx);
	vrtc_deinit(ctx);
	ioc_deinit(ctx);
	atkbdc_deinit(ctx);
	pci_irq_deinit(ctx);
	ioapic_deinit();
}

static void
vm_reset_vdevs(struct vmctx *ctx)
{
	/*
	 * The current virtual devices doesn't define virtual
	 * device reset function. So we call vdev deinit/init
	 * pairing to emulate the device reset operation.
	 *
	 * pci/ioapic deinit/init is needed because of dependency
	 * of pci irq allocation/free.
	 *
	 * acpi build is necessary because irq for each vdev
	 * could be assigned with different number after reset.
	 */
	atkbdc_deinit(ctx);

	if (debugexit_enabled)
		deinit_debugexit();

	vpit_deinit(ctx);
	vrtc_deinit(ctx);

	deinit_pci(ctx);
	pci_irq_deinit(ctx);
	ioapic_deinit();

	pci_irq_init(ctx);
	atkbdc_init(ctx);
	vrtc_init(ctx);
	vpit_init(ctx);

	if (debugexit_enabled)
		init_debugexit();

	ioapic_init(ctx);
	init_pci(ctx);

	if (acpi) {
		acpi_build(ctx, guest_ncpus);
	}
}

static void
vm_system_reset(struct vmctx *ctx)
{
	/*
	 * If we get system reset request, we don't want to exit the
	 * vcpu_loop/vm_loop/mevent_loop. So we do:
	 *   1. pause VM
	 *   2. flush and clear ioreqs
	 *   3. reset virtual devices
	 *   4. load software for UOS
	 *   5. hypercall reset vm
	 *   6. reset suspend mode to VM_SUSPEND_NONE
	 */

	vm_pause(ctx);

	/*
	 * After vm_pause, there should be no new coming ioreq.
	 *
	 * Unless under emergency mode, the vcpu writing to the ACPI PM
	 * CR should be the only vcpu of that VM that is still
	 * running. In this case there should be only one completed
	 * request which is the APIC PM CR write. VM reset will reset it
	 *
	 * When handling emergency mode triggered by one vcpu without
	 * offlining any other vcpus, there can be multiple VHM requests
	 * with various states. We should be careful on potential races
	 * when resetting especially in SMP SOS. vm_clear_ioreq can be used
	 * to clear all ioreq status in VHM after VM pause, then let VM
	 * reset in hypervisor reset all ioreqs.
	 */
	vm_clear_ioreq(ctx);

	vm_reset_vdevs(ctx);
	vm_reset(ctx);
	vm_set_suspend_mode(VM_SUSPEND_NONE);

	/* set the BSP init state */
	acrn_sw_load(ctx);
	vm_set_vcpu_regs(ctx, &ctx->bsp_regs);
	vm_run(ctx);
}

static void
vm_suspend_resume(struct vmctx *ctx)
{
	/*
	 * If we get warm reboot request, we don't want to exit the
	 * vcpu_loop/vm_loop/mevent_loop. So we do:
	 *   1. pause VM
	 *   2. flush and clear ioreqs
	 *   3. stop vm watchdog
	 *   4. wait for resume signal
	 *   5. reset vm watchdog
	 *   6. hypercall restart vm
	 */
	vm_pause(ctx);

	vm_clear_ioreq(ctx);
	vm_stop_watchdog(ctx);
	wait_for_resume(ctx);

	pm_backto_wakeup(ctx);
	vm_reset_watchdog(ctx);
	vm_reset(ctx);

	/* set the BSP init state */
	vm_set_vcpu_regs(ctx, &ctx->bsp_regs);
	vm_run(ctx);
}

static void
vm_loop(struct vmctx *ctx)
{
	int error;

	ctx->ioreq_client = vm_create_ioreq_client(ctx);
	assert(ctx->ioreq_client > 0);

	error = vm_run(ctx);
	assert(error == 0);

	while (1) {
		int vcpu_id;
		struct vhm_request *vhm_req;

		error = vm_attach_ioreq_client(ctx);
		if (error)
			break;

		for (vcpu_id = 0; vcpu_id < 4; vcpu_id++) {
			vhm_req = &vhm_req_buf[vcpu_id];
			if ((atomic_load(&vhm_req->processed) == REQ_STATE_PROCESSING)
				&& (vhm_req->client == ctx->ioreq_client))
				handle_vmexit(ctx, vhm_req, vcpu_id);
		}

		if (VM_SUSPEND_FULL_RESET == vm_get_suspend_mode() ||
		    VM_SUSPEND_POWEROFF == vm_get_suspend_mode()) {
			break;
		}

		if (VM_SUSPEND_SYSTEM_RESET == vm_get_suspend_mode()) {
			vm_system_reset(ctx);
		}

		if (VM_SUSPEND_SUSPEND == vm_get_suspend_mode()) {
			vm_suspend_resume(ctx);
		}
	}
	printf("VM loop exit\n");
}

static int
num_vcpus_allowed(struct vmctx *ctx)
{
	/* TODO: add ioctl to get gerneric information including
	 * virtual cpus, now hardcode
	 */
	return VM_MAXCPU;
}

static void
sig_handler_term(int signo)
{
	printf("Receive SIGINT to terminate application...\n");
	vm_set_suspend_mode(VM_SUSPEND_POWEROFF);
	mevent_notify();
}

enum {
	CMD_OPT_VSBL = 1000,
	CMD_OPT_OVMF,
	CMD_OPT_PART_INFO,
	CMD_OPT_TRUSTY_ENABLE,
	CMD_OPT_VIRTIO_POLL_ENABLE,
	CMD_OPT_MAC_SEED,
	CMD_OPT_PTDEV_NO_RESET,
	CMD_OPT_DEBUGEXIT,
	CMD_OPT_VMCFG,
	CMD_OPT_DUMP,
	CMD_OPT_INTR_MONITOR,
};

static struct option long_options[] = {
	{"acpi",		no_argument,		0, 'A' },
	{"pincpu",		required_argument,	0, 'p' },
	{"ncpus",		required_argument,	0, 'c' },
	{"elf_file",		required_argument,	0, 'E' },
	{"ioc_node",		required_argument,	0, 'i' },
	{"lpc",			required_argument,	0, 'l' },
	{"pci_slot",		required_argument,	0, 's' },
	{"memsize",		required_argument,	0, 'm' },
	{"uuid",		required_argument,	0, 'U' },
	{"virtio_msix",		no_argument,		0, 'W' },
	{"mptgen",		no_argument,		0, 'Y' },
	{"kernel",		required_argument,	0, 'k' },
	{"ramdisk",		required_argument,	0, 'r' },
	{"bootargs",		required_argument,	0, 'B' },
	{"version",		no_argument,		0, 'v' },
	{"gvtargs",		required_argument,	0, 'G' },
	{"help",		no_argument,		0, 'h' },

	/* Following cmd option only has long option */
#ifdef CONFIG_VM_CFG
	{"vmcfg",		required_argument,	0, CMD_OPT_VMCFG},
	{"dump",		required_argument,	0, CMD_OPT_DUMP},
#endif
	{"vsbl",		required_argument,	0, CMD_OPT_VSBL},
	{"ovmf",		required_argument,	0, CMD_OPT_OVMF},
	{"part_info",		required_argument,	0, CMD_OPT_PART_INFO},
	{"enable_trusty",	no_argument,		0,
					CMD_OPT_TRUSTY_ENABLE},
	{"virtio_poll",		required_argument,	0, CMD_OPT_VIRTIO_POLL_ENABLE},
	{"mac_seed",		required_argument,	0, CMD_OPT_MAC_SEED},
	{"ptdev_no_reset",	no_argument,		0,
		CMD_OPT_PTDEV_NO_RESET},
	{"debugexit",		no_argument,		0, CMD_OPT_DEBUGEXIT},
	{"intr_monitor",	required_argument,	0, CMD_OPT_INTR_MONITOR},
	{0,			0,			0,  0  },
};

static char optstr[] = "hAWYvE:k:r:B:p:c:s:m:l:U:G:i:";

int
dm_run(int argc, char *argv[])
{
	int c, error, err;
	int max_vcpus, mptgen;
	struct vmctx *ctx;
	size_t memsize;
	int option_idx = 0;

	progname = basename(argv[0]);
	guest_ncpus = 1;
	memsize = 256 * MB;
	mptgen = 1;

	if (signal(SIGHUP, sig_handler_term) == SIG_ERR)
		fprintf(stderr, "cannot register handler for SIGHUP\n");
	if (signal(SIGINT, sig_handler_term) == SIG_ERR)
		fprintf(stderr, "cannot register handler for SIGINT\n");

	while ((c = getopt_long(argc, argv, optstr, long_options,
			&option_idx)) != -1) {
		switch (c) {
		case 'A':
			acpi = 1;
			break;
		case 'p':
			if (pincpu_parse(optarg) != 0) {
				errx(EX_USAGE,
				"invalid vcpu pinning configuration '%s'",
				optarg);
			}
			break;
		case 'c':
			dm_strtoi(optarg, NULL, 0, &guest_ncpus);
			break;
		case 'E':
			if (acrn_parse_elf(optarg) != 0)
				exit(1);
			else
				break;
			break;
		case 'i':
			ioc_parse(optarg);
			break;

		case 'l':
			if (lpc_device_parse(optarg) != 0) {
				errx(EX_USAGE,
					"invalid lpc device configuration '%s'",
					optarg);
			}
			break;
		case 's':
			if (pci_parse_slot(optarg) != 0)
				exit(1);
			else
				break;
		case 'm':
			error = vm_parse_memsize(optarg, &memsize);
			if (error)
				errx(EX_USAGE, "invalid memsize '%s'", optarg);
			break;
		case 'U':
			guest_uuid_str = optarg;
			break;
		case 'W':
			virtio_msix = 0;
			break;
		case 'Y':
			mptgen = 0;
			break;
		case 'k':
			if (acrn_parse_kernel(optarg) != 0)
				exit(1);
			else
				break;
		case 'r':
			if (acrn_parse_ramdisk(optarg) != 0)
				exit(1);
			else
				break;
		case 'B':
			if (acrn_parse_bootargs(optarg) != 0)
				exit(1);
			else
				break;
			break;
		case 'G':
			if (acrn_parse_gvtargs(optarg) != 0) {
				errx(EX_USAGE, "invalid GVT param %s", optarg);
				exit(1);
			}
			break;
		case 'v':
			print_version();
			break;
		case CMD_OPT_VSBL:
			if (high_bios_size() == 0 && acrn_parse_vsbl(optarg) != 0) {
				errx(EX_USAGE, "invalid vsbl param %s", optarg);
				exit(1);
			}
			break;
		case CMD_OPT_OVMF:
			if (!vsbl_file_name && acrn_parse_ovmf(optarg) != 0) {
				errx(EX_USAGE, "invalid ovmf param %s", optarg);
				exit(1);
			}
			break;
		case CMD_OPT_PART_INFO:
			if (acrn_parse_guest_part_info(optarg) != 0) {
				errx(EX_USAGE,
					"invalid guest partition info param %s",
					optarg);
				exit(1);
			}
			break;
		case CMD_OPT_TRUSTY_ENABLE:
			trusty_enabled = 1;
			break;
		case CMD_OPT_VIRTIO_POLL_ENABLE:
			if (acrn_parse_virtio_poll_interval(optarg) != 0) {
				errx(EX_USAGE,
					"invalid virtio poll interval %s",
					optarg);
				exit(1);
			}
			break;
		case CMD_OPT_MAC_SEED:
			strncpy(mac_seed_str, optarg, sizeof(mac_seed_str));
			mac_seed_str[sizeof(mac_seed_str) - 1] = '\0';
			mac_seed = mac_seed_str;
			break;
		case CMD_OPT_PTDEV_NO_RESET:
			ptdev_no_reset(true);
			break;
		case CMD_OPT_DEBUGEXIT:
			debugexit_enabled = true;
			break;
		case CMD_OPT_INTR_MONITOR:
			if (acrn_parse_intr_monitor(optarg) != 0) {
				errx(EX_USAGE, "invalid intr-monitor params %s", optarg);
				exit(1);
			}
			break;
		case 'h':
			usage(0);
		default:
			usage(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage(1);

	if (!check_hugetlb_support()) {
		fprintf(stderr, "check_hugetlb_support failed\n");
		exit(1);
	}

	vmname = argv[0];

	for (;;) {
		write_kmsg("%s vm_create begin---\n", KMSG_FMT);
		ctx = vm_create(vmname, (unsigned long)vhm_req_buf);
		if (!ctx) {
			perror("vm_open");
			exit(1);
		}

		if (guest_ncpus < 1) {
			fprintf(stderr, "Invalid guest vCPUs (%d)\n",
				guest_ncpus);
			goto fail;
		}
		write_kmsg("%s vm_create end---\n", KMSG_FMT);

		max_vcpus = num_vcpus_allowed(ctx);
		if (guest_ncpus > max_vcpus) {
			fprintf(stderr, "%d vCPUs requested but %d available\n",
				guest_ncpus, max_vcpus);
			goto fail;
		}
		write_kmsg("%s num_vpcpu_allowed end---\n", KMSG_FMT);

		err = vm_setup_memory(ctx, memsize);
		if (err) {
			fprintf(stderr, "Unable to setup memory (%d)\n", errno);
			goto fail;
		}
		write_kmsg("%s vm_setup_memory end---\n", KMSG_FMT);

		err = mevent_init();
		if (err) {
			fprintf(stderr, "Unable to initialize mevent (%d)\n",
				errno);
			goto mevent_fail;
		}
		write_kmsg("%s mevent_init end---\n", KMSG_FMT);

		if (vm_init_vdevs(ctx) < 0) {
			fprintf(stderr, "Unable to init vdev (%d)\n", errno);
			goto dev_fail;
		}
		write_kmsg("%s vm_init_vdevs end---\n", KMSG_FMT);

		/*
		 * build the guest tables, MP etc.
		 */
		if (mptgen) {
			error = mptable_build(ctx, guest_ncpus);
			if (error) {
				goto vm_fail;
			}
		}
		write_kmsg("%s mptable_build end---\n", KMSG_FMT);

		error = smbios_build(ctx);
		if (error)
			goto vm_fail;

		write_kmsg("%s smbios_build end---\n", KMSG_FMT);

		if (acpi) {
			error = acpi_build(ctx, guest_ncpus);
			if (error)
				goto vm_fail;
		}
		write_kmsg("%s acpi_build end---\n", KMSG_FMT);

		error = acrn_sw_load(ctx);
		if (error)
			goto vm_fail;
		write_kmsg("%s acrn_sw_load end---\n", KMSG_FMT);

		/*
		 * Change the proc title to include the VM name.
		 */
		/*setproctitle("%s", vmname);*/

		/*
		 * Add CPU 0
		 */
		write_kmsg("%s add_cpu begin---\n", KMSG_FMT);
		error = add_cpu(ctx, guest_ncpus);
		if (error)
			goto vm_fail;
		write_kmsg("%s add_cpu end---\n", KMSG_FMT);
		/* Make a copy for ctx */
		_ctx = ctx;

		/*
		 * Head off to the main event dispatch loop
		 */
		mevent_dispatch();

		vm_pause(ctx);
		delete_cpu(ctx, BSP);

		if (vm_get_suspend_mode() != VM_SUSPEND_FULL_RESET)
			break;

		vm_deinit_vdevs(ctx);
		mevent_deinit();
		vm_unsetup_memory(ctx);
		vm_destroy(ctx);
		_ctx = 0;

		vm_set_suspend_mode(VM_SUSPEND_NONE);
	}

vm_fail:
	vm_deinit_vdevs(ctx);
dev_fail:
	mevent_deinit();
mevent_fail:
	vm_unsetup_memory(ctx);
fail:
	vm_destroy(ctx);
	exit(0);
}

int main(int argc, char *argv[])
{
	int c;
	int option_idx = 0;
	int dm_options = 0, vmcfg = 0;
	int index = -1;

	while ((c = getopt_long(argc, argv, optstr, long_options,
			&option_idx)) != -1) {
		switch (c) {
		case CMD_OPT_VMCFG:
			vmcfg = 1;
			dm_strtoi(optarg, NULL, 0, &index);
			break;
		case CMD_OPT_DUMP:
			dm_strtoi(optarg, NULL, 0, &index);
			vmcfg_dump(index, long_options, optstr);
			return 0;
		default:
			dm_options++;
		}
	}

	if (!vmcfg) {
		optind = 0;
		return dm_run(argc, argv);
	}

	if (dm_options)
		fprintf(stderr, "Waring: --vmcfg override optional args\n");

	if (index <= 0) {
		vmcfg_list();
		return -1;
	}

	if (index > num_args_buildin) {
		fprintf(stderr, "Error: --vmcfg %d,  max index is %d\n",
				index, num_args_buildin);
		return -1;
	}

	optind = 0;
	index--;
	args_buildin[index]->argv[0] = argv[0];
	if (args_buildin[index]->setup)
		args_buildin[index]->setup();

	return dm_run(args_buildin[index]->argc, args_buildin[index]->argv);
}
