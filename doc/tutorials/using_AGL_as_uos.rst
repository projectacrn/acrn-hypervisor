.. _acrn_doc:

Using AGL as the User OS
#############################

In this article we will discusse the way to run AGL as a Guest OS 
on ACRN hypervisor and the problems we got at current stage. 
We hope the steps documented in this article could help us to 
reproduce the problem much easier and provide some information 
for further debugging.

   .. image:: images/The-overview-of-AGL-as-UOS.png
      :align: center

Overview
**********************

Automotive Grade Linux (AGL) is an open source project of The Linux 
Foundation that is building a Linux-based, open software platform for 
automotive application.

For more information about AGL, please visit AGL’s official website:
https://www.automotivelinux.org/

Setup AGL as a Guest OS in ACRN
*******************************

#. Hardware preparation

    The regulatory model of NUC we used is “NUC6CAYH”, and for more 
    information about this kind of NUC, please visit the official website:
    https://www.intel.com/content/www/us/en/products/boards-kits/nuc/kits/
    nuc6cayh.html. First we need to prepare 2 displays, one for SOS and one 
    for UOS, and connect these 2 displays to NUC as picture below.

       .. image:: images/The-displayports-of-NUC.png
          :align: center


#. Setup ACRN SOS

    Follow the instructions found in the Getting started guide for Intel NUC 
    to setup SOS. https://projectacrn.github.io/latest/getting-started/apl-nuc.html#


#. Setup ACRN UOS
  To launch AGL as UOS, we need to download the image of AGL from:
  https://download.automotivelinux.org/AGL/release/eel/5.1.0/intel-corei7-64/
  deploy/images/intel-corei7-64/agl-demo-platform-crosssdk-intel-corei7-64.wic.xz

  Here we use version “eel_5.1.0”, and you can try other release of AGL in ACRN.

  .. code-block: none

     projectacrn/
        $ cd ~
        $ wget https://download.automotivelinux.org/AGL/release/eel/5.1.0/intel-
        corei7-64/deploy/images/intel-corei7-64/agl-demo-platform-crosssdk-intel-
        corei7-64.wic.xz
        $ unxz agl-demo-platform-crosssdk-intel-corei7-64.wic.xz
        
        
  You need to adjust the ``/usr/share/acrn/samples/nuc/launch_uos.sh`` script
  to match your installation. These are the couple of lines you need to modify:

  .. code-block:: none

     -s 3,virtio-blk,/root/agl-demo-platform-crosssdk-intel-corei7-64-20180726071132.rootfs.wic \
     -B "root=/dev/vda2 
     
#. Start the User OS (UOS)

  You are now all set to start the User OS (UOS)

  .. code-block:: none
     sudo /usr/share/acrn/samples/nuc/launch_uos.sh

  **Congratulations**, you are now watching the User OS booting up!

  And you should be able to see the console of AGL:
    .. code-block:: none
     sudo /usr/share/acrn/samples/nuc/launch_uos.sh
     
     
     When you see the output of the console above, that means AGL has been 
     loaded and now you could operate on the console. 
