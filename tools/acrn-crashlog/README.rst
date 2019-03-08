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

Build dependencies
==================

The ``ACRN-Crashlog`` tool depends on the following libraries
(build and runtime):

- libevent
- OpenSSL
- libxml2
- systemd
- telemetrics-client-dev (optional, detected at build time)
- libblkid
- e2fsprogs

Refer to the :ref:`getting_started` for instructions on how to set-up your
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
   '/usr/share/defaults/telemetrics/telemetrics.conf' ->
   '/etc/telemetrics/telemetrics.conf'
   ... Set server_delivery_enabled=false in /etc/telemetrics/telemetrics.conf
   ... Set record_retention_enabled=true in /etc/telemetrics/telemetrics.conf
   '/usr/share/acrn/crashlog/40-watchdog.conf' ->
   '/etc/systemd/system.conf.d/40-watchdog.conf'
   '/usr/share/acrn/crashlog/80-coredump.conf' ->
   '/etc/sysctl.d/80-coredump.conf'
   Created symlink /etc/systemd/system/hprobe.timer -> /dev/null.
   Created symlink /etc/systemd/system/telemd-update-trigger.service -> /dev/null.
   Created symlink /etc/systemd/system/pstore-clean.service -> /dev/null.
   Created symlink /etc/systemd/system/pstore-probe.service -> /dev/null.
   Created symlink /etc/systemd/system/oops-probe.service -> /dev/null.
   Created symlink /etc/systemd/system/klogscanner.service -> /dev/null.
   Created symlink /etc/systemd/system/journal-probe.service -> /dev/null.
   Created symlink /etc/systemd/system/bert-probe.service -> /dev/null.
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
   Removed /etc/systemd/system/hprobe.timer.
   Removed /etc/systemd/system/telemd-update-trigger.service.
   Removed /etc/systemd/system/pstore-clean.service.
   Removed /etc/systemd/system/pstore-probe.service.
   Removed /etc/systemd/system/oops-probe.service.
   Removed /etc/systemd/system/klogscanner.service.
   Removed /etc/systemd/system/journal-probe.service.
   Removed /etc/systemd/system/bert-probe.service.
   removed '/etc/sysctl.d/80-coredump.conf'
   removed '/etc/systemd/system.conf.d/40-watchdog.conf'
   ... Set server_delivery_enabled=true in /etc/telemetrics/telemetrics.conf
   ... Set record_retention_enabled=false in /etc/telemetrics/telemetrics.conf
   *** Please reboot your system. ***

Follow the hints to reboot the system:

.. code-block:: none

   $ sudo reboot

To check the status of this tool:

.. code-block:: none

   $ sudo crashlogctl is-active

It will show the status of the related services like:

.. code-block:: console

   telemprobd : active
   telempostd : active
   acrnprobe  : inactive
   usercrash  : inactive

Usage
*****

The ``acrnprobe`` tool can work in two ways according to the existence of
telemetrics-client on the system:

1. If telemetrics-client doesn't exist on the system, ``acrnprobe`` provides
   ``history_event`` (under ``/var/log/crashlog/history_event``) to manage the
   crash and events records on the platform. But in this case, the records
   can't be delivered to the backend.

2. If telemetrics-client exists on the system, ``acrnprobe`` works as a probe
   of the telemetrics-client: it runs as a daemon autostarted when the system
   boots, and sends the crashlog path to the telemetrics-client that records
   events of interest and reports them to the backend using ``telemd`` the
   telemetrics daemon. The work flow of ``acrnprobe`` and
   telemetrics-client is shown in :numref:`crashlog-workflow`:

.. graphviz:: images/crashlog-workflow.dot
   :name: crashlog-workflow
   :align: center
   :caption: acrnprobe and telemetrics-client workflow


Crashlog can be retrieved with ``telem_journal`` command:

.. code-block:: none

   $ telem_journal -i

.. note::

   For more details of telemetrics, please refer the `telemetrics-client`_ and
   `telemetrics-backend`_ website.

``ACRN-Crashlog`` also provides a tool ``debugger`` to dump the specific
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
  monitoring the change of related folders on the sos image, like
  ``/data/logs/``. ``acrnprobe`` also provides a flexible way to allow users to
  configure which crash or event they want to collect through the xml file
  easily.
- ``common``: some utils for logs, command and string.
- ``data``: configuration file, service files and shell script.
- ``usercrash``: to implement the tool which get the crash information for the
  crashing process in userspace.

acrnprobe
=========

The ``acrnprobe`` detects all critical events on the platform and collects
specific information for debug purpose. These information would be saved as
logs, and the log path would be delivered to telemetrics-client as a record if
the telemetrics-client existed on the system.
For more detail on arcnprobe, please refer :ref:`acrnprobe_doc`.

usercrash
=========

The ``usercrash`` is a tool to get the crash info of the crashing process in
userspace. It works in Client/Server model. Server is autostarted, and client is
configured in ``core_pattern`` or ``coredump-wrapper``, which will be
triggered once crash occurs in userspace.
For more detail on ``usercrash``, please refer :ref:`usercrash_doc`.

.. _`telemetrics-client`: https://github.com/clearlinux/telemetrics-client
.. _`telemetrics-backend`: https://github.com/clearlinux/telemetrics-backend
