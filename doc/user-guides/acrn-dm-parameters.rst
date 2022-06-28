.. _acrn-dm_parameters-and-launch-script:

Device Model Parameters and Launch Script
#########################################

Hypervisor Device Model (DM) is a QEMU-like application in the Service
VM responsible for creating a User VM and then performing devices
emulation based on command line configurations, as introduced in
the :ref:`hld-devicemodel`. ACRN Configurator generates launch scripts for
Post-launched VMs that include a call to the ``acrn-dm`` command with
parameter values that were set in the configurator. Generally, you should not
edit these launch scripts and change the parameters manually. Any edits you
make would be overwritten if you run the configurator again and save the
configuration and launch scripts.

The rest of this document provides details about the ``acrn-dm`` parameters as a
reference, and should help you understand what the generated launch scripts
are doing.  We also include information useful to ACRN contributors about how
setting in the scenario file, created by the configurator, are transformed
into the launch script.

.. _acrn-dm_parameters:

Device Model Parameters
***********************

Here are descriptions for each of these ``acrn-dm`` command line parameters:

``-B``, ``--bootargs <bootargs>``
   Set the User VM kernel command-line arguments. The maximum length is 1023.
   The bootargs string will be passed to the kernel as its cmdline.

   Example::

      -B "loglevel=7"

      specifies the kernel log level at 7

----

``--debugexit``
   Enable guest to write io port 0xf4 to exit guest. It's mainly used by guest
   unit test.

----

``-E``, ``--elf_file <elf image path>``
   This option is to define a static elf binary which could be loaded by DM.
   DM will run elf as guest of ACRN.

----

``--enable_trusty``
   Enable trusty for guest.  For Android guest OS, ACRN provides a VM
   environment with two worlds:  normal world and trusty world.  The Android
   OS runs in the normal world.  The trusty OS and security sensitive
   applications runs in the trusty world.  The trusty world can see the memory
   of normal world but not vice versa.  See :ref:`trusty_tee` for more
   information.

   By default, the trusty world is disabled. Use this option to enable it.

----

``-h``, ``--help``
   Show a summary of commands.

----

``--intr_monitor <intr_monitor_params>``
   Enable interrupt storm monitor for User VM. Use this option to prevent an
   interrupt storm from the User VM.

   usage: ``--intr_monitor threshold/s probe-period(s) delay_time(ms) delay_duration(ms)``

   Example::

      --intr_monitor 10000,10,1,100

   -  ``10000``: interrupt rate larger than 10000/s will be treated as
      interrupt storm
   -  ``10``: use the last 10s of interrupt data to detect an interrupt storm
   -  ``1``: when interrupts are identified as a storm, the next interrupt
      will be delayed 1ms before being injected to the guest
   -  ``100``: after 100ms, we will cancel the interrupt injection delay and
      restore to normal.

----

``-k``, ``--kernel <kernel_image_path>``
   Set the kernel (full path) for the User VM kernel. The maximum path length
   is 1023 characters. The DM handles bzImage image format.

   usage: ``-k /path/to/your/kernel_image``

----

``-l``, ``--lpc <lpc_device_configuration>``

----

``-m``, ``--memsize <memory_size>``
   Setup total memory size for User VM.

   memory_size format is: "<size>{K/k, B/b, M/m, G/g}", and size is an
   integer.

   usage: ``-m 4g``: set User VM memory to 4 gigabytes.

----

``-r``, ``--ramdisk <ramdisk_image_path>``
   Set the ramdisk (full path) for the User VM. The maximum length is 1023.
   The supported ramdisk format depends on your User VM kernel configuration.

   usage: ``-r /path/to/your/ramdisk_image``

----

``-s``, ``--pci_slot <slot_config>``
   Setup PCI device configuration.

   slot_config format is::

      <bus>:<slot>:<func>,<emul>[,<config>]
      <slot>[:<func>],<emul>[,<config>]

   Where:

   -  ``slot`` is 0..31
   -  ``func`` is 0..7
   -  ``emul`` is a string describing the type of PCI device, e.g.
      virtio-net
   -  ``config`` is an optional device-dependent string, used for
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

   This adds virtual block in PCI slot 9 and uses ``/root/test.img`` as the
   disk image.


   For more information about emulated device types, see :ref:'emul_config'.

----

``-v``, ``--version``
   Show Device Model version.

----

``--ovmf [w,]<ovmf_file_path>`` ``--ovmf [w,]code=<ovmf_code_file>,vars=<ovmf_vars_file>``
   Open Virtual Machine Firmware (OVMF) is an EDK II based project to enable
   UEFI support for Virtual Machines.

   ACRN does not support off-the-shelf OVMF builds targeted for QEMU and KVM.
   Compatible OVMF images are included in the source tree, under
   ``devicemodel/bios/``.

   usage::

      --ovmf /usr/share/acrn/bios/OVMF.fd

   uses ``/usr/share/acrn/bios/OVMF.fd`` as the OVMF image

   ACRN also supports using OVMF split images; ``OVMF_CODE.fd`` that contains
   the OVMF firmware executable and ``OVMF_VARS.fd`` that contains the NV
   data store.

   usage::

      --ovmf code=/usr/share/acrn/bios/OVMF_CODE.fd,vars=/usr/share/acrn/bios/OVMF_VARS.fd

   ACRN supports the option "w" for OVMF. To preserve all changes in OVMF's
   NV data store section, use this option to enable writeback mode.

   Writeback mode is only enabled for the ``OVMF_VARS.fd`` file in case of
   OVMF split images, the firmware executable (``OVMF_CODE.fd``) remains
   read-only.

   usage::

      --ovmf w,/usr/share/acrn/bios/OVMF.fd

----

.. _cpu_affinity:

``--cpu_affinity <list of lapic_ids>``
   comma-separated list of vCPUs assigned to this VM. Each CPU has a Local Programmable
   Interrupt Controller (LAPIC). The unique ID of the LAPIC (lapic_id) is used to identify vCPU.
   The ``lapic_id`` for a vCPU can be found in the service VM file ``/proc/cpuinfo``
   identified as ``apicid``.

   Example::

      --cpu_affinity 1,3

   to assign vCPUs with lapic_id 1 and 3 to this VM.

----

``--virtio_poll <poll_interval>``
   Enable virtio poll mode with poll interval xxx ns.

   Example::

      --virtio_poll 1000000

   enable virtio poll mode with poll interval 1ms.

----

``--acpidev_pt <HID>[,<UID>]``
   This option is to enable ACPI device passthrough support. The ``HID`` is a
   mandatory parameter for this option which is the Hardware ID of the ACPI
   device.

   The ``UID`` is an option and used to specify a particular instance of the
   HID device, the default is 00.

   Example::

      --acpidev_pt MSFT0101,00

   To pass through a TPM (which HID is MSFT0101 and UID is 00) ACPI device to
   a User VM.

----

``--mmiodev_pt <MMIO_Region>``
   This option is to enable MMIO device passthrough support.  The
   ``MMIO_Region`` is a mandatory parameter for this option which is the MMIO
   resource of the MMIO device.  The ``MMIO_Region`` needs to be the base
   address followed by the length of the region, both separated by a comma.

   Example::

      --mmiodev_pt 0xFED40000,0x00005000

   To pass through a MMIO device to a User VM.  The MMIO device has a MMIO
   region.  The base address of this region is 0xFED40000 and the size of the
   region is 0x00005000.

----

``--vtpm2 <sock_path>``
   This option is to enable virtual TPM support. The sock_path is a mandatory
   parameter for this option which is the path of swtpm socket fd.

----

``--virtio_msi``
   This option forces virtio to use single-vector MSI.  By default, any
   virtio-based devices will use MSI-X as its interrupt method.  If you want
   to use single-vector MSI interrupt, you can do so using this option.

----

``--lapic_pt``
   This option is to create a VM with the local APIC (LAPIC) passed-through.
   With this option, a VM is created with ``LAPIC_PASSTHROUGH`` and
   ``IO_COMPLETION_POLLING`` mode. This option is typically used for hard
   real-time scenarios.

   By default, this option is not enabled.

----

``--rtvm``
   This option is used to create a VM with real-time attributes.  With this
   option, a VM is created with ``GUEST_FLAG_RT`` and
   ``GUEST_FLAG_IO_COMPLETION_POLLING`` mode.  This kind of VM is generally
   used for soft real-time scenarios (without ``--lapic_pt``) or hard
   real-time scenarios (with ``--lapic_pt``).  With ``GUEST_FLAG_RT``, the
   Service VM cannot interfere with this kind of VM when it is running.  It
   can only be powered off from inside the VM itself.

   By default, this option is not enabled.

----

``--logger_setting <console,level=4;disk,level=4;kmsg,level=3>``
   This option sets the level of logging that is used for each log channel.
   The general format of this option is ``<log channel>,level=<log level>``.
   Different log channels are separated by a semi-colon (``;``). The various
   log channels available are: ``console``, ``disk`` and ``kmsg``.  The log
   level ranges from 1 (``error``) up to 5 (``debug``).

   By default, the log severity level is set to 4 (``info``).

----

``--windows``
   This option is used to run Windows User VMs. It supports Oracle
   ``virtio-blk``, ``virtio-net`` and ``virtio-input`` devices for Windows
   guests with secure boot.

   usage::

      --windows

   .. note::
      This option is mandatory for running Windows in a User VM. If it is
      not used, Windows will not recognize the virtual disk.

----

``--ssram``
   This option enables Software SRAM passthrough to the VM.

   usage::

      --ssram

.. _emul_config:

Emulated PCI Device Types
****************************

In the acrn-dm ``-s`` or ``--pci_slot`` command line parameter, there is a ``<slot_config>`` argument
that contains a string describing the type of emulated PCI device, along with optional device-dependent
arguments used for configuration.  Here is a table describing these emulated device types and arguments:

.. list-table:: Emulated PCI Device Types
   :header-rows: 1
   :widths: 20 80

   * - PCI Device Type string
     - Description

   * - ``xhci``
     - USB controller used to support USB 3.0 devices, (also supports USB 2.0
       and USB 1.0 devices).  Parameter ``<bus number>-<port number>`` should be
       added. The physical USB devices attached on the specified bus and port
       will be detected by User VM and used as expected, e.g., ``xhci,1-2,2-2``.

   * - ``lpc``
     - Low Pin Count (LPC) bus is used to connect low speed devices to the CPU,
       for example a serial port, keyboard, or mouse.

   * - ``igd-lpc``
     - Windows graphic driver requires this virtualized LPC device to operate
       the display function.

   * - ``ivshmem``
     - Inter-VM shared memory (ivshmem) virtualized PCI device used specifically
       for shared memory between VMs. Parameters should be added with the format
       ``ivshmem,<shm_name>,<shm_size>``. ``<shm-name>`` specifies a shared memory
       name, and must be listed in ``hv.FEATURES.IVSHMEM.IVSHMEM_REGION``
       as configured using the ACRN Configurator UI, and needs to start
       with a ``dm:/`` prefix.

   * - ``ahci``
     - Advanced Host Controller Interface provides advanced features to access
       Serial ATA (SATA) storage devices, such as a hard disk. Parameter
       ``<type:><filepath>*`` should be added: ``type`` could be
       ``hd`` (harddisk) or ``cd`` (CD-ROM). ``<filepath>`` is the path for the
       backend file and could be a partition name or a regular file, e.g.,
       ``ahci,hd:/dev/sda``.

   * - ``ahci-hd``
     - This is an alias for ``ahci``.

   * - ``ahci-cd``
     - Advanced Host Controller Interface used to connect with AT Attachment
       Packet Interface device (for CD-ROM emulation). ``ahci-cd`` supports the same
       parameters than ``ahci``.

   * - ``hostbridge``
     - Virtualized PCI hostbridge, a hardware bridge between the CPU's
       high-speed system local bus and the Peripheral Component Interconnect
       (PCI) bus.

   * - ``virtio-blk``
     - Virtio block type device, a string could be appended with the format
       ``virtio-blk,<filepath>[,options]``

       * ``<filepath>`` specifies the path of a file or disk partition.
         You can also could use ``nodisk`` to create a virtio-blk device with a dummy backend.
         ``nodisk`` is used for hot-plugging a rootfs after the User VM has been launched. It is
         achieved by triggering a rescan of the ``virtio-blk`` device by the User VM. The empty file
         will be updated to valid file after rescan.
       * ``[,options]`` includes:

         * ``writethru``: write operation is reported completed only when the data
           has been written to physical storage.
         * ``writeback``: write operation is reported completed when data is placed
           in the page cache. Needs to be flushed to the physical storage.
         * ``ro``: open file with readonly mode.
         * ``sectorsize``: configured as either ``sectorsize=<sector size>/<physical sector size>``
           or ``sectorsize=<sector size>``. The default values for sector size and physical sector size are 512.
         * ``range``: configured as ``range=<start lba in file>/<sub file size>`` meaning the virtio-blk will
           only access part of the file, from the ``<start lba in file>`` to ``<start lba in file>`` + ``<sub file site>``.

   * - ``virtio-input``
     - Virtio type device to emulate input device. ``evdev`` char device node
       should be appended, e.g., ``-s
       n,virtio-input,/dev/input/eventX[,serial]``. ``serial`` is an optional
       string used as the unique identification code of guest virtio input device.

   * - ``virtio-console``
     - Virtio console type device for data input and output.

   * - ``virtio-heci``
     - Virtio Host Embedded Controller Interface, parameters should be appended
       with the format ``<bus>:<device>:<function>,d<0~8>``. You can find the BDF
       information from the Service VM.

   * - ``virtio-i2c``
     - Virtio i2c type device,parameters with format:
       ``<bus>[:<client_addr>[@<node>]][,<bus>[:<client_addr>[@<node>]]``

       * ``<bus>`` specifies the bus number for the native I2C adapter, e.g.,
         ``2`` means ``/dev/i2c-2``.
       * ``<client_addr>`` specifies the address for the native client devices
         such as ``1C`` or  ``2F``.
       * ``@`` specifies the prefix for the ACPI node.
       * ``<node>`` specifies the ACPI node name supported in the
         ``acpi_node_table[]`` in the source code: only ``cam1``, ``cam2``, and
         ``hdac`` are supported for APL platform and  are platform-specific.

   * - ``virtio-gpio``
     - Virtio GPIO type device, parameters format is:
       ``virtio-gpio,<@controller_name{offset|name[=mapping_name]:offset|name[=mapping_name]:...}@controller_name{...}...]>``

       * ``controller_name``: use the command ``ls /sys/bus/gpio/devices`` to
         check the native GPIO controller information.  Usually, the devices
         represent the ``controller_name`` that you can use. You can also use
         the command ``cat /sys/bus/gpio/device/XXX/dev`` to get the device id
         that can be used to match ``/dev/XXX``, and then use ``XXX`` as the
         ``controller_name``. On Intel platforms, ``controller_name`` may be
         ``gpiochip0``, ``gpiochip1``, ``gpiochip2``, and ``gpiochip3``.
       * ``offset|name``: use GPIO offset or its name to locate one native GPIO
         within the GPIO controller.
       * ``mapping_name``: is optional. If you want to use a customized name for
         a FE GPIO, you can set a new name here.

   * - ``virtio-rnd``
     - Virtio random generator type device, the VBSU virtio backend is used by default.

   * - ``virtio-rpmb``
     - Virtio Replay Protected Memory Block (RPMB) type device, with
       ``physical_rpmb`` to specify RPMB in physical mode,
       otherwise RPMB is in simulated mode.

   * - ``virtio-net``
     - Virtio network type device, parameter should be appended with the format:
       ``virtio-net,<device_type>=<name>[,vhost][,mac=<XX:XX:XX:XX:XX:XX> | mac_seed=<seed_string>]``.
       The only supported ``device_type`` parameter is
       ``tap``. The ``mac`` address is optional and ``name`` is the name of the TAP
       (or MacVTap) device. ``vhost`` specifies vhost backend, otherwise the
       VBSU backend is used. ``mac_seed=<seed_string>`` sets a platform-unique
       string as a seed to generate the MAC address.  Each VM should have a
       different ``seed_string``.  The ``seed_string`` can be
       generated by the following method where ``$(vm_name)`` contains the name
       of the VM you are going to launch.

       .. code-block::

          mac=$(cat /sys/class/net/e*/address)
          seed_string=${mac:9:8}-${vm_name}

       .. note::
          ``mac`` and ``mac_seed`` are mutually exclusive, when both are set the
          latter is ignored and the MAC address is set to the ``mac`` value.
          ``mac_seed`` will only be used when ``mac`` is not set.

   * - ``virtio-gpu``
     - Virtio GPU type device, parameters format is:
       ``virtio-gpu[,geometry=<width>x<height>+<x_off>+<y_off> | fullscreen]``

       * ``geometry`` specifies the mode of virtual display, windowed or fullscreen.
         If it is not set, the virtual display will use 1280x720 resolution in windowed mode.
       * ``width`` specifies the width of the virtual display window in pixels.
       * ``height`` specifies the height of the virtual display window in pixels.
       * ``x_off`` specifies the x offset of the virtual display window from the
         upper-left corner of the screen.
       * ``y_off`` specifies the y offset of the virtual display window from the
         upper-left corner of the screen.

       For example: ``geometry=1280x720+100+50`` specifies a window 1280 pixels
       wide by 720 high, with the top left corner 100 pixels right and 50 pixels
       down from the top left corner of the screen.

   * - ``passthru``
     - Indicates a passthrough device. Use the parameter with the format
       ``passthru,<bus>/<device>/<function>,<optional parameter>``
       Optional parameters include:

       * ``keep_gsi``: keep vGSI for MSI capable passthrough device.
       * ``no_reset``: passthrough PCI devices are reset by default when
         assigning them to a post-launched VM. This parameter prevents this
         reset for debugging purposes.
       * ``d3hot_reset``: when launching a  Windows post-launched VM, this
         parameter should be appended to enable a Windows UEFI ACPI bug fix.
       * ``gpu``: create the dedicated ``igd-lpc`` on ``00:1f.0`` for IGD
         passthrough.
       * ``vmsix_on_msi,<bar_id>``: enables vMSI-X emulation based on MSI
         capability.  The specific virtual bar will be allocated.
       * ``enable_ptm``: enable PCIE precise time measurement mechanism for the
         passthrough device.

   * - ``uart``
     - Emulated PCI UART. Use the parameter with the format
       ``uart,vuart_idx:<0~9>`` to specify hypervisor-emulated PCI vUART index.

   * - ``wdt-i6300esb``
     - Emulated i6300ESB PCI Watch Dog Timer (WDT) Intel processors use to
       monitor User VMs.

Launch Script
*************

A launch script is used to start a User VM from the Service VM command line. It
is generated by the ACRN configurator according to several settings for a User
VM. Normally, you should not manually edit these generated launch scripts or
change ``acrn-dm`` command line parameters. If you do so, your changes could
be overwritten the next time you run the configurator.

In this section we describe how setting in the scenario file,
created by the configurator, are transformed into the launch script.
This information would be useful to ACRN contributors or developers
interested in knowing how the launch scripts are created.

Most configurator settings for User VMs are used at launch time.
When you exit the configurator, these settings are saved in the
``scenario.xml`` file and then processed by
``misc/config_tools/launch_config/launch_cfg_gen.py``
to add shell commands to create the launch script, according to the template
``misc/config_tools/launch_config/launch_script_template.sh``.
The template uses following helper functions to do system settings or to
generate an ``acrn-dm`` command line parameter. For details about all
``acrn-dm`` parameters, refer to the previous section.

``probe_modules``
    Install necessary modules before launching a Post-launched VM. For
    example, ``pci_stub`` is used to provide a stub pci driver that does
    nothing on attached pci devices. Passthrough PCIe devices will be unbound
    from their original driver and bound to the stub, so that they can be safely
    controlled by the User VM.

``offline_cpus <cpu_apicid>...``
    This is called if we are launching an RTVM or VM whose scheduler is
    "SCHED_NOOP". In both situations, CPU sharing between multiple VMs is
    prevented.
    This function will trigger taking a CPU offline (done by the Service VM
    kernel), and then inform the hypervisor through Hypervisor Service Module
    (HSM). The hypervisor will offline vCPU and freeze vCPU thread.

``unbind_device <bdf>``
    Unbind a PCIe device with specified BDF (bus, device and function) number
    from its original driver and re-bind it to the pci-stub driver. After that
    the Service VM kernel will not operate on that device any more and it can
    be passed through to User VM safely.

``create_tap <tap>``
    Create or reuse the tap interface that is attached to ``acrn-br0`` bridge.
    ``acrn-br0`` is registered to ``systemd-networkd.service`` after installing
    the ACRN Debian package (``.deb``). You also need to enable and start the
    service to create the bridge from the Service VM using::

        sudo systemctl enable --now systemd-networkd

    The bridge is used to add ``virtio-net``
    interface to a User VM. ``virtio-net`` interfaces for all User VMs are
    virtually connected to a subnet behind the ACRN bridge.

``mount_partition <partition>``
    Mount specified partition to a temporary directory created by ``mktemp -d``,
    and return the temporary directory for later unmount.
    Typically this function is called to mount an image file in order to use
    inner rootfs file as ``virtio-blk`` backend. For example, user could set
    "<imgfile>:/boot/initrd.img*" in the ``virtio-blk`` input box in the ACRN
    Configurator. After the ``acrn-dm`` instance exits, ``unmount_partition``
    will be called to unmount the image file.

``unmount_partition <dir>``
    Unmount partition from specified directory.

``add_cpus <cpu_apicid>...``
    Return an ``acrn-dm`` command line parameter fragment to set
    ``cpu_affinity``. Refer to `cpu_affinity`_ for details.
    ``offline_cpus`` is called if the User VM is an RTVM or its scheduler is
    ``SCHED_NOOP``.

``add_interrupt_storm_monitor <threshold_per_sec> <probe_period_in_sec> <inject_delay_in_ms> <delay_duration_in_ms>``
    This is added if PCIe devices, other than an integrated GPU, are passed through to
    the User VM to monitor if interrupt storm occurred on those devices.
    This function and parameters are not visible in the ACRN Configurator and
    handled by config scripts.
    It returns ``acrn-dm`` command line segment to set ``intr_monitor``.

``add_logger_settings console=<n> kmsg=<n> disk=<n>``
    set log level of each ``acrn-dm`` logging channel: console, kmsg, disk.
    These settings are not exposed to user in the ACRN Configurator.

``add_virtual_device <slot> <kind> <options>``
    Add specified kind of virtual device to the specified PCIe device slot.
    Some devices need options to configure further behaviors. ``<slot>`` numbers
    for virtual devices and passthrough devices are automatically allocated
    by ``launch_cfg_gen.py``.

    Typical use cases:

    - ``hostbridge``
        PCIe host bridge. ``<slot>`` must be 0.

    - ``uart vuart_idx:<int>``
        Add a PCIe vuart with specified index.

    - ``xhci <bus>-<port>[:<bus>-<port>]...``
        Config USB mediator. A list of USB ports each specified by
        ``<bus>-<port>`` will be connected to the User VM.

    - ``virtio-net tap=<tapname>[,vhost],mac_seed=<str>``
        The TAP should already be created by ``create_tap``.

    - ``virtio-blk <imgfile>[,writethru|writeback|ro]``
        Add a virtio block device to User VM. The backend is a raw image
        file. Options can be specified to control access right.

    For all types of virtual devices and options, refer to
    :ref:`emul_config`.

``add_passthrough_device <slot> <bus>/<device>/<function> <options>``
    Passthrough PCIe device to User VM in specified ``<slot>``.
    Some kinds of devices may need extra ``<options>`` to control internal
    behavior. Refer to the ``passthru`` section in :ref:`emul_config`.

These functions in the template are copied to the target launch script. Then
``launch_cfg_gen.py`` generates the following dynamic part. It first defines
necessary variables such as ``vm_type`` and ``scheduler``, and uses the
functions described above to construct the ``dm_params`` parameters per the
user settings in ``scenario.xml``.
Finally, ``acrn-dm`` is executed to launch a User VM with these parameters.
