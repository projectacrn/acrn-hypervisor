.. _acrn-dm_parameters:

Device Model Parameters
#######################

Hypervisor Device Model (DM) is a QEMU-like application in the Service
OS (SOS) responsible for creating a UOS VM and then performing devices
emulation based on command line configurations, as introduced in
:ref:`hld-devicemodel`.

Here are descriptions for each of these ``acrn-dm`` command line parameters:

.. list-table::
   :widths: 22 78
   :header-rows: 0

   * - :kbd:`-A, --acpi`
     - Create ACPI tables.
       With this option, DM will build an ACPI table into its VMs F-Segment
       (0xf2400).  This ACPI table includes full tables for RSDP, RSDT, XSDT,
       MADT, FADT, HPET, MCFG, FACS, and DSDT. All these items are programmed
       according to acrn-dm command line configuration and derived from their
       default value.

   * - :kbd:`-B, --bootargs <bootargs>`
     - Set the UOS kernel command line arguments.
       The maximum length is 1023.
       The bootargs string will be passed to the kernel as its cmdline.

       Example::

         -B "loglevel=7"

       specifies the kernel log level at 7

   * - :kbd:`-c, --ncpus <cpus>`
     - Set number of CPUs for UOS. This number is an integer and must not be
       more than the total number of CPUs in the system, minus one (which is
       used by the SOS).

   * - :kbd:`--debugexit`
     - Enable guest to write io port 0xf4 to exit guest. It's mainly used by
       guest unit test.

   * - :kbd:`--dump <vm_idx>`
     - The option dumps detailed configuration of a VM with built-in configurations.

       Example::

         --dump 1

   * - :kbd:`-E <elf image path>`
     - This option is to define a static elf binary which could be loaded by
       DM. DM will run elf as guest of ACRN.

   * - :kbd:`--enable_trusty`
     - Enable trusty for guest.
       For Android guest OS, ACRN provides a VM environment with two worlds:
       normal world and trusty world. The Android OS runs in the the normal
       world. The trusty OS and security sensitive applications runs in the
       trusty world. The trusty world can see the memory of normal world but
       not vice versa. See :ref:`trusty_tee` for more information.

       By default, the trusty world is disabled. Use this option to enable it.

   * - :kbd:`-G, --gvtargs <GVT_args>`
     - ACRN implements GVT-g for graphics virtualization (aka AcrnGT). This
       option allows you to set some of its parameters.

       GVT_args format: ``gvt_high_gm_sz gvt_low_gm_sz gvt_fence_sz``

       Where:

       - ``gvt_high_gm_sz``: GVT-g aperture size, unit is MB
       - ``gvt_low_gm_sz``: GVT-g hidden gfx memory size, unit is MB
       - ``gvt_fence_sz``: the number of fence registers

       Example::

         -G "10 128 6"

       sets up 10Mb for GVT-g aperture, 128M for GVT-g hidden
       memory, and 6 fence registers.

   * - :kbd:`-h, --help`
     - Show a summary of commands.

   * - :kbd:`-i, --ioc_node <ioc_mediator_parameters>`
     - IOC (IO Controller) is a bridge of an SoC to communicate with Vehicle Bus.
       It routes Vehicle Bus signals, for example extracted from CAN messages,
       from IOC to the SoC and back, as well as controlling the onboard
       peripherals from SoC. (The ``-i`` and ``-l`` parameters are only
       available on a platform with IOC.)

       IOC DM opens ``/dev/ptmx`` device to create a peer PTY devices,  IOC DM uses
       these to communicate with UART DM since UART DM needs a TTY capable
       device as its backend.

       The device model configuration command syntax for IOC mediator is::

          -i,[ioc_channel_path],[wakeup_reason]
          -l,[lpc_port],[ioc_channel_path]

       - ``ioc_channel_path`` is an absolute path for communication between IOC
         mediator and UART DM.
       - ``lpc_port`` is com1 or com2. IOC mediator needs one unassigned lpc
         port for data transfer between User OS and Service OS.
       - ``wakeup_reason`` is IOC mediator boot up reason, where each bit represents
         one wakeup reason.

         Currently the wakeup reason bits supported by IOC firmware are:

         - ``CBC_WK_RSN_BTN`` (bit 5): ignition button.
         - ``CBC_WK_RSN_RTC`` (bit 9): RTC timer.
         - ``CBC_WK_RSN_DOR`` (bit 11): Car door.
         - ``CBC_WK_RSN_SOC`` (bit 23): SoC active/inactive.

       As an example, the following commands are used to enable IOC feature, the
       initial wakeup reason is ignition button, and cbc_attach uses ttyS1 for
       TTY line discipline in UOS::

          -i /run/acrn/ioc_$vm_name,0x20
          -l com2,/run/acrn/ioc_$vm_name

   * - :kbd:`--intr_monitor <intr_monitor_params>`
     - Enable interrupt storm monitor for UOS. Use this option to prevent an interrupt
       storm from the UOS.

       usage: ``--intr_monitor threshold/s probe-period(s) delay_time(ms) delay_duration(ms)``

       Example::

         --intr_monitor 10000,10,1,100

       - ``10000``: interrupt rate larger than 10000/s will be treated as interrupt
         storm
       - ``10``: use the last 10s of interrupt data to detect an interrupt storm
       - ``1``: when interrupts are identified as a storm, the next interrupt will
         be delayed 1ms before being injected to the guest
       - ``100``: after 100ms, we will cancel the interrupt injection delay and restore
         to normal.

   * - :kbd:`-k, --kernel <kernel_image_path>`
     - Set the kernel (full path) for the UOS kernel. The maximum path length is
       1023 characters. The DM handles bzImage image format.

       usage: ``-k /path/to/your/kernel_image``

   * - :kbd:`-l, --lpc <lpc_device_configuration>`
     - (See :kbd:`-i, --ioc_node`)

   * - :kbd:`-m, --memsize <memory_size>`
     - Setup total memory size for UOS.

       memory_size format is: "<size>{K/k, B/b, M/m, G/g}", and size is an
       integer.

       usage: ``-m 4g``: set UOS memory to 4 gigabytes.

   * - :kbd:`--mac_seed <seed_string>`
     - Set a platform unique string as a seed to generate the mac address.
       Each VM should have a different "seed_string". The "seed_string" can
       be generated by the following method where $(vm_name) contains the
       name of the VM you are going to launch.

       ``mac=$(cat /sys/class/net/e*/address)``

       ``seed_string=${mac:9:8}-${vm_name}``

   * - :kbd:`-p, --pincpu <vcpu:hostcpu>`
     - Pin host CPU to appointed vCPU:

       - ``vcpu`` is the ID of the CPU seen by the UOS, and
       - ``hostcpu`` is the physical CPU ID on the system.

       Example: ``-p  "1:2"`` means pin the 2nd physical cpu to 1st vcpu in UOS

   * - :kbd:`--part_info <part_info_name>`
     - Set guest partition info path.

   * - :kbd:`--ptdev_no_reset`
     - Disable reset check for pci device.
       When assigning a PCI device as a passthrough device, we will reset it
       first to get it to a valid device state. So if the device doesn't have
       the reset capability, the passthrough will fail. The PCI device reset
       can be disabled using this option.

   * - :kbd:`-r, --ramdisk <ramdisk_image_path>`
     - Set the ramdisk (full path) for the UOS. The maximum length is 1023.
       The supported ramdisk format depends on your UOS kernel configuration.

       usage: ``-r /path/to/your/ramdisk_image``

   * - :kbd:`-s, --pci_slot <slot_config>`
     - Setup PCI device configuration.

       slot_config format is::

         <bus>:<slot>:<func>,<emul>[,<config>]
         <slot>[:<func>],<emul>[,<config>]

       Where:

       - ``slot`` is 0..31
       - ``func`` is 0..7
       - ``emul`` is a string describing the type of PCI device e.g. virtio-net
       - ``config`` is an optional device-dependent string, used for
         configuration.

       Examples::

         -s 7,xhci,1-2,2-2

       This configuration means the virtual xHCI will appear in PCI slot 7
       in UOS. Any physical USB device attached on 1-2 (bus 1, port 2) or
       2-2 (bus 2, port 2) will be detected by UOS and be used as expected. To
       determine which bus and port a USB device is attached, you could run
       `lsusb -t` in SOS.

       ::

         -s 9,virtio-blk,/root/test.img

       This add virtual block in PCI slot 9 and use "/root/test.img" as the
       disk image

   * - :kbd:`-U, --uuid <uuid>`
     - Set UUID for a VM.
       Every VM is identified by a UUID. You can define that UUID with this
       option. If you don't use this option, a default one
       ("d2795438-25d6-11e8-864e-cb7a18b34643") will be used.

       usage::

         -u "42795636-1d31-6512-7432-087d33b34756"

       set the newly created VM's UUID to "42795636-1d31-6512-7432-087d33b34756"

   * - :kbd:`-v, --version`
     - Show Device Model version

   * - :kbd:`--vsbl <vsbl_file_path>`
     - Virtual Slim bootloader (vSBL) is the virtual bootloader supporting
       booting of the UOS on the ACRN hypervisor platform. The vSBL design is
       derived from Slim Bootloader, which follows a staged design approach
       that provides hardware initialization and launching a payload that
       provides the boot logic.

       The vSBL image is installed on the Service OS root filesystem by the
       service-os bundle, in ``/usr/share/acrn/bios/``. In the current design,
       the vSBL supports booting Android guest OS or Linux guest OS using the
       same vSBL image. For Android VM, the vSBL will load and verify trusty OS
       first, and trusty OS will then load and verify Android OS according to
       Android OS verification mechanism.

       usage::

          --vsbl /usr/share/acrn/bios/VSBL.bin

       uses ``/usr/share/acrn/bios/VSBL.bin`` as the vSBL image

   * - :kbd:`--ovmf <ovmf_file_path>`
     - Open Virtual Machine Firmware (OVMF) is an EDK II based project to enable
       UEFI support for Virtual Machines.

       ACRN does not support off-the-shelf OVMF builds targeted for QEMU and
       KVM. Compatible OVMF images are included in the source tree, under
       ``devicemodel/bios/``.

       usage::

          --ovmf /usr/share/acrn/bios/OVMF.fd

       uses ``/usr/share/acrn/bios/OVMF.fd`` as the OVMF image

   * - :kbd:`--virtio_poll <poll_interval>`
     - Enable virtio poll mode with poll interval xxx ns.

       Example::

          --virtio_poll 1000000

       enable virtio poll mode with poll interval 1ms.

   * - :kbd:`--vmcfg <sub-options>`
     - It's an experimental option for built-in VM configuration. The
       sub-options could be 'list' or <vm_idx>.

       - ``--vmcfg list`` shows indexes of all VMs with built-in configuration.
       - ``--vmcfg <vm_idx>`` launches UOS with selected config.

       Examples::

         --vmcfg list
         --vmcfg 1

   * - :kbd:`--vtpm2 <sock_path>`
     - This option is to enable virtual TPM support. The sock_path is a mandatory
       parameter for this option which is the path of swtpm socket fd.

   * - :kbd:`-W, --virtio_msix`
     - This option forces virtio to use single-vector MSI.
       By default, any virtio-based devices will use MSI-X as its interrupt
       method.  If you want to use single-vector MSI interrupt, you can do so
       using this option.

   * - :kbd:`-Y, --mptgen`
     - Disable MPtable generation.
       The MultiProcessor Specification (MPS) for the x86 architecture is an
       open standard describing enhancements to both operating systems and
       firmware that allows them to work with x86-compatible processors in a
       multi-processor configuration. MPS covers Advanced Programmable
       Interrupt Controller (APIC) architectures.

       By default, DM will create the MPtable for you. Use this option to
       disable it.

   * - :kbd:`--lapic_pt`
     - This option is to create a VM with lapic pass-through.
       With this option, a VM is created with LAPIC_PASSTHROUGH and
       IOREQ_COMPLETION_POLLING mode. This kind of VM is generally for realtime scenarios.

       By default, DM will create VM without this option.
