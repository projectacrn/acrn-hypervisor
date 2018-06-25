#!/usr/bin/python2
# -*- coding: UTF-8 -*-

"""
This is the main script of arnalyzer, which:
- parse the options
- pre-process the trace data file
- call a specific script to do analysis
"""

import sys
import getopt
import os
import config
from vmexit_analyze import analyze_vm_exit

def usage():
    """print the usage of the script
    Args: NA
    Returns: None
    Raises: NA
    """
    print '''
    [Usage] acrnalyze.py [options] [value] ...

    [options]
    -h: print this message
    -i, --ifile=[string]: input file
    -o, --ofile=[string]: output file
    -f, --frequency=[unsigned int]: TSC frequency in MHz
    --vm_exit: to generate vm_exit report
    '''

def do_analysis(ifile, ofile, analyzer):
    """do the specific analysis

    Args:
        ifile: input trace data file
        ofile: output analysis report file
        analyzer: a function do the specific analysis
    Returns:
        None
    Raises:
        NA
    """
    analyzer(ifile, ofile)

# pre process to make sure the trace data start and end with VMENTER
def pre_process(ifile, evstr):
    """invoke sh script to preprocess the data file

    Args:
        ifile: input trace data file
        evstr: event string, after the processing, the data file should
                start and end with the string
    Returns:
        status of sh script execution
    Raises:
        NA
    """
    status = os.system('bash pre_process.sh %s %s' % (ifile, evstr))
    return status

def main(argv):
    """Main enterance function

    Args:
        argv: arguments string
    Returns:
        None
    Raises:
        GetoptError
    """
    inputfile = ''
    outputfile = ''
    opts_short = "hi:o:f:"
    opts_long = ["ifile=", "ofile=", "frequency=", "vm_exit"]

    try:
        opts, args = getopt.getopt(argv, opts_short, opts_long)
    except getopt.GetoptError:
        usage()
        sys.exit(1)

    for opt, arg in opts:
        if opt == '-h':
            usage()
            sys.exit()
        elif opt in ("-i", "--ifile"):
            inputfile = arg
        elif opt in ("-o", "--ofile"):
            outputfile = arg
        elif opt in ("-f", "--frequency"):
            TSC_FREQ = arg
        elif opt == "--vm_exit":
            analyzer = analyze_vm_exit
        else:
            assert False, "unhandled option"

    assert inputfile != '', "input file is required"
    assert outputfile != '', "output file is required"
    assert analyzer != '', 'MUST contain one of analyzer: ''vm_exit'

    status = pre_process(inputfile, 'VM_ENTER')
    if status == 0:
        do_analysis(inputfile, outputfile, analyzer)
    else:
        print "Invalid trace data file %s" % (inputfile)

if __name__ == "__main__":
    main(sys.argv[1:])
