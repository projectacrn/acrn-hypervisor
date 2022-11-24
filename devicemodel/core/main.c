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
#include "rtc.h"
#include "pit.h"
#include "hpet.h"
#include "version.h"
#include "sw_load.h"
#include "monitor.h"
#include "ioc.h"
#include "pm.h"
#include "atomic.h"
#include "tpm.h"
#include "mmio_dev.h"
#include "virtio.h"
#include "pm_vuart.h"
#include "log.h"
#include "pci_util.h"
#include "vssram.h"
#include "cmd_monitor.h"
#include "vdisplay.h"
#include "iothread.h"

#define	VM_MAXCPU		16	/* maximum virtual cpus */

#define GUEST_NIO_PORT		0x488	/* guest upcalls via i/o port */

/* Values returned for reads on invalid I/O requests. */
#define IOREQ_PIO_INVAL		(~0U)
#define IOREQ_MMIO_INVAL	(~0UL)

typedef void (*vmexit_handler_t)(struct vmctx *,
		struct acrn_io_request *, int *vcpu);

char *vmname;

char *vsbl_file_name;
char *ovmf_file_name;
char *ovmf_code_file_name;
char *ovmf_vars_file_name;
char *kernel_file_name;
char *elf_file_name;
uint8_t trusty_enabled;
bool stdio_in_use;
bool lapic_pt;
bool is_rtvm;
bool pt_tpm2;
bool ssram;
bool vtpm2;
bool is_winvm;
bool skip_pci_mem64bar_workaround = false;
bool gfx_ui = false;

static int guest_ncpus;
static int virtio_msix = 1;
static bool debugexit_enabled;
static int pm_notify_channel;
static bool cmd_monitor;

static char *progname;
static const int BSP;

static cpuset_t cpumask;

static void vm_loop(struct vmctx *ctx);

static char io_request_page[4096] __aligned(4096);
static char asyncio_page[4096] __aligned(4096);

static struct acrn_io_request *ioreq_buf =
				(struct acrn_io_request *)&io_request_page;

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

static struct vmctx *_ctx;

static void
usage(int code)
{
	fprintf(stderr,
		"Usage: %s [-hYv] [-B bootargs] [-E elf_image_path]\n"
		"       %*s [-k kernel_image_path]\n"
		"       %*s [-l lpc] [-m mem] [-r ramdisk_image_path]\n"
		"       %*s [-s pci] [--ovmf ovmf_file_path]\n"
		"       %*s [--iasl iasl_compiler_path]\n"
		"       %*s [--enable_trusty] [--intr_monitor param_setting]\n"
		"       %*s [--acpidev_pt HID] [--mmiodev_pt MMIO_Regions]\n"
		"       %*s [--vtpm2 sock_path] [--virtio_poll interval]\n"
		"       %*s [--cpu_affinity lapic_id] [--lapic_pt] [--rtvm] [--windows]\n"
		"       %*s [--debugexit] [--logger_setting param_setting]\n"
		"       %*s [--ssram] <vm>\n"
		"       -B: bootargs for kernel\n"
		"       -E: elf image path\n"
		"       -h: help\n"
		"       -k: kernel image path\n"
		"       -l: LPC device configuration\n"
		"       -m: memory size in MB\n"
		"       -r: ramdisk image path\n"
		"       -s: <slot,driver,configinfo> PCI slot config\n"
		"       -v: version\n"
		"       --ovmf: ovmf file path\n"
		"       --iasl: iasl compiler path\n"
		"       --ssram: Congfiure Software SRAM parameters\n"
		"       --cpu_affinity: list of Service VM vCPUs assigned to this User VM, the vCPUs are"
		"	     identified by their local APIC IDs.\n"
		"       --enable_trusty: enable trusty for guest\n"
		"       --debugexit: enable debug exit function\n"
		"       --intr_monitor: enable interrupt storm monitor\n"
		"            its params: threshold/s,probe-period(s),delay_time(ms),delay_duration(ms)\n"
		"       --cmd_monitor: enable command monitor\n"
		"            its params: unix domain socket path\n"
		"       --virtio_poll: enable virtio poll mode with poll interval with ns\n"
		"       --acpidev_pt: acpi device ID args: HID in ACPI Table\n"
		"       --mmiodev_pt: MMIO resources args: physical MMIO regions\n"
		"       --vtpm2: Virtual TPM2 args: sock_path=$PATH_OF_SWTPM_SOCKET\n"
		"       --lapic_pt: enable local apic passthrough\n"
		"       --rtvm: indicate that the guest is rtvm\n"
		"       --logger_setting: params like console,level=4;kmsg,level=3\n"
		"       --windows: support Oracle virtio-blk, virtio-net and virtio-input devices\n"
		"            for windows guest with secure boot\n"
		"       --virtio_msi: force virtio to use single-vector MSI\n",
		progname, (int)strnlen(progname, PATH_MAX), "", (int)strnlen(progname, PATH_MAX), "",
		(int)strnlen(progname, PATH_MAX), "", (int)strnlen(progname, PATH_MAX), "",
		(int)strnlen(progname, PATH_MAX), "", (int)strnlen(progname, PATH_MAX), "",
		(int)strnlen(progname, PATH_MAX), "", (int)strnlen(progname, PATH_MAX), "",
		(int)strnlen(progname, PATH_MAX), "", (int)strnlen(progname, PATH_MAX), "");

	exit(code);
}

static void outdate(char *msg)
{
	pr_warn("The \"%s\" parameter is obsolete and ignored\n", msg);
}

static void
print_version(void)
{
	fprintf(stdout, "DM: %s-%s-%s%s%s%s build by %s@%s\n",
			DM_BRANCH_VERSION, DM_COMMIT_TIME, DM_COMMIT_DIRTY,
			(sizeof(DM_COMMIT_TAGS) > 1) ? "(tag: " : "", DM_COMMIT_TAGS, 
			(sizeof(DM_COMMIT_TAGS) > 1) ? ")" : "",
			DM_BUILD_USER, DM_BUILD_TIME);

	exit(0);
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
	size_t size = ovmf_image_size();

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
add_cpu(struct vmctx *ctx, int vcpu_num)
{
	int i;
	int error;

	for (i = 0; i < vcpu_num; i++) {
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
		pr_err("Attempting to delete unknown cpu %d\n", vcpu);
		exit(1);
	}

	vm_destroy_ioreq_client(ctx);
	pthread_join(mt_vmm_info[0].mt_thr, NULL);

	CPU_CLR_ATOMIC(vcpu, &cpumask);
	return CPU_EMPTY(&cpumask);
}

#ifdef DM_DEBUG
void
notify_vmloop_thread(void)
{
	pthread_kill(mt_vmm_info[0].mt_thr, SIGCONT);
	return;
}
#endif

static void
vmexit_inout(struct vmctx *ctx, struct acrn_io_request *io_req, int *pvcpu)
{
	int error;
	int bytes, port, in;

	port = io_req->reqs.pio_request.address;
	bytes = io_req->reqs.pio_request.size;
	in = (io_req->reqs.pio_request.direction == ACRN_IOREQ_DIR_READ);

	error = emulate_inout(ctx, pvcpu, &io_req->reqs.pio_request);
	if (error) {
		pr_err("Unhandled %s%c 0x%04x\n",
				in ? "in" : "out",
				bytes == 1 ? 'b' : (bytes == 2 ? 'w' : 'l'),
				port);

		if (in) {
			io_req->reqs.pio_request.value = IOREQ_PIO_INVAL;
		}
	}
}

static void
vmexit_mmio_emul(struct vmctx *ctx, struct acrn_io_request *io_req, int *pvcpu)
{
	int err;

	stats.vmexit_mmio_emul++;
	err = emulate_mem(ctx, &io_req->reqs.mmio_request);

	if (err) {
		if (err == -ESRCH)
			pr_err("Unhandled memory access to 0x%lx\n",
				io_req->reqs.mmio_request.address);

		pr_err("Failed to emulate instruction [");
		pr_err("mmio address 0x%lx, size %ld",
				io_req->reqs.mmio_request.address,
				io_req->reqs.mmio_request.size);

		if (io_req->reqs.mmio_request.direction == ACRN_IOREQ_DIR_READ) {
			io_req->reqs.mmio_request.value = IOREQ_MMIO_INVAL;
		}
	}
}

static void
vmexit_pci_emul(struct vmctx *ctx, struct acrn_io_request *io_req, int *pvcpu)
{
	int err, in = (io_req->reqs.pci_request.direction == ACRN_IOREQ_DIR_READ);

	err = emulate_pci_cfgrw(ctx, *pvcpu, in,
			io_req->reqs.pci_request.bus,
			io_req->reqs.pci_request.dev,
			io_req->reqs.pci_request.func,
			io_req->reqs.pci_request.reg,
			io_req->reqs.pci_request.size,
			&io_req->reqs.pci_request.value);
	if (err) {
		pr_err("Unhandled pci cfg rw at %x:%x.%x reg 0x%x\n",
			io_req->reqs.pci_request.bus,
			io_req->reqs.pci_request.dev,
			io_req->reqs.pci_request.func,
			io_req->reqs.pci_request.reg);

		if (in) {
			io_req->reqs.pio_request.value = IOREQ_PIO_INVAL;
		}
	}
}

#define	DEBUG_EPT_MISCONFIG

#ifdef DEBUG_EPT_MISCONFIG

#define	EXIT_REASON_EPT_MISCONFIG	49
#define	VMCS_GUEST_PHYSICAL_ADDRESS	0x00002400
#define	VMCS_IDENT(x)			((x) | 0x80000000)

#endif	/* #ifdef DEBUG_EPT_MISCONFIG */

enum vm_exitcode {
	VM_EXITCODE_INOUT = 0,
	VM_EXITCODE_MMIO_EMUL,
	VM_EXITCODE_PCI_CFG,
	VM_EXITCODE_MAX
};

static vmexit_handler_t handler[VM_EXITCODE_MAX] = {
	[VM_EXITCODE_INOUT]  = vmexit_inout,
	[VM_EXITCODE_MMIO_EMUL] = vmexit_mmio_emul,
	[VM_EXITCODE_PCI_CFG] = vmexit_pci_emul,
};

static void
handle_vmexit(struct vmctx *ctx, struct acrn_io_request *io_req, int vcpu)
{
	enum vm_exitcode exitcode;

	exitcode = io_req->type;
	if (exitcode >= VM_EXITCODE_MAX || handler[exitcode] == NULL) {
		pr_err("handle vmexit: unexpected exitcode 0x%x\n",
				exitcode);
		exit(1);
	}

	(*handler[exitcode])(ctx, io_req, &vcpu);

	/* We cannot notify the HSM/hypervisor on the request completion at this
	 * point if the User VM is in suspend or system reset mode, as the VM is
	 * still not paused and a notification can kick off the vcpu to run
	 * again. Postpone the notification till vm_system_reset() or
	 * vm_suspend_resume() for resetting the ioreq states in the HSM and
	 * hypervisor.
	 */
	if ((VM_SUSPEND_SYSTEM_RESET == vm_get_suspend_mode()) ||
		(VM_SUSPEND_SUSPEND == vm_get_suspend_mode()))
		return;

	vm_notify_request_done(ctx, vcpu);
}

static int
guest_pm_notify_init(struct vmctx *ctx)
{
	int ret = 0;

	/*
	 * We don't care ioc_init return value so far.
	 * Will add return value check once ioc is full function.
	 */
	if (PWR_EVENT_NOTIFY_IOC == pm_notify_channel)
		ioc_init(ctx);
	else if (PWR_EVENT_NOTIFY_PWR_BT == pm_notify_channel)
		power_button_init(ctx);
	else if (PWR_EVENT_NOTIFY_UART == pm_notify_channel ||
		 PWR_EVENT_NOTIFY_UART_TRIG_PLAT_S5 == pm_notify_channel)
		ret = pm_by_vuart_init(ctx,
			(PWR_EVENT_NOTIFY_UART_TRIG_PLAT_S5 == pm_notify_channel));
	else
		pr_info("No pm notify channel given\n");

	return ret;
}

static void
guest_pm_notify_deinit(struct vmctx *ctx)
{
	if (PWR_EVENT_NOTIFY_IOC == pm_notify_channel)
		ioc_deinit(ctx);
	else if (PWR_EVENT_NOTIFY_PWR_BT == pm_notify_channel)
		power_button_deinit(ctx);
	else if (PWR_EVENT_NOTIFY_UART == pm_notify_channel ||
		 PWR_EVENT_NOTIFY_UART_TRIG_PLAT_S5 == pm_notify_channel)
		pm_by_vuart_deinit(ctx);
	else
		pr_err("No correct pm notify channel given\n");
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

	ret = guest_pm_notify_init(ctx);
	if (ret < 0)
		goto pm_notify_fail;

	ret = vrtc_init(ctx);
	if (ret < 0)
		goto vrtc_fail;

	ret = vpit_init(ctx);
	if (ret < 0)
		goto vpit_fail;

	ret = vhpet_init(ctx);
	if (ret < 0)
		goto vhpet_fail;

	sci_init(ctx);

	if (debugexit_enabled)
		init_debugexit();

	if ((ssram) && (init_vssram(ctx) < 0))
		goto ssram_fail;

	ret = monitor_init(ctx);
	if (ret < 0)
		goto monitor_fail;

	if ((cmd_monitor) && init_cmd_monitor(ctx) < 0)
		goto monitor_fail;

	ret = init_mmio_devs(ctx);
	if (ret < 0)
		goto mmio_dev_fail;

	ret = init_pci(ctx);
	if (ret < 0)
		goto pci_fail;

	/* FIXME: if we plan to support pass through a TPM device and emulate another b TPM device */
	init_vtpm2(ctx);

	return 0;

pci_fail:
	deinit_mmio_devs(ctx);
mmio_dev_fail:
	monitor_close();
ssram_fail:
	if (ssram)
		deinit_vssram(ctx);
monitor_fail:
	if (debugexit_enabled)
		deinit_debugexit();

	vhpet_deinit(ctx);
vhpet_fail:
	vpit_deinit(ctx);
vpit_fail:
	vrtc_deinit(ctx);
vrtc_fail:
	guest_pm_notify_deinit(ctx);
pm_notify_fail:
	atkbdc_deinit(ctx);
	pci_irq_deinit(ctx);
	ioapic_deinit();
	return -1;
}

static void
vm_deinit_vdevs(struct vmctx *ctx)
{
	/*
	 * Write ovmf NV storage back to the original file from guest
	 * memory before deinit operations.
	 */
	acrn_writeback_ovmf_nvstorage(ctx);

	deinit_pci(ctx);
	deinit_mmio_devs(ctx);
	monitor_close();

	if (debugexit_enabled)
		deinit_debugexit();

	if (ssram)
		deinit_vssram(ctx);

	vhpet_deinit(ctx);
	vpit_deinit(ctx);
	vrtc_deinit(ctx);
	guest_pm_notify_deinit(ctx);
	atkbdc_deinit(ctx);
	pci_irq_deinit(ctx);
	ioapic_deinit();
	deinit_vtpm2(ctx);
}

static void
vm_reset_vdevs(struct vmctx *ctx)
{
	/*
	 * Write ovmf NV storage back to the original file from guest
	 * memory before deinit operations.
	 */
	acrn_writeback_ovmf_nvstorage(ctx);

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

	if (ssram)
		deinit_vssram(ctx);

	vhpet_deinit(ctx);
	vpit_deinit(ctx);
	vrtc_deinit(ctx);

	deinit_pci(ctx);
	pci_irq_deinit(ctx);
	ioapic_deinit();

	pci_irq_init(ctx);
	atkbdc_init(ctx);
	vrtc_init(ctx);
	vpit_init(ctx);
	vhpet_init(ctx);

	if (debugexit_enabled)
		init_debugexit();

	if (ssram)
		init_vssram(ctx);

	ioapic_init(ctx);
	init_pci(ctx);

	acpi_build(ctx, guest_ncpus);
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
	 *   4. load software for User VM
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
	 * offlining any other vcpus, there can be multiple IO requests
	 * with various states. We should be careful on potential races
	 * when resetting especially in SMP Service VM. vm_clear_ioreq can be used
	 * to clear all ioreq status in HSM after VM pause, then let VM
	 * reset in hypervisor reset all ioreqs.
	 */
	vm_clear_ioreq(ctx);

	vm_reset_vdevs(ctx);
	vm_reset(ctx);
	pr_info("%s: setting VM state to %s\n", __func__, vm_state_to_str(VM_SUSPEND_NONE));
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
	if (ctx->ioreq_client < 0) {
		pr_err("%s, failed to create IOREQ.\n", __func__);
		return;
	}

	if (vm_run(ctx) != 0) {
		pr_err("%s, failed to run VM.\n", __func__);
		return;
	}

	while (1) {
		int vcpu_id;
		struct acrn_io_request *io_req;

		error = vm_attach_ioreq_client(ctx);
		if (error)
			break;

		for (vcpu_id = 0; vcpu_id < guest_ncpus; vcpu_id++) {
			io_req = &ioreq_buf[vcpu_id];
			if ((atomic_load(&io_req->processed) == ACRN_IOREQ_STATE_PROCESSING)
				&& !io_req->kernel_handled)
				handle_vmexit(ctx, io_req, vcpu_id);
		}

		if (VM_SUSPEND_FULL_RESET == vm_get_suspend_mode() ||
		    VM_SUSPEND_POWEROFF == vm_get_suspend_mode()) {
			break;
		}

		/* RTVM can't be reset */
		if ((VM_SUSPEND_SYSTEM_RESET == vm_get_suspend_mode()) && (!is_rtvm)) {
			vm_system_reset(ctx);
		}

		if (VM_SUSPEND_SUSPEND == vm_get_suspend_mode()) {
			vm_suspend_resume(ctx);
		}
	}
	pr_err("VM loop exit\n");
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
	pr_info("Received SIGINT to terminate application...\n");
	pr_info("%s: setting VM state to %s\n", __func__, vm_state_to_str(VM_SUSPEND_POWEROFF));
	vm_set_suspend_mode(VM_SUSPEND_POWEROFF);
	mevent_notify();
}

enum {
	CMD_OPT_VSBL = 1000,
	CMD_OPT_OVMF,
	CMD_OPT_IASL,
	CMD_OPT_CPU_AFFINITY,
	CMD_OPT_PART_INFO,
	CMD_OPT_TRUSTY_ENABLE,
	CMD_OPT_VIRTIO_POLL_ENABLE,
	CMD_OPT_MAC_SEED,
	CMD_OPT_DEBUGEXIT,
	CMD_OPT_VMCFG,
	CMD_OPT_DUMP,
	CMD_OPT_INTR_MONITOR,
	CMD_OPT_CMD_MONITOR,
	CMD_OPT_ACPIDEV_PT,
	CMD_OPT_MMIODEV_PT,
	CMD_OPT_VTPM2,
	CMD_OPT_LAPIC_PT,
	CMD_OPT_RTVM,
	CMD_OPT_SOFTWARE_SRAM,
	CMD_OPT_LOGGER_SETTING,
	CMD_OPT_PM_NOTIFY_CHANNEL,
	CMD_OPT_PM_BY_VUART,
	CMD_OPT_WINDOWS,
	CMD_OPT_FORCE_VIRTIO_MSI,
};

static struct option long_options[] = {
	{"acpi",		no_argument,	        0, 'A' },
	{"elf_file",		required_argument,	0, 'E' },
	{"ioc_node",		required_argument,	0, 'i' },
	{"lpc",			required_argument,	0, 'l' },
	{"pci_slot",		required_argument,	0, 's' },
	{"memsize",		required_argument,	0, 'm' },
	{"mptgen",		no_argument,		0, 'Y' },
	{"kernel",		required_argument,	0, 'k' },
	{"ramdisk",		required_argument,	0, 'r' },
	{"bootargs",		required_argument,	0, 'B' },
	{"version",		no_argument,		0, 'v' },
	{"gvtargs",		required_argument,	0, 'G' },
	{"help",		no_argument,		0, 'h' },
	{"mac_seed",		required_argument,	0, CMD_OPT_MAC_SEED},

	/* Following cmd option only has long option */
#ifdef CONFIG_VM_CFG
#endif
	{"vsbl",		required_argument,	0, CMD_OPT_VSBL},
	{"ovmf",		required_argument,	0, CMD_OPT_OVMF},
	{"iasl",		required_argument,	0, CMD_OPT_IASL},
	{"cpu_affinity",	required_argument,	0, CMD_OPT_CPU_AFFINITY},
	{"part_info",		required_argument,	0, CMD_OPT_PART_INFO},
	{"enable_trusty",	no_argument,		0,
					CMD_OPT_TRUSTY_ENABLE},
	{"virtio_poll",		required_argument,	0, CMD_OPT_VIRTIO_POLL_ENABLE},
	{"debugexit",		no_argument,		0, CMD_OPT_DEBUGEXIT},
	{"intr_monitor",	required_argument,	0, CMD_OPT_INTR_MONITOR},
	{"cmd_monitor",		required_argument,	0, CMD_OPT_CMD_MONITOR},
	{"acpidev_pt",		required_argument,	0, CMD_OPT_ACPIDEV_PT},
	{"mmiodev_pt",		required_argument,	0, CMD_OPT_MMIODEV_PT},
	{"vtpm2",		required_argument,	0, CMD_OPT_VTPM2},
	{"lapic_pt",		no_argument,		0, CMD_OPT_LAPIC_PT},
	{"rtvm",		no_argument,		0, CMD_OPT_RTVM},
	{"ssram",		required_argument,	0, CMD_OPT_SOFTWARE_SRAM},
	{"logger_setting",	required_argument,	0, CMD_OPT_LOGGER_SETTING},
	{"pm_notify_channel",	required_argument,	0, CMD_OPT_PM_NOTIFY_CHANNEL},
	{"pm_by_vuart",	required_argument,	0, CMD_OPT_PM_BY_VUART},
	{"windows",		no_argument,		0, CMD_OPT_WINDOWS},
	{"virtio_msi",		no_argument,		0, CMD_OPT_FORCE_VIRTIO_MSI},
	{0,			0,			0,  0  },
};

static char optstr[] = "AhYvE:k:r:B:s:m:l:U:G:i:";

int
vm_init_asyncio(struct vmctx *ctx, uint64_t base)
{
	struct shared_buf *sbuf = (struct shared_buf *)base;

	sbuf->magic = SBUF_MAGIC;
	sbuf->ele_size = sizeof(uint64_t);
	sbuf->ele_num = (4096 - SBUF_HEAD_SIZE) / sbuf->ele_size;
	sbuf->size = sbuf->ele_size * sbuf->ele_num;
	/* set flag to 0 to make sure not overrun! */
	sbuf->flags = 0;
	sbuf->overrun_cnt = 0;
	sbuf->head = 0;
	sbuf->tail = 0;
	return vm_setup_asyncio(ctx, base);
}

int
main(int argc, char *argv[])
{
	int c, error, ret=1;
	int max_vcpus, mptgen;
	struct vmctx *ctx;
	size_t memsize;
	int option_idx = 0;

	progname = basename(argv[0]);
	memsize = 256 * MB;
	mptgen = 1;

	if (signal(SIGHUP, sig_handler_term) == SIG_ERR)
		fprintf(stderr, "cannot register handler for SIGHUP\n");
	if (signal(SIGINT, sig_handler_term) == SIG_ERR)
		fprintf(stderr, "cannot register handler for SIGINT\n");
	/*
	 * Ignore SIGPIPE signal and handle the error directly when write()
	 * function fails. this will help us to catch the write failure rather
	 * than crashing the User VM.
	 */
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		fprintf(stderr, "cannot register handler for SIGPIPE\n");

	if (parse_madt()) {
		pr_err("Failed to parse the MADT table\n");
		exit(1);
	}

	while ((c = getopt_long(argc, argv, optstr, long_options,
			&option_idx)) != -1) {
		switch (c) {
		case 'A':
			pr_info("The '-A' parameter is obsolete and ignored. "
                                "Virtual ACPI tables are always enabled.\n");
			break;
		case 'E':
			if (acrn_parse_elf(optarg) != 0)
				exit(1);
			else
				break;
			break;
		case 'i': /* obsolete parameter */
			outdate("-i, --ioc_node");
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
			if (vm_parse_memsize(optarg, &memsize) != 0)
				errx(EX_USAGE, "invalid memsize '%s'", optarg);
			break;
		case 'Y': /* obsolete parameter */
			outdate("-Y, --mptgen");
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
		case 'G': /* obsolete parameter */
			outdate("-G, --gvtargs");
			break;
		case 'v':
			print_version();
			break;
		case CMD_OPT_VSBL: /* obsolete parameter */
			outdate("--vsbl");
			break;
		case CMD_OPT_OVMF:
			if (!vsbl_file_name && acrn_parse_ovmf(optarg) != 0)
				errx(EX_USAGE, "invalid ovmf param %s", optarg);
			skip_pci_mem64bar_workaround = true;
			break;
		case CMD_OPT_IASL:
			if (acrn_parse_iasl(optarg) != 0)
				errx(EX_USAGE, "invalid iasl param %s", optarg);
			break;
		case CMD_OPT_CPU_AFFINITY:
			if (acrn_parse_cpu_affinity(optarg) != 0)
				errx(EX_USAGE, "invalid pcpu param %s", optarg);
			break;
		case CMD_OPT_PART_INFO: /* obsolete parameter */
			outdate("--part_info");
			break;
		case CMD_OPT_TRUSTY_ENABLE:
			trusty_enabled = 1;
			break;
		case CMD_OPT_VIRTIO_POLL_ENABLE:
			if (acrn_parse_virtio_poll_interval(optarg) != 0) {
				errx(EX_USAGE,
					"invalid virtio poll interval %s",
					optarg);
			}
			break;
		case CMD_OPT_MAC_SEED:
			pr_warn("The \"--mac_seed\" parameter is obsolete\n");
			pr_warn("Please use the \"virtio-net,<device_type>=<name> mac_seed=<seed_string>\"\n");
			break;
		case CMD_OPT_DEBUGEXIT:
			debugexit_enabled = true;
			break;
		case CMD_OPT_LAPIC_PT:
			lapic_pt = true;
			break;
		case CMD_OPT_RTVM:
			is_rtvm = true;
			lapic_pt = true;
			break;
		case CMD_OPT_SOFTWARE_SRAM:
			if (parse_vssram_buf_params(optarg) != 0)
				errx(EX_USAGE, "invalid vSSRAM buffer size param %s", optarg);
			ssram = true;
			break;
		case CMD_OPT_ACPIDEV_PT:
			/* FIXME: check acpi TPM device rules in acpi device famework init functions */
			if (vtpm2 || create_pt_acpidev(optarg) != 0)
				errx(EX_USAGE, "invalid pt acpi dev param %s", optarg);
			break;
		case CMD_OPT_MMIODEV_PT:
			if (create_pt_mmiodev(optarg) != 0)
				errx(EX_USAGE, "invalid pt mmio dev param %s", optarg);
			break;
		case CMD_OPT_VTPM2:
			if (pt_tpm2 || acrn_parse_vtpm2(optarg) != 0)
				errx(EX_USAGE, "invalid vtpm2 param %s", optarg);
			break;
		case CMD_OPT_INTR_MONITOR:
			if (acrn_parse_intr_monitor(optarg) != 0)
				errx(EX_USAGE, "invalid intr-monitor params %s", optarg);
			break;
		case CMD_OPT_CMD_MONITOR:
			if (acrn_parse_cmd_monitor(optarg) != 0)
				errx(EX_USAGE, "invalid command monitor params %s", optarg);
			cmd_monitor = true;
			break;
		case CMD_OPT_LOGGER_SETTING:
			if (init_logger_setting(optarg) != 0)
				errx(EX_USAGE, "invalid logger setting params %s", optarg);
			break;
		case CMD_OPT_PM_NOTIFY_CHANNEL: /* obsolete parameter */
			if (strncmp("ioc", optarg, sizeof("ioc")) == 0)
				pm_notify_channel = PWR_EVENT_NOTIFY_IOC;
			else if (strncmp("power_button", optarg, sizeof("power_button")) == 0)
				pm_notify_channel = PWR_EVENT_NOTIFY_PWR_BT;
			else if (strncmp("uart", optarg, sizeof("uart") - 1) == 0) {
				if (optarg[sizeof("uart") - 1] == '\0')
					pm_notify_channel = PWR_EVENT_NOTIFY_UART;
				else if (strncmp(",allow_trigger_s5", optarg + sizeof("uart") - 1,
						sizeof(",allow_trigger_s5")) == 0)
					pm_notify_channel = PWR_EVENT_NOTIFY_UART_TRIG_PLAT_S5;
				else
					errx(EX_USAGE, "invalid pm_notify_channel: %s", optarg);
			} else
				errx(EX_USAGE, "invalid pm_notify_channel: %s", optarg);
			pr_warn("The \"--pm_notify_channel\" parameter is obsolete\n");
			break;
		case CMD_OPT_PM_BY_VUART: /* obsolete parameter */
			outdate("--pm_by_vuart");
			break;
		case CMD_OPT_WINDOWS:
			is_winvm = true;
			break;
		case CMD_OPT_FORCE_VIRTIO_MSI:
			virtio_msix = 0;
			break;
		case 'h':
			usage(0);
		default:
			usage(1);
		}
	}

	if (get_iasl_compiler() != 0) {
		pr_err("Cannot find Intel ACPI ASL compiler tool \"iasl\".\n");
		exit(1);
	}

	if (check_iasl_version() != 0) {
		pr_err("Please install iasl tool with version >= %s from https://www.acpica.org/downloads, "
			"and provide the path to iasl (by using --iasl) if it's not on the PATH \n",
			IASL_MIN_VER);
		exit(1);
	}

	argc -= optind;
	argv += optind;

	if (argc != 1) {
		pr_err("You must provide the name of the Virtual Machine (VM) you want to start. Exiting.\n");
		usage(1);
        }

	if (lapic_pt == true && is_rtvm == false) {
		lapic_pt = false;
		pr_warn("Only a Realtime VM can use local APIC pass through, '--lapic_pt' is invalid here.\n");
	}
	vmname = argv[0];

	if (strnlen(vmname, MAX_VM_NAME_LEN) >= MAX_VM_NAME_LEN) {
		pr_err("The name of the VM exceeds the maximum length: %u\n", MAX_VM_NAME_LEN - 1);
		exit(1);
	}

	if (!init_hugetlb()) {
		pr_err("init_hugetlb failed\n");
		exit(1);
	}

	if (gfx_ui) {
		if(gfx_ui_init()) {
			pr_err("gfx ui initialize failed\n");
			exit(1);
		}
	}

	for (;;) {
		pr_notice("vm_create: %s\n", vmname);
		ctx = vm_create(vmname, (unsigned long)ioreq_buf, &guest_ncpus);
		if (!ctx) {
			pr_err("vm_create failed");
			goto create_fail;
		}

		if (guest_ncpus < 1) {
			pr_err("Invalid guest vCPUs (%d)\n", guest_ncpus);
			goto fail;
		}

		max_vcpus = num_vcpus_allowed(ctx);
		if (guest_ncpus > max_vcpus) {
			pr_err("%d vCPUs requested but %d available\n",
				guest_ncpus, max_vcpus);
			goto fail;
		}

		pr_notice("vm setup asyncio page\n");
		error = vm_init_asyncio(ctx, (uint64_t)asyncio_page);
		if (error) {
			pr_warn("ASYNIO capability is not supported by kernel or hyperviosr!\n");
		}

		pr_notice("vm_setup_memory: size=0x%lx\n", memsize);
		error = vm_setup_memory(ctx, memsize);
		if (error) {
			pr_err("Unable to setup memory (%d)\n", errno);
			goto fail;
		}

		error = mevent_init();
		if (error) {
			pr_err("Unable to initialize mevent (%d)\n", errno);
			goto mevent_fail;
		}

		error = iothread_init();
		if (error) {
			pr_err("Unable to initialize iothread (%d)\n", errno);
			goto iothread_fail;
		}

		pr_notice("vm_init_vdevs\n");
		if (vm_init_vdevs(ctx) < 0) {
			pr_err("Unable to init vdev (%d)\n", errno);
			goto dev_fail;
		}

		/*
		 * build the guest tables, MP etc.
		 */
		if (mptgen) {
			error = mptable_build(ctx, guest_ncpus);
			if (error) {
				goto vm_fail;
			}
		}

		error = acpi_build(ctx, guest_ncpus);
		if (error) {
			pr_err("acpi_build failed, error=%d\n", error);
			goto vm_fail;
		}

		pr_notice("acrn_sw_load\n");
		error = acrn_sw_load(ctx);
		if (error) {
			pr_err("acrn_sw_load failed, error=%d\n", error);
			goto vm_fail;
		}

		/*
		 * Change the proc title to include the VM name.
		 */
		/*setproctitle("%s", vmname);*/

		/*
		 * Add CPU 0
		 */
		pr_notice("add_cpu\n");
		error = add_cpu(ctx, guest_ncpus);
		if (error) {
			pr_err("add_cpu failed, error=%d\n", error);
			goto vm_fail;
		}

		/* Make a copy for ctx */
		_ctx = ctx;

		/*
		 * Head off to the main event dispatch loop
		 */
		mevent_dispatch();

		vm_pause(ctx);
		delete_cpu(ctx, BSP);

		if (vm_get_suspend_mode() != VM_SUSPEND_FULL_RESET){
			ret = 0;
			break;
		}

		vm_deinit_vdevs(ctx);
		mevent_deinit();
		iothread_deinit();
		vm_unsetup_memory(ctx);
		vm_destroy(ctx);
		_ctx = 0;

		pr_info("%s: setting VM state to %s\n", __func__, vm_state_to_str(VM_SUSPEND_NONE));
		vm_set_suspend_mode(VM_SUSPEND_NONE);
	}

vm_fail:
	vm_deinit_vdevs(ctx);
	if (ssram)
		clean_vssram_configs();

dev_fail:
	iothread_deinit();
iothread_fail:
	mevent_deinit();
mevent_fail:
	vm_unsetup_memory(ctx);
fail:
	vm_pause(ctx);
	vm_destroy(ctx);
create_fail:
	if (cmd_monitor)
		deinit_cmd_monitor();
	if (gfx_ui) {
		gfx_ui_deinit();
	}
	uninit_hugetlb();
	deinit_loggers();
	exit(ret);
}
