ACRN V0.1 release note


Main features supported in this release:

Acrnctl
Acrntrace
Acrnlog
sos lifecycle
vSBL
virtio-blk
virtio-net
USB passthru
CSE passthru
IOC sharing (incl. cbc attach, cbc driver)
IPU passthru
BT passthru
SD card passthru
audio passthru
pre-emption
surface sharing
multi-plane, multi-pipe
HDMI
eDP

Commands Supported by ACRN Hypervisor:

Command
help
Provides helpful description about all available commands in hypervisor shell.

vm_list
Lists available VMs and displays the VM name, ID, and running state (“ON” for a running VM, and “OFF” otherwise).

vm_console
Switches the console from the hypervisor shell to a VM’s shell.

set_loglevel
set_loglevel <console_loglevel> [mem_loglevel] - Set loglevel[0-6]
set log level so that people are able to get what they need from logout put, acrnlog defined 5 log levels, please refer to section 9.5 of Getting started guide 

get_loglevel
get the log level 

logdump
logdump <pcpu id> 

dump log

vmexit
show vmexit profiling 

dump_ioapic
show native ioapic info

vioapic
vioapic <vm_id>
show vioapic info

int
show interrupt info per CPU

lsreq
show ioreq info

pt
show pass-through device info

vcpu_dumpmem
vcpu_dumpmem <vm_id,vcpu_id, gva, length>

dump memory for a specific vcpu

vcpu_dumpreg
vcpu_dumpreg <vm_id, vcpu_id>
dump registers for a specific vcpu

vcpu_list
lists all VCPU in all VMs


Known Issues:

GPU – Preemption (Prioritized Rendering,
Batch Preemption, QoS Rendering)
Preemption feature works, but performance is not optimized yet. 

Wifi
Wifi not supported in guest OS

Audio
Audio pass-through to guest OS, but can only be validated on driver level, using command line or alsa-player, and only supports limited formats 

CSME
CSME pass-through to guest OS

Graphic
GVT-g is available, need to perform features after configured properly 

Camera/IPU
Camera works in Android guest. But camera app may crash after 5 mins

SD card
SD card works in Android guest, but does not support hot-plug.

Surface Sharing
Sometimes the window setup on Service OS takes up to 30 second

stability
Sometimes system hangs, especially when workload is high (e.g. running benchmarks, playing videos)

Change log:


