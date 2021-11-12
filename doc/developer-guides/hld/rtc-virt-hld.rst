.. _rtc-virt-hld:

RTC Virtualization
##################

This document describes the real-time clock (RTC) virtualization implementation
in the ACRN Device Model.

vRTC is a read-only RTC for the pre-launched VM, Service VM, and post-launched
RTVM. It supports read/write (RW) for the CMOS address port 0x70 and read only
(RO) for the CMOS data port 0x71. Reads to the CMOS RAM offsets are fetched from
the CMOS hardware directly. Writes to the CMOS offsets are discarded.
