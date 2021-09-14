.. _release_notes_2.6:

ACRN v2.6 (Sep 2021) - Draft
############################

We are pleased to announce the release of the Project ACRN hypervisor
version 2.6.

ACRN is a flexible, lightweight reference hypervisor that is built with
real-time and safety-criticality in mind. It is optimized to streamline
embedded development through an open-source platform. See the
:ref:`introduction` introduction for more information.

All project ACRN source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, tools, and
documentation. You can download this source code either as a zip or
tar.gz file (see the `ACRN v2.6 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v2.6>`_) or
use Git ``clone`` and ``checkout`` commands::

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v2.6

The project's online technical documentation is also tagged to
correspond with a specific release: generated v2.6 documents can be
found at https://projectacrn.github.io/2.6/.  Documentation for the
latest development branch is found at https://projectacrn.github.io/latest/.

ACRN v2.6 requires Ubuntu 18.04.  Follow the instructions in the
:ref:`gsg` to get started with ACRN.


What's New in v2.6
******************

Topic
  Description


Upgrading to v2.6 From Previous Releases
****************************************

We highly recommended that you follow these instructions to
upgrade to v2.6 from previous ACRN releases.


Document Updates
****************

We've made major improvements to the introductory ACRN documentation including:

* :ref:`introduction`
* :ref:`overview_dev`
* :ref:`gsg`
* :ref:`acrn_configuration_tool`

We’ve also made edits throughout the documentation to improve clarity,
formatting, and presentation:

.. rst-class:: rst-columns2

* :ref:`hld-devicemodel`
* :ref:`hld-overview`
* :ref:`hld-power-management`
* :ref:`hld-virtio-devices`
* :ref:`hld-io-emulation`
* :ref:`virtio-net`
* :ref:`acrn_on_qemu`
* :ref:`cpu_sharing`
* :ref:`enable-ptm`
* :ref:`nested_virt`
* :ref:`setup_openstack_libvirt`
* :ref:`using_hybrid_mode_on_nuc`
* :ref:`acrn_doc`

Fixed Issues Details
********************

.. comment example item
   - :acrn-issue:`5626` - [CFL][industry] Host Call Trace once detected


Known Issues
************

