.. _acrnctl:

acrnctl and acrnd
#################


Description
***********

The ``acrnctl`` tool helps users create, delete, launch, and stop a User
OS (UOS).  The tool runs under the Service OS, and UOSs should be based
on ``acrn-dm``. The daemon for acrn-manager is `acrnd`_.



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
     continue
     suspend
     resume
     reset
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

.. _acrnd:

acrnd
*****

The ``acrnd`` daemon process provides a way for launching or resuming a UOS
should the UOS shut down, either planned or unexpected. A UOS can ask ``acrnd``
to set up a timer to make sure the UOS is running, even if the SOS is
suspended or stopped.
The ``acrnd`` daemon stores pending UOS work to ``/usr/share/acrn/conf/timer_list``
and sets an RTC timer to wake up the SOS or bring the SOS back up again.
When ``acrnd`` daemon is restarted, it restores the previously saved timer
list and launches the UOSs at the right time.

A ``systemd`` service file (``acrnd.service``) is installed by default that will
start the ``acrnd`` daemon when the Service OS comes up.
You can restart/stop acrnd service using ``systemctl``

Build and Install
*****************

Source code for both ``acrnctl`` and ``acrnd`` is in the ``tools/acrn-manager`` folder.
Change to that folder and run:

.. code-block:: none

   # make
   # make install
