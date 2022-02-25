.. _trusty_tee:

Trusty TEE
##########

Introduction
************

`Trusty`_ is a set of software components supporting a Trusted Execution
Environment (TEE). TEE is commonly known as an isolated processing environment
in which applications can be securely executed irrespective of the rest of the
system. For more information about TEE, visit the
`Trusted Execution Environment wiki page <https://en.wikipedia.org/wiki/Trusted_execution_environment>`_.
Trusty consists of:

1. An operating system (the Trusty OS) that runs on a processor intended to
   provide a TEE
#. Drivers for the Android kernel (Linux) to facilitate communication with
   applications running under the Trusty OS
#. A set of libraries for Android/Linux systems software to facilitate
   communication with trusted applications executed within the Trusty OS using
   the kernel drivers

LK (`Little Kernel`_) is a tiny operating system for small embedded
devices, bootloaders, and other environments that need OS primitives such as
threads, mutexes, and timers.  LK has been chosen as the Trusty OS kernel.

Trusty Architecture
*******************

.. figure:: images/trusty-arch.png
   :align: center
   :width: 800px
   :name: trusty-architectural-diagram

   Trusty Architectural Diagram

.. note::
   The Trusty OS is running in the Secure World in the architecture drawing
   above.

.. _trusty-hypercalls:

Trusty Specific Hypercalls
**************************

The following :ref:`hypercall_apis` are related to Trusty.

.. doxygengroup:: trusty_hypercall
   :project: Project ACRN
   :content-only:

Trusty Boot Flow
****************

By design, the User VM OS bootloader will trigger the Trusty
boot process. The complete boot flow is illustrated below.

.. graphviz:: images/trusty-boot-flow.dot
   :name: trusty-boot-flow
   :align: center
   :caption: Trusty Boot Flow

As shown in the above figure, here are some details about the Trusty
boot flow processing:

1. User VM OS bootloader

   a. Load and verify Trusty image from virtual disk
   #. Allocate runtime memory for Trusty
   #. Do ELF relocation of Trusty image and get entry address
   #. Call ``hcall_initialize_trusty`` with Trusty memory base and
      entry address
#. ACRN (``hcall_initialize_trusty``)

   a. Save World context for Normal World
   #. Init World context for Secure World (RIP, RSP, EPT, etc.)
   #. Resume to Secure World
#. Trusty

   a. Booting
   #. Call ``hcall_world_switch`` to switch back to Normal World if
      boot completed
#. ACRN (``hcall_world_switch``)

   a. Save World context for the World that caused this ``vmexit``
      (Secure World)
   #. Restore World context for next World (Normal World: User VM OS bootloader)
   #. Resume to next World (User VM OS bootloader)
#. User VM OS bootloader

   a. Continue to boot

EPT Hierarchy
*************

As per the Trusty design, Trusty can access the Normal World's memory, but the
Normal World cannot access the Secure World's memory.  The Secure
World EPTP page table hierarchy must contain the Normal World GPA address space,
while the Trusty world's GPA address space must be removed from the Normal World
EPTP page table hierarchy.

Design
======

Put the Secure World's GPA to a very high position:  511 GB - 512 GB.  The
PML4/PDPT for the Trusty World are separated from the Normal World.  PD and PT
for low memory
(< 511 GB) are shared in both the Trusty World's EPT and the Normal World's EPT.
PD and PT for high memory (>= 511 GB) are valid for the Trusty World's EPT only.

Benefit
=======

The Normal World's EPT can be modified during runtime. Examples include
increasing memory and changing attributes. If such behavior happens, only PD and
PT for the Normal World need to be updated.

.. figure:: images/ept-hierarchy.png
   :align: center
   :width: 800px
   :name: ept-hierarchy

   EPT Hierarchy

API
===

.. doxygengroup:: trusty_apis
   :project: Project ACRN
   :content-only:

.. _Trusty: https://source.android.com/security/trusty/
.. _Little Kernel: https://github.com/littlekernel/lk
