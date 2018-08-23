/*
 * Copyright (C)2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vmcfg_config.h>
#include <vmcfg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#define TOSTR(X) #X
#define STR(X) TOSTR(X)

#ifdef CONFIG_MRB_VM1
static char cmdline_fmt[] = "maxcpus="  STR(CONFIG_MRB_VM1_CPU) " "
				"nohpet tsc=reliable intel_iommu=off "
				"androidboot.serialno=%s%s "
				"i915.enable_rc6=1 "
				"i915.enable_fbc=1 "
				"i915.enable_guc_loading=0 "
				"i915.avail_planes_per_pipe=0x070F00 "
				"i915.enable_hangcheck=0 "
				"use_nuclear_flip=1 "
				"i915.enable_guc_submission=0 "
				"i915.enable_guc=0 ";

static char kernel_cmdline[sizeof(cmdline_fmt) + 256];

static char *mrb_vm1_options[] = {
	NULL,			/*Reserved for program name*/

	#ifdef CONFIG_MRB_VM1_UUID
	"-U", CONFIG_MRB_VM1_UUID_VAL,
	#endif				/*CONFIG_MRB_VM1_UUID*/

	#ifdef CONFIG_MRB_VM1_ACPI
	"-A",
	#endif				/*CONFIG_MRB_VM1_ACPI*/

	#ifdef CONFIG_MRB_VM1_MEM
	"-m", STR(CONFIG_MRB_VM1_MEM) "M",
	#endif				/*CONFIG_MRB_VM1_MEN*/

	#ifdef CONFIG_MRB_VM1_CPU
	"-c", STR(CONFIG_MRB_VM1_CPU),
	#endif				/*CONFIG_MRB_VM1_CPU*/

	#ifdef CONFIG_MRB_VM1_GVT
	"-s", CONFIG_MRB_VM1_GVT_DEV,
	"-G", CONFIG_MRB_VM1_GVT_ARG,
	#endif				/*CONFIG_MRB_VM1_GVT*/

#ifdef CONFIG_MRB_VM1_HOSTBRIDGE
	"-s", CONFIG_MRB_VM1_HOSTBRIDGE_OPT,

	#ifdef CONFIG_MRB_VM1_USB_XHCI
	"-s", CONFIG_MRB_VM1_USB_XHCI_OPT,
	#endif				/*CONFIG_MRB_VM1_USB_XHCI*/

	#ifdef CONFIG_MRB_VM1_PTDEV_USB
	"-s", CONFIG_MRB_VM1_PTDEV_USB_OPT_1,
	#endif				/*CONFIG_MRB_VM1_PTDEV_USB*/

	#ifdef CONFIG_MRB_VM1_VIRTIO_RPMB
	"-s", CONFIG_MRB_VM1_VIRTIO_RPMB_OPT,
	#endif				/*CONFIG_MRB_VM1_VIRTIO_RPMB*/

	#ifdef CONFIG_MRB_VM1_VIRTIO_HYPER_DMABUF
	"-s", CONFIG_MRB_VM1_VIRTIO_HYPER_DMABUF_OPT,
	#endif				/*CONFIG_MRB_VM1_VIRTIO_HYPER_DMABUF*/

	#ifdef CONFIG_MRB_VM1_WDT_I6300ESB
	"-s", CONFIG_MRB_VM1_WDT_I6300ESB_OPT,
	#endif				/*CONFIG_MRB_VM1_WDT_I6300ESB*/

	#ifdef CONFIG_MRB_VM1_VIRTIO_BLK
	"-s", CONFIG_MRB_VM1_VIRTIO_BLK_OPT,
	#endif				/*CONFIG_MRB_VM1_VIRTIO_BLK*/

	#ifdef CONFIG_MRB_VM1_VIRTIO_NET
	"-s", CONFIG_MRB_VM1_VIRTIO_NET_OPT,
	#endif				/*CONFIG_MRB_VM1_VIRTIO_NET*/

	#ifdef CONFIG_MRB_VM1_PTDEV_AUDIO
	"-s", CONFIG_MRB_VM1_PTDEV_AUDIO_OPT,
	#endif				/*CONFIG_MRB_VM1_PTDEV_AUDIO*/

	#ifdef CONFIG_MRB_VM1_PTDEV_AUDIO_CODEC
	"-s", CONFIG_MRB_VM1_PTDEV_AUDIO_CODEC_OPT,
	#endif				/*CONFIG_MRB_VM1_PTDEV_AUDIO_CODEC*/

	#ifdef CONFIG_MRB_VM1_PTDEV_CSME
	"-s", CONFIG_MRB_VM1_PTDEV_CSME_OPT,
	#endif				/*CONFIG_MRB_VM1_PTDEV_CSME */

	#ifdef CONFIG_MRB_VM1_PTDEV_IPU
	"-s", CONFIG_MRB_VM1_PTDEV_IPU_OPT,	/*ipu device*/
	"-s", CONFIG_MRB_VM1_PTDEV_IPU_I2C,	/*ipu related i2c*/
	#endif				/*CONFIG_MRB_VM1_PTDEV_IPU*/

	#ifdef CONFIG_MRB_VM1_PTDEV_SD
	"-s", CONFIG_MRB_VM1_PTDEV_SD_OPT,
	#endif				/*CONFIG_MRB_VM1_PTDEV_SD*/

	#ifdef CONFIG_MRB_VM1_PTDEV_WIFI
	"-s", CONFIG_MRB_VM1_PTDEV_WIFI_OPT,
	#endif				/*CONFIG_MRB_VM1_PTDEV_WIFI*/

	#ifdef CONFIG_MRB_VM1_PTDEV_BLUETOOTH
	"-s", CONFIG_MRB_VM1_PTDEV_BLUETOOTH_OPT,
	#endif				/*CONFIG_MRB_VM1_PTDEV_BLUETOOTH*/

#endif				/*CONFIG_MRB_VM1_HOSTBRIDGE */

#ifdef CONFIG_MRB_VM1_LPC
	"-s", CONFIG_MRB_VM1_LPC_OPT,

	#ifdef CONFIG_MRB_VM1_IOC
	"-i", CONFIG_MRB_VM1_IOC_UART  CONFIG_MRB_VM1_NAME ","
		CONFIG_MRB_VM1_IOC_REASON,
	#endif				/*CONFIG_MRB_VM1_IOC */

	#ifdef CONFIG_MRB_VM1_LPC_COM1
	"-l", CONFIG_MRB_VM1_LPC_COM1_OPT,
	#endif				/*CONFIG_MRB_VM1_LPC_COM1 */

	#ifdef CONFIG_MRB_VM1_LPC_COM2
	"-l", CONFIG_MRB_VM1_LPC_COM2_OPT CONFIG_MRB_VM1_NAME,
	#endif				/*CONFIG_MRB_VM1_LPC_COM2 */

#endif				/*CONFIG_MRB_VM1_LPC */

	#ifdef CONFIG_MRB_VM1_VSBL_IMAGE
	"--vsbl", CONFIG_MRB_VM1_VSBL_IMAGE,
	#endif				/*CONFIG_MRB_VM1_VSBL_IMAGE */

	"--enable_trusty",

	/*command line */
	"-B",

	kernel_cmdline,

	/* VM name */
	CONFIG_MRB_VM1_NAME,
};

#ifdef CONFIG_MRB_VM1_PTDEV
#define PCI_UNBIND_PATH(X)	\
	"/sys/bus/pci/devices/" X "/driver/unbind"

static int pci_dev_unbind(char *path, char *bdf)
{
	int fd = -1;
	int ret = -1;

	fd = open(path, O_WRONLY);
	if (fd < 0) {
		perror(path);
		return -1;
	}

	ret = write(fd, bdf, strnlen(bdf, 64));
	if (ret != strnlen(bdf, 64)) {
		perror(bdf);
		ret = -1;
	}

	close(fd);
	return ret;
}

static int pci_dev_newid(char *id)
{
	int fd = -1;
	int ret = -1;

	fd = open("/sys/bus/pci/drivers/pci-stub/new_id", O_WRONLY);
	if (fd < 0) {
		perror("/sys/bus/pci/drivers/pci-stub/new_id");
		return -1;
	}

	ret = write(fd, id, strnlen(id, 64));
	if (ret != strnlen(id, 64)) {
		perror(id);
		ret = -1;
	}

	close(fd);
	return ret;
}

#define PTDEV_SETUP(ID, DEVINFO) 			\
do {							\
	pci_dev_newid(ID);				\
	pci_dev_unbind(PCI_UNBIND_PATH(DEVINFO), DEVINFO);	\
} while(0)

#endif /*CONFIG_MRB_VM1_PTDEV*/

static void check_str(char *str, int len)
{
	int i = 0;

	for (i = 0; i < len; i++)
		if (str[i] < 0x20 || str[i] > 0x7e)
			str[i] = 0;
}

static int mrb_vm1_setup(void)
{
	char mmc_name[64] = {};
	char mmc_serial[64] = {};
	int ret = 0;
	int fd = -1;

#ifdef CONFIG_MRB_VM1_PTDEV_USB
	PTDEV_SETUP(CONFIG_MRB_VM1_PTDEV_USB_ID_1,
			CONFIG_MRB_VM1_PTDEV_USB_DEVINFO_1);
#endif /*CONFIG_MRB_VM1_PTDEV_USB*/

#ifdef CONFIG_MRB_VM1_PTDEV_AUDIO
	PTDEV_SETUP(CONFIG_MRB_VM1_PTDEV_AUDIO_ID,
			CONFIG_MRB_VM1_PTDEV_AUDIO_DEVINFO);
#endif /*CONFIG_MRB_VM1_PTDEV_AUDIO*/

#ifdef CONFIG_MRB_VM1_PTDEV_AUDIO_CODEC
	PTDEV_SETUP(CONFIG_MRB_VM1_PTDEV_AUDIO_CODEC_ID,
			CONFIG_MRB_VM1_PTDEV_AUDIO_CODEC_DEVINFO);
#endif /*CONFIG_MRB_VM1_PTDEV_AUDIO*/

#ifdef CONFIG_MRB_VM1_PTDEV_CSME
	PTDEV_SETUP(CONFIG_MRB_VM1_PTDEV_CSME_ID,
			CONFIG_MRB_VM1_PTDEV_CSME_DEVINFO);
#endif /*CONFIG_MRB_VM1_PTDEV_CSME*/

#ifdef CONFIG_MRB_VM1_PTDEV_IPU
	PTDEV_SETUP(CONFIG_MRB_VM1_PTDEV_IPU_ID,
			CONFIG_MRB_VM1_PTDEV_IPU_DEVINFO);
	PTDEV_SETUP(CONFIG_MRB_VM1_PTDEV_IPU_I2C_ID,
			CONFIG_MRB_VM1_PTDEV_IPU_I2C_DEVINFO);
#endif /*CONFIG_MRB_VM1_PTDEV_IPU*/

#ifdef CONFIG_MRB_VM1_PTDEV_SD
	PTDEV_SETUP(CONFIG_MRB_VM1_PTDEV_SD_ID,
			CONFIG_MRB_VM1_PTDEV_SD_DEVINFO);
#endif /*CONFIG_MRB_VM1_PTDEV_SD*/

#ifdef CONFIG_MRB_VM1_PTDEV_WIFI
	PTDEV_SETUP(CONFIG_MRB_VM1_PTDEV_WIFI_ID,
			CONFIG_MRB_VM1_PTDEV_WIFI_DEVINFO);
#endif /*CONFIG_MRB_VM1_PTDEV_WIFI*/

#ifdef CONFIG_MRB_VM1_PTDEV_BLUETOOTH
	PTDEV_SETUP(CONFIG_MRB_VM1_PTDEV_BLUETOOTH_ID,
			CONFIG_MRB_VM1_PTDEV_BLUETOOTH_DEVINFO);
#endif /*CONFIG_MRB_VM1_PTDEV_BLUETOOTH*/

	memset(kernel_cmdline, 0, sizeof(kernel_cmdline));

	fd = open("/sys/block/mmcblk1/device/name", O_RDONLY);
	if (fd >= 0) {
		ret = read(fd, mmc_name, sizeof(mmc_name));
		if (ret >= sizeof(mmc_name))
			mmc_name[sizeof(mmc_name) - 1] = 0;
		check_str(mmc_name, sizeof(mmc_name));
		close(fd);
	}

	fd = open("/sys/block/mmcblk1/device/serial", O_RDONLY);
	if (fd >= 0) {
		ret = read(fd, mmc_serial, sizeof(mmc_serial));
		if (ret >= sizeof(mmc_serial))
			mmc_name[sizeof(mmc_serial) - 1] = 0;
		check_str(mmc_serial, sizeof(mmc_serial));
		close(fd);
	}

	snprintf(kernel_cmdline, sizeof(kernel_cmdline) - 1, cmdline_fmt,
		       mmc_name, mmc_serial);

	return 0;
}

struct vmcfg_arg mrb_vm1_args = {
	.argv = mrb_vm1_options,
	.argc = sizeof(mrb_vm1_options) / sizeof(char *),
	.setup = mrb_vm1_setup,
};
#endif				/*CONFIG_MRB_VM1 */
