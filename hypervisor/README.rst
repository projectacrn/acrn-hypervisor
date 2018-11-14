ACRN Hypervisor
###############

The open source `Project ACRN`_ defines a device hypervisor reference stack and
an architecture for running multiple software subsystems, managed securely, on
a consolidated system by means of a virtual machine manager. It also defines a
reference framework implementation for virtual device emulation, called the
“ACRN Device Model”.

The ACRN Hypervisor is a Type 1 reference hypervisor stack, running directly on
the bare-metal hardware, and is suitable for a variety of IoT and embedded
device solutions. The ACRN hypervisor addresses the gap that currently exists
between datacenter hypervisors, and hard partitioning hypervisors. The ACRN
hypervisor architecture partitions the system into different functional
domains, with carefully selected guest OS sharing optimizations for IoT and
embedded devices.

You can find out more about Project ACRN on the `Project ACRN documentation`_
website.

.. _`Project ACRN`: https://projectacrn.org
.. _`ACRN Hypervisor`: https://github.com/projectacrn/acrn-hypervisor
.. _`Project ACRN documentation`: https://projectacrn.github.io/
