.. _acrntrace:

Acrntrace
#########

Description
***********

``acrntrace`` is a tool running on the Service VM to capture trace data.
A ``scripts`` directory includes scripts to analyze the trace data.

Usage
*****

Acrntrace
=========

The ``acrntrace`` tool runs on the Service VM to capture trace data and output
the data to a trace file under ``./acrntrace`` in raw (binary) data format.

Options:

-h                      print this message
-i period               specify polling interval in milliseconds [1-999]
-t max_time             max time to capture trace data (in seconds)
-c                      clear the buffered old data (deprecated)
-r                      capture the buffered old data instead of clearing it
-a cpu-set              only capture the trace data on the configured cpu-set

acrntrace_format.py
===================

The ``acrntrace_format.py`` is an offline tool for parsing trace data (as output
by ``acrntrace``) to human-readable formats based on a given format.

Here's an explanation of the tool's parameters:

.. code-block:: none

   acrntrace_format.py [options] [formats] [trace_data]

Options:

-h    print this message

The *formats* file specifies the rules to reformat the *trace_data* collected by
``acrntrace`` into a human-readable text form. The rules in this file follow
this form::

   event_id  text_format_string

The ``text_format_string`` may include format specifiers, such as ``%(cpu)d``,
``%(tsc)d``, ``%(event)d``, ``%(1)d``, and ``%(2)d``. The 'd' format specifier
outputs the data in decimal format. Alternatively, 'x' outputs the data in
hexadecimal format, and 'o' outputs the data in octal format.

These respectively correspond to the CPU number (cpu), timestamp
counter (tsc), event ID (event), and the data logged in the trace file.
There can be only one such rule for each type of event.

An example *formats* file is available in the ``acrn_hypervisor`` repo in
``misc/debug_tools/acrn_trace/scripts``.

acrnalyze.py
============

The ``acrnalyze.py`` is an offline tool to analyze trace data (as output by
``acrntrace``) based on a given analyzer, such as ``vm_exit`` or ``irq``.

Options:

-h                                print this message
-i, --ifile=string                input file name
-o, --ofile=string                output file name
-f, --frequency=unsigned_int      TSC frequency in MHz
--vm_exit                         generate a vm_exit report
--irq                             generate an IRQ-related report

.. note:: The tool depends on TSC frequency to do time-based analysis. Be sure
   to configure the right TSC frequency that ACRN runs on. TSC frequency can be
   obtained from the ACRN console log (``calibrate_tsc, tsc_hz=xxx``) when the
   hypervisor boots.

   The tool does not take into account CPU frequency variation that can
   occur during normal operation (aka CPU throttling) on a processor that
   doesn't support an invariant TSC. The results may therefore not be
   completely accurate in that regard.

Typical Use Example
===================

Here's a typical use of ``acrntrace`` to capture trace data from the Service VM,
convert the binary data to human-readable form, copy the processed trace
data to your development computer (Linux system), and run the analysis tool.

1. On the Service VM, start capturing buffered trace data:

   .. code-block:: none

      sudo acrntrace

   Trace files are created under the current directory where you launched
   ``acrntrace``, with a date-time-based directory name such as
   ``./acrntrace/20211027-101605``.

#. When done, stop a running ``acrntrace``:

   .. code-block:: none

      q <enter>

#. Convert trace data to human-readable format:

   .. code-block:: none

      sudo acrntrace_format.py formats trace_data

   Trace data will be converted to human-readable format based on a given format
   and printed to stdout.

#. Analysis of the collected trace data is done on your development computer.
   Copy the collected trace data to your development computer via USB disk or
   ``scp`` as shown in this example:

   .. code-block:: none

      sudo scp -r ./acrntrace/20211027-101605/ \
          username@hostname:/home/username/trace_data

   Replace username and hostname with appropriate values.

#. On the development computer, run the provided Python3 script to analyze, for
   example, the ``vm_exits``, ``irq``:

   .. code-block:: none

      sudo acrnalyze.py -i /home/xxxx/trace_data/20211027-101605/0 \
           -o /home/xxxx/trace_data/20211027-101605/cpu0 --vm_exit --irq

   - The analysis report is written to stdout, or to a CSV file if
     a file name is specified using ``-o filename``.
   - The scripts require Python3.

Build and Install
*****************

The source files for ``acrntrace`` are in the ``misc/debug_tools/acrn_trace``
directory. To build and install ``acrntrace``, run these commands:

.. code-block:: none

   make
   sudo make install

The processing scripts are in ``misc/debug_tools/acrn_trace/scripts``. The
``acrnalyze.py`` tool needs to be copied to and run on your development
computer.
