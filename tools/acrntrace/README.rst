``acrntrace``
==============

DESCRIPTION
###########

``acrntrace``: is a tool running on SOS, to capture trace data.
scripts directory includes scripts to analyze the trace data.

USAGE
#####

Capture trace data on SOS

1) Launch ``acrntrace``

Capture buffered trace data:

 ::

   # acrntrace

or clear buffer before tracing start:

 ::

   # acrntrace -c

Trace files are created under ``/tmp/acrntrace/``, directory name with time string eg: ``20171115-101605``

2) To stop acrntrace

 ::

   # q <enter>

3) Copy the trace data to linux pc

 ::

   # scp -r /tmp/acrntrace/20171115-101605/   xxx@10.239.142.239:/home/xxxx/trace_data


**Analyze the trace data on Linux PC**

1) Run the python script to analyze the ``vm_exits``:

  ::

   # acrnalyze.py -i /home/xxxx/trace_data/20171115-101605/0 -o /home/xxxx/trac
     e_data/20171115-101605/cpu0 --vm_exit
   - "--vm_exit" specify the analysis to do, currently, only vm_exit analysis
     is supported.
   - A preprocess would be taken out to make the trace data start and end with
     an VM_ENTER, and a copy of original data file is saved with suffix ".orig";
   - Analysis report would be given on the std output and in a csv file with
     name specified via "-o outpu_file";
   Script usage:
   [Usage] acrnalyze.py [options] [value] ...
   [options]
   -h: print this message
   -i, --ifile=[string]: input file
   -o, --ofile=[string]: output file
   --vm_exit: to generate vm_exit report

The scripts require bash and python2.

BUILD
#####

::

# make
