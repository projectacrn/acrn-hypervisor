``acrnlog``
===========

DESCRIPTION
###########
``acrnlog`` is a userland tool to capture ACRN hypervisor log, it runs as an
SOS service at boot. It captures two kinds of logs:

- log of current running;

- log of last running if crashed and logs remaining.

The path to save log files is ``/tmp/acrnog/``, so the log files would be lost
after reset.

USAGE
#####
The ``acrnlog`` tool is launched as a service at boot, with 4 1MB log files limited.
To change the log file limitation:

- temporary change:
    Stop the ``acrnlog`` service:

 ::

 # systemctl disable acrnlog

Restart ``acrnlog`` running in backgroud with size and number of files.
 For example:

 ::

 # acrnlog -n 8 -s 4 &

Use ``get_loglevel``/``set_loglevel`` to query and change the hypervisor loglevel.

The ``mem_loglevel`` controls log to be saved using ``acrnlog``, while
``console_loglevel`` controls log to output to console. For example:

 ::

  ACRN:\>get_loglevel
  console_loglevel: 2, mem_loglevel: 4
  ACRN:\>set_loglevel 2 5
  ACRN:\>get_loglevel
  console_loglevel: 2, mem_loglevel: 5

- permanent change:
   Edit ``/usr/lib/systemd/system/acrnlog.service`` to attached the ``-n`` and ``-s`` options to the ``ExecStart`` cmd, and restart the service. For example:

 ::

  ExecStart=/usr/bin/acrnlog -n 8 -s 4


BUILD & INSTALL
##################

::

 # make
 
copy acrnlog to ``/usr/bin/`` and copy ``acrnlog.service`` to ``/usr/lib/systemd/system/``
