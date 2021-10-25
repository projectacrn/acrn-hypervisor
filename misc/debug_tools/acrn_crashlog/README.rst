ACRN-Crashlog
#############

Introduction
************

``ACRN-Crashlog`` is a collective name for various tools (``acrnprobe``,
``usercrash_s``, ``usercrash_c``, ``debugger``, and more) and a overall
control utility called ``crashlogctl``. Together these tools collect logs
and information after each crash or event on an ACRN platform, including
the hypervisor, Service OS (SOS), and Android as a Guest (AaaG).
``ACRN-Crashlog`` provides a flexible way to configure which events are
of interest, by using an XML configuration file.

Building
********

Build Dependencies
==================

The ``ACRN-Crashlog`` tool depends on the following libraries
(build and runtime):

- libevent
- OpenSSL
- libxml2
- systemd
- libblkid
- e2fsprogs

Refer to the :ref:`getting_started` for instructions on how to set up your
build environment, and follow the instructions below to build and configure the
``ACRN-Crashlog`` tool.

Build
=====

To build the ``ACRN-Crashlog``, run:

.. code-block:: none

   $ cd acrn-crashlog
   $ make

To remove all generated files and return the folder to its clean state,
use:

.. code-block:: none

   $ cd acrn-crashlog
   $ make clean

Installing
**********

To install the build:

.. code-block:: none

   $ cd acrn-crashlog
   $ sudo make install

Enabling/Disabling
******************

To enable this tool:

.. code-block:: none

   $ sudo crashlogctl enable

Then it will show:

.. code-block:: console

   ... Backup core pattern to /var/log/crashlog/default_core_pattern
   '/usr/share/acrn/crashlog/40-watchdog.conf' ->
   '/etc/systemd/system.conf.d/40-watchdog.conf'
   '/usr/share/acrn/crashlog/80-coredump.conf' ->
   '/etc/sysctl.d/80-coredump.conf'
   Created symlink /etc/systemd/system/multi-user.target.wants/acrnprobe.service -> /usr/lib/systemd/system/acrnprobe.service.
   Created symlink /etc/systemd/system/multi-user.target.wants/usercrash.service -> /usr/lib/systemd/system/usercrash.service.
   *** Please reboot your system. ***

Follow the hints to reboot the system:

.. code-block:: none

   $ sudo reboot

To disable this tool:

.. code-block:: none

   $ sudo crashlogctl disable

Then it will show:

.. code-block:: console

   Removed /etc/systemd/system/multi-user.target.wants/acrnprobe.service.
   Removed /etc/systemd/system/multi-user.target.wants/usercrash.service.
   removed '/etc/sysctl.d/80-coredump.conf'
   removed '/etc/systemd/system.conf.d/40-watchdog.conf'
   *** Please reboot your system. ***

Follow the hints to reboot the system:

.. code-block:: none

   $ sudo reboot

To check the status of this tool:

.. code-block:: none

   $ sudo crashlogctl is-active

It will show the status of the related services like:

.. code-block:: console

   acrnprobe  : inactive
   usercrash  : inactive

Usage
*****

The ``acrnprobe`` tool provides ``history_event`` (under
``/var/log/crashlog/history_event``) to record ACRN-related events and
crash information.

``ACRN-Crashlog`` also provides a tool called ``debugger`` to dump specific
process information:

.. code-block:: none

   $ debugger <pid>

.. note::

   You need to be ``root`` to use the ``debugger``.

Source Code
***********

The source code structure:

.. code-block:: none

   acrn-crashlog/
   ├── acrnprobe
   │   └── include
   ├── common
   │   └── include
   ├── data
   └── usercrash
       └── include

- ``acrnprobe``: to gather all the crash and event logs on the platform, and
  probe on telemetrics-client. For the logs on hypervisor, it's collected with
  acrnlog. For the log on SOS, the userspace crash log is collected with
  usercrash, and the kernel crash log is collected with the inherent mechanism
  like ``ipanic``, ``pstore`` and etc. For the log on AaaG, it's collected with
  monitoring the change of related folders on the Service VM OS image, like
  ``/data/logs/``. ``acrnprobe`` also provides a flexible way to allow users to
  configure which crash or event they want to collect through the XML file
  easily.
- ``common``: some utils for logs, command and string.
- ``data``: configuration file, service files and shell script.
- ``usercrash``: to implement the tool which get the crash information for the
  crashing process in userspace.

Acrnprobe
=========

The ``acrnprobe`` detects all critical events on the platform and collects
specific information for debug purpose. These information would be saved as
logs, and the log path would be delivered to telemetrics-client as a record if
the telemetrics-client existed on the system.
For more detail on acrnprobe, please refer :ref:`acrnprobe_doc`.

Usercrash
=========

The ``usercrash`` is a tool to get the crash info of the crashing process in
userspace. It works in Client/Server model. Server is autostarted, and client is
configured in ``core_pattern`` or ``coredump-wrapper``, which will be
triggered once crash occurs in userspace.
For more detail on ``usercrash``, please refer :ref:`usercrash_doc`.
