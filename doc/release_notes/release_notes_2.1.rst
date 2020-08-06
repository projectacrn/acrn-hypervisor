.. _release_notes_2.1:

ACRN v2.1 (August 2020)
#######################

We are pleased to announce the release of the Project ACRN
hypervisor version 2.1.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline
embedded development through an open source platform. Check out
:ref:`introduction` introduction for more information.  All project ACRN
source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, tools, and
documentation. You can either download this source code as a zip or
tar.gz file (see the `ACRN v2.1 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v2.1>`_) or
use Git clone and checkout commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v2.1

The project's online technical documentation is also tagged to
correspond with a specific release: generated v2.1 documents can be
found at https://projectacrn.github.io/2.1/.  Documentation for the
latest under-development branch is found at
https://projectacrn.github.io/latest/.

ACRN v2.1 requires Ubuntu 18.04.  Follow the instructions in the
:ref:`rt_industry_ubuntu_setup` to get started with ACRN.

We recommend that all developers upgrade to ACRN release v2.1.

What's new in v2.1
******************

* Preempt-RT Linux has been validated as a pre-launched realtime VM.

* A Trusted Platform Module (TPM) MMIO device can be passthroughed to a
  pre-launched VM (with some limitations discussed in
  :ref:`mmio-device-passthrough`).  Previously passthrough was only
  supported for PCI devices.

* Open Virtual Machine Firmware (OVMF) now uses a Local Advanced
  Programmable Interrupt Controller (LAPIC) timer as its local time
  instead of the High Precision Event Timer (HPET).


Improvements, updates, and corrections have been made throughout our documentation,
including these:

* :ref:`contribute_guidelines`
* :ref:`hv_rdt`
* :ref:`ivshmem-hld`
* :ref:`mmio-device-passthrough`
* :ref:`virtio-net`
* :ref:`getting-started-building`
* :ref:`acrn_configuration_tool`
* :ref:`pre_launched_rt`
* :ref:`rdt_configuration`
* :ref:`using_hybrid_mode_on_nuc`
* :ref:`using_partition_mode_on_nuc`
* :ref:`using_windows_as_uos`
* :ref:`debian_packaging`

Fixed Issues Details
********************

.. comment Issues should be carefully formatted like this
   - :acrn-issue:`3715` -  Add support for multiple RDT resource allocation and fix L3 CAT config overwrite by L2

Known Issues
************
