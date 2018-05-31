.. _usercrash_doc:

Usercrash
#########

Description
***********

The ``usercrash`` is to get the crash info for the crashing process in
userpace. The collected information is saved as usercrash_xx under
``/var/log/usercrashes/``.

Design
******

The ``usercrash`` is designed as Client/Server model. The server is autostarted
at boot, and the client is configured in ``core_pattern``, which will be
triggered once crash occurs in userspace. Then client sends the crash event to
server. The server will check the files under ``/var/log/usercrashes/`` and
create a new file usercrash_xx(xx means the index of the crash files), then
it will send the fd to client. The client will be responsible for collecting
crash information and saving it in the crashlog file. After saving work is done,
client will notify server. Then the server will clean up.

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
  ``core_pattern``. The content of ``core_pattern`` is configured as
  ``usercrash_c`` while booting up:

.. code-block:: console

   $ echo "|/usr/bin/usercrash_c %p %e %s" > /proc/sys/kernel/core_pattern

That means client will be triggered once userspace crash occurs. Then the
event will be sent to server from client.

- The ``debugger`` is an independent tool to dump the debug information of the
  specific process, including backtrace, stack, opened files, registers value,
  memory content around registers, and etc.

.. code-block:: console

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
