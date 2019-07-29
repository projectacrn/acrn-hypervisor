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

- The server is launched automatically at boot after this tool is enabled with
  instruction ``sudo crashlogctl enable``, and the client is configured in
  ``usercrash-wrapper``, which is set as the app of ``core_pattern``. In
  ``usercrash-wrapper``, it will collect and reorder the parameters of the
  client and default app. Once a crash occurs in user space, the client and
  default app will be invoked separately.

- The ``debugger`` is an independent tool to dump the debug information of the
  specific process, including backtrace, stack, opened files, registers value,
  memory content around registers, and etc.

.. code-block:: none

   $ debugger <pid>

.. note::

   You need to be ``root`` to use the ``debugger``.

Source Code
***********

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
