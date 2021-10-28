ACRN-Crashlog
#############

Introduction
************

``ACRN-Crashlog`` is a collective name for various tools (``acrnprobe``,
``usercrash_s``, ``usercrash_c``, ``debugger``, and more) and an overall
control utility called ``crashlogctl``. Together these tools collect logs
and information after each crash or event on an ACRN platform, including
the hypervisor, Service VM, and Android as a Guest (AaaG).
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

   cd misc/debug_tools/acrn_crashlog
   make

To remove all generated files and return the folder to its clean state,
use:

.. code-block:: none

   cd misc/debug_tools/acrn_crashlog
   make clean

Installing
**********

To install the build:

.. code-block:: none

   cd misc/debug_tools/acrn_crashlog
   sudo make install

Enabling/Disabling
******************

To enable this tool:

.. code-block:: none

   sudo crashlogctl enable

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

   sudo reboot

To disable this tool:

.. code-block:: none

   sudo crashlogctl disable

Then it will show:

.. code-block:: console

   Removed /etc/systemd/system/multi-user.target.wants/acrnprobe.service.
   Removed /etc/systemd/system/multi-user.target.wants/usercrash.service.
   removed '/etc/sysctl.d/80-coredump.conf'
   removed '/etc/systemd/system.conf.d/40-watchdog.conf'
   *** Please reboot your system. ***

Follow the hints to reboot the system:

.. code-block:: none

   sudo reboot

To check the status of this tool:

.. code-block:: none

   sudo crashlogctl is-active

It will show the status of the related services. Example:

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

   sudo debugger 12

Replace ``12`` with the process ID you want to dump.

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

- ``acrnprobe``: tool that gathers all the crash and event logs on the
  platform. For the hypervisor, the log is collected with ``acrnlog``. For the
  Service VM, the userspace crash log is collected with ``usercrash``, and the
  kernel crash log is collected with the inherent mechanism, such as ``ipanic``
  or ``pstore``. For an AaaG VM, the log is collected by monitoring the change
  of related folders on the Service VM image, such as ``/data/logs/``.
  ``acrnprobe`` also provides a flexible way to configure which crash or event
  to collect, by using an XML configuration file.
- ``common``: some utils for logs, command and string.
- ``data``: configuration file, service files and shell script.
- ``usercrash``: tool that gets the crash information for the
  crashing process in userspace.

Acrnprobe
=========

The ``acrnprobe`` tool detects all critical events on the platform and collects
specific information for debug purposes. The information is saved as
logs.
For more details on ``acrnprobe``, see :ref:`acrnprobe_doc`.

Usercrash
=========

The ``usercrash`` tool gets the crash information of the crashing process in
userspace. It works in a client/server model. The server is autostarted, and
the client is
configured in ``core_pattern`` or ``coredump-wrapper``, which will be
triggered once a crash occurs in userspace.
For more details on ``usercrash``, see :ref:`usercrash_doc`.
