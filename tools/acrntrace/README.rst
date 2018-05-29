.. _acrntrace:

acrntrace
#########

Description
***********

``acrntrace`` is a tool running on the Service OS (SOS) to capture trace data.
A ``scripts`` directory includes scripts to analyze the trace data.

Usage
*****

1. On the SOS, clear buffers before starting a trace, with:

   .. code-block:: none

      # acrntrace -c

#. Start capturing buffered trace data with:

   .. code-block:: none

      # acrntrace

   Trace files are created under ``/tmp/acrntrace/``, with a
   date-time-based directory name such as ``20171115-101605``

#. When done, stop a running ``acrntrace``, with:

   .. code-block:: none

      q <enter>

#. Analysis of the collected data is done on a Linux PC, so you'll need
   to copy the collected trace data to your Linux system (using ``scp`` is
   recommended):

   .. code-block:: none

      # scp -r /tmp/acrntrace/20171115-101605/ \
          username@hostname:/home/username/trace_data

   Replace username and hostname with appropriate values.

#. On the Linux system, run the provided python2 script to analyze the
   ``vm_exits`` (currently only vm_exit analysis is supported):

   .. code-block:: none

      # acrnalyze.py -i /home/xxxx/trace_data/20171115-101605/0 \
           -o /home/xxxx/trace_data/20171115-101605/cpu0 --vm_exit

   - A preprocess makes some changes to the datafile for processing but
     a copy of the original data file is saved with suffix ``.orig``.
   - Analysis report is written to stdout, or to a CSV file if
     a filename is specified using ``-o filename``.
   - The scripts require bash and python2.

Build and Install
*****************

The source files for ``acrntrace`` are in the ``tools/acrntrace`` folder,
and can be built and installed using:

.. code-block:: none

   # make
   # make install

The processing scripts are in ``tools/acrntrace/scripts`` and need to be
copied to and run on your Linux system.
