.. _enable_ivshmem:

Enable Inter-VM Communication Based on Ivshmem
##############################################

You can use inter-VM communication based on the ``ivshmem`` dm-land
solution or hv-land solution, according to the usage scenario needs.
(See :ref:`ivshmem-hld` for a high-level description of these solutions.)
While both solutions can be used at the same time, VMs using different
solutions cannot communicate with each other.

Ivshmem DM-Land Usage
*********************

Add this line as an ``acrn-dm`` boot parameter::

     -s slot,ivshmem,shm_name,shm_size

where

-  ``-s slot``  - Specify the virtual PCI slot number

-  ``ivshmem``  - Virtual PCI device name

-  ``shm_name`` - Specify a shared memory name. Post-launched VMs with the same
   ``shm_name`` share a shared memory region. The ``shm_name`` needs to start
   with ``dm:/`` prefix. For example, ``dm:/test``

-  ``shm_size`` - Specify a shared memory size. The unit is megabyte. The size
   ranges from 2 megabytes to 512 megabytes and must be a power of 2 megabytes.
   For example, to set up a shared memory of 2 megabytes, use ``2``
   instead of ``shm_size``. The two communicating VMs must define the same size.

.. note:: This device can be used with real-time VM (RTVM) as well.

.. _ivshmem-hv:

Ivshmem HV-Land Usage
*********************

The ``ivshmem`` hv-land solution is disabled by default in ACRN. You can enable
it using the :ref:`ACRN configuration toolset <acrn_config_workflow>` with these
steps:

- Enable ``ivshmem`` hv-land in ACRN XML configuration file.

   - Edit :option:`hv.FEATURES.IVSHMEM.IVSHMEM_ENABLED` to ``y`` in ACRN scenario XML configuration
     to enable ``ivshmem`` hv-land

   - Edit :option:`hv.FEATURES.IVSHMEM.IVSHMEM_REGION` to specify the shared memory name, size and
     communication VMs in ACRN scenario XML configuration. The :option:`hv.FEATURES.IVSHMEM.IVSHMEM_REGION`
     format is ``shm_name,shm_size,VM IDs``:

     -  ``shm_name`` - Specify a shared memory name. The name needs to start
        with the ``hv:/`` prefix. For example, ``hv:/shm_region_0``

     -  ``shm_size`` - Specify a shared memory size. The unit is megabyte. The
        size ranges from 2 megabytes to 512 megabytes and must be a power of 2 megabytes.
        For example, to set up a shared memory of 2 megabytes, use ``2``
        instead of ``shm_size``.

     -  ``VM IDs``   - Specify the VM IDs to use the same shared memory
        communication and separate it with ``:``. For example, the
        communication between VM0 and VM2, it can be written as ``0:2``

   .. note:: You can define up to eight ``ivshmem`` hv-land shared regions.

- Build with the XML configuration, refer to :ref:`getting-started-building`.

Ivshmem Notification Mechanism
******************************

Notification (doorbell) of ivshmem device allows VMs with ivshmem
devices enabled to notify (interrupt) each other following this flow:

Notification Sender (VM):
   VM triggers the notification to target VM by writing target Peer ID
   (Equals to VM ID of target VM) and vector index to doorbell register of
   ivshmem device, the layout of doorbell register is described in
   :ref:`ivshmem-hld`.

Hypervisor:
   When doorbell register is programmed, hypervisor will search the
   target VM by target Peer ID and inject MSI interrupt to the target VM.

Notification Receiver (VM):
   VM receives MSI interrupt and forward it to related application.

ACRN supports up to 8 (MSI-X) interrupt vectors for ivshmem device.
Guest VMs shall implement their own mechanism to forward MSI interrupts
to applications.

.. note:: Notification is supported only for HV-land ivshmem devices. (Future
   support may include notification for DM-land ivshmem devices.)

Inter-VM Communication Examples
*******************************

DM-Land Example
===============

This example uses dm-land inter-VM communication between two
Linux-based post-launched VMs (VM1 and VM2).

.. note:: An ``ivshmem`` Windows driver exists and can be found
   `here <https://github.com/virtio-win/kvm-guest-drivers-windows/tree/master/ivshmem>`_.

1. Add a new virtual PCI device for both VMs: the device type is
   ``ivshmem``, shared memory name is ``dm:/test``, and shared memory
   size is 2MB. Both VMs must have the same shared memory name and size:

   - VM1 Launch Script Sample

     .. code-block:: none
        :emphasize-lines: 7

        acrn-dm -A -m $mem_size -s 0:0,hostbridge \
         -s 2,pci-gvt -G "$2" \
         -s 5,virtio-console,@stdio:stdio_port \
         -s 6,virtio-hyper_dmabuf \
         -s 3,virtio-blk,/home/acrn/uos1.img \
         -s 4,virtio-net,tap0 \
         -s 6,ivshmem,dm:/test,2 \
         -s 7,virtio-rnd \
         --ovmf /usr/share/acrn/bios/OVMF.fd \
         $vm_name


   - VM2 Launch Script Sample

     .. code-block:: none
        :emphasize-lines: 5

        acrn-dm -A -m $mem_size -s 0:0,hostbridge \
         -s 2,pci-gvt -G "$2" \
         -s 3,virtio-blk,/home/acrn/uos2.img \
         -s 4,virtio-net,tap0 \
         -s 5,ivshmem,dm:/test,2 \
         --ovmf /usr/share/acrn/bios/OVMF.fd \
         $vm_name

2. Boot two VMs and use ``lspci | grep "shared memory"`` to verify that the virtual device is ready for each VM.

   -  For VM1, it shows ``00:06.0 RAM memory: Red Hat, Inc. Inter-VM shared memory (rev 01)``
   -  For VM2, it shows ``00:05.0 RAM memory: Red Hat, Inc. Inter-VM shared memory (rev 01)``

3. As recorded in the `PCI ID Repository <https://pci-ids.ucw.cz/read/PC/1af4>`_,
   the ``ivshmem`` device vendor ID is ``1af4`` (Red Hat) and device ID is ``1110``
   (Inter-VM shared memory).  Use these commands to probe the device::

     $ sudo modprobe uio
     $ sudo modprobe uio_pci_generic
     $ sudo echo "1af4 1110" > /sys/bus/pci/drivers/uio_pci_generic/new_id

.. note:: These commands are applicable to Linux-based guests with ``CONFIG_UIO`` and ``CONFIG_UIO_PCI_GENERIC`` enabled.

4. Finally, a user application can get the shared memory base address from
   the ``ivshmem`` device BAR resource
   (``/sys/class/uio/uioX/device/resource2``) and the shared memory size from
   the ``ivshmem`` device config resource
   (``/sys/class/uio/uioX/device/config``).

   The ``X`` in ``uioX`` above, is a number that can be retrieved using the
   ``ls`` command:

   - For VM1 use ``ls -lh /sys/bus/pci/devices/0000:00:06.0/uio``
   - For VM2 use ``ls -lh /sys/bus/pci/devices/0000:00:05.0/uio``

HV-Land Example
===============

This example uses hv-land inter-VM communication between two
Linux-based VMs (VM0 is a pre-launched VM and VM2 is a post-launched VM).

1. Make a copy of the predefined hybrid_rt scenario on whl-ipc-i5 (available at
   ``acrn-hypervisor/misc/config_tools/data/whl-ipc-i5/hybrid_rt.xml``) and
   configure shared memory for the communication between VM0 and VM2. The shared
   memory name is ``hv:/shm_region_0``, and shared memory size is 2M bytes. The
   resulting scenario XML should look like this:

   .. code-block:: none
      :emphasize-lines: 2,3

      <IVSHMEM desc="IVSHMEM configuration">
             <IVSHMEM_ENABLED>y</IVSHMEM_ENABLED>
             <IVSHMEM_REGION>hv:/shm_region_0, 2, 0:2</IVSHMEM_REGION>
      </IVSHMEM>

2. Build ACRN based on the XML configuration for hybrid_rt scenario on whl-ipc-i5 board::

	make BOARD=whl-ipc-i5 SCENARIO=<path/to/edited/scenario.xml> TARGET_DIR=xxx

3. Add a new virtual PCI device for VM2 (post-launched VM): the device type is
   ``ivshmem``, shared memory name is ``hv:/shm_region_0``, and shared memory
   size is 2MB.

   - VM2 Launch Script Sample

     .. code-block:: none
        :emphasize-lines: 5

        acrn-dm -A -m $mem_size -s 0:0,hostbridge \
         -s 2,pci-gvt -G "$2" \
         -s 3,virtio-blk,/home/acrn/uos2.img \
         -s 4,virtio-net,tap0 \
         -s 5,ivshmem,hv:/shm_region_0,2 \
         --ovmf /usr/share/acrn/bios/OVMF.fd \
         $vm_name

4. Continue following the dm-land steps 2-4 and the ``ivshmem`` device BDF may be different
   depending on the configuration.
