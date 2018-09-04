.. _partition_mode:

Using partition mode
#########################
This tutorial describes all the steps you need to take in order to be able to
use partition mode with the ACRN hypervisor. Partition mode provides an
isolation of PCI devices to a given UOS so that the PCI devices can be used
exclusively by that UOS, bypassing the ACRN hypervisor and achieving near-native
performance.


Enabling partition mode
***********************

To enable partition mode, perform the following steps:

1. Set ``Hypervisor mode`` to ``PARTITION_MODE`` in
``hypervisor/arch/x86/Kconfig``:

   .. figure:: images/partition_mode_kconfig.png
      :align: center
      :name: partition_mode_kconfig



For MRB board,  you need to perform the following additional step:

Follow the URL `EBTool for MRB <https://wiki.ith.intel.com/display/OTCCWPQA/EBtool+for+MRB>`_
to get acrn-ebtool working on your development machine.

Change directory to ``acrn-ebtool`` (the top-level directory where the ACRN hypervisor
and ACRN kernel code reside), open ``Makefile`` and edit the ``stitch: fw_stitch2``
target rule to use UOS kernel to stitch out:

   .. figure:: images/partition_mode_makefile.png
      :align: center
      :name: partition_mode_makefile

2. Do modifications to the ``vpci_vdev_array`` structure (one per UOS VM) in
   ``hypervisor/partition/vm_description.c`` to suit your needs.

The vpci_vdev_array structure defines the virtual PCI devices to be attached
to UOS VM, all virtual PCI devices hang off on a virtual PCI bus. There
are two types of virtual PCI devices:
``PCI passthrough device`` and ``PCI host bridge``

For a PCI passthrough device, only its BARs are virtualized. All PCI BARs are
initialized to 0 and UOS will program the memory-mapped addresses (GPA) into
the device's BAR configuration register at its virtual BDF, hypervisor
intercepts the BAR access and reads back the host BAR value (HPA) from the
corresponding physical PCI's bar register at its physical BDF to establish
the GPA to HPA mapping for this BAR. Host BARs are allocated by system firmware
at boot time and are not changed by hypervisor during runtime.


A PCI host bridge at virtual BDF 00:00.0 will be attached to each UOS VM, but
unlike a PCI passthrough device, virtual PCI host bridge is fully virtualized
and does not have a physical PCI device counterpart.

The vpci_vdev_array structure describes the pci_vdev list for the virtual PCI bus.
The table below lists all of the members of the vpci_vdev_array structure:

  .. table:: vpci_vdev_array
      :widths: auto
      :name: vpci_vdev_array

      +--------------------+-------------------------------------------------------------+
      | Member             | Description                                                 |
      +====================+=============================================================+
      | num_pci_vdev       | Specify the number of elements in the vpci_vdev_list array  |
      +--------------------+-------------------------------------------------------------+
      | vpci_vdev_list     | Specify an array of pci_vdev elements that comprise a       |
      |                    | pci_vdev list. Each array element is a structure of type    |
      |                    | pci_vdev                                                    |
      +--------------------+-------------------------------------------------------------+


Each virtual PCI device is represented by pci_vdev structure:

  .. table:: pci_vdev
      :widths: auto
      :name: pci_vdev

      +--------------------+----------------------------------------------------------------+
      | Member             | Description                                                    |
      +====================+================================================================+
      | vbdf               | Attach a PCI device to UOS VM by desired virtual BDF, needs to |
      |                    | be unique to one device. For host bridge, its vbdf should be   |
      |                    | 00:00.0                                                        |
      +--------------------+----------------------------------------------------------------+
      | ops                | Define ops callbacks for a virtual PCI device.                 |
      |                    | pci_ops_vdev_hostbridge: ops callbacks for PCI host bridge     |
      |                    | pci_ops_vdev_pt: ops callbacks for PCI passthrough device      |
      +--------------------+----------------------------------------------------------------+
      | bar                | Specify the UOS BARs (up to 6) for the attached PCI            |
      |                    | device. The bar is virtualized, so host bar and UOS bar are    |
      |                    | different. host bar is specified by the bar member of pdev     |
      |                    |                                                                |
      |                    | base: BAR base address, must be set to 0                       |
      |                    |                                                                |
      |                    | size: host bar size rounded up to the nearest 4k               |
      |                    |                                                                |
      |                    | type: should always be set to PCIBAR_MEM32.                    |
      |                    | For 64-bit memory space host BAR, it must be smaller than      |
      |                    | 4GB in size, so that 32-bit memory UOS bar can always be used. |
      |                    |                                                                |
      |                    | I/O space bar is not supported so it can be left unspecified   |
      +--------------------+----------------------------------------------------------------+
      | pdev               | Specify the host bdf/bar for the attached PCI device.          |
      |                    | For hostbridge, pdev is not used and can be left unspecified   |
      +--------------------+----------------------------------------------------------------+



Each PCI passthrough device is represented by pci_pdev structure:


  .. table:: vpci_pdev
      :widths: auto
      :name: vpci_pdev

      +--------------------+-------------------------------------------------------------+
      | Member             | Description                                                 |
      +====================+=============================================================+
      | bdf                | Specify physical BDF for PCI passthrough device.            |
      |                    | Use ``lspci`` command in SOS/native OS to select the PCI BDF|
      |                    | you want to assign to UOS. Note that only PCI devices that  |
      |                    | use MSI/MSI-X interrupts can be assigned to UOS, INTx is not|
      |                    | supported                                                   |
      +--------------------+-------------------------------------------------------------+
      | bar                | Specify the host bar for the PCI passthrough device.        |
      |                    | Use ``lspci -v`` command in SOS/native OS to locate the     |
      |                    | entry for device and note down the host bar base and size,  |
      |                    | and accordingly fill in the values in bar.                  |
      |                    | I/O space bar is not supported so it can be left unspecified|
      +--------------------+-------------------------------------------------------------+


3. Do modifications for assigning processors to each UOS VM

   A. In ``hypervisor/partition/vm_description.c``, change the following as they suit the platform resources.

        ``NUM_USER_VMS`` - Denotes the number of UOS VMs booted by ACRN in partition mode.Change
                           it according to the number of UOS VMs need to be booted in partition
                           mode by ACRN.

        ``VM1_NUM_CPUS`` - Denotes the number of CPUs used by UOS VM1


        ``VM1_CPUS``     - This is an array of size ``VM1_NUM_CPUS``. Use this array to
                         indicate the processors used for UOS VM1.
                         Processors are  numbered in the same sequence as they are in MADT.
                         For example, if UOS VM1 needs to use processors 0, 2, 4 and 6 listed
                         in the MADT, do the following:

                  .. code-block:: none

                   #define VM1_NUM_CPUS    4U

                   /* Logical CPU IDs assigned to this VM */
                   uint16_t VM1_CPUS[VM1_NUM_CPUS] = {0U, 2U, 4U, 6U};



        ``VM2_NUM_CPUS`` - Denotes the number of CPUs used by UOS VM2.

        ``VM2_CPUS``     - Same as ``VM1_CPUS``. Make sure the processor numbers
                           used in ``VM1_CPUS`` are not repeated.


   B.   Do modifications to the ``vm_desc_array`` as needed, one ``vm_description`` per each UOS VM.

        ``start_hpa``   -  Denotes the starting address in host physical address space for the
                           memory used by UOS VM.

        ``mem_size``    -  Denotes the size of memory used by UOS VM.
                              To avoid any use of reserved memory regions below 4GB on the host,
                              start_hpa for UOS VMs is used starting from 4GB.
                              Memory ranges (start_hpa to (start_hpa+mem_size))
                              used for each UOS VM should not overlap with each
                              other. Sum of ``mem_size`` of all UOSes should not
                              exceed total memory available above 4GB on the host.

        ``bootargs``    -  String passed in here is passed to each UOS VM as kernel
                           boot parameters.

        ``mptable``     -  MPTable is used to pass processor and bus info for VMs
                           in partition mode. How to configure MPTable is discussed
                           in the following section.

   C.   Do modifications to the ``pcpu_vm_desc_map`` as needed, one per each CPU on the host.

        ``vm_desc_ptr`` -  Points to the vm_description in the ``vm_desc_array``
                           (described above) that needs to be used by this physical CPU.
                           Thereby, pointing to the UOS VM that runs on this physical CPU.

        ``is_bsp``      -  Denotes if this physical CPU is BSP of the UOS VM.

        Make sure, this array is initialized for each physical CPU (Number of
        entries in the array should be same as the number of physical CPUs).


   D.   Do modifications to ``e820_default_entries`` as needed. All the UOS
        VMs booted by ACRN in partition mode uses the same amount of memory ``mem_size``
        as described above. Different memory requirements for each UOS VM is
        not supported in this version of ACRN.

        Second entry, total of 64K starting from 0xF0000U, is used for reserving
        memory for MPTable info.

        PCI hole is between 0xC0000000 and 0xDFFFFFFF.

        First and Third entry is used for denoting memory that can be used by
        UOS VM kernel. First and third entries should total to ``mem_size``.

        Rest of the memory between ``mem_size`` and start of PCI hole i.e.
        0xC0000000 should go into fourth entry. Reference code is for 512MB
        for each UOS VM. If the memory requirement for each UOS VM is 1GB
        instead, ``e820_default_entries`` looks like below:

         .. code-block:: none

            const struct e820_entry e820_default_entries[NUM_E820_ENTRIES] = {
                    {   /* 0 to mptable */
                        .baseaddr =  0x0U,
                        .length   =  0xEFFFFU,
                        .type     =  E820_TYPE_RAM
                    },

                    {   /* mptable 65536U */
                        .baseaddr =  0xF0000U,
                        .length   =  0x10000U,
                        .type     =  E820_TYPE_RESERVED
                    },

                    {   /* mptable to lowmem */
                       .baseaddr =  0x100000U,
                       .length   =  0x3FF00000U,
                       .type     =  E820_TYPE_RAM
                    },

                    {    /* lowmem to PCI hole */
                        .baseaddr =  0x40000000U,
                        .length   =  0x80000000U,
                        .type     =  E820_TYPE_RESERVED
                    },

                    {   /* PCI hole to 4G */
                        .baseaddr =  0xe0000000U,
                        .length   =  0x20000000U,
                        .type     =  E820_TYPE_RESERVED
                    },
                }


   E.   Do modifications to ``mptable_vm(n)`` (where n stands for each UOS VM). Each UOS
        VM has a mptable defined in ``arch/x86/guest/mptable.c``.

         Make sure ``VM1_NUM_CPUS`` and ``VM2_NUM_CPUS`` are defined to the same
         values as they are in ``partition\vm_description.c``.

         ``proc_entry_array`` member in struct ``mptable_info`` is a flexible array.
         Each cpu of the UOS should be represented by a ``proc_entry`` in ``proc_entry_array``.

        ``apic_id`` in the ``proc_entry`` should be same as physical APIC ID
        of physical CPU that belongs to the UOS VM.

        For the following configuration defined in ``partition/vm_description.c``,

               .. code-block:: none

                  #define VM1_NUM_CPUS    4U

                  /* Logical CPU IDs assigned to this VM */
                  uint16_t VM1_CPUS[VM1_NUM_CPUS] = {0U, 2U, 4U, 6U};

         ``proc_entry_array`` in ``mptable_vm1`` should have 4 ``proc_entry``
         members and the ``apic_id`` of each ``proc_entry`` should be corresponding
         to the physical APIC id of the physical processors ``{0U, 2U, 4U, 6U}``.

Building ACRN with partition mode enabled
*****************************************

For MRB board, change directory to ``acrn-ebtool`` (the top-level directory where
the ACRN hypervisor and ACRN kernel code reside), then:

#. Build the UOS kernel and rootfs.

   .. code-block:: none

      $ make uos_kernel uos_rootfs

   The build results are found under the ``out/uos_kernel`` and ``out/uos_rootfs`` directories.



#. Build and flash the ACRN hypervisor.

   .. code-block:: none

      $ make hypervisor stitch sos_boot
      $ make flash_acrn


Copying UOS rootfs to storage device
************************************

Connect the storage device to your computer and identify it using the ``lsblk`` command.
Throughout this procedure, the target storage device name used is ``/dev/sdX``, substitute 
sdX with the correct device for your storage device.

.. code-block:: none

     $ sudo mount out/uos_rootfs.img /mnt/uos_rootfs/
     $ sudo umount /dev/sdX
     $ sudo mkfs.ext4 /dev/sdX
     $ sudo mkdir /mnt/sdX
     $ sudo mount /dev/sdX /mnt/sdX
     $ cd /mnt/sdX
     $ sudo rm -fr *
     $ sudo cp -ax /mnt/uos_rootfs/* .
     $ cd ~
     $ sync
     $ sudo umount /mnt/sdX


Accessing UOS console from ACRN console
***************************************

Once ACRN is booted by the bootloader, ACRN starts the UOS VMs. Each UOS VM console can be accessed by the command ``sos_console x``.
``x`` stands for the UOS VM id. It is same as the ``vm_id`` programmed in the ``partition\vm_description.c`` for the corresponding 
UOS VM.
