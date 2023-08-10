.. _multi-arch-support:

Hypervisor Multi-Architecture and RISC-V Support
################################################

.. note:: This is a preliminary draft of a planned and as yet unreleased effort
   to port the ACRN Hypervisor to non-Intel architectures.

From its first release in July 2018, the ACRN Hypervisor was designed for and
targeted to Intel platforms and relied on Intel Virtualization Technology (Intel
VT). From that base, we're expanding support to enable the ACRN hypervisor to
RISC-V64 architecture with a Hypervisor Extension.

RISC-V Support
**************

Adding multi-architecture support begins by refining the current architecture
abstraction layer and defining architecture-neutral APIs covering the management
of cores, caches, memory, interrupts, timers, and hardware virtualization
facilities.  Then an implementation of those APIs for RISC-V will be introduced.

Based on its wide availability and flexibility, QEMU is the first RISC-V
(virtual) platform this project targets. Real platforms may be selected later
based on business and community interests.

Current State
=============

This project is currently under development and is not yet ready for production.
Once this support is implemented and has sufficient quality, this port will
become a part of the upstream ACRN project and we'll continue development there
and encourage contributions by the ACRN community.

License
=======

This project will be released under the BSD-3-Clause license, the same as the
rest of project ACRN.
