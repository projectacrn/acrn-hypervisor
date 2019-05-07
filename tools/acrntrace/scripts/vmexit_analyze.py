#!/usr/bin/python3
# -*- coding: UTF-8 -*-

"""
This script defines the function to do the vm_exit analysis
"""

import csv
import struct

TSC_BEGIN = 0
TSC_END = 0
TOTAL_NR_EXITS = 0

VM_EXIT = 0x10
VM_ENTER = 0x11
VMEXIT_ENTRY = 0x10000

LIST_EVENTS = {
    'VMEXIT_EXCEPTION_OR_NMI':     VMEXIT_ENTRY + 0x00000000,
    'VMEXIT_EXTERNAL_INTERRUPT':   VMEXIT_ENTRY + 0x00000001,
    'VMEXIT_INTERRUPT_WINDOW':     VMEXIT_ENTRY + 0x00000002,
    'VMEXIT_CPUID':                VMEXIT_ENTRY + 0x00000004,
    'VMEXIT_RDTSC':                VMEXIT_ENTRY + 0x00000010,
    'VMEXIT_VMCALL':               VMEXIT_ENTRY + 0x00000012,
    'VMEXIT_CR_ACCESS':            VMEXIT_ENTRY + 0x0000001C,
    'VMEXIT_IO_INSTRUCTION':       VMEXIT_ENTRY + 0x0000001E,
    'VMEXIT_RDMSR':                VMEXIT_ENTRY + 0x0000001F,
    'VMEXIT_WRMSR':                VMEXIT_ENTRY + 0x00000020,
    'VMEXIT_EPT_VIOLATION':        VMEXIT_ENTRY + 0x00000030,
    'VMEXIT_EPT_MISCONFIGURATION': VMEXIT_ENTRY + 0x00000031,
    'VMEXIT_RDTSCP':               VMEXIT_ENTRY + 0x00000033,
    'VMEXIT_APICV_WRITE':          VMEXIT_ENTRY + 0x00000038,
    'VMEXIT_APICV_ACCESS':         VMEXIT_ENTRY + 0x00000039,
    'VMEXIT_APICV_VIRT_EOI':       VMEXIT_ENTRY + 0x0000003A,
    'VMEXIT_UNHANDLED': 0x20000
}

NR_EXITS = {
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
    'VMEXIT_EPT_MISCONFIGURATION': 0,
    'VMEXIT_RDTSCP': 0,
    'VMEXIT_APICV_WRITE': 0,
    'VMEXIT_UNHANDLED': 0
}

TIME_IN_EXIT = {
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
    'VMEXIT_EPT_MISCONFIGURATION': 0,
    'VMEXIT_RDTSCP': 0,
    'VMEXIT_APICV_WRITE': 0,
    'VMEXIT_UNHANDLED': 0
}

# 4 * 64bit per trace entry
TRCREC = "QQQQ"

def parse_trace_data(ifile):
    """parse the trace data file
    Args:
        ifile: input trace data file
    Return:
        None
    """

    global TSC_BEGIN, TSC_END, TOTAL_NR_EXITS
    last_ev_id = ''
    tsc_enter = 0
    tsc_exit = 0
    tsc_last_exit_period = 0

    fd = open(ifile, 'rb')

    while True:
        try:
            line = fd.read(struct.calcsize(TRCREC))
            if not line:
                break
            (tsc, event, d1, d2) = struct.unpack(TRCREC, line)

            event = event & 0xffffffffffff

            if event == VM_ENTER:
                if TSC_BEGIN == 0:
                    TSC_BEGIN = tsc
                    tsc_exit = tsc
                    TOTAL_NR_EXITS = 0

                tsc_enter = tsc
                TSC_END = tsc_enter
                tsc_last_exit_period = tsc_enter - tsc_exit

                if tsc_last_exit_period != 0:
                    TIME_IN_EXIT[last_ev_id] += tsc_last_exit_period

            elif event == VM_EXIT:
                tsc_exit = tsc
                TSC_END = tsc_exit
                TOTAL_NR_EXITS += 1

            else:
                for key in LIST_EVENTS.keys():
                    if event == LIST_EVENTS.get(key):
                        NR_EXITS[key] += 1
                        last_ev_id = key

                    else:
                        # Skip the non-VMEXIT trace event
                        pass

        except (IOError, struct.error) as e:
            sys.exit()

def generate_report(ofile, freq):
    """ generate analysis report
    Args:
        ofile: output report
        freq: TSC frequency of the device trace data from
    Return:
        None
    """
    global TSC_BEGIN, TSC_END, TOTAL_NR_EXITS

    csv_name = ofile + '.csv'
    try:
        with open(csv_name, 'a') as filep:
            f_csv = csv.writer(filep)

            total_exit_time = 0
            rt_cycle = TSC_END - TSC_BEGIN
            assert rt_cycle != 0, "total_run_time in cycle is 0,\
                                tsc_end %d, tsc_begin %d"\
                                % (TSC_END, TSC_BEGIN)

            rt_sec = float(rt_cycle) / (float(freq) * 1000 * 1000)

            for event in LIST_EVENTS:
                total_exit_time += TIME_IN_EXIT[event]

            print ("Total run time: %d cycles" % (rt_cycle))
            print ("TSC Freq: %s MHz" % (freq))
            print ("Total run time: %d sec" % (rt_sec))

            f_csv.writerow(['Run time(cycles)', 'Run time(Sec)', 'Freq(MHz)'])
            f_csv.writerow(['%d' % (rt_cycle),
                            '%.3f' % (rt_sec),
                            '%s' % (freq)])

            print ("%-28s\t%-12s\t%-12s\t%-24s\t%-16s" % ("Event", "NR_Exit",
                   "NR_Exit/Sec", "Time Consumed(cycles)", "Time percentage"))
            f_csv.writerow(['Exit_Reason',
                            'NR_Exit',
                            'NR_Exit/Sec',
                            'Time Consumed(cycles)',
                            'Time Percentage'])

            for event in LIST_EVENTS:
                ev_freq = float(NR_EXITS[event]) / rt_sec
                pct = float(TIME_IN_EXIT[event]) * 100 / float(rt_cycle)

                print ("%-28s\t%-12d\t%-12.2f\t%-24d\t%-16.2f" %
                       (event, NR_EXITS[event], ev_freq, TIME_IN_EXIT[event], pct))
                row = [event, NR_EXITS[event], '%.2f' % ev_freq, TIME_IN_EXIT[event],
                       '%2.2f' % (pct)]
                f_csv.writerow(row)

            ev_freq = float(TOTAL_NR_EXITS) / rt_sec
            pct = float(total_exit_time) * 100 / float(rt_cycle)
            print("%-28s\t%-12d\t%-12.2f\t%-24d\t%-16.2f"
                  % ("Total", TOTAL_NR_EXITS, ev_freq, total_exit_time, pct))
            row = ["Total", TOTAL_NR_EXITS, '%.2f' % ev_freq, total_exit_time,
                   '%2.2f' % (pct)]
            f_csv.writerow(row)

    except IOError as err:
        print ("Output File Error: " + str(err))

def analyze_vm_exit(ifile, ofile, freq):
    """do the vm exits analysis
    Args:
        ifile: input trace data file
        ofile: output report file
        freq: TSC frequency of the host where we capture the trace data
    Return:
        None
    """

    print("VM exits analysis started... \n\tinput file: %s\n"
          "\toutput file: %s.csv" % (ifile, ofile))

    parse_trace_data(ifile)
    # save report to the output file
    generate_report(ofile, freq)
