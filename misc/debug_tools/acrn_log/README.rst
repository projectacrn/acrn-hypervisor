.. _acrnlog:

acrnlog
#######

Description
***********

``acrnlog`` is a userland tool used to capture a ACRN hypervisor log. It runs as an
SOS service at boot, capturing two kinds of logs:

- log of the currently running hypervisor
- log of last running hypervisor if it crashed and the logs remain.

Log files are saved in ``/tmp/acrnlog/``, so the log files would be lost
after a system reset.

Usage
*****

The ``acrnlog`` tool is launched as a service at boot, and limited to
supporting four 1MB log files by default.  You can change this log file
limitation temporarily or permanently.

Options:

  -h  display help
  -t  specify a polling interval (ms). Once buffer is empty, acrnlog stops
      and starts reading in specified interval.
      If an incomplete log warning is reported, please try with a smaller
      interval to get a complete log.
  -s  limit the size of each log file, in KB. 0 means no limitation.
  -n  specify the number of log files to keep, old files would be deleted.

Temporary log file changes
==========================

You can temporarily change the log file setting by following these
steps:

1. Stop the ``acrnlog`` service:

   .. code-block:: none

      # systemctl disable acrnlog

2. Restart ``acrnlog``, running in the background, and specify the new
   number of log files and their size (in MB).  For example:

   .. code-block:: none

      # acrnlog -n 8 -s 4 &

You can use ``loglevel`` command in the hypervisor shell (not the Service
OS shell) to query and change the hypervisor log level.

The ``mem_loglevel`` parameter controls the log to be saved using
``acrnlog``, while the ``console_loglevel`` parameter controls the log
output to the console. For example, in the hypervisor shell you
can use these commands:

.. code-block:: none

   ACRN:\>loglevel
   console_loglevel: 3, mem_loglevel: 5, npk_loglevel: 5
   ACRN:\>loglevel 2 5
   ACRN:\>loglevel
   console_loglevel: 2, mem_loglevel: 5, npk_loglevel: 5


Permanent log file changes
==========================

You can also permanently change the log file settings by
editing ``/usr/lib/systemd/system/acrnlog.service`` and use the ``-n``
and ``-s`` options on the ``ExecStart`` cmd, and restart the service.
For example, ``acrnlog.service`` could have these parameters added:

.. code-block:: none

   ExecStart=/usr/bin/acrnlog -n 8 -s 4

and then restart the service with:

.. code-block:: none

   # systemctl daemon-reload
   # systemctl restart acrnlog

Build and Install
*****************

Source code for the ``acrnlog`` tools is in the ``tools/acrnlog``
folder.  Build and install the tools from source using:

.. code-block:: none

   # make
   # make install

and if you changed the ``acrnlog.service`` file, install it using:

.. code-block:: none

   # cp acrnlog.service /usr/lib/systemd/system/
