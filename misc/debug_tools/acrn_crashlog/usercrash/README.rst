.. _usercrash_doc:

Usercrash
#########

Description
***********

The ``usercrash`` tool gets the crash information for the crashing process in
user space. The collected information is saved as usercrash_xx under
``/var/log/usercrashes/``.

Design
******

``usercrash`` is designed using a  client/server model. The server is
autostarted at boot. The client is configured in ``core_pattern``, which
will be triggered when a crash occurs in user space. The client then
sends the crash event to the server. The server checks the files under
``/var/log/usercrashes/`` and creates a file usercrash_xx (xx means
the index of the crash file).  Then it sends the file descriptor (fd) to
the client. The client collects the crash information
and saves it in the crash file. After the saving work is done, the
client notifies the server. The server cleans up.

The workflow diagram:

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

- The ``debugger`` is an independent tool to dump the debugging information of the
  specific process, including backtrace, stack, opened files, register values,
  and memory content around registers.

.. code-block:: none

   $ debugger <pid>

.. note::

   You need to be ``root`` to use the ``debugger``.

Source Code
***********

- client.c : This file is the implementation for the client of ``usercrash``.
  The client is responsible for delivering the ``usercrash`` event to the
  server, and collecting crash information and saving it to the crash file.
- crash_dump.c : This file is the implementation for dumping the crash
  information, including backtrace stack, opened files, register values, and
  memory content around registers.
- debugger.c : This file implements a tool, which runs in command line to
  dump the process information listed above.
- protocol.c : This file is the socket protocol implementation file.
- server.c : This file is the implementation file for the server of
  ``usercrash``. The server is responsible for creating the crash file and
  handling the events from the client.
