.. _acrnprobe_doc:

acrnprobe
#########

Description
***********

The ``acrnprobe`` is a tool to detect all critical events on the platform and
collect specific information for them. The collected information would be saved
as logs. The log path would be delivered to `telemetrics-client`_ as a record if
telemetrics-client exists on the system. In this case ``acrnprobe`` works as a
*probe* of telemetrics-client. If telemetrics-client doesn't exist on the
system, ``acrnprobe`` provides ``history_event`` (under ``/var/log/crashlog/``
by default) to manage the crash and events records on the platform instead of
``telem_journal``. But in this case, the records can't be delivered to the
backend.

Usage
*****

The ``acrnprobe`` is launched as a service at boot. Also, it provides some basic
options:

Specify a configuration file for ``acrnprobe``. If this option is unused,
``acrnprobe`` will use the configuration file located in CUSTOM CONFIGURATION
PATH or INSTALLATION PATH (see `CONFIGURATION FILES`_).

.. code-block:: none

   $ acrnprobe -c [configuration_path]

To see the version of ``acrnprobe``.

.. code-block:: none

   $ acrnprobe -V

Architecture
************

Terms
=====

- channel :
  Channel represents a way of detecting the system's events. There are 3
  channels:

  + oneshot: detect once while ``acrnprobe`` startup.
  + polling: run a detecting job with fixed time interval.
  + inotify: monitor the change of file or dir.

- trigger :
  Essentially, trigger represents one section of content. It could be
  a file's content, a directory's content, or a memory's content which can be
  obtained. By monitoring it ``acrnprobe`` could detect certain events which
  happened in the system.

- crash :
  A subtype of event. It often corresponds to a crash of programs, system, or
  hypervisor. ``acrnprobe`` detects it and reports it as ``CRASH``.

- info :
  A subtype of event. ``acrnprobe`` detects it and reports it as ``INFO``.

- event queue :
  There is a global queue to receive all events detected.
  Generally, events are enqueued in channel, and dequeued in event handler.

- event handler :
  Event handler is a thread to handle events detected by channel.
  It's awakened by an enqueued event.

- sender :
  The sender corresponds to an exit of event.
  There are two senders:

  + Crashlog is responsible for collecting logs and saving it locally.
  + Telemd is responsible for sending log records to telemetrics client.

Description
===========

As a log collection mechanism to record critical events on the platform,
``acrnprobe`` provides these functions:

1. detect event

   From experience, the occurrence of an system event is usually accompanied
   by some effects. The effects could be a generated file, an error message in
   kernel's log, or a system reboot. To get these effects, for some of them we
   can monitor a directory, for other of them we might need to do a detection
   in a time loop.
   *So we implement the channel, which represents a common method of detection.*

2. analyze event and determine the event type

   Generally, a specific effect correspond to a particular type of events.
   However, it is the icing on the cake for analyzing the detailed event types
   according to some phenomena. *Crash reclassify is implemented for this
   purpose.*

3. collect information for detected events

   This is for debug purpose. Events without information are meaningless,
   and developers need to use this information to improve their system. *Sender
   crashlog is implemented for this purpose.*

4. archive these information as logs, and generate records

   There must be a central place to tell user what happened in system.
   *Sender telemd is implemented for this purpose.*

Diagram
=======
::

 +---------------------------------------------+
 | channel:   |oneshot|  |polling|   |inotify| |
 +--------------------------------------+------+
                                        |
 +---------------------+    +-----+     |
 | event queue         +<---+event+<----+
 +-+-------------------+    +-----+
   |
   v
 +-+---------------------------------------------------------------------------+
 |  event handler:                                                             |
 |                                                                             |
 |  event handler will handle internal event                                   |
 |    +----------+    +------------+                                           |
 |    |heart beat+--->+fed watchdog|                                           |
 |    +----------+    +------------+                                           |
 |                                                                             |
 |  call sender for other types                                                |
 |    +--------+   +----------------+   +------------+   +------------------+  |
 |    |crashlog+-->+crash reclassify+-->+collect logs+-->+generate crashfile|  |
 |    +--------+   +----------------+   +------------+   +------------------+  |
 |                                                                             |
 |    +------+    +------------------+                                         |
 |    |telemd+--->+telemetrics client|                                         |
 |    +------+    +------------------+                                         |
 +-----------------------------------------------------------------------------+


Source files
************

- main.c
  Entry of ``acrnprobe``.
- channel.c
  The implementation of *channel* (see `Terms`_).
- crash_reclassify.c
  Analyzing the detailed types for crash event.
- probeutils.c
  Provide some utils ``acrnprobe`` needs.
- event_queue.c
  The implementation of *event queue* (see `Terms`_).
- event_handler.c
  The implementation of *event handler* (see `Terms`_).
- history.c
  There is a history_event file to manage all logs that ``acrnprobe`` archived.
  "history.c" provides the interfaces to modify the file in fixed format.
- load_conf.c
  Parse and load the configuration file.
- property.c
  The ``acrnprobe`` needs to know some HW/SW properties, such as board version,
  build version. These properties are managed centrally in this file.
- sender.c
  The implementation of *sender* (see `Terms`_).
- startupreason.c
  This file provides the function to get system reboot reason from kernel
  command line.
- android_events.c
  Sync events detected by android crashlog.
- loop.c
  This file provides interfaces to read from image.

Configuration files
*******************

* ``/usr/share/defaults/telemetrics/acrnprobe.xml``

  If no custom configuration file is found, ``acrnprobe`` uses the settings in
  this file.

* ``/etc/acrnprobe.xml``

  Custom configuration file that ``acrnprobe`` reads.

For details about configuration file, please refer to :ref:`acrnprobe-conf`.

.. _`telemetrics-client`: https://github.com/clearlinux/telemetrics-client
