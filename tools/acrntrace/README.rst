.. _acrntrace:

acrntrace
#########

Description
***********

``acrntrace`` is a tool running on the Service OS (SOS) to capture trace data.
A ``scripts`` directory includes scripts to analyze the trace data.

Usage
*****

acrntrace
=========

The ``acrntrace`` tool runs on the Service OS (SOS) to capture trace data and
output to trace file under ``./acrntrace`` with raw (binary) data format.

Options:

-h                      print this message
-i period               specify polling interval in milliseconds [1-999]
-t max_time             max time to capture trace data (in second)
-c                      clear the buffered old data

acrntrace_format.py
===================

The ``acrntrace_format.py`` is a offline tool for parsing trace data (as output
by acrntrace) to human-readable formats based on given format.

Here's an explanation of the tool's parameters:

.. code-block:: none

   acrntrace_format.py [options] [formats] [trace_data]

Options:

-h    print this message

*formats* file specifies the rules to reformat the *trace_data* collected by
``acrntrace`` into a human-readable text form. The rules in this file are of
the form::

   event_id  text_format_string

The text_format_string may include format specifiers, such as
``%(cpu)d``, ``%(tsc)d``, ``%(event)d``, ``%(1)d``, and ``%(2)d``.
The 'd' format specifier outputs in decimal, alternatively 'x' will
output in hexadecimal and 'o' will output in octal.

These respectively correspond to the CPU number (cpu), timestamp
counter (tsc), event ID (event) and the data logged in the trace file.
There can be only one such rule for each type of event.

An example *formats_file* is available in the acrn_hypervisor repo in
``hypervisor/tools/acrntrace/scripts/formats``.

acrnalyze.py
============

The ``acrnalyze.py`` is a offline tool to analyze trace data (as output by
acrntrace) based on given analyzer, such as ``vm_exit`` or ``irq``.

Options:

.. list-table::

   * - :kbd:`-h`
     - print this message

   * - :kbd:`-i, --ifile=string`
     - input file name

   * - :kbd:`-o, --ofile=string`
     - output filename

   * - :kbd:`-f, --frequency=unsigned_int`
     - TSC frequency in MHz

   * - :kbd:`--vm_exit`
     - generate a vm_exit report

   * - :kbd:`--irq`
     - generate an IRQ-related report

.. note:: We depend on TSC frequency to do time-based analysis. Please configure
   the right TSC frequency that acrn runs on. TSC frequency can be obtained
   from the ACRN console log (calibrate_tsc, tsc_hz=xxx) when the hypervisor boots.

   The tool does not take into account CPU frequency variation that can
   occur during normal operation (aka CPU throttling) on the processor which
   doesn't support for invariant TSC. The results may therefore not be
   completely accurate in that regard.

Typical use example
===================

Here's a typical use of ``acrntrace`` to capture trace data from the SOS,
converting the binary data to human-readable form, copying the processed trace
data to your linux system, and running the analysis tool.

1. On the SOS, clear buffers before starting a trace, with:

   .. code-block:: none

      # acrntrace -c

#. Start capturing buffered trace data with:

   .. code-block:: none

      # acrntrace

   Trace files are created under current directory where we launch acrntrace,
   with a date-time-based directory name such as ``./acrntrace/20171115-101605``

#. When done, stop a running ``acrntrace``, with:

   .. code-block:: none

      q <enter>

#. Convert trace data to human-readable format, with:

   .. code-block:: none

      # acrntrace_format.py formats trace_data

   Trace data will be converted to human-readable format based on given format
   and printed to stdout.

#. Analysis of the collected data is done on a Linux PC, so you'll need
   to copy the collected trace data to your Linux system (using ``scp`` is
   recommended):

   .. code-block:: none

      # scp -r ./acrntrace/20171115-101605/ \
          username@hostname:/home/username/trace_data

   Replace username and hostname with appropriate values.

#. On the Linux system, run the provided Python3 script to analyze the
   ``vm_exits``, ``irq``:

   .. code-block:: none

      # acrnalyze.py -i /home/xxxx/trace_data/20171115-101605/0 \
           -o /home/xxxx/trace_data/20171115-101605/cpu0 --vm_exit --irq

   - Analysis report is written to stdout, or to a CSV file if
     a filename is specified using ``-o filename``.
   - The scripts require Python3.

Build and Install
*****************

The source files for ``acrntrace`` are in the ``tools/acrntrace`` folder,
and can be built and installed using:

.. code-block:: none

   # make
   # make install

The processing scripts are in ``tools/acrntrace/scripts`` and need to be
copied to and run on your Linux system.
