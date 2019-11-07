.. _rtc-virt-hld:

RTC Virtualization
##################

This document describes the RTC virtualization implementation in
ACRN device model.

vRTC is a read-only RTC for the pre-launched VM, Service OS, and post-launched RT VM. It supports RW for the CMOS address port 0x70 and RO for the CMOS data port 0x71. Reads to the CMOS RAM offsets are fetched by reading the CMOS h/w directly and writes to CMOS offsets are discarded.
