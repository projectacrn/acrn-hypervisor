Using Ubuntu as the Service OS with the ACRN hypervisor


Based on the ACRN open source project and getting started guide. This documentation is to guide people how to boot Ubuntu system as the 
Service OS with the ACRN hypervisor. By default, in the getting started guide documentation of the ACRN open source project, Clear Linux
is being used as the Service OS and User OS on top of the ACRN hypervisor. 
Here we are trying to boot Ubuntu system as the Service OS and launch Clear Linux as User OS with the ACRN hypervisor on official 
supported NUC, playing steps as below:

Step #1,  Installing a Ubuntu system on the NUC with standard installing steps. 
For example with Ubuntu 16.04.4 LTS distribution.
Download it here: https://www.ubuntu.com/download/desktop
Burn Ubuntu 16.04.4 image to a USB key
Install Ubuntu system with the USB key 
After complete installation and rebooting, you could boot Ubuntu from grub menu.

Step #2,  setup proxy with you prefered ways, command line tool or system settings tool.  If you don’t need to setup proxy, you could 
skip this step

Step #3,  installing ssh service with below commands and allow “root” login over SSH optional, 
People could just ssh into the Service OS and use 'sudo' once logged in (which would be preferable from a security standpoint), then 
restart SSH service. Of cause, this step is not must 
if you know how to play, just skip this step.
# sudo apt-get install openssh-server
# sudo service ssh status
In /etc/ssh/sshd_config 
Change PermitRootLogin from prohibit-password to yes
# sudo service ssh restart

Step #4, additional packages needed to install, please check the related sections of getting started guide in the ACRN open source
project, if you don’t have an chance to setup additional packages flowing up that guid to build source. You can reference below commands
to install directly here. 
# sudo apt-get update 
# sudo apt-get install git
# sudo apt-get install libpciaccess-dev
# sudo apt-get install gnu-efi
# sudo apt-get install uuid-dev
# sudo apt-get install libssl-dev
# sudo apt-get install vim
# sudo apt-get install libsystemd-dev
# sudo apt-get install libevent-dev
# sudo apt-get install libxml2-dev

Step #5, Checkout source code from open source project repositories
# git clone https://github.com/projectacrn/acrn-hypervisor
Note: if you encounter issues with “git clone”, you can download ZIP package directly. It’s so easy.

Step # 6, building the ACRN hypervisor and device model source code on Ubuntu system
Building device model and hypervisor from source trees on NUC you just setup. You can reference the section of Getting started guide 
of the ACRN open source project, or following up below commands to build directly here. 
# cd acrn-hypervisor
# make PLATFORM=uefi
The build results are found in the build directory.
For example:
# ls acrn-hypervisor/build
devicemodel
hypervisor
tools

Steps #7,  Preparing Service OS kernel 
You can download latest Service OS kernel from below links:
https://download.clearlinux.org/releases/current/clear/x86_64/os/Packages/

for example:
https://download.clearlinux.org/releases/current/clear/x86_64/os/Packages/linux-pk414-sos-4.14.41-39.x86_64.rpm
or
https://cdn.download.clearlinux.org/current/x86_64/os/Packages/linux-pk414-sos-4.14.41-39.x86_64.rpm

# mkdir ~/kernel-build
# cd ~/kernel-build
# wget  https://download.clearlinux.org/releases/current/clear/x86_64/os/Packages/linux-pk414-sos-4.14.41-39.x86_64.rpm
# sudo apt-get install rpm2cpio
# rpm2cpio linux-pk414-sos-4.14.41-39.x86_64.rpm | cpio -idmv 
# sudo cp -r ~/kernel-build/usr/lib/modules/4.14.41-39.pk414-sos/ /lib/modules/

Step #8, Deploying the hypervisor
Mount the EFI partition and verify you have the following files, then add the ACRN hypervisor and Clear Linux kernel to the EFI 
Partition, 
# umount /boot/efi 
# lsblk
# mount /dev/sda1 /mnt
# ls /mnt/EFI/ubuntu
  fw  fwupx64.efi  grub.cfg  grubx64.efi  MokManager.efi  shimx64.efi

Copy acrn.efi  to /mnt/EFI/acrn/ 
# sudo mkdir /mnt/EFI/acrn/
# sudo cp acrn-hypervisor/build/acrn.efi /mnt/EFI/acrn	

Step #9, Configure the EFI firmware to boot the ACRN hypervisor by default
# sudo -s
# efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L "ACRN Hypervisor                                                                                                    NUC" -u "bootloader=\EFI\ubuntu\grubx64.efi"
Then you can see the result of boot order. Also you can check the boot order with below command:
# efibootmgr -v            	
or setting boot order with this command to make sure the acrn.efi is the first boot option:
# efibootmgr -o x, x       	
Note: by default, the “ACRN Hypervisor” you just added should be the first one to boot. But please make sure it is, or select it 
manually after press “F10” when system booting up.   

Step #10, Deploy the SOS kernel
# sudo cp ~/kernel-build/usr/lib/kernel/org.clearlinux.pk414-sos.4.14.41-39 /boot/acrn/

Step #11, Customize the boot of the grub entry 
Use grub custom file (/etc/grub.d/40_custom) to create a new grub entry
Add the following to the end of grub file
menuentry 'ACRN ubuntu SOS' {
        recordfail
        load_video
        insmod gzio
        insmod part_gpt
        insmod ext4
        linux /boot/acrn/org.clearlinux.pk414-sos.4.14.41-39 pci_devices_ignore=(0:18:1) maxcpus=1 console=tty0 console=ttyS0 
        i915.nuclear_pageflip=1 root=PARTUUID=<ID of rootfs partition> rw rootwait ignore_loglevel no_timer_check consoleblank=0 
        i915.tsd_init=7 i915.tsd_delay=2000 i915.avail_planes_per_pipe=0x00000F i915.domain_plane_owners=0x011111110000 
        i915.enable_guc_loading=0 i915.enable_guc_submission=0 i915.enable_preemption=1 i915.context_priority_mode=2 
        i915.enable_gvt=1 hvlog=2M@0x1FE00000
}
Note: here you have to use PARTUUID, UUID doesn’t work.  

Step #12,  Update grub
# sudo update-grub
At this moment, you need to modify /boot/grub/grub.cfg file manually to enable the timeout so that the system has opportunity to show 
you grub menu to select. Or you need to set the “ACRN ubuntu SOS” as the default boot option. Without enabling  timeout, the system 
will not show grub menu for people to select and boot into incorrect kernel.

Enable timeout:
 if [ x$feature_timeout_style = xy ] ; then
 -  set timeout_style=hidden
 + #set timeout_style=hidden
 -  set timeout=0
 + set timeout = 10 
 # Fallback hidden-timeout code in case the timeout_style feature is
  # unavailable.

Step #13, Check the boot process 
Reboot system to see if the acrn.efi is loaded successfully, and if the Menu of Grub being shown to select “ACRN ubuntu SOS”.  
after boot successfully, the system will start to Ubuntu Desktop successfully.  to check if the system is running on virtualization 
evn and the ACNR hypervisor is running. 
For example:
   
NUC6CAYH:~$ dmesg | grep ACRN
[    0.000000] Hypervisor detected: ACRN
[    0.862942] ACRN HVLog: acrn_hvlog_init

This output means the boot flow is correct and the SOS system with ACRN is running well.
 

Step #14, Deploy Clear Linux as the UOS 
Download clear linux UOS from download.clearlinux.org
#cd /root
# wget https://download.clearlinux.org/releases/22780/clear/clear-22780-kvm.img.xz
# unxz clear-22780-kvm.img.xz
Copy  launch_uos.sh from source code
# cp ~/Download/acrn-hypervisor/devicemodel/samples/nuc/launch_uos.sh /root
Download standard PK kernel with below command:
# wget https://download.clearlinux.org/releases/22780/clear/x86_64/os/Packages/linux-pk414-standard-4.14.47-44.x86_64.rpm
# rpm2cpio linux-pk414-standard-4.14.47-44.x86_64.rpm | cpio -idmv

Use the following steps to update clearlinux kernel:
# sudo losetup -f -P --show /root/clear-22789-kvm.img  
# sudo mount /dev/loop0p3 /mnt
# sudo cp -r /root/usr/lib/modules/4.14.47-44.pk414-standard /mnt/lib/modules/
# sudo cp -r /root/usr/lib/kernel /lib/modules/
# sudo umount /mnt
# sync

You may Need to a permission issue
# sudo chmod 777 /dev/acrn_vhm
You also need to install a package.
# sudo apt-get instal iasl
# sudo cp /usr/bin/iasl /usr/sbin/iasl

Step 15, Deploy device module - acrn-dm
# cp ~/download/acrn-hypervisor//build/devicemodel/acrn-dm /usr/bin

Step 16, modify launch_uos.sh 
-s 3,virtio-blk,/root/clear-22780-kvm.img
-k /lib/modules/kernel/org.clearlinux.pk414-standard.4.14.47-44

Step 17, Run UOS
	# cd /root
	# ./launch_uos.sh

Congratulation !!! 
You are now see UOS booting up…..   
