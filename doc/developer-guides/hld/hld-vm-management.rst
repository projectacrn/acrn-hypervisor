.. _hld-vm-management:

VM Management high-level design
###############################

Management of a Virtual Machine (VM) means to switch a VM to the right
state, according to the requirements of applications or system power
operations.

VM state
********

Generally, a VM is not running at the beginning: it is in a 'stopped'
state. After its UOS is launched successfully, the VM enter a 'running'
state. When the UOS powers off, the VM returns to a 'stopped' state again.
A UOS can sleep when it is running, so there is also a 'paused' state.

Because VMs are designed to work under an SOS environment, a VM can
only run and change its state when the SOS is running. A VM must be put to
'paused' or 'stopped' state before the SOS can sleep or power-off.
Otherwise the VM may be damaged and user data would be lost.

Scenarios of VM state change
****************************

Button-initiated System Power On
================================

When the user presses the power button to power on the system,
everything is started at the beginning. VMs that run user applications
are launched automatically after the SOS is ready.

Button-initiated VM Power on
============================

At SOS boot up, SOS-Life-Cycle-Service and Acrnd are automatically started
as system services. SOS-Life-Cycle-Service notifies Acrnd that SOS is
started, then Acrnd starts an Acrn-DM for launching each UOS, whose state
changes from 'stopped' to 'running'.

Button-initiated VM Power off
=============================

When SOS is about to shutdown, IOC powers off all VMs.
SOS-Life-Cycle-Service delays the SOS shutdown operation using heartbeat,
and waits for Acrnd to notify it can shutdown.

Acrnd keeps query states of all VMs. When all of them are 'stopped',
it notifies SOS-Life-Cycle-Service. SOS-Life-Cycle-Service stops the send delay
shutdown heartbeat, allowing SOS to continue the shutdown process.

RTC S3/S5 entry
===============

UOS asks Acrnd to resume/restart itself later by sending an RTC timer request,
and suspends/powers-off. SOS suspends/powers-off before that RTC
timer expires. Acrnd stores the RTC resume/restart time to a file, and
send the RTC timer request to SOS-Life-Cycle-Service.
SOS-Life-Cycle-Service sets the RTC timer to IOC. Finally, the SOS is
suspended/powered-off.

RTC S3/S5 exiting
=================

SOS is resumed/started by IOC RTC timer. SOS-Life-Cycle-Service notifies
Acrnd SOS has become alive again. Acrnd checks that the wakeup reason
was because SOS is resumed/started by IOC RTC. It then reads UOS
resume/restart time from the file, and resumes/restarts the UOS when
time is expired.

VM State management
*******************

Overview of VM State Management
===============================

Management of VMs on SOS uses the
SOS-Life-Cycle-Service, Acrnd, and Acrn-dm, working together and using
Acrn-Manager-AIP as IPC interface.

* The Lifecycle-Service get the Wakeup-Reason from IOC controller. It can set
  different power cycle method, and RTC timer, by sending a heartbeat to IOC
  with proper data.

* The Acrnd get Wakeup Reason from Lifecycle-Service and forwards it to
  Acrn-dm. It coordinates the lifecycle of VMs and SOS and handles IOC-timed
  wakeup/poweron.

* Acrn-Dm is the device model of a VM running on SOS. Virtual IOC
  inside Acrn-DM is responsible to control VM power state, usually triggered by Acrnd.

SOS Life Cycle Service
======================

SOS-Life-Cycle-Service (SOS-LCS) is a daemon service running on SOS.

SOS-LCS listens on ``/dev/cbc-lifecycle`` tty port to receive "wakeup
reason" information from IOC controller. SOS-LCS keeps reading system
status from IOC, to discover which power cycle method IOC is
doing. SOS-LCS should reply a heartbeat to IOC. This heartbeat can tell
IOC to keep doing this power cycle method, or change to another power
cycle method. SOS-LCS heartbeat can also set RTC timer to IOC.

SOS-LCS handles SHUTDOWN, SUSPEND, and REBOOT acrn-manager messages
request from Acrnd. When these messages are received, SOS-LCS switchs IOC
power cycle method to shutdown, suspend, and reboot, respectively.

SOS-LCS handles WAKEUP_REASON acrn-manager messages request from Acrnd.
When it receives this message, SOS-LCS sends "wakeup reason" to Acrnd.

SOS-LCS handles RTC_TIMER acrn-manager messages request from Acrnd.
When it receives this message, SOS-LCS setup IOC RTC timer for Acrnd.

SOS-LCS notifies Acrnd at the moment system becomes alive from other
status.

Acrnd
=====

Acrnd is a daemon service running on SOS.

Acrnd can start/resume VMs and query VM states for SOS-LCS, helping
SOS-LCS to decide which power cycle method is right. It also helps UOS
to be started/resumed by timer, required by S3/S5 feature.

Acrnd forwards wakeup reason to acrn-dm. Acrnd is responsible to retrieve
wakeup reason from SOS-LCS service and attach the wakeup reason to
acrn-dm parameter for ioc-dm.

When SOS is about to suspend/shutdown, SOS lifecycle service will send a
request to Acrnd to guarantee all guest VMs are suspended or shutdown
before SOS suspending/shutdown process continue. On receiving the
request, Acrnd starts polling the guest VMs state, and notifies SOS
lifecycle service when all guest VMs are put in proper state gracefully.

Guest UOS may need to
resume/start in a future time for some tasks. To
setup a timed resume/start, ioc-dm will send a request to acrnd to
maintain a list of timed requests from guest VMs. acrnd selects the
nearest request and sends it to SOS lifecycle service who will setup the
physical IOC.

Acrn-DM
=======

Acrn-Dm is the device model of VM running on SOS. Dm-IOC inside Acrn-DM
operates virtual IOC to control VM power state, and collects VM power
state information. Acrn-DM Monitor abstracts these Virtual IOC
functions into monitor-vm-ops, and allows Acrnd to use them via
Acrn-Manager IPC helper functions.

Acrn-manager IPC helper
=======================

SOS-LCS, Acrnd, and Acrn-DM use sockets to do IPC. Acrn-Manager IPC helper API
makes socket transparent for them. These are:

-  int mngr_open_un() - create a descriptor for vm management IPC
-  void mngr_close() - close descriptor and release the resources
-  int mngr_add_handler() - add a handler for message specified by message
-  int mngr_send_msg() - send a message and wait for acknowledgement
