# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import launch_cfg_lib
import pt


def is_nuc_clr(names, vmid):
    uos_type = names['uos_types'][vmid]
    board_name = names['board_name']

    if uos_type == "CLEARLINUX" and 'nuc' in board_name:
        return True

    return False


def tap_uos_net(names, vmid, config):
    uos_type = names['uos_types'][vmid]
    board_name = names['board_name']

    if uos_type in ("CLEARLINUX", "ANDROID", "ALIOS"):
        if board_name in ("apl-mrb", "apl-up2"):
            print('if [ ! -f "/data/$3/$3.img" ]; then', file=config)
            print('  echo "no /data/$3/$3.img, exit"', file=config)
            print("  exit", file=config)
            print("fi", file=config)
            print("", file=config)
        print("#vm-name used to generate uos-mac address", file=config)
        print("mac=$(cat /sys/class/net/e*/address)", file=config)
        print("vm_name=vm$1", file=config)
        print("mac_seed=${mac:9:8}-${vm_name}", file=config)
        print("", file=config)

    if uos_type in ("VXWORKS", "ZEPHYR", "WINDOWS"):
        print("vm_name={}_vm$1".format(uos_type), file=config)

    if uos_type in ("CLEARLINUX", "ANDROID", "ALIOS"):
        if board_name in ("apl-mrb", "apl-up2"):
            print("# create a unique tap device for each VM", file=config)
            print("tap=tap_$4", file=config)
            print('tap_exist=$(ip a | grep "$tap" | awk \'{print $1}\')', file=config)
            print('if [ "$tap_exist"x != "x" ]; then', file=config)
            print('  echo "tap device existed, reuse $tap"', file=config)
            print("else", file=config)
            print("  ip tuntap add dev $tap mode tap", file=config)
            print("fi", file=config)
            print("", file=config)
            print("# if acrn-br0 exists, add VM's unique tap device under it", file=config)
            print("br_exist=$(ip a | grep acrn-br0 | awk '{print $1}')", file=config)
            print('if [ "$br_exist"x != "x" -a "$tap_exist"x = "x" ]; then', file=config)
            print('  echo "acrn-br0 bridge aleady exists, adding new tap device to it..."', file=config)
            print('  ip link set "$tap" master acrn-br0', file=config)
            print('  ip link set dev "$tap" down', file=config)
            print('  ip link set dev "$tap" up', file=config)
            print("fi", file=config)
            print("", file=config)
    print("#check if the vm is running or not", file=config)
    print("vm_ps=$(pgrep -a -f acrn-dm)", file=config)
    print('result=$(echo $vm_ps | grep -w "${vm_name}")', file=config)
    print('if [[ "$result" != "" ]]; then', file=config)
    print('  echo "$vm_name is running, can\'t create twice!"', file=config)
    print("  exit", file=config)
    print("fi", file=config)
    print("", file=config)


def delay_use_usb_storage(uos_type, config):
    if uos_type == "CLEARLINUX":
        print("echo 100 > /sys/bus/usb/drivers/usb-storage/module/parameters/delay_use", file=config)


def off_line_cpus(uos_type, config):

    print('offline_path="/sys/class/vhm/acrn_vhm"', file=config)
    print("", file=config)

    if uos_type in ("ANDROID", "CLEARLINUX", "ALIOS"):
        print("# Check the device file of /dev/acrn_hsm to determine the offline_path", file=config)
        print('if [ -e "/dev/acrn_hsm" ]; then', file=config)
        print('offline_path="/sys/class/acrn/acrn_hsm"', file=config)
        print('fi', file=config)
        print("", file=config)
    print("# offline SOS CPUs except BSP before launch UOS", file=config)
    print("for i in `ls -d /sys/devices/system/cpu/cpu[1-99]`; do", file=config)
    print("        online=`cat $i/online`", file=config)
    print('        idx=`echo $i | tr -cd "[1-99]"`', file=config)
    print("        echo cpu$idx online=$online", file=config)
    print('        if [ "$online" = "1" ]; then', file=config)
    print("                echo 0 > $i/online", file=config)

    if uos_type in ("ANDROID", "CLEARLINUX", "ALIOS"):
        print("             online=`cat $i/online`", file=config)
        print("             # during boot time, cpu hotplug may be disabled by pci_device_probe during a pci module insmod", file=config)
    if uos_type != "PREEMPT-RT LINUX":
        print('             while [ "$online" = "1" ]; do', file=config)
        print("                     sleep 1", file=config)
        print("                     echo 0 > $i/online", file=config)
        print("                     online=`cat $i/online`", file=config)
        print("             done", file=config)
    print("                echo $idx > ${offline_path}/offline_cpu", file=config)
    print("        fi", file=config)
    print("done", file=config)
    print("", file=config)


def run_container(board_name, config):
    if board_name != "apl-mrb":
        return

    print("function run_container()", file=config)
    print("{", file=config)
    print("vm_name=vm1", file=config)
    print('config_src="/usr/share/acrn/samples/apl-mrb/runC.json"', file=config)
    print('shell="/usr/share/acrn/conf/add/$vm_name.sh"', file=config)
    print('arg_file="/usr/share/acrn/conf/add/$vm_name.args"', file=config)
    print('runc_bundle="/usr/share/acrn/conf/add/runc/$vm_name"', file=config)
    print('rootfs_dir="/usr/share/acrn/conf/add/runc/rootfs"', file=config)
    print('config_dst="$runc_bundle/config.json"', file=config)
    print("", file=config)
    print("", file=config)
    print("input=$(runc list -f table | awk '{print $1}''{print $3}')", file=config)
    print("arr=(${input// / })", file=config)
    print("", file=config)
    print("for((i=0;i<${#arr[@]};i++))", file=config)
    print("do", file=config)
    print('	if [ "$vm_name" = "${arr[$i]}" ]; then', file=config)
    print('		if [ "running" = "${arr[$i+1]}" ]; then', file=config)
    print('			echo "runC instance ${arr[$i]} is running"', file=config)
    print("			exit", file=config)
    print("		else", file=config)
    print("			runc kill ${arr[$i]}", file=config)
    print("			runc delete ${arr[$i]}", file=config)
    print("		fi", file=config)
    print("	fi", file=config)
    print("done", file=config)
    print("vmsts=$(acrnctl list)", file=config)
    print("vms=(${vmsts// / })", file=config)
    print("for((i=0;i<${#vms[@]};i++))", file=config)
    print("do", file=config)
    print('	if [ "$vm_name" = "${vms[$i]}" ]; then', file=config)
    print('		if [ "stopped" != "${vms[$i+1]}" ]; then', file=config)
    print('			echo "Uos ${vms[$i]} ${vms[$i+1]}"', file=config)
    print("			acrnctl stop ${vms[$i]}", file=config)
    print("		fi", file=config)
    print("	fi", file=config)
    print("done", file=config)


def boot_image_type(args, vmid, config):

    if not args['vbootloader'][vmid] or (args['vbootloader'][vmid] and args['vbootloader'][vmid] != "vsbl"):
        return

    print('boot_dev_flag=",b"', file=config)
    print("if [ $5 == 1 ];then", file=config)
    print('  boot_image_option="--vsbl /usr/share/acrn/bios/VSBL_debug.bin"', file=config)
    print("else", file=config)
    print('  boot_image_option="--vsbl /usr/share/acrn/bios/VSBL.bin"', file=config)
    print("fi", file=config)
    print("", file=config)


def interrupt_storm(names, vmid, config):
    uos_type = names['uos_types'][vmid]

    if uos_type not in ("CLEARLINUX", "ANDROID", "ALIOS") or is_nuc_clr(names, vmid):
        return

    print("#interrupt storm monitor for pass-through devices, params order:", file=config)
    print("#threshold/s,probe-period(s),intr-inject-delay-time(ms),delay-duration(ms)", file=config)
    print('intr_storm_monitor="--intr_monitor 10000,10,1,100"', file=config)
    print("", file=config)


def gvt_arg_set(uos_type, config):

    if uos_type not in ('CLEARLINUX', 'ANDROID', 'ALIOS', 'WINDOWS'):
        return
    print('   -s 2,pci-gvt -G "$2"  \\', file=config)


def log_level_set(uos_type, config):

    if uos_type not in ('CLEARLINUX', 'ANDROID', 'ALIOS'):
        return
    print("#logger_setting, format: logger_name,level; like following", file=config)
    print('logger_setting="--logger_setting console,level=4;kmsg,level=3;disk,level=5"', file=config)
    print("", file=config)


def launch_begin(uos_type, config):
    launch_uos = '_'.join(uos_type.lower().split())
    print("function launch_{}()".format(launch_uos), file=config)
    print("{", file=config)


def wa_usage(uos_type, config):
    if uos_type in ("ANDROID", "ALIOS"):
        print("# WA for USB role switch hang issue, disable runtime PM of xHCI device", file=config)
        print("echo on > /sys/devices/pci0000:00/0000:00:15.0/power/control", file=config)
        print("", file=config)

def mem_size_set(names, args, vmid, config):

    uos_type = names['uos_types'][vmid]
    #board_name = names['board_name']
    mem_size = args['mem_size'][vmid]

    if uos_type not in ("CLEARLINUX", "ANDROID", "ALIOS") or is_nuc_clr(names, vmid):
        print("mem_size={}M".format(mem_size), file=config)
        return

    print("#for memsize setting, total 8GB(>7.5GB) uos->6GB, 4GB(>3.5GB) uos->2GB", file=config)
    print("memsize=`cat /proc/meminfo|head -n 1|awk '{print $2}'`", file=config)
    print("if [ $memsize -gt 7500000 ];then", file=config)
    print("    mem_size=6G", file=config)
    print("elif [ $memsize -gt 3500000 ];then", file=config)
    print("    mem_size=2G", file=config)
    print("else", file=config)
    print("    mem_size=1750M", file=config)
    print("fi", file=config)
    print("", file=config)
    print('if [ "$setup_mem" != "" ];then', file=config)
    print("    mem_size=$setup_mem", file=config)
    print("fi", file=config)
    print("", file=config)


def function_help(config):
    print("function help()", file=config)
    print("{", file=config)
    print('echo "Use luanch_uos.sh like that ./launch_uos.sh -V <#>"', file=config)
    print('echo "The option -V means the UOSs group to be launched by vsbl as below"', file=config)
    print('echo "-V 1 means just launching 1 clearlinux UOS"', file=config)
    print('echo "-V 2 means just launching 1 android UOS"', file=config)
    print('echo "-V 4 means launching 2 clearlinux UOSs"', file=config)
    print('echo "-V 5 means just launching 1 alios UOS"', file=config)
    print('echo "-V 6 means auto check android/linux/alios UOS; if exist, launch it"', file=config)
    print("}", file=config)
    print("", file=config)


def uos_launch(names, args, vmid, config):

    gvt_args = args['gvt_args'][vmid]
    uos_type = names['uos_types'][vmid]
    launch_uos = '_'.join(uos_type.lower().split())

    if uos_type in ("CLEARLINUX", "ANDROID", "ALIOS") and not is_nuc_clr(names, vmid):

        print("", file=config)
        print("case $launch_type in", file=config)
        print('	1) echo "Launch clearlinux UOS"', file=config)
        print('		launch_clearlinux 1 "{}" clearlinux "LaaG" $debug'.format(gvt_args), file=config)
        print("		;;", file=config)
        print('	2) echo "Launch android UOS"', file=config)
        print('		launch_android 1 "{}" android "AaaG" $debug'.format(gvt_args), file=config)
        print("		;;", file=config)
        print('	4) echo "Launch two clearlinux UOSs"', file=config)
        print('		launch_clearlinux 1 "{}" clearlinux "L1aaG" $debug &'.format(gvt_args), file=config)
        print("		sleep 5", file=config)
        print('		launch_clearlinux 2 "{}" clearlinux_dup "L2aaG" $debug'.format(gvt_args), file=config)
        print("		;;", file=config)
        print('	5) echo "Launch alios UOS"', file=config)
        print('		launch_alios 1 "{}" alios "AliaaG" $debug'.format(gvt_args), file=config)

        print("		;;", file=config)
        print("esac", file=config)
        print("", file=config)
        print("umount /data", file=config)

    if uos_type not in ("CLEARLINUX", "ANDROID", "ALIOS"):
        if uos_type == "VXWORKS":
            print("launch_{} 1".format(launch_uos), file=config)
        if uos_type == "PREEMPT-RT LINUX":
            print("launch_{}".format(launch_uos), file=config)
        if uos_type == "WINDOWS":
            print('launch_{} 1 "{}"'.format(launch_uos, gvt_args), file=config)
        if uos_type == "ZEPHYR":
            print("launch_{} 1".format(launch_uos), file=config)

    if is_nuc_clr(names, vmid):
        print('if [ "$1" = "-C" ];then', file=config)
        print('    echo "runc_container"', file=config)
        print("    run_container", file=config)
        print("else", file=config)
        print('    launch_{} 1 "{}"'.format(launch_uos, gvt_args), file=config)
        print("fi", file=config)


def launch_type(names, args, vmid, config):

    uos_type = names['uos_types'][vmid]
    #board_name = names['board_name']
    mem_size = args["mem_size"][vmid]

    if uos_type in ("ANDROID", "ALIOS") and not is_nuc_clr(names, vmid):
        function_help(config)

    if uos_type in ("CLEARLINUX", "ANDROID", "ALIOS") and not is_nuc_clr(names, vmid):
        print("launch_type=1", file=config)
        print("debug=0", file=config)
        print("", file=config)
        print('while getopts "V:M:hd" opt', file=config)
        print("do", file=config)
        print("	case $opt in", file=config)
        print("		V) launch_type=$[$OPTARG]", file=config)
        print("			;;", file=config)

        if not mem_size:
            print("		M) setup_mem=$OPTARG", file=config)
        else:
            print("		M) setup_mem={}M".format(mem_size), file=config)
        print("			;;", file=config)
        print("		d) debug=1", file=config)
        print("			;;", file=config)
        print("		h) help", file=config)
        print("			exit 1", file=config)
        print("			;;", file=config)
        print("		?) help", file=config)
        print("			exit 1", file=config)
        print("			;;", file=config)
        print("	esac", file=config)
        print("done", file=config)
        print("", file=config)

    if uos_type in ("CLEARLINUX", "ANDROID", "ALIOS") and not is_nuc_clr(names, vmid):
        root_fs = args['rootfs_dev'][vmid]

        print('if [ ! -b "{}" ]; then'.format(root_fs), file=config)
        print('  echo "no {} data partition, exit"'.format(root_fs), file=config)
        print("  exit", file=config)
        print("fi", file=config)
        print("", file=config)
        print("mkdir -p /data", file=config)
        print("mount {} /data".format(root_fs), file=config)
        print("", file=config)
        print("if [ $launch_type == 6 ]; then", file=config)
        print('	if [ -f "/data/android/android.img" ]; then', file=config)
        print("	  launch_type=2;", file=config)
        print('	elif [ -f "/data/alios/alios.img" ]; then', file=config)
        print("	  launch_type=5;", file=config)
        print("	else", file=config)
        print("	  launch_type=1;", file=config)
        print("	fi", file=config)
        print("fi", file=config)
        print("", file=config)

    off_line_cpus(uos_type, config)

    uos_launch(names, args, vmid, config)


def launch_end(names, args, vmid, config):
    #uos_type = names['uos_types']
    launch_type(names, args, vmid, config)


def set_dm_pt(names, sel, vmid, config):

    uos_type = names['uos_types'][vmid]

    if sel.bdf['usb_xdci'][vmid] and sel.slot['usb_xdci'][vmid]:
        print('   -s {},passthru,{}/{}/{} \\'.format(sel.slot["usb_xdci"][vmid], sel.bdf["usb_xdci"][vmid][0:2],\
            sel.bdf["usb_xdci"][vmid][3:5], sel.bdf["usb_xdci"][vmid][6:7]), file=config)

    if uos_type in ("ANDROID", "ALIOS"):
        print("   $boot_audio_option \\", file=config)
    if uos_type == "WINDOWS":
        if sel.bdf['audio'][vmid] and sel.slot['audio'][vmid]:
            print("   -s {},passthru,{}/{}/{}  \\".format(
                launch_cfg_lib.virtual_dev_slot("win_audio"),
                sel.bdf['audio'][vmid][0:2], sel.bdf['audio'][vmid][3:5],
                sel.bdf['audio'][vmid][6:7]), file=config)

    if sel.bdf['cse'][vmid] and sel.slot['cse'][vmid]:
        print("   $boot_cse_option \\", file=config)

    if sel.bdf["sd_card"][vmid] and sel.slot['sd_card'][vmid]:
        print('   -s {},passthru,{}/{}/{} \\'.format(sel.slot["sd_card"][vmid], sel.bdf["sd_card"][vmid][0:2], \
            sel.bdf["sd_card"][vmid][3:5], sel.bdf["sd_card"][vmid][6:7]), file=config)

    if sel.bdf['bluetooth'][vmid] and sel.slot['bluetooth'][vmid]:
        print('   -s {},passthru,{}/{}/{} \\'.format(sel.slot["bluetooth"][vmid], sel.bdf["bluetooth"][vmid][0:2], \
            sel.bdf["bluetooth"][vmid][3:5], sel.bdf["bluetooth"][vmid][6:7]), file=config)

    if sel.bdf['wifi'][vmid] and sel.slot['wifi'][vmid]:
        print("   -s {},passthru,{}/{}/{},keep_gsi \\".format(sel.slot["wifi"][vmid], sel.bdf["wifi"][vmid][0:2], \
            sel.bdf["wifi"][vmid][3:5], sel.bdf["wifi"][vmid][6:7]), file=config)

    if sel.bdf['ipu'][vmid] or sel.bdf['ipu_i2c'][vmid]:
        print("   $boot_ipu_option      \\", file=config)

    if sel.bdf['ethernet'][vmid] and sel.slot['ethernet'][vmid]:
        print("   -s {},passthru,{}/{}/{} \\".format(sel.slot["ethernet"][vmid], sel.bdf["ethernet"][vmid][0:2], \
            sel.bdf["ethernet"][vmid][3:5], sel.bdf["ethernet"][vmid][6:7]), file=config)

    if sel.bdf['sata'] and sel.slot["sata"][vmid]:
        print("   -s {},passthru,{}/{}/{} \\".format(sel.slot["sata"][vmid], sel.bdf["sata"][vmid][0:2], \
            sel.bdf["sata"][vmid][3:5], sel.bdf["sata"][vmid][6:7]), file=config)

    if sel.bdf['nvme'] and sel.slot["nvme"][vmid]:
        print("   -s {},passthru,{}/{}/{} \\".format(sel.slot["nvme"][vmid], sel.bdf["nvme"][vmid][0:2], \
            sel.bdf["nvme"][vmid][3:5], sel.bdf["nvme"][vmid][6:7]), file=config)


def dm_arg_set(names, sel, dm, vmid, config):

    uos_type = names['uos_types'][vmid]
    board_name = names['board_name']

    # vbootlaoder for vsbl
    boot_image_type(dm, vmid, config)

    # uuid get
    scenario_uuid = launch_cfg_lib.get_scenario_uuid()
    sos_vmid = launch_cfg_lib.get_sos_vmid()

    # clearlinux/android/alios
    dm_str = 'acrn-dm -A -m $mem_size -s 0:0,hostbridge  -s 1:0,lpc -U {}'.format(scenario_uuid[vmid + sos_vmid])
    if uos_type in ("CLEARLINUX", "ANDROID", "ALIOS"):
        if uos_type == "CLEARLINUX":
            print("{} \\".format(dm_str), file=config)
        else:
            print('{} $npk_virt \\'.format(dm_str), file=config)

    if board_name == "apl-up2" or is_nuc_clr(names, vmid):
        print("   $logger_setting \\", file=config)

    if uos_type in ("CLEARLINUX", "ANDROID", "ALIOS"):
        if uos_type in ("ANDROID", "ALIOS"):
            print("   -s {},virtio-rpmb \\".format(launch_cfg_lib.virtual_dev_slot("virtio-rpmb")), file=config)
        if board_name == "apl-up2":
            print("   --pm_notify_channel power_button \\", file=config)
        if board_name == "apl-mrb":
            print("   --pm_notify_channel ioc \\", file=config)

    if is_nuc_clr(names, vmid):
        print("   --pm_notify_channel uart \\", file=config)
        print('   --pm_by_vuart pty,/run/acrn/life_mngr_$vm_name  \\', file=config)
        print('   -l com2,/run/acrn/life_mngr_$vm_name \\', file=config)
        print("   -s {},virtio-net,tap0 \\".format(launch_cfg_lib.virtual_dev_slot("virtio-net")), file=config)

    # mac_seed
    if uos_type in ("CLEARLINUX", "ANDROID", "ALIOS"):
        print("   --mac_seed $mac_seed \\", file=config)

    # hard rt os
    if uos_type == "PREEMPT-RT LINUX":
        print("acrn-dm -A -m $mem_size -s 0:0,hostbridge -U {} \\".format(scenario_uuid[vmid + sos_vmid]), file=config)
        #print("   -k /usr/lib/kernel/default-iot-lts2018-preempt-rt \\", file=config)
        print("   -k /usr/lib/kernel/default-iot-lts2018-preempt-rt \\", file=config)
        print("   --lapic_pt \\", file=config)
        print("   --rtvm \\", file=config)
        print("   --virtio_poll 1000000 \\", file=config)
        print("   --pm_notify_channel uart --pm_by_vuart tty,/dev/ttyS1 \\", file=config)

    # vxworks
    if uos_type == "VXWORKS":
        print("acrn-dm -A -m $mem_size -s 0:0,hostbridge -U {} \\".format(scenario_uuid[vmid + sos_vmid]), file=config)
        print("   -s {},virtio-blk,./VxWorks.img \\".format(launch_cfg_lib.virtual_dev_slot("virtio-blk")), file=config)
        print("   --virtio_poll 1000000 \\", file=config)
        print("   --lapic_pt \\", file=config)

    # zephyr
    if uos_type == "ZEPHYR":
        print("acrn-dm -A -m $mem_size -s 0:0,hostbridge -s 1:0,lpc -U {} \\".format(scenario_uuid[vmid + sos_vmid]), file=config)
        print("   -s {},virtio-blk,./zephyr.img \\".format(launch_cfg_lib.virtual_dev_slot("virtio-blk")), file=config)

    # windows
    if uos_type == "WINDOWS":
        print("acrn-dm -A -m $mem_size -s 0:0,hostbridge -s 1:0,lpc -U {} \\".format(scenario_uuid[vmid + sos_vmid]), file=config)
        print("   -s {},virtio-blk,./win10-ltsc.img \\".format(launch_cfg_lib.virtual_dev_slot("virtio-blk")), file=config)

    # GVT args set
    gvt_arg_set(uos_type, config)

    # vbootloader of ovmf
    #if uos_type != "PREEMPT-RT LINUX" and dm['vbootloader'][vmid] == "ovmf":
    if dm['vbootloader'][vmid] == "ovmf":
        print("   --ovmf /usr/share/acrn/bios/OVMF.fd \\", file=config)

    # redirect console
    if dm['console_type'][vmid] == "com1(ttyS0)":
        print("   -l com1,stdio \\", file=config)
        print("   -s {},{} \\".format(launch_cfg_lib.virtual_dev_slot("com1(ttyS0)"),
            launch_cfg_lib.RE_CONSOLE_MAP['com1(ttyS0)']), file=config)
    else:
        print("   -s {},{} \\".format(
            launch_cfg_lib.virtual_dev_slot("virtio-console(hvc0)"),
                launch_cfg_lib.RE_CONSOLE_MAP['virtio-console(hvc0)']), file=config)

    if uos_type in ("CLEARLINUX", "ANDROID", "ALIOS"):
        if board_name in ("apl-mrb", "apl-up2"):
            print("   -s {},virtio-net,$tap \\".format(launch_cfg_lib.virtual_dev_slot("virtio-net")), file=config)

    if uos_type in ("CLEARLINUX", "ANDROID", "ALIOS"):
        print("   -s {},virtio-hyper_dmabuf \\".format(launch_cfg_lib.virtual_dev_slot("virtio-hyper_dmabuf")), file=config)
        if board_name == "apl-mrb":
            print("   -i /run/acrn/ioc_$vm_name,0x20 \\", file=config)
            print("   -l com2,/run/acrn/ioc_$vm_name \\", file=config)

        if not is_nuc_clr(names, vmid):
            print("   -s {},wdt-i6300esb \\".format(launch_cfg_lib.virtual_dev_slot("wdt-i6300esb")), file=config)
            print("   $intr_storm_monitor \\", file=config)
            if dm['vbootloader'][vmid] == "vsbl":
                print("   $boot_image_option \\",file=config)
            print("   -s {},virtio-blk$boot_dev_flag,/data/$3/$3.img \\".format(launch_cfg_lib.virtual_dev_slot("virtio-blk")), file=config)
            print("   -s {},xhci,1-1:1-2:1-3:2-1:2-2:2-3:cap=apl \\".format(launch_cfg_lib.virtual_dev_slot("xhci")), file=config)
        else:
            print("   -s {},virtio-blk,/home/clear/uos/uos.img \\".format(launch_cfg_lib.virtual_dev_slot("virtio-blk")), file=config)

    if uos_type in ("ANDROID", "ALIOS"):
        print("   --enable_trusty \\", file=config)

    set_dm_pt(names, sel, vmid, config)
    if uos_type != "PREEMPT-RT LINUX":
        print("   $vm_name", file=config)
    else:
        print("   hard_rtvm", file=config)
    print("}", file=config)


def gen(names, pt_sel, dm, vmid, config):

    uos_type = names['uos_types'][vmid]

    # passthrough bdf/vpid dictionay
    pt.gen_pt_head(names, pt_sel, vmid, config)

    # gen launch header
    launch_begin(uos_type, config)
    tap_uos_net(names, vmid, config)

    # passthrough device
    pt.gen_pt(names, pt_sel, vmid, config)
    wa_usage(uos_type, config)
    delay_use_usb_storage(uos_type, config)
    mem_size_set(names, dm, vmid, config)
    interrupt_storm(names, vmid, config)
    log_level_set(uos_type, config)

    # gen acrn-dm args
    dm_arg_set(names, pt_sel, dm, vmid, config)

    # gen launch end
    launch_end(names, dm, vmid, config)
