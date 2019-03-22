.. _kernel-parameters:

ACRN Kernel Parameters
######################

ACRN supports flexible configurations so customers can configure SOS and
UOS behavior easily, and according to their requirements.  This document
introduces kernel command line options used to configure and boot VMs.


.. list-table::
   :header-rows: 1
   :widths: 10,10,50,30

   * - Parameter
     - Used in SOS or UOS
     - Description
     - Usage example

   * - module_blacklist
     - SOS
     - A comma-separated list of modules that should not be loaded.
       Useful to debug or work
       around issues related to specific modules.
     - ::

         module_blacklist=dwc3_pci

   * - no_timer_check
     - SOS,UOS
     - Disables the code which tests for broken timer IRQ sources.
     - ::

         no_timer_check

   * - console
     - SOS,UOS
     - Output console device and options.

       ``tty<n>``
         Use the virtual console device <n>.

       ``ttyS<n>[,options]``
         Use the specified serial port and options. Default options are
         ``9600n8`` meaning 9600 baud, no parity, 8 bits. Options are of the form *bbbbpnf*,
         where:

            | *bbbb* is baud rate, for example 9600;
            | *p* is parity, one of ``n``, ``o``, or ``e`` (for none, odd, or even),
            | *n* is number of bits (typically 8),
            | *f* is flow control (``r`` for RTS, or left blank)

       ``hvc<n>``
         Use the hypervisor console device <n>. (This is for both Xen and
         PowerPC hypervisors.)
     - ::

          console=tty0
          console=ttyS0
          console=hvc0

   * - loglevel
     - SOS
     - All Kernel messages with a loglevel less than the console loglevel will
       be printed to the console. The loglevel can also be changed with
       ``klogd`` or other programs. The loglevels are defined as follows:

       .. list-table::
          :header-rows: 1

          * - loglevel value
            - Definition
          * - 0 (KERN_EMERG)
            - system is unusable
          * - 1 (KERN_ALERT)
            - action must be taken immediately
          * - 2 (KERN_CRIT)
            - critical conditions
          * - 3 (KERN_ERR)
            - error conditions
          * - 4 (KERN_WARNING)
            - warning conditions
          * - 5 (KERN_NOTICE)
            - normal but significant condition
          * - 6 (KERN_INFO)
            - informational
          * - 7 (KERN_DEBUG)
            - debug-level messages
     - ::

          loglevel=7

   * - ignore_loglevel
     - UOS
     - Ignoring loglevel setting will print **all**
       kernel messages to the console. Useful for debugging.
       We also add it as printk module parameter, so users
       could change it dynamically, usually by changing
       ``/sys/module/printk/parameters/ignore_loglevel``.
     - ::

          ignore_loglevel


   * - log_buf_len
     - UOS
     - Sets the size of the printk ring buffer,
       in bytes.  n must be a power of two and greater
       than the minimal size. The minimal size is defined
       by LOG_BUF_SHIFT kernel config parameter. There is
       also CONFIG_LOG_CPU_MAX_BUF_SHIFT config parameter
       that allows to increase the default size depending on
       the number of CPUs. See init/Kconfig for more details."
     - ::

          log_buf_len=16M

   * - consoleblank
     - SOS,UOS
     - The console blank (screen saver) timeout in
       seconds. Defaults to 600 (10 minutes). A value of 0
       disables the blank timer.
     - ::

          consoleblank=0

   * - rootwait
     - SOS,UOS
     - Wait (indefinitely) for root device to show up.
       Useful for devices that are detected asynchronously
       (e.g. USB and MMC devices).
     - ::

          rootwait

   * - root
     - SOS,UOS
     - Define the root filesystem

       ``/dev/<disk_name><decimal>``
          represents the device number of the partition - device
          number of disk plus the partition number

       ``/dev/<disk_name>p<decimal>``
          same as above, this form is used when disk name of
          the partitioned disk ends with a digit. To separate
          disk name and partition slot, a 'p' is inserted.

       ``PARTUUID=00112233-4455-6677-8899-AABBCCDDEEFF``
          representing the unique id of a partition if the
          partition table provides it.  The UUID may be either
          an EFI/GPT UUID, or refer to an MSDOS
          partition using the format SSSSSSSS-PP, where SSSSSSSS is a
          zero-filled hexadecimal representation of the 32-bit
          "NT disk signature", and PP is a zero-filled hexadecimal
          representation of the 1-based partition number.
     - ::

          root=/dev/mmcblk0p1
          root=/dev/vda2
          root=PARTUUID=00112233-4455-6677-8899-AABBCCDDEEFF

   * - rw
     - SOS,UOS
     - Mount root device read-write on boot
     - ::

          rw

   * - tsc
     - UOS
     - Disable clocksource stability checks for TSC.

       Format: <string>, where the only supported value is:

       ``reliable``:
          Mark TSC clocksource as reliable, and disables clocksource
          verification at runtime, and the stability checks done at bootup.
          Used to enable high-resolution timer mode on older hardware, and in
          virtualized environments.
     - ::

          tsc=reliable

   * - cma
     - SOS
     - Sets the size of the kernel global memory area for
       contiguous memory allocations, and optionally the
       placement constraint by the physical address range of
       memory allocations. A value of 0 disables CMA
       altogether. For more information, see
       ``include/linux/dma-contiguous``.
     - ::

          cma=64M@0

   * - hvlog
     - SOS
     - Reserve memory for the ACRN hypervisor log. The reserved space should not
       overlap any other blocks (e.g.  hypervisor's reserved space).
     - ::

          hvlog=2M@0x6de00000

   * - memmap
     - SOS
     - ::

         memmap=0x400000$0x6da00000

     - Mark specific memory as reserved.

       ``memmap=nn[KMG]$ss[KMG]``
         Region of memory to be reserved is from ``ss`` to ``ss+nn``,
         using ``K``, ``M``, and ``G`` representing Kilobytes, Megabytes, and
         Gigabytes, respectively.

   * - ramoops.mem_address
       ramoops.mem_size
       ramoops.console_size
     - SOS
     - Ramoops is an oops/panic logger that writes its logs to RAM
       before the system crashes. Ramoops uses a predefined memory area
       to store the dump. See `Linux Kernel Ramoops oops/panic logger
       <https://www.kernel.org/doc/html/v4.19/admin-guide/ramoops.html#ramoops-oops-panic-logger>`_
       for details.
     - ::

         ramoops.mem_address=0x6da00000
         ramoops.mem_size=0x400000
         ramoops.console_size=0x200000


   * - reboot_panic
     - SOS
     - Reboot in case of panic

       The comma-delimited parameters are:

       reboot_mode:
         ``w`` (warm), ``s`` (soft), ``c`` (cold), or ``g`` (gpio)

       reboot_type:
         ``b`` (bios), ``a`` (acpi), ``k`` (kbd), ``t`` (triple), ``e`` (efi),
         or ``p`` (pci)

       reboot_cpu:
         ``s###`` (smp, and processor number to be used for rebooting)

       reboot_force:
         ``f`` (force), or not specified.
     - ::

         reboot_panic=p,w

   * - maxcpus
     - UOS
     - Maximum number of processors that an SMP kernel
       will bring up during bootup.

       ``maxcpus=n`` where n >= 0 limits
       the kernel to bring up ``n`` processors during system bootup.
       Giving n=0 is a special case, equivalent to ``nosmp``,which
       also disables the I/O APIC.

       After bootup, you can bring up additional plugged CPUs by executing

       ``echo 1 > /sys/devices/system/cpu/cpuX/online``
     - ::

         maxcpus=1

   * - nohpet
     - UOS
     -  Don't use the HPET timer
     - ::

         nohpet

   * - intel_iommu
     - UOS
     - Intel IOMMU driver (DMAR) option

       ``on``:
         Enable intel iommu driver.

       ``off``:
         Disable intel iommu driver.

       ``igfx_off``:
         By default, gfx is mapped as normal device. If a gfx
         device has a dedicated DMAR unit, the DMAR unit is
         bypassed by not enabling DMAR with this option. In
         this case, gfx device will use physical address for DMA.
     - ::

         intel_iommu=off


Intel GVT-g Parameters
**********************

.. list-table::
   :header-rows: 1
   :widths: 10,10,50,30

   * - Parameter
     - Used in SOS or UOS
     - Description
     - Usage example

   * - i915.nuclear_pageflip
     - SOS,UOS
     - Force enable atomic functionality on platforms that don't have full support yet.
     - ::

         i915.nuclear_pageflip=1

   * - i915.enable_initial_modeset
     - SOS
     - On MRB, value must be ``1``.  On NUC or UP2 boards, value must be
       ``0``. See :ref:`i915-enable-initial-modeset`.
     - ::

        i915.enable_initial_modeset=1
        i915.enable_initial_modeset=0

   * - i915.avail_planes_per_pipe
     - SOS
     - See :ref:`i915-avail-planes-owners`.
     - ::

         i915.avail_planes_per_pipe=0x01010F

   * - i915.domain_plane_owners
     - SOS
     - See :ref:`i915-avail-planes-owners`.
     - ::

         i915.domain_plane_owners=0x011111110000

   * - i915.enable_gvt
     - SOS
     - Enable Intel GVT-g graphics virtualization support in the host
     - ::

         i915.enable_gvt=1

   * - i915.enable_guc
     - SOS
     - Enable GuC load for HuC load.
     - ::

         i915.enable_guc=0x02

   * - i915.avail_planes_per_pipe
     - UOS
     - See :ref:`i915-avail-planes-owners`.
     - ::

         i915.avail_planes_per_pipe=0x070F00

   * - i915.enable_guc
     - UOS
     - Disable GuC
     - ::

         i915.enable_guc=0

   * - i915.enable_hangcheck
     - UOS
     - Disable check GPU activity for detecting hangs.
     - ::

         i915.enable_hangcheck=0

   * - i915.enable_fbc
     - UOS
     - Enable frame buffer compression for power savings
     - ::

         i915.enable_fbc=1
