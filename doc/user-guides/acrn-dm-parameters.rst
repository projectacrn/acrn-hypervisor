.. _acrn-dm_parameters:

Device Model Parameters
#######################

Hypervisor Device Model (DM) is a QEMU-like application in the Service
VM responsible for creating a User VM and then performing devices
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
     - Set the User VM kernel command line arguments.
       The maximum length is 1023.
       The bootargs string will be passed to the kernel as its cmdline.

       Example::

         -B "loglevel=7"

       specifies the kernel log level at 7

   * - :kbd:`--debugexit`
     - Enable guest to write io port 0xf4 to exit guest. It's mainly used by
       guest unit test.

   * - :kbd:`-E, --elf_file <elf image path>`
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
       TTY line discipline in User VM::

          -i /run/acrn/ioc_$vm_name,0x20
          -l com2,/run/acrn/ioc_$vm_name

   * - :kbd:`--intr_monitor <intr_monitor_params>`
     - Enable interrupt storm monitor for User VM. Use this option to prevent an interrupt
       storm from the User VM.

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
     - Set the kernel (full path) for the User VM kernel. The maximum path length is
       1023 characters. The DM handles bzImage image format.

       usage: ``-k /path/to/your/kernel_image``

   * - :kbd:`-l, --lpc <lpc_device_configuration>`
     - (See :kbd:`-i, --ioc_node`)

   * - :kbd:`-m, --memsize <memory_size>`
     - Setup total memory size for User VM.

       memory_size format is: "<size>{K/k, B/b, M/m, G/g}", and size is an
       integer.

       usage: ``-m 4g``: set User VM memory to 4 gigabytes.

   * - :kbd:`--mac_seed <seed_string>`
     - Set a platform unique string as a seed to generate the mac address.
       Each VM should have a different "seed_string". The "seed_string" can
       be generated by the following method where $(vm_name) contains the
       name of the VM you are going to launch.

       ``mac=$(cat /sys/class/net/e*/address)``

       ``seed_string=${mac:9:8}-${vm_name}``

   * - :kbd:`--part_info <part_info_name>`
     - Set guest partition info path.

   * - :kbd:`-r, --ramdisk <ramdisk_image_path>`
     - Set the ramdisk (full path) for the User VM. The maximum length is 1023.
       The supported ramdisk format depends on your User VM kernel configuration.

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
       in User VM. Any physical USB device attached on 1-2 (bus 1, port 2) or
       2-2 (bus 2, port 2) will be detected by User VM and be used as expected. To
       determine which bus and port a USB device is attached, you could run
       ``lsusb -t`` in Service VM.

       ::

         -s 9,virtio-blk,/root/test.img

       This add virtual block in PCI slot 9 and use ``/root/test.img`` as the
       disk image

   * - :kbd:`-U, --uuid <uuid>`
     - Set UUID for a VM.
       Every VM is identified by a UUID. You can define that UUID with this
       option. If you don't use this option, a default one
       ("d2795438-25d6-11e8-864e-cb7a18b34643") will be used.

       usage::

         -u "42795636-1d31-6512-7432-087d33b34756"

       set the newly created VM's UUID to ``42795636-1d31-6512-7432-087d33b34756``

   * - :kbd:`-v, --version`
     - Show Device Model version

   * - :kbd:`--vsbl <vsbl_file_path>`
     - Virtual Slim bootloader (vSBL) is the virtual bootloader supporting
       booting of the User VM on the ACRN hypervisor platform. The vSBL design is
       derived from Slim Bootloader, which follows a staged design approach
       that provides hardware initialization and launching a payload that
       provides the boot logic.

       The vSBL image is installed on the Service OS root filesystem by the
       service-os bundle, in ``/usr/share/acrn/bios/``. In the current design,
       the vSBL supports booting Android guest OS or Linux guest OS using the
       same vSBL image. For Android VM, the vSBL will load and verify trusty OS
       first, and trusty OS will then load and verify Android OS according to
       Android OS verification mechanism.

       .. note::
          vSBL is currently only supported on Apollo Lake processors.

       usage::

          --vsbl /usr/share/acrn/bios/VSBL.bin

       uses ``/usr/share/acrn/bios/VSBL.bin`` as the vSBL image

   * - :kbd:`--ovmf [w,]<ovmf_file_path>`
     - Open Virtual Machine Firmware (OVMF) is an EDK II based project to enable
       UEFI support for Virtual Machines.

       ACRN does not support off-the-shelf OVMF builds targeted for QEMU and
       KVM. Compatible OVMF images are included in the source tree, under
       ``devicemodel/bios/``.

       usage::

          --ovmf /usr/share/acrn/bios/OVMF.fd

       uses ``/usr/share/acrn/bios/OVMF.fd`` as the OVMF image

       ACRN supports option "w" of OVMF. To preserve any change of OVMF NV data
       store section, using this option to enable NV data store section writeback.

       usage::

          --ovmf w,/usr/share/acrn/bios/OVMF.fd



   * - :kbd:`--cpu_affinity <list of pCPUs>`
     - list of pCPUs assigned to this VM.

       Example::

          --cpu_affinity 1,3

       to assign physical CPUs (pCPUs) 1 and 3 to this VM.

   * - :kbd:`--virtio_poll <poll_interval>`
     - Enable virtio poll mode with poll interval xxx ns.

       Example::

          --virtio_poll 1000000

       enable virtio poll mode with poll interval 1ms.

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
     - This option is to create a VM with the local APIC (LAPIC) passed-through.
       With this option, a VM is created with ``LAPIC_PASSTHROUGH`` and
       ``IO_COMPLETION_POLLING`` mode. This option is typically used for hard
       realtime scenarios.

       By default, this option is not enabled.

   * - :kbd:`--rtvm`
     - This option is used to create a VM with realtime attributes.
       With this option, a VM is created with ``GUEST_FLAG_RT`` and
       ``GUEST_FLAG_IO_COMPLETION_POLLING`` mode. This kind of VM is
       generally used for soft realtime scenarios (without ``--lapic_pt``) or
       hard realtime scenarios (with ``--lapic_pt``). With ``GUEST_FLAG_RT``,
       the Service VM cannot interfere with this kind of VM when it is
       running. It can only be powered off from inside the VM itself.

       By default, this option is not enabled.

   * - :kbd:`--logger_setting <console,level=4;disk,level=4;kmsg,level=3>`
     - This option sets the level of logging that is used for each log channel.
       The general format of this option is ``<log channel>,level=<log level>``.
       Different log channels are separated by a semi-colon (``;``). The various
       log channels available are: ``console``, ``disk`` and ``kmsg``.  The log
       level ranges from 1 (``error``) up to 5 (``debug``).

       By default, the log severity level is set to 4 (``info``).

   * - :kbd:`--pm_notify_channel <channel>`
     - This option is used to define which channel could be used DM to
       communicate with VM about power management event.

       ACRN supports three channels: ``ioc``, ``power button`` and ``uart``.

       usage::

          --pm_notify_channel ioc

       Use ioc as power management event motify channel.

   * - :kbd:`--pm_by_vuart [pty|tty],<node_path>`
     - This option is used to set a user OS power management by virtual UART.
       With acrn-dm UART emulation and hypervisor UART emulation and configure,
       service OS can communicate with user OS through virtual UART. By this
       option, service OS can notify user OS to shutdown itself by vUART.

       It need work with `--pm_notify_channel` and PCI UART setting (lpc and -l).

       Example::

        for general User VM, like LaaG or WaaG, it need set:
          --pm_notify_channel uart --pm_by_vuart pty,/run/acrn/life_mngr_vm1
          -l com2,/run/acrn/life_mngr_vm1
        for RTVM, like RT-Linux:
          --pm_notify_channel uart --pm_by_vuart tty,/dev/ttyS1

       For different User VM, it can be configured as needed.

   * - :kbd:`--windows`
     - This option is used to run Windows User VMs. It supports Oracle
       ``virtio-blk``, ``virtio-net`` and ``virtio-input`` devices for Windows
       guests with secure boot.

       usage::

          --windows
