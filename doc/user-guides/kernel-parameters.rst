.. _kernel-parameters:

ACRN Kernel Parameters
######################

Generic kernel parameters
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
     - Usage example

   * - module_blacklist
     - Service VM
     - A comma-separated list of modules that should not be loaded.
       Useful to debug or work
       around issues related to specific modules.
     - ::

         module_blacklist=dwc3_pci

   * - no_timer_check
     - Service VM,User VM
     - Disables the code which tests for broken timer IRQ sources.
     - ::

         no_timer_check

   * - console
     - Service VM,User VM
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
     - Service VM
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
     - User VM
     - Ignoring loglevel setting will print **all**
       kernel messages to the console. Useful for debugging.
       We also add it as printk module parameter, so users
       could change it dynamically, usually by changing
       ``/sys/module/printk/parameters/ignore_loglevel``.
     - ::

          ignore_loglevel


   * - log_buf_len
     - User VM
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
     - Service VM,User VM
     - The console blank (screen saver) timeout in
       seconds. Defaults to 600 (10 minutes). A value of 0
       disables the blank timer.
     - ::

          consoleblank=0

   * - rootwait
     - Service VM,User VM
     - Wait (indefinitely) for root device to show up.
       Useful for devices that are detected asynchronously
       (e.g. USB and MMC devices).
     - ::

          rootwait

   * - root
     - Service VM,User VM
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
     - Service VM,User VM
     - Mount root device read-write on boot
     - ::

          rw

   * - tsc
     - User VM
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
     - Service VM
     - Sets the size of the kernel global memory area for
       contiguous memory allocations, and optionally the
       placement constraint by the physical address range of
       memory allocations. A value of 0 disables CMA
       altogether. For more information, see
       ``include/linux/dma-contiguous``.
     - ::

          cma=64M@0

   * - hvlog
     - Service VM
     - Sets the guest physical address and size of the dedicated hypervisor
       log ring buffer between the hypervisor and Service VM.
       A ``memmap`` parameter is also required to reserve the specified memory
       from the guest VM.

       If hypervisor relocation is disabled, verify that
       :option:`CONFIG_HV_RAM_START` and :option:`CONFIG_HV_RAM_SIZE`
       does not overlap with the hypervisor's reserved buffer space allocated
       in the Service VM. Service VM GPA and HPA are a 1:1 mapping.

       If hypervisor relocation is enabled, reserve the memory below 256MB,
       since hypervisor could be relocated anywhere between 256MB and 4GB.

       You should enable ASLR on SOS. This ensures that when guest Linux is
       relocating kernel image, it will avoid this buffer address.
       
     - ::

          hvlog=2M@0xe00000

   * - memmap
     - Service VM
     - Mark specific memory as reserved.

       ``memmap=nn[KMG]$ss[KMG]``
         Region of memory to be reserved is from ``ss`` to ``ss+nn``,
         using ``K``, ``M``, and ``G`` representing Kilobytes, Megabytes, and
         Gigabytes, respectively.
     - ::

         memmap=0x400000$0xa00000

   * - ramoops.mem_address
       ramoops.mem_size
       ramoops.console_size
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


   * - reboot_panic
     - Service VM
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
     - User VM
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
     - User VM
     -  Don't use the HPET timer
     - ::

         nohpet

   * - intel_iommu
     - User VM
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


Intel GVT-g (AcrnGT) Parameters
*******************************

This table gives an overview of all the Intel GVT-g parameters that are
available to tweak the behavior of the graphics sharing (Intel GVT-g, aka
AcrnGT) capabilities in ACRN. The `GVT-g-kernel-options`_
section below has more details on a few select parameters.

.. list-table::
   :header-rows: 1
   :widths: 10,10,50,30

   * - Parameter
     - Used in Service VM or User VM
     - Description
     - Usage example

   * - i915.enable_gvt
     - Service VM
     - Enable Intel GVT-g graphics virtualization support in the host
     - ::

         i915.enable_gvt=1

   * - i915.gvt_workload_priority
     - Service VM
     - Define the priority level of User VM graphics workloads
     - ::

         i915.gvt_workload_priority=1

   * - i915.enable_initial_modeset
     - Service VM
     - On MRB, value must be ``1``.  On NUC or UP2 boards, value must be
       ``0``. See :ref:`i915-enable-initial-modeset`.
     - ::

         i915.enable_initial_modeset=1
         i915.enable_initial_modeset=0

   * - i915.nuclear_pageflip
     - Service VM,User VM
     - Force enable atomic functionality on platforms that don't have full support yet.
     - ::

         i915.nuclear_pageflip=1

   * - i915.domain_scaler_owner
     - Service VM
     - See `i915.domain_scaler_owner`_
     - ::

         i915.domain_scaler_owner=0x021100

   * - i915.enable_guc
     - Service VM
     - Enable GuC load for HuC load.
     - ::

         i915.enable_guc=0x02

   * - i915.enable_guc
     - User VM
     - Disable GuC
     - ::

         i915.enable_guc=0

   * - i915.enable_hangcheck
     - User VM
     - Disable check GPU activity for detecting hangs.
     - ::

         i915.enable_hangcheck=0

   * - i915.enable_fbc
     - User VM
     - Enable frame buffer compression for power savings
     - ::

         i915.enable_fbc=1

.. _GVT-g-kernel-options:

GVT-g (AcrnGT) Kernel Options details
=====================================

This section provides additional information and details on the kernel command
line options that are related to AcrnGT.

i915.enable_gvt
---------------

This option enables support for Intel GVT-g graphics virtualization
support in the host. By default, it's not enabled, so we need to add
``i915.enable_gvt=1`` in the Service VM kernel command line.  This is a Service
OS only parameter, and cannot be enabled in the User VM.

i915.gvt_workload_priority
--------------------------

AcrnGT supports **Prioritized Rendering** as described in the
:ref:`GVT-g-prioritized-rendering` high-level design.  This
configuration option controls the priority level of GVT-g guests.
Priority levels range from -1023 to 1023.

The default priority is zero, the same priority as the Service VM. If
the level is less than zero, the guest's priority will be lower than the
Service VM, so graphics preemption will work and the prioritized
rendering feature will be enabled.  If the level is greater than zero,
User VM graphics workloads will preempt most of the Service VM graphics workloads,
except for display updating related workloads that use a default highest
priority (1023).

Currently, all User VMs share the same priority.
This is a Service VM only parameters, and does
not work in the User VM.

.. _i915-enable-initial-modeset:

i915.enable_initial_modeset
---------------------------

At time, kernel graphics must be initialized with a valid display
configuration with full display pipeline programming in place before the
user space is initialized and without a fbdev & fb console.

When ``i915.enable_initial_modeset=1``, the FBDEV of i915 will not be
initialized, so users would not be able to see the fb console on screen.
If there is no graphics UI running by default, users will see black
screens displayed.

When ``i915.enable_initial_modeset=0`` in Service VM, the plane restriction
(also known as plane-based domain ownership) feature will be disabled.
(See the next section and :ref:`plane_restriction` in the ACRN GVT-g
High Level Design for more information about this feature.)

In the current configuration, we will set
``i915.enable_initial_modeset=1`` in Service VM and
``i915.enable_initial_modeset=0`` in User VM.

This parameter is not used on UEFI platforms.

i915.domain_scaler_owner
========================

On each Intel GPU display pipeline, there are several plane scalers
to zoom in/out the planes. For example, if a 720p video is played
full-screen on a 1080p display monitor, the kernel driver will use a
scaler to zoom in the video plane to a 1080p image and present it onto a
display pipeline. (Refer to "Intel Open Source Graphics PRM Vol 7:
display" for the details.)

On Broxton platforms, Pipe A and Pipe B each
have two plane scalers, and Pipe C has one plane scaler. To support the
plane scaling in AcrnGT guest OS, we introduced the parameter
``i915.domain_scaler_owner``, to assign a specific scaler to the target
guest OS.

As with the parameter ``i915.domain_plane_owners``, each nibble of
``i915.domain_scaler_owner`` represents the domain id that owns the scaler;
every nibble (4 bits) represents a scaler and every group of 2 nibbles
represents a pipe. This is a Service VM only configuration and cannot be
modified at runtime. Domain ID 0x0 is for the Service VM, the User VM
use domain IDs from 0x1 to 0xF.

For example, if we set ``i915.domain_scaler_owner=0x021100``, the Service VM
owns scaler 1A, 2A; User VM #1 owns scaler 1B, 2B; and User VM #2 owns scaler
1C.

i915.enable_hangcheck
=====================

This parameter enable detection of a GPU hang. When enabled, the i915
will start a timer to check if the workload is completed in a specific
time. If not, i915 will treat it as a GPU hang and trigger a GPU reset.

In AcrnGT, the workload in Service VM and User VM can be set to different
priorities. If Service VM is assigned a higher priority than the User VM, the User VM's
workload might not be able to run on the HW on time. This may lead to
the guest i915 triggering a hangcheck and lead to a guest GPU reset.
This reset is unnecessary so we use ``i915.enable_hangcheck=0`` to
disable this timeout check and prevent guest from triggering unnecessary
GPU resets.
