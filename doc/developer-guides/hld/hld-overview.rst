.. _hld-overview:

ACRN high-level design overview
###############################

ACRN is an open source reference hypervisor (HV) running on top of Intel
Apollo Lake platforms for Software Defined Cockpit (SDC) or In-Vehicle
Experience (IVE) solutions. ACRN provides embedded hypervisor vendors
with a reference I/O mediation solution with a permissive license and
provides auto makers a reference software stack for in-vehicle use.

ACRN Supported Use Cases
************************

Software Defined Cockpit
========================

The SDC system consists of multiple systems: the instrument cluster (IC)
system, the In-vehicle Infotainment (IVI) system, and one or more rear
seat entertainment (RSE) systems.  Each system is run as a VM for better
isolation.

The Instrument Control (IC) system manages graphics display of

- driving speed, engine RPM, temperature, fuel level, odometer, trip mile, etc.
- alerts of low fuel or tire pressure
- rear-view camera(RVC) and surround-camera view for driving assistance.

In-Vehicle Infotainment
=======================

A typical In-Vehicle Infotainment (IVI) system would support:

- Navigation systems;
- Radios, audio and video playback;
- Mobile devices connection for  calls, music and applications via voice
  recognition and/or gesture Recognition / Touch.
- Rear-seat RSE services of entertainment system;
- virtual office;
- Connection to IVI front system and mobile devices (cloud
  connectivity).

ACRN supports guest OSes of Clear Linux and Android. OEMs can use the ACRN
hypervisor and Linux or Android guest OS reference code to implement their own
VMs for a customized IC/IVI/RSE.

Hardware Requirements
*********************

Mandatory IA CPU features are support for:

- Long mode
- MTRR
- TSC deadline timer
- NX, SMAP, SMEP
- Intel-VT including VMX, EPT, VT-d, APICv, VPID, invept and invvpid

Recommended Memory: 4GB, with 8GB preferred.
