.. _virtio-rnd:

Virtio-rnd
##########

virtio-rnd provides a hardware random source for UOS. The
virtual random device is based on virtio user mode framework and
simulates a PCI device based on virtio specification.

:numref:`virtio-rnd-arch` shows the Random Device Virtualization
Architecture in ACRN.  virtio-rnd is implemented as a virtio legacy
device in the ACRN device model (DM), and is registered as a PCI virtio
device to the guest OS (UOS).

When the FE driver requires some random bytes, the BE device will place
bytes of random data onto the virtqueue.

Tools such as ``od`` can be used to read randomness from
``/dev/random``.  This device file in UOS is bound with frontend
virtio-rng driver (The guest kernel must be built with
``CONFIG_HW_RANDOM_VIRTIO=y``). The backend virtio-rnd reads the HW
randomness from ``/dev/random`` in SOS and sends them to frontend.

.. figure:: images/virtio-hld-image61.png
   :align: center
   :name: virtio-rnd-arch

   Virtio-rnd Architecture on ACRN

To launch the virtio-rnd device, use the following virtio command::

   -s <slot>,virtio-rnd

To verify the correctness in user OS, use the following
command::

   od /dev/random
