.. _virtio-hld:

Virtio high-level design
########################

The ACRN Hypervisor follows the `Virtual I/O Device (virtio)
specification
<http://docs.oasis-open.org/virtio/virtio/v1.0/virtio-v1.0.html>`_ to
realize I/O virtualization for many performance-critical devices
supported in the ACRN project. Adopting the virtio specification lets us
reuse many frontend virtio drivers already available in a Linux-based
User OS, drastically reducing potential development effort for frontend
virtio drivers.  To further reduce the development effort of backend
virtio drivers, the hypervisor  provides the virtio backend service
(VBS) APIs, that make  it very straightforward to implement a virtio
device in the hypervisor.

The virtio APIs can be divided into 3 groups: DM APIs, virtio backend
service (VBS) APIs, and virtqueue (VQ) APIs, as shown in
:numref:`be-interface`.

.. figure:: images/virtio-hld-image0.png
   :width: 900px
   :align: center
   :name: be-interface

   ACRN Virtio Backend Service Interface

-  **DM APIs** are exported by the DM, and are mainly used during the
   device initialization phase and runtime. The DM APIs also include
   PCIe emulation APIs because each virtio device is a PCIe device in
   the SOS and UOS.
-  **VBS APIs** are mainly exported by the VBS and related modules.
   Generally they are callbacks to be
   registered into the DM.
-  **VQ APIs** are used by a virtio backend device to access and parse
   information from the shared memory between the frontend and backend
   device drivers.

Virtio Device
*************

Virtio framework is the para-virtualization specification that ACRN
follows to implement I/O virtualization of performance-critical
devices such as audio, eAVB/TSN, IPU, and CSMU devices. This section gives
an overview about virtio history, motivation, and advantages, and then
highlights virtio key concepts. Second, this section will describe
ACRN's virtio architectures, and elaborates on ACRN virtio APIs. Finally
this section will introduce all the virtio devices currently supported
by ACRN.

Introduction
============

Virtio is an abstraction layer over devices in a para-virtualized
hypervisor. Virtio was developed by Rusty Russell when he worked at IBM
research to support his lguest hypervisor in 2007, and it quickly became
the de-facto standard for KVM's para-virtualized I/O devices.

Virtio is very popular for virtual I/O devices because is provides a
straightforward, efficient, standard, and extensible mechanism, and
eliminates the need for boutique, per-environment, or per-OS mechanisms.
For example, rather than having a variety of device emulation
mechanisms, virtio provides a common frontend driver framework that
standardizes device interfaces, and increases code reuse across
different virtualization platforms.

Given the advantages of virtio, ACRN also follows the virtio
specification.

Key Concepts
============

To better understand virtio, especially its usage in ACRN, we'll
highlight several key virtio concepts important to ACRN:


Frontend virtio driver (FE)
  Virtio adopts a frontend-backend architecture that enables a simple but
  flexible framework for both frontend and backend virtio drivers. The FE
  driver merely needs to offer services configure the interface, pass messages,
  produce requests, and kick backend virtio driver. As a result, the FE
  driver is easy to implement and the performance overhead of emulating
  a device is eliminated.

Backend virtio driver (BE)
  Similar to FE driver, the BE driver, running either in user-land or
  kernel-land of the host OS, consumes requests from the FE driver and sends them
  to the host native device driver. Once the requests are done by the host
  native device driver, the BE driver notifies the FE driver that the
  request is complete.

  Note: to distinguish BE driver from host native device driver, the host
  native device driver is called "native driver" in this document.

Straightforward: virtio devices as standard devices on existing buses
  Instead of creating new device buses from scratch, virtio devices are
  built on existing buses. This gives a straightforward way for both FE
  and BE drivers to interact with each other. For example, FE driver could
  read/write registers of the device, and the virtual device could
  interrupt FE driver, on behalf of the BE driver, in case something of
  interest is happening.

  Currently virtio supports PCI/PCIe bus and MMIO bus. In ACRN, only
  PCI/PCIe bus is supported, and all the virtio devices share the same
  vendor ID 0x1AF4.

  Note: For MMIO, the "bus" is a little bit an overstatement since
  basically it is a few descriptors describing the devices.

Efficient: batching operation is encouraged
  Batching operation and deferred notification are important to achieve
  high-performance I/O, since notification between FE and BE driver
  usually involves an expensive exit of the guest. Therefore batching
  operating and notification suppression are highly encouraged if
  possible. This will give an efficient implementation for 
  performance-critical devices.

Standard: virtqueue
  All virtio devices share a standard ring buffer and descriptor
  mechanism, called a virtqueue, shown in :numref:`virtqueue`. A virtqueue is a
  queue of scatter-gather buffers. There are three important methods on
  virtqueues:

  - **add_buf** is for adding a request/response buffer in a virtqueue, 
  - **get_buf** is for getting a response/request in a virtqueue, and
  - **kick** is for notifying the other side for a virtqueue to consume buffers.

  The virtqueues are created in guest physical memory by the FE drivers.
  BE drivers only need to parse the virtqueue structures to obtain
  the requests and process them. How a virtqueue is organized is
  specific to the Guest OS. In the Linux implementation of virtio, the
  virtqueue is implemented as a ring buffer structure called vring.

  In ACRN, the virtqueue APIs can be leveraged directly so that users
  don't need to worry about the details of the virtqueue. (Refer to guest
  OS for more details about the virtqueue implementation.)

.. figure:: images/virtio-hld-image2.png
   :width: 900px
   :align: center
   :name: virtqueue

   Virtqueue

Extensible: feature bits
  A simple extensible feature negotiation mechanism exists for each
  virtual device and its driver. Each virtual device could claim its
  device specific features while the corresponding driver could respond to
  the device with the subset of features the driver understands. The
  feature mechanism enables forward and backward compatibility for the
  virtual device and driver.

Virtio Device Modes
  The virtio specification defines three modes of virtio devices:
  a legacy mode device, a transitional mode device, and a modern mode
  device. A legacy mode device is compliant to virtio specification
  version 0.95, a transitional mode device is compliant to both
  0.95 and 1.0 spec versions, and a modern mode
  device is only compatible to the version 1.0 specification.

  In ACRN, all the virtio devices are transitional devices, meaning that
  they should be compatible with both 0.95 and 1.0 versions of virtio
  specification.

Virtio Device Discovery
  Virtio devices are commonly implemented as PCI/PCIe devices. A
  virtio device using virtio over PCI/PCIe bus must expose an interface to
  the Guest OS that meets the PCI/PCIe specifications.

  Conventionally, any PCI device with Vendor ID 0x1AF4,
  PCI_VENDOR_ID_REDHAT_QUMRANET, and Device ID 0x1000 through 0x107F
  inclusive is a virtio device. Among the Device IDs, the
  legacy/transitional mode virtio devices occupy the first 64 IDs ranging
  from 0x1000 to 0x103F, while the range 0x1040-0x107F belongs to
  virtio modern devices. In addition, the Subsystem Vendor ID should
  reflect the PCI/PCIe vendor ID of the environment, and the Subsystem
  Device ID indicates which virtio device is supported by the device.

Virtio Frameworks
=================

This section describes the overall architecture of virtio, and then
introduce ACRN specific implementations of the virtio framework.

Architecture
------------

Virtio adopts a frontend-backend
architecture, as shown in :numref:`virtio-arch`. Basically the FE and BE driver
communicate with each other through shared memory, via the
virtqueues. The FE driver talks to the BE driver in the same way it
would talk to a real PCIe device. The BE driver handles requests
from the FE driver, and notifies the FE driver if the request has been
processed.

.. figure:: images/virtio-hld-image1.png
   :width: 900px
   :align: center
   :name: virtio-arch

   Virtio Architecture

In addition to virtio's frontend-backend architecture, both FE and BE
drivers follow a layered architecture, as shown in
:numref:`virtio-fe-be`. Each
side has three layers: transports, core models, and device types.
All virtio devices share the same virtio infrastructure, including
virtqueues, feature mechanisms, configuration space, and buses.

.. figure:: images/virtio-hld-image4.png
   :width: 900px
   :align: center
   :name: virtio-fe-be

   Virtio Frontend/Backend Layered Architecture

Virtio Framework Considerations
-------------------------------

How to realize the virtio framework is specific to a
hypervisor implementation. In ACRN, the virtio framework implementations
can be classified into two types, virtio backend service in user-land
(VBS-U) and virtio backend service in kernel-land (VBS-K), according to
where the virtio backend service (VBS) is located. Although different in BE
drivers, both VBS-U and VBS-K share the same FE drivers. The reason
behind the two virtio implementations is to meet the requirement of
supporting a large amount of diverse I/O devices in ACRN project.

When developing a virtio BE device driver, the device owner should choose
carefully between the VBS-U and VBS-K. Generally VBS-U targets
non-performance-critical devices, but enables easy development and
debugging. VBS-K targets performance critical devices.

The next two sections introduce ACRN's two implementations of the virtio
framework.

User-Land Virtio Framework
--------------------------

The architecture of ACRN user-land virtio framework (VBS-U) is shown in
:numref:`virtio-userland`.

The FE driver talks to the BE driver as if it were talking with a PCIe
device. This means for "control plane", the FE driver could poke device
registers through PIO or MMIO, and the device will interrupt the FE
driver when something happens. For "data plane", the communication
between the FE and BE driver is through shared memory, in the form of
virtqueues.

On the service OS side where the BE driver is located, there are several
key components in ACRN, including device model (DM), virtio and HV
service module (VHM), VBS-U, and user-level vring service API helpers.

DM bridges the FE driver and BE driver since each VBS-U module emulates
a PCIe virtio device. VHM bridges DM and the hypervisor by providing
remote memory map APIs and notification APIs. VBS-U accesses the
virtqueue through the user-level vring service API helpers.

.. figure:: images/virtio-hld-image3.png
   :width: 900px
   :align: center
   :name: virtio-userland

   ACRN User-Land Virtio Framework

Kernel-Land Virtio Framework
----------------------------

The architecture of ACRN kernel-land virtio framework (VBS-K) is shown
in :numref:`virtio-kernelland`.

VBS-K provides acceleration for performance critical devices emulated by
VBS-U modules by handling the "data plane" of the devices directly in
the kernel. When VBS-K is enabled for certain device, the kernel-land
vring service API helpers are used to access the virtqueues shared by
the FE driver. Compared to VBS-U, this eliminates the overhead of
copying data back-and-forth between user-land and kernel-land within the
service OS, but pays with the extra implementation complexity of the BE
drivers.

Except for the differences mentioned above, VBS-K still relies on VBS-U
for feature negotiations between FE and BE drivers. This means the
"control plane" of the virtio device still remains in VBS-U. When
feature negotiation is done, which is determined by FE driver setting up
an indicative flag, VBS-K module will be initialized by VBS-U, after
which all request handling will be offloaded to the VBS-K in kernel.

The FE driver is not aware of how the BE driver is implemented, either
in the VBS-U or VBS-K model. This saves engineering effort regarding FE
driver development.

.. figure:: images/virtio-hld-image6.png
   :width: 900px
   :align: center
   :name: virtio-kernelland

   ACRN Kernel-Land Virtio Framework

Virtio APIs
===========

This section provides details on the ACRN virtio APIs. As outlined previously,
the ACRN virtio APIs can be divided into three groups: DM_APIs,
VBS_APIs, and VQ_APIs. The following sections will elaborate on
these APIs.

VBS-U Key Data Structures
-------------------------

The key data structures for VBS-U are listed as following, and their
relationships are shown in :numref:`VBS-U-data`.

``struct pci_virtio_blk``
  An example virtio device, such as virtio-blk.
``struct virtio_common``
  A common component to any virtio device.
``struct virtio_ops``
  Virtio specific operation functions for this type of virtio device.
``struct pci_vdev``
  Instance of a virtual PCIe device, and any virtio
  device is a virtual PCIe device.
``struct pci_vdev_ops``
  PCIe device's operation functions for this type
  of device.
``struct vqueue_info``
  Instance of a virtqueue.

.. figure:: images/virtio-hld-image5.png
   :width: 900px
   :align: center
   :name: VBS-U-data

   VBS-U Key Data Structures

Each virtio device is a PCIe device. In addition, each virtio device
could have none or multiple virtqueues, depending on the device type.
The ``struct virtio_common`` is a key data structure to be manipulated by
DM, and DM finds other key data structures through it. The ``struct
virtio_ops`` abstracts a series of virtio callbacks to be provided by
device owner.

VBS-K Key Data Structures
-------------------------

The key data structures for VBS-K are listed as follows, and their
relationships are shown in :numref:`VBS-K-data`.

``struct vbs_k_rng``
  In-kernel VBS-K component handling data plane of a
  VBS-U virtio device, for example virtio random_num_generator.
``struct vbs_k_dev``
  In-kernel VBS-K component common to all VBS-K.
``struct vbs_k_vq``
  In-kernel VBS-K component to be working with kernel
  vring service API helpers.
``struct vbs_k_dev_inf``
  Virtio device information to be synchronized
  from VBS-U to VBS-K kernel module.
``struct vbs_k_vq_info``
  A single virtqueue information to be
  synchronized from VBS-U to VBS-K kernel module.
``struct vbs_k_vqs_info``
  Virtqueue(s) information, of a virtio device,
  to be synchronized from VBS-U to VBS-K kernel module.

.. figure:: images/virtio-hld-image8.png
   :width: 900px
   :align: center
   :name: VBS-K-data

   VBS-K Key Data Structures

In VBS-K, the struct vbs_k_xxx represents the in-kernel component
handling a virtio device's data plane. It presents a char device for VBS-U
to open and register device status after feature negotiation with the FE
driver.

The device status includes negotiated features, number of virtqueues,
interrupt information, and more. All these status will be synchronized
from VBS-U to VBS-K. In VBS-U, the ``struct vbs_k_dev_info`` and ``struct
vbs_k_vqs_info`` will collect all the information and notify VBS-K through
ioctls. In VBS-K, the ``struct vbs_k_dev`` and ``struct vbs_k_vq``, which are
common to all VBS-K modules, are the counterparts to preserve the
related information. The related information is necessary to kernel-land
vring service API helpers.

DM APIs
=======

The DM APIs are exported by DM, and they should be used when realizing
BE device drivers on ACRN.

[API Material from doxygen comments]

VBS APIs
========

The VBS APIs are exported by VBS related modules, including VBS, DM, and
SOS kernel modules. They can be classified into VBS-U and VBS-K APIs
listed as follows.

VBS-U APIs
----------

These APIs provided by VBS-U are callbacks to be registered to DM, and
the virtio framework within DM will invoke them appropriately.

[API Material from doxygen comments]

VBS-K APIs
----------

The VBS-K APIs are exported by VBS-K related modules. Users could use
the following APIs to implement their VBS-K modules.

APIs provided by DM
~~~~~~~~~~~~~~~~~~~

[API Material from doxygen comments]

APIs provided by VBS-K modules in service OS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

VQ APIs
-------

The virtqueue APIs, or VQ APIs, are used by a BE device driver to
access the virtqueues shared by the FE driver. The VQ APIs abstract the
details of virtqueues so that users don't need to worry about the data
structures within the virtqueues. In addition, the VQ APIs are designed
to be identical between VBS-U and VBS-K, so that users don't need to
learn different APIs when implementing BE drivers based on VBS-U and
VBS-K.

[API Material from doxygen comments]

Below is an example showing a typical logic of how a BE driver handles
requests from a FE driver.

.. code-block:: c

   static void BE_callback(struct pci_virtio_xxx *pv, struct vqueue_info *vq ) {
      while (vq_has_descs(vq)) {
         vq_getchain(vq, &idx, &iov, 1, NULL);
                /* handle requests in iov */
                request_handle_proc();
                /* Release this chain and handle more */
                vq_relchain(vq, idx, len);
         }
      /* Generate interrupt if appropriate. 1 means ring empty \*/
      vq_endchains(vq, 1);
   }

Current Virtio Devices
======================

This section introduces the status of the current virtio devices
supported in ACRN. All the BE virtio drivers are implemented using the
ACRN virtio APIs, and the FE drivers are reusing the standard Linux FE
virtio drivers. For the devices with FE drivers available in the Linux
kernel, they should use standard virtio Vendor ID/Device ID and
Subsystem Vendor ID/Subsystem Device ID. For other devices within ACRN,
their temporary IDs are listed in the following table.

.. table:: Virtio Devices without existing FE drivers in Linux
   :align: center
   :name: virtio-device-table

   +--------------+-------------+-------------+-------------+-------------+
   | virtio       | Vendor ID   | Device ID   | Subvendor   | Subdevice   |
   | device       |             |             | ID          | ID          |
   +--------------+-------------+-------------+-------------+-------------+
   | RPMB         | 0x8086      | 0x8601      | 0x8086      | 0xFFFF      |
   +--------------+-------------+-------------+-------------+-------------+
   | HECI         | 0x8086      | 0x8602      | 0x8086      | 0xFFFE      |
   +--------------+-------------+-------------+-------------+-------------+
   | audio        | 0x8086      | 0x8603      | 0x8086      | 0xFFFD      |
   +--------------+-------------+-------------+-------------+-------------+
   | IPU          | 0x8086      | 0x8604      | 0x8086      | 0xFFFC      |
   +--------------+-------------+-------------+-------------+-------------+
   | TSN/AVB      | 0x8086      | 0x8605      | 0x8086      | 0xFFFB      |
   +--------------+-------------+-------------+-------------+-------------+
   | hyper_dmabuf | 0x8086      | 0x8606      | 0x8086      | 0xFFFA      |
   +--------------+-------------+-------------+-------------+-------------+
   | HDCP         | 0x8086      | 0x8607      | 0x8086      | 0xFFF9      |
   +--------------+-------------+-------------+-------------+-------------+
   | COREU        | 0x8086      | 0x8608      | 0x8086      | 0xFFF8      |
   +--------------+-------------+-------------+-------------+-------------+

Virtio-rnd
==========

The virtio-rnd entropy device supplies high-quality randomness for guest
use. The virtio device ID of the virtio-rnd device is 4, and it supports
one virtqueue, the size of which is 64, configurable in the source code.
It has no feature bits defined.

When the FE driver requires some random bytes, the BE device will place
bytes of random data onto the virtqueue.

To launch the virtio-rnd device, use the following virtio command::

   -s <slot>,virtio-rnd

To verify the correctness in user OS, use the following
command::

   od /dev/random

Virtio-blk
==========

The virtio-blk device is a simple virtual block device. The FE driver
places read, write, and other requests onto the virtqueue, so that the
BE driver can process them accordingly.

The virtio device ID of the virtio-blk is 2, and it supports one
virtqueue, the size of which is 64, configurable in the source code. The
feature bits supported by the BE device are shown as follows:

VTBLK_F_SEG_MAX(bit 2)
  Maximum number of segments in a request is in seg_max.
VTBLK_F_BLK_SIZE(bit 6)
  block size of disk is in blk_size.
VTBLK_F_FLUSH(bit 9)
  cache flush command support.
VTBLK_F_TOPOLOGY(bit 10)
  device exports information on optimal I/O alignment.

To use the virtio-blk device, use the following virtio command::

   -s <slot>,virtio-blk,<filepath>

Successful booting of the User OS verifies the correctness of the
device.

Virtio-net
==========

The virtio-net device is a virtual Ethernet device. The virtio device ID
of the virtio-net is 1, and ACRN's virtio-net device supports twp
virtqueues, one for transmitting packets and the other for receiving
packets. The FE driver places empty buffers onto one virtqueue for
receiving packets, and enqueue outgoing packets onto another virtqueue
for transmission. Currently the size of each virtqueue is 1000,
configurable in the source code.

To access the external network from user OS, as shown in
:numref:`virtio-network`, a L2 virtual switch should be created in the
service OS, and the BE driver is bonded to a tap/tun device linking
under the L2 virtual switch.

.. figure:: images/virtio-hld-image7.png
   :width: 900px
   :align: center
   :name: virtio-network

   Virtio-net Accessing External Network

Currently the feature bits supported by the BE device are shown as
follows:

VIRTIO_NET_F_MAC(bit 5)
  device has given MAC address.
VIRTIO_NET_F_MRG_RXBUF(bit 15)
  BE driver can merge receive buffers.
VIRTIO_NET_F_STATUS(bit 16)
  configuration status field is available.
VIRTIO_F_NOTIFY_ON_EMPTY(bit 24)
  device will issue an interrupt if
  it runs out of available descriptors on a virtqueue.

To enable the virtio-net device, use the following virtio command::

   -s <slot>,virtio-net,<vale/tap/vmnet>

To verify the correctness of the device, access the external
network within the user OS.

Virtio-console
==============

The virtio-console device is a simple device for data input and output.
The virtio device ID of the virtio-console device is 3. A device could
have one or up to 16 ports in ACRN. Each port has a pair of input and
output virtqueues. A device has a pair of control virtqueues, which are
used to communicate information between the FE and BE drivers. Currently
the size of each virtqueue is 64, configurable in the source code.

Similar to virtio-net device, two virtqueues specific to a port are
transmitting virtqueue and receiving virtqueue. The FE driver places
empty buffers onto the receiving virtqueue for incoming data, and
enqueues outgoing characters onto transmitting virtqueue.

Currently the feature bits supported by the BE device are shown as
follows:

VTCON_F_SIZE(bit 0)
  configuration columns and rows are valid.
VTCON_F_MULTIPORT(bit 1)
  device supports multiple ports, and control
  virtqueues will be used.
VTCON_F_EMERG_WRITE(bit 2)
  device supports emergency write.

To use the virtio-console device, use the following virtio command::

   -s <slot>,virtio-console,[@]<stdio|tty|pty|sock>:<portname>[=portpath]

.. note::

   Here are some notes about the virtio-console device:

   - ``@`` : marks the port as a console port, otherwise it is a normal
     virtio serial port
   - stdio/tty/pty: tty capable, which means :kbd:`TAB` and :kbd:`BACKSPACE`
     are supported as in regular terminals
   - When tty are used, please make sure the redirected tty is sleep, e.g. by
     "sleep 2d" command, and will not read input from stdin before it is used
     by virtio-console to redirect guest output;
   - Claiming multiple virtio serial ports as consoles are supported, however
     the guest Linux will only use one of them, through "console=hvcN" kernel
     parameters, as the hvc.
