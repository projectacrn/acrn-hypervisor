.. _release_notes_0.2:

ACRN v0.2 (Sep 2018) DRAFT
##########################

We are pleased to announce the release of Project ACRN version 0.2.

ACRN is a flexible, lightweight reference hypervisor, built with
real-time and safety-criticality in mind, optimized to streamline
embedded development through an open source platform. Check out the
:ref:`introduction` for more information.

The project ACRN reference code can be found on GitHub in
https://github.com/projectacrn.  It includes the ACRN hypervisor, the
ACRN device model, and documentation.

Version 0.2 new features
************************

Fixed Issues
============
:acrn-issue:`663` - Black screen displayed after booting SOS/UOS

:acrn-issue:`676` - Hypervisor and DM version numbers incorrect

:acrn-issue:`1126` - VPCI coding style and bugs fixes for partition mode

:acrn-issue:`1125` - VPCI coding style and bugs fixes found in integration testing for partition mode

:acrn-issue:`1101` - missing acrn_mngr.h

:acrn-issue:`1071` - hypervisor cannot boot on skylake i5-6500

:acrn-issue:`1003` - CPU: cpu info is not correct

:acrn-issue:`971` -  acrncrashlog funcitons need to be enhance

:acrn-issue:`843` - ACRN boot failure

:acrn-issue:`721` - DM for IPU mediation

:acrn-issue:`707` - Issues found with instructions for using Ubuntu as SOS

:acrn-issue:`706` - Invisible mouse cursor in UOS

:acrn-issue:`424` - ClearLinux desktop GUI of SOS fails to launch


Known Issues
************

.. comment
   Use the syntax:

   :acrn-issue:`663` - Black screen displayed after booting SOS/UOS
     The ``weston`` display server, window manager, and compositor used by ACRN
     (from Clear Linux) may not have been properly installed and started.
     **Workaround** is described in ACRN GitHub issue :acrn-issue:`663`.


Change Log
**********

These commits have been added to the acrn-hypervisor repo since the v0.1
release in July 2018 (click on the CommitID link to see details):

.. comment

   This list is obtained from the command:
   git log --pretty=format:'- :acrn-commit:`%h` %s' --after="2018-03-01"
