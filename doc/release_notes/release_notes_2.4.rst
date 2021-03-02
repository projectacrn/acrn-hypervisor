.. _release_notes_2.4:

ACRN v2.4 (Mar 2021) - DRAFT
############################

We are pleased to announce the release of the Project ACRN
hypervisor version 2.4.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline
embedded development through an open source platform. Check out the
:ref:`introduction` introduction for more information.  All project ACRN
source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, tools, and
documentation. You can either download this source code as a zip or
tar.gz file (see the `ACRN v2.4 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v2.4>`_) or
use Git clone and checkout commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v2.4

The project's online technical documentation is also tagged to
correspond with a specific release: generated v2.4 documents can be
found at https://projectacrn.github.io/2.4/.  Documentation for the
latest under-development branch is found at
https://projectacrn.github.io/latest/.

ACRN v2.4 requires Ubuntu 18.04.  Follow the instructions in the
:ref:`rt_industry_ubuntu_setup` to get started with ACRN.


What's New in v2.4
******************

ACRN Configuration and Build
============================

[Add a short description of the changes to the ACRN configuration and build
process that no longer uses Kconfig and relies on XML files, and refer them to
the updated building from source and configuration options documents]

Document Updates
****************

New and updated reference documents are available, including:

.. rst-class:: rst-columns2

* :ref:`APL_GVT-g-hld`
* :ref:`hld-devicemodel`
* :ref:`hld-trace-log`
* :ref:`hld-virtio-devices`
* :ref:`hv-cpu-virt`
* :ref:`IOC_virtualization_hld`
* :ref:`partition-mode-hld`
* :ref:`hv-vm-management`
* :ref:`vt-d-hld`
* :ref:`virtio-console`
* :ref:`virtio-i2c`
* :ref:`rt_industry_ubuntu_setup`
* :ref:`introduction`
* :ref:`scenario-config-options`
* :ref:`how-to-enable-acrn-secure-boot-with-grub`
* :ref:`acrn_configuration_tool`
* :ref:`acrn_doc`
* :ref:`gpu-passthrough`
* :ref:`rt_performance_tuning`
* :ref:`setup_openstack_libvirt`
* :ref:`using_windows_as_uos`
* :ref:`vuart_config`
* :ref:`acrn-dm_parameters`
* :ref:`kernel-parameters`

Because we dropped deprivileged boot mode support (in v2.3), we also
switched our Service VM of choice away from Clear Linux and have
removed Clear Linux-specific tutorials.  Deleted documents are still
available in the `version-specific v2.1 documentation
<https://projectacrn.github.io/v2.1/>`_.


Fixed Issues Details
********************

.. example - :acrn-issue:`4958` - clean up spin lock for hypervisor

Known Issues
************

