.. _introduction:

Introducing Project ACRN
########################

The Project ACRN Embedded Hypervisor is a flexible and lighweight bare
metal hypervisor, built with real-time, functional safety, and security
in mind.  It streamlines embedded development through a scalable open
source reference platform that addresses embedded developers' needs.

This open source embedded hypervisor defines a software architecture for
running multiple software subsystems managed securely on a consolidated
system (by means of a virtual machine manager), and defines a reference
framework Device Model implementation for devices emulation.

This embedded hypervisor is type-1 reference hypervisor, running
directly on the system hardware. It can be used for building software
defined cockpit (SDC) or In-Vehicle Experience (IVE) solutions running
on Intel Architecture Apollo Lake platforms. As a reference
implementation, it provides the basis for embedded hypervisor vendors to
build solutions with an open source reference I/O mediation solution,
and provides auto makers a reference software stack for SDC usage.

This embedded hypervisor is a partitioning hypervisor reference stack,
also suitable for non-automotive IoT & embedded device solutions. It
will be addressing the gap that currently exists between datacenter
hypervisors, hard partitioning hypervisors, and select industrial
applications.  Extending the scope of this open source embedded
hypervisor relies on the involvement of community developers like you!

This embedded hypervisor is able to support both Linux* and Android* as
a Guest OS, managed by the hypervisor, where applications can run.
