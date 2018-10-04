.. _virtio-rnd:

Virtio-rnd
##########

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
