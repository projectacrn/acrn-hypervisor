acrnctl
#######

DESCRIPTION
###########
acrnctl: The acrnctl can help user to create, delete, launch and stop UOSs.
It runs under Service OS, and UOSs should be based on acrn-dm 

USAGE
#####
To see what it can do, just run:
        # acrnctl
or
        # acrnctl help
you may see:
        support:
                list
                start
                stop
                del
                add
        Use acrnctl [cmd] help for details

There are examples:
(1) add a VM
    Each time you can just add one VM. Suppose you have an UOS
    launch script, such as launch_UOS.sh
    you can run:
	# acrnctl add launch_UOS.sh -U 1
        vm1-14:59:30 added
    Note that, launch script shoud be able to launch ONE UOS. If
    it fail, it is better to print some error logs, to tell user
    the reason, so that he knows how to solve it.
    The vmname is important, the acrnctl searchs VMs by their
    names. so duplicated VM names are not allowed. Beside, if the
    launch script changes VM name at launch time, acrnctl will
    not recgonize it.
(2) delete VMs
        # acrnctl del vm1-14:59:30
(3) show VMs
        # acrnctl list
        vm1-14:59:30            untracked
        vm-yocto                stop
        vm-android              stop
(4) start VM
    you can start a vm with 'stop' status, each time can start
    one VM.
        # acrnctl start vm-yocto
(5) stop VM
    you can stop VMs, if their status is not 'stop'
        # acrnctl stop vm-yocto vm1-14:59:30 vm-android
BUILD
#####
# make
