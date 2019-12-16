# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import launch_cfg_lib
import pt


def is_nuc_whl_clr(names, vmid):
    uos_type = names['uos_types'][vmid]
    board_name = names['board_name']

    if uos_type == "CLEARLINUX" and board_name not in ("apl-mrb", "apl-up2"):
        return True

    return False


def is_mount_needed(virt_io, vmid):
    rootfs_img = ''
    blk_dev_list = launch_cfg_lib.get_rootdev_info(launch_cfg_lib.BOARD_INFO_FILE)
    if virt_io['block'][vmid]:
        if ':' in virt_io['block'][vmid]:
            blk_dev = virt_io['block'][vmid].split(':')[0]
            rootfs_img = virt_io['block'][vmid].split(':')[1]
        else:
            blk_dev = virt_io['block'][vmid]

        if blk_dev in blk_dev_list and rootfs_img:
            return True

    return False


def tap_uos_net(names, virt_io, vmid, config):
    uos_type = names['uos_types'][vmid]
    board_name = names['board_name']

    vm_name = launch_cfg_lib.undline_name(uos_type).lower()

    if uos_type in ("CLEARLINUX", "ANDROID", "ALIOS"):
        if board_name in ("apl-mrb", "apl-up2"):
            print('if [ ! -f "/data/{}/{}.img" ]; then'.format(vm_name, vm_name), file=config)
            print('  echo "no /data/{}/{}.img, exit"'.format(vm_name, vm_name), file=config)
            print("  exit", file=config)
            print("fi", file=config)
            print("", file=config)
        print("#vm-name used to generate uos-mac address", file=config)
        print("mac=$(cat /sys/class/net/e*/address)", file=config)
        print("vm_name=vm$1", file=config)
        print("mac_seed=${mac:9:8}-${vm_name}", file=config)
        print("", file=config)

    if uos_type in ("VXWORKS", "ZEPHYR", "WINDOWS", "PREEMPT-RT LINUX"):
        print("vm_name={}_vm$1".format(vm_name), file=config)

    for net in virt_io['network'][vmid]:
        if net:
            net_name = net
            if ',' in net:
                net_name = net.split(',')[0]
            print("tap_net {}".format(net_name), file=config)

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


def off_line_cpus(args, vmid, uos_type, config):
    """
    :param args: the dictionary of argument for acrn-dm
    :param vmid: ID of the vm
    :param uos_type: the type of UOS
    :param config: it is a file pointer to write offline cpu information
    """
    cpus = ''
    cpus = '..'.join(list(args["off_pcpus"][vmid]))
    if not cpus.strip():
        key = "launch script error:"
        launch_cfg_lib.ERR_LIST[key] = "No available cpu to offline and pass it to vm {}".format(vmid)

    print('offline_path="/sys/class/vhm/acrn_vhm"', file=config)
    print("", file=config)

    print("# Check the device file of /dev/acrn_hsm to determine the offline_path", file=config)
    print('if [ -e "/dev/acrn_hsm" ]; then', file=config)
    print('offline_path="/sys/class/acrn/acrn_hsm"', file=config)
    print('fi', file=config)
    print("", file=config)
    print("# offline pinned vCPUs from SOS before launch UOS", file=config)
    print("for i in `ls -d /sys/devices/system/cpu/cpu[{}]`; do".format(cpus), file=config)
    print("        online=`cat $i/online`", file=config)
    print('        idx=`echo $i | tr -cd "[1-99]"`', file=config)
    print("        echo cpu$idx online=$online", file=config)
    print('        if [ "$online" = "1" ]; then', file=config)
    print("             echo 0 > $i/online", file=config)
    print("             online=`cat $i/online`", file=config)
    print("             # during boot time, cpu hotplug may be disabled by pci_device_probe during a pci module insmod", file=config)
    print('             while [ "$online" = "1" ]; do', file=config)
    print("                     sleep 1", file=config)
    print("                     echo 0 > $i/online", file=config)
    print("                     online=`cat $i/online`", file=config)
    print("             done", file=config)
    print("             echo $idx > ${offline_path}/offline_cpu", file=config)
    print("        fi", file=config)
    print("done", file=config)
    print("", file=config)


def run_container(board_name, uos_type, config):
    """
    The container contains the clearlinux as rootfs
    :param board_name: board name
    :param uos_type: the os name of user os
    :param config: the file pointer to store the information
    """
    # the runC.json is store in the path under board name, but for nuc7i7dnb/nuc6cayh/kbl-nuc-i7 is under nuc/
    if 'nuc' in board_name:
        board_name = 'nuc'

    if board_name not in ("apl-mrb", "nuc") or uos_type != "CLEARLINUX":
        return

    print("function run_container()", file=config)
    print("{", file=config)
    print("vm_name=vm1", file=config)
    print('config_src="/usr/share/acrn/samples/{}/runC.json"'.format(board_name), file=config)
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

    dst_str = """    cp  "$config_src"  "$config_dst"
    args=$(sed '{s/-C//g;s/^[ \\t]*//g;s/^/\\"/;s/ /\\",\\"/g;s/$/\\"/}' ${arg_file})
    sed -i "s|\\"sh\\"|\\"$shell\\", $args|" $config_dst"""
    print('', file=config)
    print('if [ ! -f "$shell" ]; then', file=config)
    print('        echo "Pls add the vm at first!"', file=config)
    print('        exit', file=config)
    print('fi', file=config)
    print('', file=config)
    print('if [ ! -f "$arg_file" ]; then', file=config)
    print('        echo "Pls add the vm args!"', file=config)
    print('        exit', file=config)
    print('fi', file=config)
    print('', file=config)
    print('if [ ! -d "$rootfs_dir" ]; then', file=config)
    print('        mkdir -p "$rootfs_dir"', file=config)
    print('fi', file=config)
    print('if [ ! -d "$runc_bundle" ]; then', file=config)
    print('	mkdir -p "$runc_bundle"', file=config)
    print('fi', file=config)
    print('if [ ! -f "$config_dst" ]; then', file=config)
    print('{}'.format(dst_str), file=config)
    print('fi', file=config)
    print('runc run --bundle $runc_bundle -d $vm_name', file=config)
    print('echo "The runC container is running in backgroud"', file=config)
    print('echo "\'#runc exec <vmname> bash\' to login the container bash"', file=config)
    print('exit', file=config)
    print('}', file=config)
    print('', file=config)

def boot_image_type(args, vmid, config):

    if not args['vbootloader'][vmid] or (args['vbootloader'][vmid] and args['vbootloader'][vmid] != "vsbl"):
        return

    print('boot_dev_flag=",b"', file=config)
    print("if [ $4 == 1 ];then", file=config)
    print('  boot_image_option="--vsbl /usr/share/acrn/bios/VSBL_debug.bin"', file=config)
    print("else", file=config)
    print('  boot_image_option="--vsbl /usr/share/acrn/bios/VSBL.bin"', file=config)
    print("fi", file=config)
    print("", file=config)


def interrupt_storm(pt_sel, config):
    if not pt_sel:
        return

    # TODO: --intr_monitor should be configurable by user
    print("#interrupt storm monitor for pass-through devices, params order:", file=config)
    print("#threshold/s,probe-period(s),intr-inject-delay-time(ms),delay-duration(ms)", file=config)
    print('intr_storm_monitor="--intr_monitor 10000,10,1,100"', file=config)
    print("", file=config)


def gvt_arg_set(dm, vmid, uos_type, config):

    if uos_type not in ('CLEARLINUX', 'ANDROID', 'ALIOS', 'WINDOWS'):
        return
    gvt_args = dm['gvt_args'][vmid]
    if gvt_args:
        print('   -s 2,pci-gvt -G "$2"  \\', file=config)


def log_level_set(uos_type, config):

    if uos_type not in ('CLEARLINUX', 'ANDROID', 'ALIOS'):
        return
    print("#logger_setting, format: logger_name,level; like following", file=config)
    print('logger_setting="--logger_setting console,level=4;kmsg,level=3;disk,level=5"', file=config)
    print("", file=config)

def tap_network(virt_io, vmid, config):

    none_i = 0
    tap_net_list = virt_io['network'][vmid]
    for net in tap_net_list:
        if net == None:
            none_i += 1
    tap_net_num = len(tap_net_list) - none_i

    if tap_net_num >= 1:
        print("function tap_net() {", file=config)
        print("# create a unique tap device for each VM", file=config)
        print("tap=$1", file=config)
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
        print("}", file=config)
        print("", file=config)


def launch_begin(names, virt_io, vmid, config):
    board_name = names['board_name']
    uos_type = names['uos_types'][vmid]

    launch_uos = launch_cfg_lib.undline_name(uos_type).lower()
    tap_network(virt_io, vmid, config)
    run_container(board_name, uos_type, config)
    print("function launch_{}()".format(launch_uos), file=config)
    print("{", file=config)


def wa_usage(uos_type, config):
    if uos_type in ("ANDROID", "ALIOS"):
        print("# WA for USB role switch hang issue, disable runtime PM of xHCI device", file=config)
        print("echo on > /sys/devices/pci0000:00/0000:00:15.0/power/control", file=config)
        print("", file=config)

def mem_size_set(args, vmid, config):
    mem_size = args['mem_size'][vmid]

    print("mem_size={}M".format(mem_size), file=config)


def uos_launch(names, args, virt_io, vmid, config):

    gvt_args = args['gvt_args'][vmid]
    uos_type = names['uos_types'][vmid]
    launch_uos = launch_cfg_lib.undline_name(uos_type).lower()
    board_name = names['board_name']
    if 'nuc' in board_name:
        board_name = 'nuc'

    if uos_type == "CLEARLINUX" and board_name in ("apl-mrb", "nuc"):
        print('if [ "$1" = "-C" ];then', file=config)
        print('    if [ $(hostname) = "runc" ]; then', file=config)
        print('            echo "Already in container exit!"', file=config)
        print("            exit", file=config)
        print("    fi", file=config)
        print('    echo "runc_container"', file=config)
        print("    run_container", file=config)
        if board_name == "apl-mrb":
            print("    exit", file=config)
            print("fi", file=config)
        else:
            print("else", file=config)
            print('    launch_{} 1 "{}"'.format(launch_uos, gvt_args), file=config)
            print("fi", file=config)
    else:
        if uos_type in ("VXWORKS", "PREEMPT-RT LINUX", "ZEPHYR"):
            print("launch_{} 1".format(launch_uos), file=config)
        if uos_type in ("CLEARLINUX", "WINDOWS"):
            print('launch_{} 1 "{}"'.format(launch_uos, gvt_args), file=config)

    if is_mount_needed(virt_io, vmid):
        print("", file=config)
        print('launch_{} {} "{}" "{}" $debug'.format(launch_uos, vmid, gvt_args, vmid), file=config)
        print("", file=config)
        print("umount /data", file=config)


def launch_end(names, args, virt_io, vmid, config):

    board_name = names['board_name']
    uos_type = names['uos_types'][vmid]
    mem_size = args["mem_size"][vmid]

    if uos_type in ("CLEARLINUX", "ANDROID", "ALIOS") and not is_nuc_whl_clr(names, vmid):
        print("debug=0", file=config)
        print("", file=config)
        print('while getopts "hdC" opt', file=config)
        print("do", file=config)
        print("	case $opt in", file=config)
        print("		d) debug=1", file=config)
        print("			;;", file=config)
        print("		C)", file=config)
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

    if is_mount_needed(virt_io, vmid):
        root_fs = virt_io['block'][vmid].split(':')[0]

        print('if [ ! -b "{}" ]; then'.format(root_fs), file=config)
        print('  echo "no {} data partition, exit"'.format(root_fs), file=config)
        print("  exit", file=config)
        print("fi", file=config)
        print("", file=config)
        print("mkdir -p /data", file=config)
        print("mount {} /data".format(root_fs), file=config)
        print("", file=config)

    off_line_cpus(args, vmid, uos_type, config)

    uos_launch(names, args, virt_io, vmid, config)


def set_dm_pt(names, sel, vmid, config):

    uos_type = names['uos_types'][vmid]

    if sel.bdf['usb_xdci'][vmid] and sel.slot['usb_xdci'][vmid]:
        print('   -s {},passthru,{}/{}/{} \\'.format(sel.slot["usb_xdci"][vmid], sel.bdf["usb_xdci"][vmid][0:2],\
            sel.bdf["usb_xdci"][vmid][3:5], sel.bdf["usb_xdci"][vmid][6:7]), file=config)

    # pass through audio/audio_codec
    if sel.bdf['audio'][vmid]:
        print("   $boot_audio_option \\", file=config)

    if sel.bdf['cse'][vmid] and sel.slot['cse'][vmid]:
        print("   $boot_cse_option \\", file=config)

    if sel.bdf["sd_card"][vmid] and sel.slot['sd_card'][vmid]:
        print('   -s {},passthru,{}/{}/{} \\'.format(sel.slot["sd_card"][vmid], sel.bdf["sd_card"][vmid][0:2], \
            sel.bdf["sd_card"][vmid][3:5], sel.bdf["sd_card"][vmid][6:7]), file=config)

    if sel.bdf['bluetooth'][vmid] and sel.slot['bluetooth'][vmid]:
        print('   -s {},passthru,{}/{}/{} \\'.format(sel.slot["bluetooth"][vmid], sel.bdf["bluetooth"][vmid][0:2], \
            sel.bdf["bluetooth"][vmid][3:5], sel.bdf["bluetooth"][vmid][6:7]), file=config)

    if sel.bdf['wifi'][vmid] and sel.slot['wifi'][vmid]:
        if uos_type == "ANDROID":
            print("   -s {},passthru,{}/{}/{},keep_gsi \\".format(sel.slot["wifi"][vmid], sel.bdf["wifi"][vmid][0:2], \
                sel.bdf["wifi"][vmid][3:5], sel.bdf["wifi"][vmid][6:7]), file=config)
        else:
            print("   -s {},passthru,{}/{}/{} \\".format(sel.slot["wifi"][vmid], sel.bdf["wifi"][vmid][0:2], \
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


def vboot_arg_set(dm, vmid, config):
    """
    Set the argument of vbootloader
    :param dm: the dictionary of argument for acrn-dm
    :param vmid: ID of the vm
    :param config: it is a file pointer to write vboot loader information
    :return: None
    """
    # TODO: Support to generate '-k' xml config from webUI and to parse it
    if dm['vbootloader'][vmid] == "ovmf":
        print("   --ovmf /usr/share/acrn/bios/OVMF.fd \\", file=config)
    elif dm['vbootloader'][vmid] == "vsbl":
        print("   $boot_image_option \\",file=config)


def xhci_args_set(dm, vmid, config):
    # usb_xhci set, the value is string
    if dm['xhci'][vmid]:
        print("   -s {},xhci,{} \\".format(
            launch_cfg_lib.virtual_dev_slot("xhci"), dm['xhci'][vmid]), file=config)


def virtio_args_set(dm, virt_io, vmid, config):

    # virtio-input set, the value type is a list
    for input_val in virt_io['input'][vmid]:
        if input_val:
            print("   -s {},virtio-input,{} \\".format(
                launch_cfg_lib.virtual_dev_slot("virtio-input{}".format(input_val)), input_val), file=config)

    # virtio-blk set, the value type is a list
    for blk in virt_io['block'][vmid]:
        if blk:
            print("   -s {},virtio-blk,{} \\".format(launch_cfg_lib.virtual_dev_slot("virtio-blk{}".format(blk)), blk.strip(':')), file=config)

    # virtio-net set, the value type is a list
    for net in virt_io['network'][vmid]:
        if net:
            net_name = net
            if ',' in net:
                net_name = net.split(',')[0]
            print("   -s {},virtio-net,{} \\".format(launch_cfg_lib.virtual_dev_slot("virtio-net{}".format(net)), net_name), file=config)

    # virtio-console set, the value type is a string
    if virt_io['console'][vmid]:
        print("   -s {},virtio-console,{} \\".format(
            launch_cfg_lib.virtual_dev_slot("virtio-console"),
                virt_io['console'][vmid]), file=config)


def dm_arg_set(names, sel, virt_io, dm, vmid, config):

    uos_type = names['uos_types'][vmid]
    board_name = names['board_name']

    boot_image_type(dm, vmid, config)

    # uuid get
    scenario_uuid = launch_cfg_lib.get_scenario_uuid()
    sos_vmid = launch_cfg_lib.get_sos_vmid()
    if not str(sos_vmid).isnumeric():
        sos_vmid = 0
        key = "launch config err:"
        launch_cfg_lib.ERR_LIST[key] = "There is no SOS_VM in scenario config file!"

    # clearlinux/android/alios
    dm_str = 'acrn-dm -A -m $mem_size -s 0:0,hostbridge -U {}'.format(scenario_uuid[vmid + sos_vmid])
    if uos_type in ("CLEARLINUX", "ANDROID", "ALIOS"):
        if uos_type == "CLEARLINUX":
            print("{} \\".format(dm_str), file=config)
        else:
            print('{} $npk_virt \\'.format(dm_str), file=config)

    if uos_type in ("CLEARLINUX", "ANDROID", "ALIOS"):
        if uos_type in ("ANDROID", "ALIOS"):
            print("   -s {},virtio-rpmb \\".format(launch_cfg_lib.virtual_dev_slot("virtio-rpmb")), file=config)

    # mac_seed
    if uos_type in ("CLEARLINUX", "ANDROID", "ALIOS"):
        print("   --mac_seed $mac_seed \\", file=config)

    # hard rt os
    if uos_type == "PREEMPT-RT LINUX":
        print("{} \\".format(dm_str), file=config)
        print("   --lapic_pt \\", file=config)
        print("   --rtvm \\", file=config)
        print("   --virtio_poll 1000000 \\", file=config)

    # vxworks
    if uos_type == "VXWORKS":
        print("{} \\".format(dm_str), file=config)
        print("   --virtio_poll 1000000 \\", file=config)
        print("   --lapic_pt \\", file=config)

    # zephyr
    if uos_type == "ZEPHYR":
        print("{} \\".format(dm_str), file=config)

    # windows
    if uos_type == "WINDOWS":
        print("{} \\".format(dm_str), file=config)
        print("   --windows \\", file=config)

    # pm_channel set
    if dm['pm_channel'][vmid] and dm['pm_channel'][vmid] != None:
        pm_key = dm['pm_channel'][vmid]
        if pm_key == "vuart1(tty)":
            vuart_base = launch_cfg_lib.get_vuart1_from_scenario(sos_vmid + vmid)
            if vuart_base == "INVALID_COM_BASE":
                err_key = "uos:id={}:poweroff_channel".format(vmid)
                launch_cfg_lib.ERR_LIST[err_key] = "vuart1 of VM{} in scenario file should select 'SOS_COM2_BASE'".format(sos_vmid + vmid)
                return
        print("   {} \\".format(launch_cfg_lib.PM_CHANNEL_DIC[pm_key]), file=config)

    # set logger_setting for all vm
    print("   $logger_setting \\", file=config)

    # XHCI args set
    xhci_args_set(dm, vmid, config)

    # VIRTIO args set
    virtio_args_set(dm, virt_io, vmid, config)

    # GVT args set
    gvt_arg_set(dm, vmid, uos_type, config)

    # vbootloader setting
    vboot_arg_set(dm, vmid, config)

    # redirect console
    if dm['vuart0'][vmid] == "Enable":
        print("   -s 1:0,lpc \\", file=config)
        print("   -l com1,stdio \\", file=config)

    if uos_type in ("CLEARLINUX", "ANDROID", "ALIOS"):
        print("   -s {},virtio-hyper_dmabuf \\".format(launch_cfg_lib.virtual_dev_slot("virtio-hyper_dmabuf")), file=config)
        if board_name == "apl-mrb":
            print("   -i /run/acrn/ioc_$vm_name,0x20 \\", file=config)
            print("   -l com2,/run/acrn/ioc_$vm_name \\", file=config)

        if sel:
            print("   $intr_storm_monitor \\", file=config)

        if not is_nuc_whl_clr(names, vmid):
            print("   -s {},wdt-i6300esb \\".format(launch_cfg_lib.virtual_dev_slot("wdt-i6300esb")), file=config)

    if uos_type in ("ANDROID", "ALIOS"):
        print("   --enable_trusty \\", file=config)

    set_dm_pt(names, sel, vmid, config)
    print("   $vm_name", file=config)
    print("}", file=config)


def gen(names, pt_sel, virt_io, dm, vmid, config):

    board_name = names['board_name']
    uos_type = names['uos_types'][vmid]

    # passthrough bdf/vpid dictionay
    pt.gen_pt_head(names, pt_sel, vmid, config)

    # gen launch header
    launch_begin(names, virt_io, vmid, config)
    tap_uos_net(names, virt_io,  vmid, config)

    # passthrough device
    pt.gen_pt(names, pt_sel, vmid, config)
    wa_usage(uos_type, config)
    delay_use_usb_storage(uos_type, config)
    mem_size_set(dm, vmid, config)
    interrupt_storm(pt_sel, config)
    log_level_set(uos_type, config)

    # gen acrn-dm args
    dm_arg_set(names, pt_sel, virt_io, dm, vmid, config)

    # gen launch end
    launch_end(names, dm, virt_io, vmid, config)
