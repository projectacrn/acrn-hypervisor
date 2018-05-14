#!/usr/bin/python2
# -*- coding: UTF-8 -*-

"""
This script defines the function to do the vm_exit analysis
"""

import csv

TSC_BEGIN = 0L
TSC_END = 0L
TOTAL_NR_EXITS = 0L

LIST_EVENTS = [
    'VMEXIT_EXCEPTION_OR_NMI',
    'VMEXIT_EXTERNAL_INTERRUPT',
    'VMEXIT_INTERRUPT_WINDOW',
    'VMEXIT_CPUID',
    'VMEXIT_RDTSC',
    'VMEXIT_VMCALL',
    'VMEXIT_CR_ACCESS',
    'VMEXIT_IO_INSTRUCTION',
    'VMEXIT_RDMSR',
    'VMEXIT_WRMSR',
    'VMEXIT_APICV_ACCESS',
    'VMEXIT_APICV_VIRT_EOI',
    'VMEXIT_EPT_VIOLATION',
    'VMEXIT_EPT_VIOLATION_GVT',
    'VMEXIT_EPT_MISCONFIGURATION',
    'VMEXIT_RDTSCP',
    'VMEXIT_APICV_WRITE',
    'VMEXIT_UNHANDLED'
]

NR_EXITS = {
    'VM_EXIT': 0,
    'VM_ENTER': 0,
    'VMEXIT_EXCEPTION_OR_NMI': 0,
    'VMEXIT_EXTERNAL_INTERRUPT': 0,
    'VMEXIT_INTERRUPT_WINDOW': 0,
    'VMEXIT_CPUID': 0,
    'VMEXIT_RDTSC': 0,
    'VMEXIT_VMCALL': 0,
    'VMEXIT_CR_ACCESS': 0,
    'VMEXIT_IO_INSTRUCTION': 0,
    'VMEXIT_RDMSR': 0,
    'VMEXIT_WRMSR': 0,
    'VMEXIT_APICV_ACCESS': 0,
    'VMEXIT_APICV_VIRT_EOI': 0,
    'VMEXIT_EPT_VIOLATION': 0,
    'VMEXIT_EPT_VIOLATION_GVT': 0,
    'VMEXIT_EPT_MISCONFIGURATION': 0,
    'VMEXIT_RDTSCP': 0,
    'VMEXIT_APICV_WRITE': 0,
    'VMEXIT_UNHANDLED': 0
}

TIME_IN_EXIT = {
    'VM_EXIT': 0,
    'VM_ENTER': 0,
    'VMEXIT_EXCEPTION_OR_NMI': 0,
    'VMEXIT_EXTERNAL_INTERRUPT': 0,
    'VMEXIT_INTERRUPT_WINDOW': 0,
    'VMEXIT_CPUID': 0,
    'VMEXIT_RDTSC': 0,
    'VMEXIT_VMCALL': 0,
    'VMEXIT_CR_ACCESS': 0,
    'VMEXIT_IO_INSTRUCTION': 0,
    'VMEXIT_RDMSR': 0,
    'VMEXIT_WRMSR': 0,
    'VMEXIT_APICV_ACCESS': 0,
    'VMEXIT_APICV_VIRT_EOI': 0,
    'VMEXIT_EPT_VIOLATION': 0,
    'VMEXIT_EPT_VIOLATION_GVT': 0,
    'VMEXIT_EPT_MISCONFIGURATION': 0,
    'VMEXIT_RDTSCP': 0,
    'VMEXIT_APICV_WRITE': 0,
    'VMEXIT_UNHANDLED': 0
}

IRQ_EXITS = {}

def count_irq(info):
    vec = info[5:15]
    if IRQ_EXITS.has_key(vec):
        IRQ_EXITS[vec] += 1
    else:
        IRQ_EXITS[vec] = 1

def parse_trace_data(ifile):
    """parse the trace data file
    Args:
        ifile: input trace data file
    Return:
        None
    """
    global TSC_BEGIN, TSC_END, TOTAL_NR_EXITS
    tsc_enter = 0L
    tsc_exit = 0L
    tsc_last_exit_period = 0L

    ev_id = ''
    last_ev_id = ''

    try:
        ifp = open(ifile)

        line = ifp.readline()

        # should preprocess to make sure first event is VM_ENTER
        while 1:
            line = ifp.readline()
            if line == '':
                ifp.close()
                break

            try:
                (cpuid, tsc, payload) = line.split(" | ")

                (ev_id, info) = payload.split(":")

            except ValueError, execp:
                print execp
                print line
                continue

            if ev_id == 'VM_ENTER':
                if TSC_BEGIN == 0:
                    TSC_BEGIN = long(tsc)
                    tsc_exit = long(tsc)
                    TOTAL_NR_EXITS = 0

                tsc_enter = long(tsc)
                TSC_END = tsc_enter
                tsc_last_exit_period = tsc_enter - tsc_exit
                if tsc_last_exit_period != 0:
                    TIME_IN_EXIT[last_ev_id] += tsc_last_exit_period
            elif ev_id == 'VM_EXIT':
                tsc_exit = long(tsc)
                TSC_END = tsc_exit
                TOTAL_NR_EXITS += 1
            elif ev_id.startswith('VMEXIT_'):
                if (ev_id == 'VMEXIT_EPT_VIOLATION'
                        and (eval(info[6:24]) & 0x38) == 0x28):
                    ev_id = 'VMEXIT_EPT_VIOLATION_GVT'

                if ev_id.startswith('VMEXIT_EX'):
                    count_irq(info)

                NR_EXITS[ev_id] += 1
                last_ev_id = ev_id
            else:
                # skip the non-VMEXIT trace event
                pass

    except IOError as err:
        print "Input File Error: " + str(err)

    finally:
        if 'ifp' in locals():
            ifp.close()

def generate_report(ofile, freq):
    """ generate analysis report
    Args:
        ofile: output report
        freq: CPU frequency of the device trace data from
    Return:
        None
    """
    global TSC_BEGIN, TSC_END, TOTAL_NR_EXITS

    csv_name = ofile + '.csv'
    try:
        with open(csv_name, 'w') as filep:
            f_csv = csv.writer(filep)

            total_exit_time = 0L
            rt_cycle = TSC_END - TSC_BEGIN
            assert rt_cycle != 0, "total_run_time in cycle is 0,\
                                tsc_end %d, tsc_begin %d"\
                                % (TSC_END, TSC_BEGIN)

            rt_sec = float(rt_cycle) / (float(freq) * 1000 * 1000)

            for event in LIST_EVENTS:
                total_exit_time += TIME_IN_EXIT[event]

            print "Total run time: %d (cycles)" % (rt_cycle)
            print "CPU Freq: %f MHz)" % (freq)
            print "Total run time %d (Sec)" % (rt_sec)

            f_csv.writerow(['Run time(cycles)', 'Run time(Sec)', 'Freq(MHz)'])
            f_csv.writerow(['%d' % (rt_cycle),
                            '%.3f' % (rt_sec),
                            '%d' % (freq)])

            print "Event \tNR_Exit \tNR_Exit/Sec \tTime Consumed \tTime Percentage"
            f_csv.writerow(['Exit_Reason',
                            'NR_Exit',
                            'NR_Exit/Sec',
                            'Time Consumed(cycles)',
                            'Time Percentage'])

            for event in LIST_EVENTS:
                ev_freq = float(NR_EXITS[event]) / rt_sec
                pct = float(TIME_IN_EXIT[event]) * 100 / float(rt_cycle)

                print ("%s \t%d \t%.2f \t%d \t%2.2f" %
                       (event, NR_EXITS[event], ev_freq, TIME_IN_EXIT[event], pct))
                row = [event, NR_EXITS[event], '%.2f' % ev_freq, TIME_IN_EXIT[event],
                       '%2.2f' % (pct)]
                f_csv.writerow(row)

            ev_freq = float(TOTAL_NR_EXITS) / rt_sec
            pct = float(total_exit_time) * 100 / float(rt_cycle)
            print("Total \t%d \t%.2f \t%d \t%2.2f"
                  % (TOTAL_NR_EXITS, ev_freq, total_exit_time, pct))
            row = ["Total", TOTAL_NR_EXITS, '%.2f' % ev_freq, total_exit_time,
                   '%2.2f' % (pct)]
            f_csv.writerow(row)

            # insert a empty row to separate two tables
            f_csv.writerow([''])

            print "\nVector \t\tCount \tNR_Exit/Sec"
            f_csv.writerow(['Vector', 'NR_Exit', 'NR_Exit/Sec'])
            for e in IRQ_EXITS.keys():
                pct = float(IRQ_EXITS[e]) / rt_sec
                print "%s \t %d \t%.2f" % (e,IRQ_EXITS[e], pct)
                f_csv.writerow([e, IRQ_EXITS[e], '%.2f' % pct])

    except IOError as err:
        print "Output File Error: " + str(err)

def get_freq(ifile):
    """ get cpu freq from the first line of trace file
    Args:
        ifile: input trace data file
    Return:
        cpu frequency
    """
    try:
        ifp = open(ifile)
        line = ifp.readline()
        freq = float(line[10:])

    except IOError as err:
        print "Failed to get cpu freq"
        freq = 1920.00

    finally:
        if 'ifp' in locals():
            ifp.close()

    return freq

def analyze_vm_exit(ifile, ofile):
    """do the vm exits analysis
    Args:
        ifile: input trace data file
        ofile: output report file
    Return:
        None
    """

    print("VM exits analysis started... \n\tinput file: %s\n"
          "\toutput file: %s.csv" % (ifile, ofile))

    freq = get_freq(ifile)

    parse_trace_data(ifile)
    # save report to the output file
    generate_report(ofile, freq)
