.. _kernel-parameters:

ACRN Kernel Parameters
######################

Generic Kernel Parameters
*************************

A number of kernel parameters control the behavior of ACRN-based systems. Some
are applicable to the Service VM kernel, others to the User VM
kernel, and some are applicable to both.

This section focuses on generic parameters from the Linux kernel which are
relevant for configuring or debugging ACRN-based systems.

.. list-table::
   :header-rows: 1
   :widths: 10,10,50,30

   * - Parameter
     - Used in Service VM or User VM
     - Description
     - Usage Example

   * - ``module_blacklist``
     - Service VM
     - A comma-separated list of modules that should not be loaded.
       Useful to debug or work
       around issues related to specific modules.
     - ::

         module_blacklist=dwc3_pci

   * - ``no_timer_check``
     - Service VM, User VM
     - Disables the code that tests for broken timer IRQ sources.
     - ::

         no_timer_check

   * - ``console``
     - Service VM, User VM
     - Output console device and options.

       ``tty<n>``
         Use the virtual console device <n>.

       ``ttyS<n>[,options]``
         Use the specified serial port and options. Default options are
         ``9600n8`` meaning 9600 baud, no parity, 8 bits. Options are of the form *bbbbpnf*,
         where:

            | *bbbb* is baud rate, for example, 9600
            | *p* is parity, one of ``n``, ``o``, or ``e`` (for none, odd, or even)
            | *n* is number of bits (typically 8)
            | *f* is flow control (``r`` for RTS, or left blank)

       ``hvc<n>``
         Use the hypervisor console device <n>. (This is for both Xen and
         PowerPC hypervisors.)
     - ::

          console=tty0
          console=ttyS0
          console=hvc0

   * - ``loglevel``
     - Service VM
     - All kernel messages with a loglevel less than the console loglevel will
       be printed to the console. The loglevel can also be changed with
       ``klogd`` or other programs. The loglevels are defined as follows:

       .. list-table::
          :header-rows: 1

          * - Loglevel Value
            - Definition
          * - 0 (KERN_EMERG)
            - System is unusable
          * - 1 (KERN_ALERT)
            - Action must be taken immediately
          * - 2 (KERN_CRIT)
            - Critical conditions
          * - 3 (KERN_ERR)
            - Error conditions
          * - 4 (KERN_WARNING)
            - Warning conditions
          * - 5 (KERN_NOTICE)
            - Normal but significant condition
          * - 6 (KERN_INFO)
            - Informational
          * - 7 (KERN_DEBUG)
            - Debug-level messages
     - ::

          loglevel=7

   * - ``ignore_loglevel``
     - User VM
     - Ignoring loglevel setting will print **all**
       kernel messages to the console. Useful for debugging.
       We also add it as the ``printk`` module parameter, so users
       can change it dynamically, usually by changing
       ``/sys/module/printk/parameters/ignore_loglevel``.
     - ::

          ignore_loglevel


   * - ``log_buf_len``
     - User VM
     - Sets the size of the ``printk`` ring buffer,
       in bytes.  n must be a power of two and greater
       than the minimal size. The minimal size is defined
       by the ``LOG_BUF_SHIFT`` kernel config parameter. There is
       also the ``CONFIG_LOG_CPU_MAX_BUF_SHIFT`` config parameter
       that allows to increase the default size depending on
       the number of CPUs. See ``init/Kconfig`` for more details.
     - ::

          log_buf_len=16M

   * - ``consoleblank``
     - Service VM, User VM
     - The console blank (screen saver) timeout in
       seconds. Defaults to 600 (10 minutes). A value of 0
       disables the blank timer.
     - ::

          consoleblank=0

   * - ``rootwait``
     - Service VM, User VM
     - Wait (indefinitely) for root device to show up.
       Useful for devices that are detected asynchronously
       (e.g., USB and MMC devices).
     - ::

          rootwait

   * - ``root``
     - Service VM, User VM
     - Define the root filesystem.

       ``/dev/<disk_name><decimal>``
          represents the device number of the partition - device
          number of disk plus the partition number.

       ``/dev/<disk_name>p<decimal>``
          same as above, this form is used when the disk name of
          the partitioned disk ends with a digit. To separate the
          disk name and partition slot, a ``p`` is inserted.

       ``PARTUUID=00112233-4455-6677-8899-AABBCCDDEEFF``
          represents the unique ID of a partition if the
          partition table provides it.  The UUID may be either
          an EFI/GPT UUID, or refer to an MSDOS
          partition using the format SSSSSSSS-PP, where SSSSSSSS is a
          zero-filled hexadecimal representation of the 32-bit
          NT disk signature, and PP is a zero-filled hexadecimal
          representation of the 1-based partition number.
     - ::

          root=/dev/mmcblk0p1
          root=/dev/vda2
          root=PARTUUID=00112233-4455-6677-8899-AABBCCDDEEFF

   * - ``rw``
     - Service VM, User VM
     - Mount the root device read/write on boot.
     - ::

          rw

   * - ``tsc``
     - User VM
     - Disable clocksource stability checks for TSC.

       Format: <string>, where the only supported value is:

       ``reliable``:
          Mark TSC clocksource as reliable, and disable clocksource
          verification at runtime, and the stability checks done at boot.
          Used to enable high-resolution timer mode on older hardware, and in
          virtualized environments.
     - ::

          tsc=reliable

   * - ``cma``
     - Service VM
     - Sets the size of the kernel global memory area for
       contiguous memory allocations, and optionally the
       placement constraint by the physical address range of
       memory allocations. A value of 0 disables CMA
       altogether. For more information, see
       ``include/linux/dma-contiguous``.
     - ::

          cma=64M@0

   * - ``hvlog``
     - Service VM
     - Sets the guest physical address and size of the dedicated hypervisor
       log ring buffer between the hypervisor and Service VM.
       A ``memmap`` parameter is also required to reserve the specified memory
       from the guest VM.

       If hypervisor relocation is enabled, reserve the memory below 256MB,
       since the hypervisor could be relocated anywhere between 256MB and 4GB.

       Enable address space layout randomization (ASLR) on the Service VM.
       This ensures that when the guest Linux is relocating the kernel image,
       it will avoid this buffer address.

     - ::

          hvlog=2M@0xe00000

   * - ``memmap``
     - Service VM
     - Mark specific memory as reserved.

       ``memmap=nn[KMG]$ss[KMG]``
         Region of memory to be reserved is from ``ss`` to ``ss+nn``,
         using ``K``, ``M``, and ``G`` representing kilobytes, megabytes, and
         gigabytes, respectively.
     - ::

         memmap=0x400000$0xa00000

   * - ``ramoops.mem_address``
       ``ramoops.mem_size``
       ``ramoops.console_size``
     - Service VM
     - Ramoops is an oops/panic logger that writes its logs to RAM
       before the system crashes. Ramoops uses a predefined memory area
       to store the dump. See `Linux Kernel Ramoops oops/panic logger
       <https://www.kernel.org/doc/html/v4.19/admin-guide/ramoops.html#ramoops-oops-panic-logger>`_
       for details.

       This buffer should not overlap with hypervisor reserved memory and
       guest kernel image. See ``hvlog``.
     - ::

         ramoops.mem_address=0xa00000
         ramoops.mem_size=0x400000
         ramoops.console_size=0x200000


   * - ``reboot_panic``
     - Service VM
     - Reboot in case of panic.

       The comma-delimited parameters are:

       reboot_mode:
         ``w`` (warm), ``s`` (soft), ``c`` (cold), or ``g`` (GPIO)

       reboot_type:
         ``b`` (BIOS), ``a`` (ACPI), ``k`` (kbd), ``t`` (triple), ``e`` (EFI),
         or ``p`` (PCI)

       reboot_cpu:
         ``s###`` (SMP, and processor number to be used for rebooting)

       reboot_force:
         ``f`` (force), or not specified.
     - ::

         reboot_panic=p,w

   * - ``maxcpus``
     - User VM
     - Maximum number of processors that an SMP kernel
       will bring up during boot.

       ``maxcpus=n`` where n >= 0 limits
       the kernel to bring up ``n`` processors during system boot.
       Giving n=0 is a special case, equivalent to ``nosmp``, which
       also disables the I/O APIC.

       After booting, you can bring up additional plugged CPUs by executing

       ``echo 1 > /sys/devices/system/cpu/cpuX/online``
     - ::

         maxcpus=1

   * - ``nohpet``
     - User VM
     - Don't use the HPET timer.
     - ::

         nohpet

   * - ``intel_iommu``
     - User VM
     - Intel IOMMU driver (DMAR) option

       ``on``:
         Enable Intel IOMMU driver.

       ``off``:
         Disable Intel IOMMU driver.

       ``igfx_off``:
         By default, gfx is mapped as a normal device. If a gfx
         device has a dedicated DMAR unit, the DMAR unit is
         bypassed by not enabling DMAR with this option. In
         this case, the gfx device will use the physical address for DMA.
     - ::

         intel_iommu=off

   * - ``hugepages``
       ``hugepagesz``
     - Service VM, User VM
     - ``hugepages``:
         HugeTLB pages to allocate at boot.

       ``hugepagesz``:
         The size of the HugeTLB pages. On x86-64 and PowerPC,
         this option can be specified multiple times interleaved
         with ``hugepages`` to reserve huge pages of different sizes.
         Valid page sizes on x86-64 are 2M (when the CPU supports Page Size Extension (PSE))
         and 1G (when the CPU supports the ``pdpe1gb`` cpuinfo flag).
     - ::

         hugepages=10
         hugepagesz=1G

   * - ``i915.modeset``
     - Service VM
     - GPU driver loading option.

       ``0``:
         Disable the GPU driver loading for Intel GPU device.

       ``1``:
         Enable the GPU driver loading for Intel GPU device.
     - ::

          i915.modeset=0

.. note:: The ``hugepages`` and ``hugepagesz`` parameters are automatically
   taken care of by the ACRN Configurator tool. If users have customized
   hugepage settings to satisfy their particular workloads in the Service VM,
   the ``hugepages`` and ``hugepagesz`` parameters can be redefined in the GRUB
   menu to override the settings from the ACRN Configurator tool.
