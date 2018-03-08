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

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
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

#include "types.h"
#include "vmm.h"
#include "vmmapi.h"
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
#include "version.h"

#define GUEST_NIO_PORT		0x488	/* guest upcalls via i/o port */

#define MB		(1024UL * 1024)
#define GB		(1024UL * MB)

typedef int (*vmexit_handler_t)(struct vmctx *,
		struct vhm_request *, int *vcpu);

char *vmname;

int guest_ncpus;
char *guest_uuid_str;
bool stdio_in_use;

static int guest_vmexit_on_hlt, guest_vmexit_on_pause;
static int virtio_msix = 1;
static int x2apic_mode;	/* default is xAPIC */

static int strictio;
static int strictmsr = 1;

static int acpi;

static char *progname;
static const int BSP;

static cpuset_t cpumask;

static void do_close_pre(struct vmctx *ctx);
static void do_close_post(struct vmctx *ctx);
static void vm_loop(struct vmctx *ctx);

static int quit_vm_loop;

static char vhm_request_page[4096] __attribute__ ((aligned(4096)));

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
		"Usage: %s [-abehuwxACHPSWY] [-c vcpus] [-g <gdb port>] [-l <lpc>]\n"
		"       %*s [-m mem] [-p vcpu:hostcpu] [-s <pci>] [-U uuid] <vm>\n"
		"       -a: local apic is in xAPIC mode (deprecated)\n"
		"       -A: create ACPI tables\n"
		"       -c: # cpus (default 1)\n"
		"       -C: include guest memory in core file\n"
		"       -e: exit on unhandled I/O access\n"
		"       -g: gdb port\n"
		"       -h: help\n"
		"       -H: vmexit from the guest on hlt\n"
		"       -l: LPC device configuration\n"
		"       -m: memory size in MB\n"
		"       -M: do not hide INTx link for MSI&INTx capable ptdev\n"
		"       -p: pin 'vcpu' to 'hostcpu'\n"
		"       -P: vmexit from the guest on pause\n"
		"       -s: <slot,driver,configinfo> PCI slot config\n"
		"       -S: guest memory cannot be swapped\n"
		"       -u: RTC keeps UTC time\n"
		"       -U: uuid\n"
		"       -w: ignore unimplemented MSRs\n"
		"       -W: force virtio to use single-vector MSI\n"
		"       -x: local apic is in x2APIC mode\n"
		"       -Y: disable MPtable generation\n"
		"       -k: kernel image path\n"
		"       -r: ramdisk image path\n"
		"       -B: bootargs for kernel\n"
		"       -v: version\n",
		progname, (int)strlen(progname), "");

	exit(code);
}

static void
print_version(void)
{
	if (DM_RC_VERSION)
		fprintf(stderr, "DM version is: %d.%d-%d-%s, build by %s@%s\n",
			DM_MAJOR_VERSION, DM_MINOR_VERSION, DM_RC_VERSION,
			DM_BUILD_VERSION, DM_BUILD_USER, DM_BUILD_TIME);
	else
		fprintf(stderr, "DM version is: %d.%d-%s, build by %s@%s\n",
			DM_MAJOR_VERSION, DM_MINOR_VERSION, DM_BUILD_VERSION,
			DM_BUILD_USER, DM_BUILD_TIME);

	exit(0);
}

static int
pincpu_parse(const char *opt)
{
	int vcpu, pcpu;

	if (sscanf(opt, "%d:%d", &vcpu, &pcpu) != 2) {
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

void *
paddr_guest2host(struct vmctx *ctx, uintptr_t gaddr, size_t len)
{
	return vm_map_gpa(ctx, gaddr, len);
}

void *
dm_gpa2hva(uint64_t gpa, size_t size)
{
	return vm_map_gpa(_ctx, gpa, size);
}

int
fbsdrun_vmexit_on_pause(void)
{
	return guest_vmexit_on_pause;
}

int
fbsdrun_vmexit_on_hlt(void)
{
	return guest_vmexit_on_hlt;
}

int
fbsdrun_virtio_msix(void)
{
	return virtio_msix;
}

static void *
fbsdrun_start_thread(void *param)
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

void
fbsdrun_addcpu(struct vmctx *ctx, int guest_ncpus)
{
	int i;
	int error;

	for (i = 0; i < guest_ncpus; i++) {
		error = vm_create_vcpu(ctx, i);
		if (error != 0)
			err(EX_OSERR, "could not create CPU %d", i);

		CPU_SET_ATOMIC(i, &cpumask);

		mt_vmm_info[i].mt_ctx = ctx;
		mt_vmm_info[i].mt_vcpu = i;
	}

	error = pthread_create(&mt_vmm_info[0].mt_thr, NULL,
	    fbsdrun_start_thread, &mt_vmm_info[0]);
	assert(error == 0);
}

static int
fbsdrun_deletecpu(struct vmctx *ctx, int vcpu)
{
	if (!CPU_ISSET(vcpu, &cpumask)) {
		fprintf(stderr, "Attempting to delete unknown cpu %d\n", vcpu);
		exit(1);
	}

	/* wait for vm_loop cleanup */
	quit_vm_loop = 1;
	vm_destroy_ioreq_client(ctx);
	while (quit_vm_loop)
		usleep(10000);

	CPU_CLR_ATOMIC(vcpu, &cpumask);
	return CPU_EMPTY(&cpumask);
}

static int
vmexit_inout(struct vmctx *ctx, struct vhm_request *vhm_req, int *pvcpu)
{
	int error;
	int bytes, port, in;

	port = vhm_req->reqs.pio_request.address;
	bytes = vhm_req->reqs.pio_request.size;
	in = (vhm_req->reqs.pio_request.direction == REQUEST_READ);

	error = emulate_inout(ctx, pvcpu, &vhm_req->reqs.pio_request, strictio);
	if (error) {
		fprintf(stderr, "Unhandled %s%c 0x%04x\n",
				in ? "in" : "out",
				bytes == 1 ? 'b' : (bytes == 2 ? 'w' : 'l'),
				port);
		return VMEXIT_ABORT;
	} else {
		return VMEXIT_CONTINUE;
	}
}

static int
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
		vhm_req->processed = REQ_STATE_FAILED;
		return VMEXIT_ABORT;
	}
	vhm_req->processed = REQ_STATE_SUCCESS;
	return VMEXIT_CONTINUE;
}

static int
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
		return VMEXIT_ABORT;
	}

	vhm_req->processed = REQ_STATE_SUCCESS;
	return VMEXIT_CONTINUE;
}

#define	DEBUG_EPT_MISCONFIG

#ifdef DEBUG_EPT_MISCONFIG

#define	EXIT_REASON_EPT_MISCONFIG	49
#define	VMCS_GUEST_PHYSICAL_ADDRESS	0x00002400
#define	VMCS_IDENT(x)			((x) | 0x80000000)

#endif	/* #ifdef DEBUG_EPT_MISCONFIG */

static int
vmexit_bogus(struct vmctx *ctx, struct vhm_request *vhm_req, int *pvcpu)
{
	stats.vmexit_bogus++;

	return VMEXIT_CONTINUE;
}

static int
vmexit_reqidle(struct vmctx *ctx, struct vhm_request *vhm_req, int *pvcpu)
{
	stats.vmexit_reqidle++;

	return VMEXIT_CONTINUE;
}

static int
vmexit_hlt(struct vmctx *ctx, struct vhm_request *vhm_req, int *pvcpu)
{
	stats.vmexit_hlt++;

	/*
	 * Just continue execution with the next instruction. We use
	 * the HLT VM exit as a way to be friendly with the host
	 * scheduler.
	 */
	return VMEXIT_CONTINUE;
}

static int
vmexit_pause(struct vmctx *ctx, struct vhm_request *vhm_req, int *pvcpu)
{
	stats.vmexit_pause++;

	return VMEXIT_CONTINUE;
}

static int
vmexit_mtrap(struct vmctx *ctx, struct vhm_request *vhm_req, int *pvcpu)
{
	stats.vmexit_mtrap++;

	return VMEXIT_CONTINUE;
}

static vmexit_handler_t handler[VM_EXITCODE_MAX] = {
	[VM_EXITCODE_INOUT]  = vmexit_inout,
	[VM_EXITCODE_MMIO_EMUL] = vmexit_mmio_emul,
	[VM_EXITCODE_PCI_CFG] = vmexit_pci_emul,
	[VM_EXITCODE_BOGUS]  = vmexit_bogus,
	[VM_EXITCODE_REQIDLE] = vmexit_reqidle,
	[VM_EXITCODE_MTRAP]  = vmexit_mtrap,
	[VM_EXITCODE_HLT]  = vmexit_hlt,
	[VM_EXITCODE_PAUSE]  = vmexit_pause,
};

static void
handle_vmexit(struct vmctx *ctx, struct vhm_request *vhm_req, int vcpu)
{
	int rc;
	enum vm_exitcode exitcode;

	exitcode = vhm_req->type;
	if (exitcode >= VM_EXITCODE_MAX || handler[exitcode] == NULL) {
		fprintf(stderr, "handle vmexit: unexpected exitcode 0x%x\n",
				exitcode);
		exit(1);
	}

	rc = (*handler[exitcode])(ctx, vhm_req, &vcpu);
	switch (rc) {
	case VMEXIT_CONTINUE:
		vhm_req->processed = REQ_STATE_SUCCESS;
		break;
	case VMEXIT_ABORT:
		vhm_req->processed = REQ_STATE_FAILED;
		abort();
	default:
		exit(1);
	}
	vm_notify_request_done(ctx, vcpu);
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
		int vcpu;
		struct vhm_request *vhm_req;

		error = vm_attach_ioreq_client(ctx);
		if (error)
			break;

		for (vcpu = 0; vcpu < 4; vcpu++) {
			vhm_req = &vhm_req_buf[vcpu];
			if (vhm_req->valid
				&& (vhm_req->processed == REQ_STATE_PROCESSING)
				&& (vhm_req->client == ctx->ioreq_client))
				handle_vmexit(ctx, vhm_req, vcpu);
		}
	}
	quit_vm_loop = 0;
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

static struct vmctx *
do_open(const char *vmname)
{
	struct vmctx *ctx;
	int error;

	error = vm_create(vmname);
	if (error) {
		perror("vm_create");
		exit(1);

	}

	ctx = vm_open(vmname);
	if (ctx == NULL) {
		perror("vm_open");
		exit(1);
	}

	return ctx;
}

static void
do_close_pre(struct vmctx *ctx)
{
	vm_destroy(ctx);
	vm_close(ctx);
}

static void
do_close_post(struct vmctx *ctx)
{
	pci_irq_deinit(ctx);
	deinit_pci(ctx);
	vm_destroy(ctx);
	vm_close(ctx);
}

static void
sig_handler_term(int signo)
{
	printf("Receive SIGINT to terminate application...\n");
	vm_set_suspend_mode(VM_SUSPEND_POWEROFF);
	mevent_notify();
}

int
main(int argc, char *argv[])
{
	int c, error, gdb_port, err, bvmcons;
	int max_vcpus, mptgen, memflags;
	int rtc_localtime;
	struct vmctx *ctx;
	size_t memsize;
	char *optstr;

	bvmcons = 0;
	progname = basename(argv[0]);
	gdb_port = 0;
	guest_ncpus = 1;
	memsize = 256 * MB;
	mptgen = 1;
	rtc_localtime = 1;
	memflags = 0;
	quit_vm_loop = 0;

	if (signal(SIGINT, sig_handler_term) == SIG_ERR)
		fprintf(stderr, "cannot register handler for SIGINT\n");

	optstr = "abehuwxACHIMPSWYvk:r:B:p:g:c:s:m:l:U:G:";
	while ((c = getopt(argc, argv, optstr)) != -1) {
		switch (c) {
		case 'a':
			x2apic_mode = 0;
			break;
		case 'A':
			acpi = 1;
			break;
		case 'b':
			bvmcons = 1;
			break;
		case 'p':
			if (pincpu_parse(optarg) != 0) {
				errx(EX_USAGE,
				"invalid vcpu pinning configuration '%s'",
				optarg);
			}
			break;
		case 'c':
			guest_ncpus = atoi(optarg);
			break;
		case 'C':
			memflags |= VM_MEM_F_INCORE;
			break;
		case 'g':
			gdb_port = atoi(optarg);
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
		case 'S':
			memflags |= VM_MEM_F_WIRED;
			break;
		case 'm':
			error = vm_parse_memsize(optarg, &memsize);
			if (error)
				errx(EX_USAGE, "invalid memsize '%s'", optarg);
			break;
		case 'H':
			guest_vmexit_on_hlt = 1;
			break;
		case 'I':
			/*
			 * The "-I" option was used to add an ioapic to the
			 * virtual machine.
			 *
			 * An ioapic is now provided unconditionally for each
			 * virtual machine and this option is now deprecated.
			 */
			break;
		case 'P':
			guest_vmexit_on_pause = 1;
			break;
		case 'e':
			strictio = 1;
			break;
		case 'u':
			rtc_localtime = 0;
			break;
		case 'U':
			guest_uuid_str = optarg;
			break;
		case 'w':
			strictmsr = 0;
			break;
		case 'W':
			virtio_msix = 0;
			break;
		case 'x':
			x2apic_mode = 1;
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
		case 'M':
			ptdev_prefer_msi(false);
			break;
		case 'v':
			print_version();
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

	vmname = argv[0];

	for (;;) {
		ctx = do_open(vmname);

		/* set IOReq buffer page */
		error = vm_set_shared_io_page(ctx, (unsigned long)vhm_req_buf);
		if (error)
			do_close_pre(ctx);
		assert(error == 0);

		if (guest_ncpus < 1) {
			fprintf(stderr, "Invalid guest vCPUs (%d)\n",
				guest_ncpus);
			do_close_pre(ctx);
			exit(1);
		}

		max_vcpus = num_vcpus_allowed(ctx);
		if (guest_ncpus > max_vcpus) {
			fprintf(stderr, "%d vCPUs requested but %d available\n",
				guest_ncpus, max_vcpus);
			do_close_pre(ctx);
			exit(1);
		}

		vm_set_memflags(ctx, memflags);
		err = vm_setup_memory(ctx, memsize, VM_MMAP_ALL);
		if (err) {
			fprintf(stderr, "Unable to setup memory (%d)\n", errno);
			do_close_pre(ctx);
			exit(1);
		}

		init_mem();
		init_inout();
		pci_irq_init(ctx);
		atkbdc_init(ctx);
		ioapic_init(ctx);

		vrtc_init(ctx, rtc_localtime);
		sci_init(ctx);

		/*
		 * Exit if a device emulation finds an error in its
		 * initialization
		 */
		if (init_pci(ctx) != 0) {
			do_close_pre(ctx);
			exit(1);
		}

		if (gdb_port != 0)
			fprintf(stderr, "dbgport not supported\n");

		if (bvmcons)
			init_bvmcons();

		/*
		 * build the guest tables, MP etc.
		 */
		if (mptgen) {
			error = mptable_build(ctx, guest_ncpus);
			if (error) {
				do_close_post(ctx);
				exit(1);
			}
		}

		error = smbios_build(ctx);
		if (error)
			do_close_post(ctx);
		assert(error == 0);

		if (acpi) {
			error = acpi_build(ctx, guest_ncpus);
			if (error)
				do_close_post(ctx);
			assert(error == 0);
		}

		error = acrn_sw_load(ctx);
		if (error)
			do_close_post(ctx);
		assert(error == 0);

		/*
		 * Change the proc title to include the VM name.
		 */
		/*setproctitle("%s", vmname);*/

		/*
		 * Add CPU 0
		 */
		fbsdrun_addcpu(ctx, guest_ncpus);

		/* Make a copy for ctx */
		_ctx = ctx;

		/*
		 * Head off to the main event dispatch loop
		 */
		mevent_dispatch();

		vm_pause(ctx);
		fbsdrun_deletecpu(ctx, BSP);
		vm_unsetup_memory(ctx);
		do_close_post(ctx);
		_ctx = 0;

		if (vm_get_suspend_mode() != VM_SUSPEND_RESET)
			break;
		vm_set_suspend_mode(VM_SUSPEND_NONE);
	}
	exit(0);
}
