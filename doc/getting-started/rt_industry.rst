.. _rt_industry_setup:

Getting Started Guide for ACRN Industry Scenario
################################################

Verified version
****************

- Clear Linux version: **31080**
- ACRN-hypervisor tag: **v1.3**
- ACRN-Kernel(Service VM kernel): **acrn-2019w39.1-140000p**
- ACRN-Kernel(Preempt-RT kernel): **acrn-2019w39.1-143000p**

Prerequisites
*************

The example below is based on the Intel Kaby Lake NUC platform with two
disks, a SATA disk for the Clear Linux-based Service VM and an NVMe disk
for the RTVM.

- Intel Kaby Lake (aka KBL) NUC platform with two disks inside
  (refer to :ref:`the tables <hardware_setup>` for detailed information).
- Clear Linux OS (Ver: 31080) installation onto both disks on the KBL NUC.

.. _installation guide:
   https://docs.01.org/clearlinux/latest/get-started/bare-metal-install-server.html

.. note:: Follow the `installation guide`_ to install a Clear Linux OS.

.. _hardware_setup:

Hardware Setup
==============

.. table:: Hardware Setup
   :widths: auto
   :name: Hardware Setup

   +----------------------+-------------------+----------------------+-----------------------------------------------------------+
   | Platform (Intel x86) | Product/kit name  | Hardware             | Descriptions                                              |
   +======================+===================+======================+===========================================================+
   | Kaby Lake            | NUC7i7DNH         | Processor            | - Intel |reg| Core |trade| i7-8650U CPU @ 1.90GHz         |
   |                      |                   +----------------------+-----------------------------------------------------------+
   |                      |                   | Graphics             | - UHD Graphics 620                                        |
   |                      |                   |                      | - Two HDMI 2.0a ports supporting 4K at 60 Hz              |
   |                      |                   +----------------------+-----------------------------------------------------------+
   |                      |                   | System memory        | - 8GiB SODIMM DDR4 2400 MHz                               |
   |                      |                   +----------------------+-----------------------------------------------------------+
   |                      |                   | Storage capabilities | - SATA: 1TB WDC WD10SPZX-22Z                              |
   |                      |                   |                      | - NVMe: 256G Intel Corporation SSD Pro 7600p/760p/E 6100p |
   +----------------------+-------------------+----------------------+-----------------------------------------------------------+

Set up the ACRN Hypervisor for industry scenario
************************************************

The ACRN industry scenario environment can be set up in several ways. The
two listed below are recommended:

- :ref:`Using the pre-installed industry ACRN hypervisor <use pre-installed industry efi>`
- :ref:`Using the ACRN industry out-of-the-box image <use industry ootb image>`

.. _use pre-installed industry efi:

Use the pre-installed industry ACRN hypervisor
==============================================

.. note:: Skip this section if you choose :ref:`Using the ACRN industry out-of-the-box image <use industry ootb image>`.

Follow :ref:`ACRN quick setup guide <quick-setup-guide>` to set up the
ACRN Service VM. The industry hypervisor image is installed in the ``/usr/lib/acrn/``
directory once the Service VM boots. Follow the steps below to use
``acrn.kbl-nuc-i7.industry.efi`` instead of the original SDC hypervisor:

.. code-block:: none

   $ sudo mount /dev/sda1 /mnt
   $ sudo mv /mnt/EFI/acrn/acrn.efi /mnt/EFI/acrn/acrn.efi.bak
   $ sudo cp /usr/lib/acrn/acrn.kbl-nuc-i7.industry.efi /mnt/EFI/acrn/acrn.efi
   $ sync && umount /mnt
   $ sudo reboot

.. _use industry ootb image:

Use the ACRN industry out-of-the-box image
==========================================

#. Download the
   `sos-industry-31080.img.xz <https://github.com/projectacrn/acrn-hypervisor/releases/download/acrn-2019w39.1-140000p/sos-industry-31080.img.xz>`_
   to your development machine.

#. Decompress the xz image:

   .. code-block:: none

      $ xz -d sos-industry-31080.img.xz

#. Follow the instructions at :ref:`Deploy the Service VM image <deploy_ootb_service_vm>`
   to deploy the Service VM image on the SATA disk.

Install and launch the Preempt-RT VM
************************************

#. Download
   `preempt-rt-31080.img.xz <`https://github.com/projectacrn/acrn-hypervisor/releases/download/acrn-2019w39.1-140000p/preempt-rt-31080.img.xz>`_ to your development machine.

#. Decompress the xz image:

   .. code-block:: none

      $ xz -d preempt-rt-31080.img.xz

#. Follow the instructions at :ref:`Deploy the User VM Preempt-RT image <deploy_ootb_rtvm>`
   to deploy the Preempt-RT vm image on the NVMe disk.

#. Upon deployment completion, launch the RTVM directly on your KBL NUC::

   $ sudo /usr/share/acrn/samples/nuc/launch_hard_rt_vm.sh

.. note:: Use the ``lspci`` command to ensure that the correct NMVe device IDs will be used for the passthru before launching the script::

      $ sudo lspci -v | grep -iE 'nvm|ssd' 02:00.0 Non-Volatile memory controller: Intel Corporation Device f1a6 (rev 03) (prog-if 02 [NVM Express])
      $ sudo lspci -nn | grep "Non-Volatile memory controller" 02:00.0 Non-Volatile memory controller [0108]: Intel Corporation Device [8086:f1a6] (rev 03)


RT Performance Test
*******************

.. _cyclictest:

Cyclictest introduction
=======================

The cyclictest is most commonly used for benchmarking RT systems. It is one of the
most frequently used tools for evaluating the relative performance of real-time
systems. Cyclictest accurately and repeatedly measures the difference between a
thread's intended wake-up time and the time at which it actually wakes up in order
to provide statistics about the system's latencies. It can measure latencies in
real-time systems that are caused by hardware, firmware, and the operating system.
The cyclictest is currently maintained by Linux Foundation and is part of the test
suite rt-tests.

Pre-Configurations
==================

Recommended BIOS settings
-------------------------

.. csv-table::
   :widths: 15, 30, 10

   "Hyper-Threading", "Intel Advanced Menu -> CPU Configuration", "Disabled"
   "Intel VMX", "Intel Advanced Menu -> CPU Configuration", "Enable"
   "Speed Step", "Intel Advanced Menu -> Power & Performance -> CPU - Power Management Control", "Disabled"
   "Speed Shift", "Intel Advanced Menu -> Power & Performance -> CPU - Power Management Control", "Disabled"
   "C States", "Intel Advanced Menu -> Power & Performance -> CPU - Power Management Control", "Disabled"
   "RC6", "Intel Advanced Menu -> Power & Performance -> GT - Power Management", "Disabled"
   "GT freq", "Intel Advanced Menu -> Power & Performance -> GT - Power Management", "Lowest"
   "SA GV", "Intel Advanced Menu -> Memory Configuration", "Fixed High"
   "VT-d", "Intel Advanced Menu -> System Agent Configuration", "Enable"
   "Gfx Low Power Mode", "Intel Advanced Menu -> System Agent Configuration -> Graphics Configuration", "Disabled"
   "DMI spine clock gating", "Intel Advanced Menu -> System Agent Configuration -> DMI/OPI Configuration", "Disabled"
   "PCH Cross Throttling", "Intel Advanced Menu -> PCH-IO Configuration", "Disabled"
   "Legacy IO Low Latency", "Intel Advanced Menu -> PCH-IO Configuration -> PCI Express Configuration", "Enabled"
   "PCI Express Clock Gating", "Intel Advanced Menu -> PCH-IO Configuration -> PCI Express Configuration", "Disabled"
   "Delay Enable DMI ASPM", "Intel Advanced Menu -> PCH-IO Configuration -> PCI Express Configuration", "Disabled"
   "DMI Link ASPM", "Intel Advanced Menu -> PCH-IO Configuration -> PCI Express Configuration", "Disabled"
   "Aggressive LPM Support", "Intel Advanced Menu -> PCH-IO Configuration -> SATA And RST Configuration", "Disabled"
   "USB Periodic Smi", "Intel Advanced Menu -> LEGACY USB Configuration", "Disabled"
   "ACPI S3 Support", "Intel Advanced Menu -> ACPI Settings", "Disabled"
   "Native ASPM", "Intel Advanced Menu -> ACPI Settings", "Disabled"

.. note:: The BIOS settings depend on the platform and BIOS version; some may not be applicable.

Configure CAT
-------------

With the ACRN Hypervisor shell, we can use ``cpuid`` and ``wrmsr``/``rdmsr`` debug
commands to enumerate the CAT capability and set the CAT configuration without rebuilding binaries.
Because ``lapic`` is a pass-through to the RTVM, the CAT configuration must be
set before launching the RTVM.

Check CAT ability with cupid
````````````````````````````

First run ``cpuid 0x10 0x0``. The return value of ``ebx[bit 2]`` reports that the L2 CAT is supported.
Next, run ``cpuid 0x10 0x2`` to query the L2 CAT capability; the return value of ``eax[bit 4:0]``
reports that the cache mask has 8 bits, and ``edx[bit 15:0]`` reports that 04 CLOS are supported,
as shown below. The reported data is in the format of ``[ eax:ebx:ecx:edx ]``::

   ACRN:\>cpuid 0x10 0x0
   cpuid leaf: 0x10, subleaf: 0x0, 0x0:0x4:0x0:0x0

   ACRN:\>cpuid 0x10 0x2
   cpuid leaf: 0x10, subleaf: 0x2, 0x7:0x0:0x0:0x3

Set CLOS (QOS MASK) and PQR_ASSOC MSRs to configure the CAT
```````````````````````````````````````````````````````````

Apollo Lake doesn't have L3 cache and it supports L2 CAT. The CLOS MSRs are per L2 cache and starts from 0x00000D10. In the case of 4 CLOS MSRs, the address is as follows::

   MSR_IA32_L2_QOS_MASK_0    0x00000D10
   MSR_IA32_L2_QOS_MASK_1    0x00000D11
   MSR_IA32_L2_QOS_MASK_2    0x00000D12
   MSR_IA32_L2_QOS_MASK_3    0x00000D13

The PQR_ASSOC MSR is per CPU core; each core has its own PQR_ASSOC::

   MSR_IA32_PQR_ASSOC        0x00000C8F

To set the CAT, first set the CLOS MSRs. Next, set the PQR_ASSOC of each CPU
so that the CPU of the RTVM uses dedicated cache and other CPUs use other cache.
Taking a Quad Core Apollo Lake platform for example, CPU0 and CPU1 share L2 cache while CPU2 and CPU3 share the other L2 cache.

- If we allocate CPU2 and CPU3, no extra action is required.
- If we allocate only CPU1 to the RTVM, we need to set the CAT as follows.
  These commands actually set the CAT configuration for L2 cache shared by CPU0 and CPU1.

a. Set CLOS with ``wrmsr <reg_num> <value>``, we want VM1 to use the lower 6 ways of cache,
   so CLOS0 is set to 0xf0 for the upper 4 ways, and CLOS1 is set to 0x0f for the lower 4 ways::

      ACRN:\>wrmsr -p1 0xd10 0xf0
      ACRN:\>wrmsr -p1 0xd11 0x0f

#. Attach COS1 to PCPU1. Because MSR is IA32_PQR_ASSOC [bit 63:32], we’ll write
   0x100000000 to it to use CLOS1::

      ACRN:\>wrmsr -p0 0xc8f 0x000000000
      ACRN:\>wrmsr -p1 0xc8f 0x100000000

In addition to setting the CAT configuration via HV commands, we allow developers to add
the CAT configurations to the VM config and do the configure automatically at the
time of RTVM creation. Refer to the :ref:`configure_cat_vm` for details.

Set up the core allocation for the RTVM
---------------------------------------

In our recommended configuration, two cores are allocated to the RTVM:
core 0 for housekeeping and core 1 for RT tasks. In order to achieve
this, follow the below steps to allocate all housekeeping tasks to core 0:

.. code-block:: bash

   #!/bin/bash
   # Move all IRQs to core 0.
   for i in `cat /proc/interrupts | grep '^ *[0-9]*[0-9]:' | awk {'print $1'} | sed 's/:$//' `;
   do
       echo setting $i to affine for core zero
       echo 1 > /proc/irq/$i/smp_affinity
   done

   # Move all rcu tasks to core 0.
   for i in `pgrep rcu`; do taskset -pc 0 $i; done

   # Change realtime attribute of all rcu tasks to SCHED_OTHER and priority 0
   for i in `pgrep rcu`; do chrt -v -o -p 0 $i; done

   # Change realtime attribute of all tasks on core 1 to SCHED_OTHER and priority 0
   for i in `pgrep /1`; do chrt -v -o -p 0 $i; done

   # Change realtime attribute of all tasks to SCHED_OTHER and priority 0
   for i in `ps -A -o pid`; do chrt -v -o -p 0 $i; done

   echo disabling timer migration
   echo 0 > /proc/sys/kernel/timer_migration

Run cyclictest
==============

Use the following command to start cyclictest::

   $ cyclictest -a 1 -p 80 -m -N -D 1h -q -H 30000 --histfile=test.log

- Usage:

    :-a 1:                           to bind the RT task to core 1
    :-p 80:                          to set the priority of the highest prio thread
    :-N:                             print results in ns instead of us (default us)
    :-D 1h:                          to run for 1 hour, you can change it to other values
    :-q:                             quiee mode; print a summary only on exit
    :-H 30000 --histfile=test.log:   dump the latency histogram to a local file
