#!/usr/bin/python3
# -*- coding: UTF-8 -*-

"""
This is the main script of arnalyzer, which:
- parse the options
- call a specific script to do analysis
"""

import sys
import getopt
import os
from vmexit_analyze import analyze_vm_exit
from irq_analyze import analyze_irq

def usage():
    """print the usage of the script
    Args: NA
    Returns: None
    Raises: NA
    """
    print ('''
    [Usage] acrnalyze.py [options] [value] ...

    [options]
    -h: print this message
    -i, --ifile=[string]: input file
    -o, --ofile=[string]: output file
    -f, --frequency=[unsigned int]: TSC frequency in MHz
    --vm_exit: to generate vm_exit report
    --irq: to generate irq related report
    ''')

def do_analysis(ifile, ofile, analyzer, freq):
    """do the specific analysis

    Args:
        ifile: input trace data file
        ofile: output analysis report file
        analyzer: a function do the specific analysis
        freq: TSC frequency of the host where we capture the trace data
    Returns:
        None
    Raises:
        NA
    """
    for alyer in analyzer:
        alyer(ifile, ofile, freq)

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
    # Default TSC frequency of MRB in MHz
    freq = 1881.6
    opts_short = "hi:o:f:"
    opts_long = ["ifile=", "ofile=", "frequency=", "vm_exit", "irq"]
    analyzer = []

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
            freq = arg
        elif opt == "--vm_exit":
            analyzer.append(analyze_vm_exit)
        elif opt == "--irq":
            analyzer.append(analyze_irq)
        else:
            assert False, "unhandled option"

    assert inputfile != '', "input file is required"
    assert outputfile != '', "output file is required"
    assert analyzer != '', 'MUST contain one of analyzer: ''vm_exit'

    do_analysis(inputfile, outputfile, analyzer, freq)

if __name__ == "__main__":
    main(sys.argv[1:])
