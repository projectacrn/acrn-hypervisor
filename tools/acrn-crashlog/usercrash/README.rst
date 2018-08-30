.. _usercrash_doc:

usercrash
#########

Description
***********

The ``usercrash`` tool gets the crash info for the crashing process in
userspace. The collected information is saved as usercrash_xx under
``/var/log/usercrashes/``.

Design
******

``usercrash`` is designed using a  Client/Server model. The server is
autostarted at boot. The client is configured in ``core_pattern``, which
will be triggered when a crash occurs in userspace. The client then
sends the crash event to the server. The server checks the files under
``/var/log/usercrashes/`` and creates a new file usercrash_xx (xx means
the index of the crash file).  Then it sends the file descriptor (fd) to
the client. The client is responsible for collecting crash information
and saving it in the crashlog file. After the saving work is done, the
client notifies server and the server will clean up.

The work flow diagram:

::

   +--------------------------------------------------+
   |                                                  |
   |        Server                    Client          |
   |           +                         +            |
   |           |  Send crash event       |            |
   |           | <-----------------------+            |
   |           |                         |            |
   |    Create usercrash_xx              |            |
   |           |                         |            |
   |           | Send usercrash_xx fd    |            |
   |           +-----------------------> |            |
   |           |                         |            |
   |           |                   Fill usercrash_xx  |
   |           |                         |            |
   |           |  Notify completion      |            |
   |           | <-----------------------+            |
   |           |                         |            |
   |       Clean up                      |            |
   |           |                         |            |
   |           v                         v            |
   |                                                  |
   +--------------------------------------------------+

Usage
*****

- The server is launched automatically at boot, and the client is configured in
  ``core_pattern`` or ``coredump-wrapper``. In ``prepare.service``, it will
  check the content of ``/proc/sys/kernel/core_pattern``. If there is
  ``coredump-wrapper``, which means that ``core_pattern`` has been set in
  ``systemd``, no need to do it again. Otherwise, the content should be
  changed by:

.. code-block:: none

   $ echo "|/usr/bin/usercrash_c %p %e %s" > /proc/sys/kernel/core_pattern

That means client will be triggered once userspace crash occurs. Then the
event will be sent to server from client.

- The ``debugger`` is an independent tool to dump the debug information of the
  specific process, including backtrace, stack, opened files, registers value,
  memory content around registers, and etc.

.. code-block:: none

   $ debugger <pid>

.. note::

   You need to be ``root`` to use the ``debugger``.

Souce Code
**********

- client.c : This file is the implementation for client of ``usercrash``, which
  is responsible for delivering the ``usercrash`` event to the server, and
  collecting crash information and saving it to the crashfile.
- crash_dump.c : This file is the implementation for dumping the crash
  information, including backtrace stack, opened files, registers value, memory
  content around registers, and etc.
- debugger.c : This file is to implement a tool, which runs in command line to
  dump the process information list above.
- protocol.c : This file is the socket protocol implement file.
- server.c : This file is the implement file for server of ``usercrash``, which
  is responsible for creating the crashfile and handle the events from client.
