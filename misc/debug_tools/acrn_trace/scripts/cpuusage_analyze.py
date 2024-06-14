#!/usr/bin/python3
# -*- coding: UTF-8 -*-

"""
This script defines the function to do cpu usage analysis
"""
import sys
import string
import struct
import csv
import os

cpu_id = 0
stat_tsc = 0

# map event TRACE_SCHED_NEXT defined in file trace.h
SCHED_NEXT = 0x20

# max number of vm is 16, another 1 is for hv
VM_NUM = 16
time_vm_running = [0] * (VM_NUM + 1)
count_all_trace = 0
count_sched_trace = 0

# Q: 64-bit tsc, Q: 64-bit event, 16c: char name[16]
TRCREC = "QQ16c"

def process_trace_data(ifile):
    """parse the trace data file
    Args:
        ifile: input trace data file
    Return:
        None
    """

    global stat_tsc, cpu_id, time_vm_running, count_all_trace, count_sched_trace

    # The duration of cpu running is tsc_end - tsc_begin
    tsc_begin = 0
    tsc_end = 0
    time_ambiguous = 0

    fd = open(ifile, 'rb')
    while True:
        try:
            count_all_trace += 1
            line = fd.read(struct.calcsize(TRCREC))
            if not line:
                break
            x = struct.unpack(TRCREC, line)
            if x[0] == 0:
                break
            tsc_end = x[0]
            if count_all_trace == 1:
                tsc_begin = tsc_end
                tsc_last_sched = tsc_begin
            event = x[1]
            cpu_id = event >> 56
            event = event & 0xffffffffffff
            if event == SCHED_NEXT:
                count_sched_trace += 1
                s=''
                for _ in list(x[2:]):
                    d = _.decode('ascii', errors='ignore')
                    s += d

                if s[:2] == "vm":
                    vm_prev = int(s[2])
                    if s[3] != ":":
                        vm_prev = vm_prev*10 + int(s[3])
                elif s[:4] == "idle":
                    vm_prev = VM_NUM
                else:
                    print("Error: trace data is not correct!")
                    return

                if s[4:6] == "vm":
                    vm_next = int(s[6])
                    if s[7] != ":":
                        vm_next = vm_next*10 + int(s[7])
                elif s[4:8] == "idle":
                    vm_next = VM_NUM
                else:
                    print("Error: trace data is not correct!")
                    return

                if (count_sched_trace == 1) or (vm_prev == vm_prev_last):
                    time_vm_running[vm_prev] += tsc_end - tsc_last_sched
                else:
                    print("line %d: last_next =vm%d, current_prev=vm%d" % (count_all_trace, vm_prev, vm_prev_last))
                    print("Warning: last schedule next is not the current task. Trace log is lost.")
                    time_ambiguous += tsc_end - tsc_last_sched

                tsc_last_sched = tsc_end
                vm_prev_last = vm_next

        except (IOError, struct.error) as e:
            sys.exit()

    print ("Start trace %d tsc cycle" % (tsc_begin))
    print ("End trace %d tsc cycle" % (tsc_end))
    stat_tsc = tsc_end - tsc_begin
    assert stat_tsc != 0, "total_run_time in statistic is 0,\
                           tsc_end %d, tsc_begin %d"\
                           % (tsc_end, tsc_begin)

    if count_sched_trace == 0:
        print ("There is no context switch in HV scheduling during this period. "
               "This CPU may be exclusively owned by one vm.\n"
               "The CPU usage is 100%")
        return
    if time_ambiguous > 0:
        print("Warning: ambiguous running time: %d tsc cycle, occupying %2.2f%% cpu."
                % (time_ambiguous, float(time_ambiguous)*100/stat_tsc))

    # the last time
    time_vm_running[vm_next] += tsc_end - tsc_last_sched


def generate_report(ofile, freq):
    """ generate analysis report
    Args:
        ofile: output report
        freq: TSC frequency of the device trace data from
    Return:
        None
    """
    global stat_tsc, cpu_id, time_vm_running, count_all_trace, count_sched_trace

    if (count_sched_trace == 0):
        return

    csv_name = ofile + '.csv'
    try:
        with open(csv_name, 'a') as filep:
            f_csv = csv.writer(filep)

            stat_sec = float(stat_tsc) / (float(freq) * 1000 * 1000)
            print ("Total run time: %d cpu cycles" % (stat_tsc))
            print ("TSC Freq: %s MHz" % (freq))
            print ("Total run time: %.2f sec" % (stat_sec))
            print ("Total trace items: %d" % (count_all_trace))
            print ("Total scheduling trace: %d" % (count_sched_trace))

            f_csv.writerow(['Total run time(tsc cycles)', 'Total run time(sec)', 'Freq(MHz)'])
            f_csv.writerow(['%d' % (stat_tsc),
                            '%.2f' % (stat_sec),
                            '%s' % (freq)])

            print ("%-28s\t%-12s\t%-12s\t%-24s\t%-16s" % ("PCPU ID", "VM ID",
                   "VM Running/sec", "VM Running(tsc cycles)", "CPU Usage"))
            f_csv.writerow(['PCPU ID',
                            'VM_ID',
                            'Time Consumed/sec',
                            'Time Consumed(tsc cycles)',
                            'CPU Usage%'])

            for vmid, tsc in enumerate(time_vm_running):
                run_tsc = tsc
                run_per = float(run_tsc) * 100 / float(stat_tsc)
                run_sec = float(run_tsc) / (float(freq) * 1000 * 1000)
                if vmid != VM_NUM:
                    print ("%-28d\t%-12d\t%-10.2f\t%-24d\t%-2.2f%%" %
                           (cpu_id, vmid, run_sec, run_tsc, run_per))
                    row = [cpu_id, vmid, '%.2f' % (run_sec), run_tsc, '%2.2f' % (run_per)]
                else:
                    print ("%-28d\t%-12s\t%-10.2f\t%-24d\t%-2.2f%%" %
                           (cpu_id, 'Idle', run_sec, run_tsc, run_per))
                    row = [cpu_id, 'Idle', '%.2f' % (run_sec), run_tsc, '%2.2f' % (run_per)]
                f_csv.writerow(row)

    except IOError as err:
        print ("Output File Error: " + str(err))

def analyze_cpu_usage(ifile, ofile, freq):
    """do cpu usage analysis of each vm
    Args:
        ifile: input trace data file
        ofile: output report file
        freq: TSC frequency of the host where we capture the trace data
    Return:
        None
    """

    print("VM CPU usage analysis started... \n\tinput file: %s\n"
          "\toutput file: %s.csv" % (ifile, ofile))

    if os.stat(ifile).st_size == 0:
        print("The input trace file is empty. The corresponding CPU may be offline.")
        return
    if float(freq) == 0.0:
        print("The input cpu frequency cannot be zero!")
        return
    process_trace_data(ifile)
    # print cpu usage of each vm
    generate_report(ofile, freq)
