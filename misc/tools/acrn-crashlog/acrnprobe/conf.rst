.. _acrnprobe-conf:

acrnprobe Configuration
#######################

Description
***********
``acrnprobe`` uses XML as the format of its configuration file, namely
``acrnprobe.xml``, following the `XML standard`_.

Layout
******

.. code-block:: xml

   <?xml version="1.0" encoding="UTF-8"?>
   <conf>
   Root node of configuration.

   <senders>
   Configuration section of senders.
   <sender id='1'>Configuration of sender 1</sender>
   <sender id='2'>Configuration of sender 2</sender>
   </senders>

   <triggers>
   Configuration section of triggers.
   <trigger id='1'>Configuration of trigger 1</trigger>
   <trigger id='2'>Configuration of trigger 2</trigger>
   </triggers>

   <vms>
   Configuration section of virtual machines.
   <vm id='1'>Configuration of vm 1</vm>
   <vm id='2'>Configuration of vm 2</vm>
   </vms>

   <logs>
   Configuration section of logs.
   <log id='1'>Configuration of log 1</log>
   <log id='2'>Configuration of log 2</log>
   </logs>

   <crashes>
   Configuration section of crashes.
   Note that this section must be configured after triggers and logs, as
   crashes depend on these two sections.
   <crash id='1'>Configuration of crash 1</crash>
   <crash id='2'>Configuration of crash 2</crash>
   </crashes>

   <infos>
   Configuration section of infos.
   Note that this section must be configured after triggers and logs, as
   infos depend on these two sections.
   <info id='1'>Configuration of info 1</info>
   <info id='2'>Configuration of info 2</info>
   </infos>

   </conf>

As for the definition of ``sender``, ``trigger``, ``crash`` and ``info``
please refer to :ref:`acrnprobe_doc`.

Properties of group members
***************************

``acrnprobe`` defined different groups in configuration file, which are
``senders``, ``triggers``, ``crashes`` and ``infos``.

Common properties
=================

- ``id``:
  The index, which grows from 1 consecutively, in its group.
- ``enable``:
  This group member will be ignored if the value is NOT ``true``.

Other properties
================

- ``inherit``:
  Specify a parent for a certain crash.
  The child crash will inherit all configurations from the specified (by id)
  crash. These inherited configurations could be overwritten by new ones.
  Also, this property helps build the crash tree in ``acrnprobe``.
- ``expression``:
  See `Crash`_.

Crash tree in acrnprobe
***********************

There could be a parent-child relationship between crashes. Refer to the
diagrams below, crash B and D are the children of crash A, because crash B and
D inherit from crash A, and crash C is the child of crash B.

Build crash tree in configuration
=================================

.. graphviz:: images/crash-config.dot
   :name: crash-config
   :align: center
   :caption: Build crash tree in configuration

Match crash at runtime
======================

In order to find a more specific type, if one crash type matches
successfully ``acrnprobe`` will do a match for each child of it (if it has any)
continually, and return the last successful one.
About how to determine a match is successful, please refer to the ``content`` of
`Crash`_.

Supposing these crash trees are like the diagram above at runtime:
If a crash E is triggered, crash E will be returned immediately.
If a crash A is triggered, then the candidates are crash A, B, C and D.
The following diagram describes what ``acrnprobe`` will do if the matched
result is crash D.

.. graphviz:: images/crash-match.dot
   :name: crash-match
   :align: center
   :caption: Match crash at runtime

Sections
********

Sender
======

Example:

.. code-block:: xml

   <sender id="1" enable="true">
           <name>crashlog</name>
           <outdir>/var/log/crashlog</outdir>
           <maxcrashdirs>1000</maxcrashdirs>
           <maxlines>5000</maxlines>
           <spacequota>90</spacequota>
           <uptime>
                   <name>UPTIME</name>
                   <frequency>5</frequency>
                   <eventhours>6</eventhours>
           </uptime>
   </sender>

* ``name``:
  Name of sender. ``acrnprobe`` uses this label to distinguish different
  senders.
  For more information about sender, please refer to :ref:`acrnprobe_doc`.
* ``outdir``:
  Directory to store generated files of sender. ``acrnprobe`` will create
  this directory if it doesn't exist.
* ``maxcrashdirs``:
  The maximum serial number of generated ``crash directories``,
  ``stat directories`` and ``vmevent directories``. The serial number will be
  reset to 0 if it reaches the specified maximum (``maxcrashdirs``).
  Only used by sender crashlog.
* ``maxlines``:
  If the number of lines in the ``history_event`` file reaches the specified
  ``maxlines``, the ``history_event`` file will be renamed to
  ``history_event.bak`` and logging will continue with a now empty
  ``history_event`` file.
* ``spacequota``:
  ``acrnprobe`` will stop collecting logs if
  ``(used space / total space) * 100 > spacequota``. Only used by sender
  crashlog.
* ``uptime``:
  Configuration to trigger ``UPTIME`` event.
  sub-nodes:

  + ``name``:
    The name of event.
  + ``frequency``:
    Time interval in seconds to trigger ``uptime`` event.
  + ``eventhours``:
    Time interval in hours to generate a record.


Trigger
=======

Example:

.. code-block:: xml

   <trigger id="1" enable="true">
           <name>t_pstore</name>
           <type>node</type>
           <path>/sys/fs/pstore/console-ramoops-0</path>
   </trigger>
   <trigger id="2" enable="true">
           <name>t_acrnlog_last</name>
           <type>file</type>
           <path>/tmp/acrnlog/acrnlog_last.[*]</path>
   </trigger>

* ``name``:
  The name of trigger. It's used by crash and info configuration module.
* ``type`` and ``path``:
  These two labels are used to get the content of trigger files.
  ``type`` have been implemented:

  + ``node``:
    It means that ``path`` is a device node on virtual file system, which cannot
    support ``mmap(2)-like`` operations. ``acrnprobe`` can use only ``read(2)``
    to get its content.
  + ``file``:
    It means that ``path`` is a regular file which supports ``mmap(2)-like``
    operations.
  + ``dir``:
    It means that ``path`` is a directory.
  + ``rebootreason``:
    It means that the trigger's content is the reboot reason of system. The
    content of ``rebootreason`` is not obtained in a common way. So, it doesn't
    work with ``path``.
  + ``cmd``:
    It means that ``path`` is a command which will be launched by ``execvp(3)``.

  Some programs often use format ``string%d`` instead of static file name to
  generate target file dynamically. So ``path`` supports simple formats for
  these cases:

  + /.../dir/string[*] --> all files with prefix "string" under dir.
  + /.../dir/string[0] --> the first file of files, sorted by ``alphasort(3)``,
    with prefix "string" under dir.
  + /.../dir/string[-1] --> the last file of files, sorted by ``alphasort(3)``,
    with prefix "string" under dir.

  Example of formats:
  If there are 4 files under ``/tmp``:
  ``acrnlog_last.1`` ``acrnlog_last.2`` ``acrnlog_last.3`` ``other.txt``

  + ``/tmp/acrnlog_last.[-1]`` indicates ``acrnlog_last.3``.
  + ``/tmp/acrnlog_last.[0]`` indicates ``acrnlog_last.1``.
  + ``/tmp/acrnlog_last.[*]`` indicates the file set including
    ``acrnlog_last.1``, ``acrnlog_last.2`` and ``acrnlog_last.3``.


Vm
==

Example:

.. code-block:: xml

   <vm id="1" enable="true">
           <name>VM1</name>
           <channel>polling</channel>
           <interval>60</interval>
           <syncevent id="1">CRASH/TOMBSTONE</syncevent>
           <syncevent id="2">CRASH/UIWDT</syncevent>
           <syncevent id="3">CRASH/IPANIC</syncevent>
           <syncevent id="4">REBOOT</syncevent>
   </vm>

* ``name``:
  The name of virtual machine.
* ``channel``:
  The ``channel`` name to get the virtual machine events.
* ``interval``:
  Time interval in seconds of polling vm's image.
* ``syncevent``:
  Event type ``acrnprobe`` will synchronize from virtual machine's ``crashlog``.
  User could specify different types by id. The event type can also be
  indicated by ``type/subtype``.

Log
===

Example:

.. code-block:: xml

   <log id="1" enable="true">
           <name>pstore</name>
           <type>node</type>
           <path>/sys/fs/pstore/console-ramoops-0</path>
   </log>

* ``name``:
  By default, ``acrnprobe`` will take this ``name`` as generated log's name in
  ``outdir`` of sender crashlog.
  If ``path`` is specified by simple formats (includes [*], [0] or [-1]) the
  file name of generated logs will be the same as original. More details about
  simple formats, see `Trigger`_.
* ``type`` and ``path``:
  Same as `Trigger`_.
* ``lines``:
  By default, all contents in the original will be copied to generated log.
  If this label is configured, only the ``lines`` at the end in the original
  will be copied to the generated log. It takes effect only when the ``type`` is
  ``file``.

Crash
=====

Example:

.. code-block:: xml

   <crash id='1' inherit='0' enable='true'>
           <name>UNKNOWN</name>
           <trigger>t_rebootreason</trigger>
           <channel>oneshot</channel>
           <content id='1'>WARM</content>
           <log id='1'>pstore</log>
           <log id='2'>acrnlog_last</log>
   </crash>
   <crash id='2' inherit='1' enable='true'>
           <name>IPANIC</name>
           <trigger>t_pstore</trigger>
           <content id='1'> </content>
           <mightcontent expression='1' id='1'>Kernel panic - not syncing:</mightcontent>
           <mightcontent expression='1' id='2'>BUG: unable to handle kernel</mightcontent>
           <data id='1'>kernel BUG at</data>
           <data id='2'>EIP is at</data>
           <data id='3'>Comm:</data>
   </crash>

* ``name``:
  The type of the ``crash``.
* ``trigger``:
  The trigger name of the crash.
* ``channel``:
  The name of channel crash use.
* ``content`` and ``mightcontent``:
  They're used to match crash type. The match is successful if all the
  following conditions are met:

  a. All ``contents`` with different ``ids`` are included in trigger's
     content.
  b. One of ``mightcontents`` with the same ``expression`` is included in
     trigger's content at least.
  c. If there are ``mightcontents`` with different ``expressions``, each group
     with the same ``expression`` should meet condition b.
* ``log``:
  The log to be collected. The value is the configured ``name`` in log module.
  User could specify different logs by ``id``.
* ``data``:
  It is used to generate ``DATA`` fields in ``crashfile``. ``acrnprobe`` will
  copy the line which starts with configured ``data`` in trigger's content
  to ``DATA`` fields. There are 3 fields in ``crashfile`` and they could be
  specified by ``id`` 1, 2, 3.

Info
=====

Example:

.. code-block:: xml

   <info id='1' enable='true'>
           <name>BOOT_LOGS</name>
           <trigger>t_boot</trigger>
           <channel>oneshot</channel>
           <log id='1'>kmsg</log>
           <log id='2'>cmdline</log>
           <log id='3'>acrnlog_cur</log>
           <log id='4'>acrnlog_last</log>
   </info>

* ``name``:
  The type of info.
* ``trigger``:
  The trigger name of the info.
* ``channel``:
  The name of channel info use.
* ``log``:
  The log to be collected. The value is the configured name in log module. User
  could specify different logs by id.

.. _`XML standard`: http://www.w3.org/TR/REC-xml
