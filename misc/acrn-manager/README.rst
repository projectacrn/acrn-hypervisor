.. _acrnctl:

acrnctl and acrnd
#################


Description
***********

The ``acrnctl`` tool helps users create, delete, launch, and stop a User
OS (UOS).  The tool runs under the Service OS, and UOSs should be based
on ``acrn-dm``. The daemon for acrn-manager is `acrnd`_.



Usage
=====

You can see the available ``acrnctl`` commands by running:

.. code-block:: none

   # acrnctl help
   support:
     list
     start
     stop [--force/-f]
     del
     add
     pause
     continue
     suspend
     resume
     reset
     blkrescan
   Use acrnctl [cmd] help for details

.. note::
   You must run ``acrnctl`` with root privileges.

Here are some usage examples:

Add a VM
========

The ``add`` command lets you add a VM by specifying a
script that will launch a UOS, for example ``launch_uos.sh``:

.. code-block:: none

   # acrnctl add launch_uos.sh -U 1
   vm1-14:59:30 added

If a ``-C`` option is also specified, the VM is launched in a runC
container::

   # acrnctl add launch_uos.sh -C

.. note:: You can download an `example launch_uos.sh script
   <https://raw.githubusercontent.com/projectacrn/acrnhypervisor/master/devicemodel/samples/nuc/launch_uos.sh>`_
   that supports the ``-C``  (``run_container`` function) option. You may refer to :ref:`acrn-dm_qos`
   for more details about this option.

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

Use the optional ``-f`` or ``--force`` argument to force the stop operation.
This will trigger an immediate shutdown of the User VM by the ACRN Device Model
and can be useful when the User VM is in a bad state and not shutting down
gracefully by itself.

.. code-block:: none

   # acrnctl stop -f vm-yocto

RESCAN BLOCK DEVICE
===================

Use the ``blkrescan`` command to trigger a rescan of
virtio-blk device by guest VM, in order to revalidate and
update the backend file.

.. code-block:: none

   # acrnctl blkrescan vmname slot,newfilepath
   vmname:     Name of VM with dummy backend file attached to virtio-blk device.
   slot:       Slot number of the virtio-blk device.
   newfilepath: File path for the backend of virtio-blk device.

   acrnctl blkrescan vm1 6,actual_file.img

.. note:: blkrescan is only supported when VM is launched with
   empty backend file (using **nodisk**) for virtio-blk device.
   Replacing a valid backend file is not supported and will
   result in error.

.. _acrnd:

acrnd
*****

The ``acrnd`` daemon process provides a way for launching or resuming a UOS
should the UOS shut down, either planned or unexpected. A UOS can ask ``acrnd``
to set up a timer to make sure the UOS is running, even if the SOS is
suspended or stopped.

Usage
=====

You can see the available ``acrnd`` commands by running:

.. code-block:: none

   $ acrnd -h
   acrnd - Daemon for ACRN VM Management
   [Usage] acrnd [-t] [-d delay] [-h]
   -t: print messages to stdout
   -d: delay the autostarting of VMs, <0-60> in second (not available in the
       ``RELEASE=1`` build)
   -h: print this message

Normally, ``acrnd`` runs silently (messages are directed to
``/dev/null``).  Use the ``-t`` option to direct messages to ``stdout``,
useful for debugging.

The ``acrnd`` daemon stores pending UOS work to ``/usr/share/acrn/conf/timer_list``
and sets an RTC timer to wake up the SOS or bring the SOS back up again.
When ``acrnd`` daemon is restarted, it restores the previously saved timer
list and launches the UOSs at the right time.

A ``systemd`` service file (``acrnd.service``) is installed by default that will
start the ``acrnd`` daemon when the Service OS comes up.
You can restart/stop acrnd service using ``systemctl``

.. note::
   You must run ``acrnd`` with root privileges.

Build and Install
*****************

Source code for both ``acrnctl`` and ``acrnd`` is in the ``tools/acrn-manager`` folder.
Change to that folder and run:

.. code-block:: none

   # make
   # make install
