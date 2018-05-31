.. _acrnctl:

acrnctl
#######


Description
***********

The ``acrnctl`` tool helps users create, delete, launch, and stop a User
OS (UOS).  The tool runs under the Service OS, and UOSs should be based
on ``acrn-dm``.



Usage
*****

You can see the available ``acrnctl`` commands by running:

.. code-block:: none

   # acrnctl help
   support:
     list
     start
     stop
     del
     add
     pause
   Use acrnctl [cmd] help for details

Here are some usage examples:

Add a VM
========

The ``add`` command lets you add a VM by specifying a
script that will launch a UOS, for example ``launch_UOS.sh``:

.. code-block:: none

   # acrnctl add launch_UOS.sh -U 1
   vm1-14:59:30 added

Note that the launch script must only launch one UOS instance.
The VM name is important. ``acrnctl`` searches VMs by their
names so duplicate VM names are not allowed. If the
launch script changes the VM name at launch time, ``acrnctl``
will not recognize it.

Delete VMs
==========

Use the ``delete`` command with a VM name to delete that VM:

.. code-block:: none

   # acrnctl del vm1-14:59:30

List VMs
========

Use the ``list`` command to display VMs and their state:

.. code-block:: none

   # acrnctl list
   vm1-14:59:30            untracked
   vm-yocto                stopped
   vm-android              stopped

Start VM
========

If a VM is in a ``stopped`` state, you can start it with the ``start``
command:

.. code-block:: none

   # acrnctl start vm-yocto

Stop VM
=======

Use the ``stop`` command to stop one or more running VM:

.. code-block:: none

   # acrnctl stop vm-yocto vm1-14:59:30 vm-android

Build and Install
*****************

Source code for ``acrnctl`` is in the ``tools/acrn-manager`` folder.
Change to that folder and run:

.. code-block:: none

   # make
   # make install
