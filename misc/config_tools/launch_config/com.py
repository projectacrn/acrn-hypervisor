# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import scenario_cfg_lib
import launch_cfg_lib
import lxml.etree
import common
import pt


def is_nuc_whl_linux(names, vmid):
    user_vm_type = names['user_vm_types'][vmid]
    board_name = names['board_name']

    if launch_cfg_lib.is_linux_like(user_vm_type) and board_name not in ("apl-mrb", "apl-up2"):
        return True

    return False


def is_mount_needed(virt_io, vmid):

    if True in launch_cfg_lib.MOUNT_FLAG_DIC[vmid]:
        return True

    return False


def tap_user_vm_net(names, virt_io, vmid, config):
    user_vm_type = names['user_vm_types'][vmid]
    board_name = names['board_name']

    vm_name = names['user_vm_names'][vmid]

    if launch_cfg_lib.is_linux_like(user_vm_type) or user_vm_type in ("ANDROID", "ALIOS"):
        i = 0
        for mount_flag in launch_cfg_lib.MOUNT_FLAG_DIC[vmid]:
            if not mount_flag:
                i += 1
                continue
            blk = virt_io['block'][vmid][i]
            rootfs_img = blk.split(':')[1].strip(':')
            print('if [ ! -f "/data{}/{}" ]; then'.format(i, rootfs_img), file=config)
            print('  echo "no /data{}/{}, exit"'.format(i, rootfs_img), file=config)
            print("  exit", file=config)
            print("fi", file=config)
            print("", file=config)
            i += 1

    print("#vm-name used to generate user-vm-mac address", file=config)
    print("mac=$(cat /sys/class/net/e*/address)", file=config)
    print("vm_name=\"{}\"".format(vm_name), file=config)
    print("mac_seed=${mac:0:17}-${vm_name}", file=config)
    print("", file=config)


    for net in virt_io['network'][vmid]:
        if net:
            net_name = net
            if ',' in net:
                net_name = net.split(',')[0]
            print("tap_net tap_{}".format(net_name), file=config)

    print("#check if the vm is running or not", file=config)
    print("vm_ps=$(pgrep -a -f acrn-dm)", file=config)
    print('result=$(echo $vm_ps | grep -w "${vm_name}")', file=config)
    print('if [[ "$result" != "" ]]; then', file=config)
    print('  echo "$vm_name is running, can\'t create twice!"', file=config)
    print("  exit", file=config)
    print("fi", file=config)
    print("", file=config)


def off_line_cpus(args, vmid, user_vm_type, config):
    """
    :param args: the dictionary of argument for acrn-dm
    :param vmid: ID of the vm
    :param user_vm_type: the type of User VM
    :param config: it is a file pointer to write offline cpu information
    """
    pcpu_id_list = get_cpu_affinity_list(args["cpu_affinity"], vmid)
    if not pcpu_id_list:
        sos_vmid = launch_cfg_lib.get_sos_vmid()
        cpu_affinity = common.get_leaf_tag_map(common.SCENARIO_INFO_FILE, "cpu_affinity", "pcpu_id")
        pcpu_id_list = get_cpu_affinity_list(cpu_affinity, sos_vmid+vmid)

    if not pcpu_id_list:
        key = "scenario config error"
        launch_cfg_lib.ERR_LIST[key] = "No available cpu to offline and pass it to vm {}".format(vmid)

    print("# offline pinned vCPUs from Service VM before launch User VM", file=config)
    print('cpu_path="/sys/devices/system/cpu"', file=config)
    print("for i in `ls ${cpu_path}`; do", file=config)
    print("    for j in {}; do".format(' '.join([str(i) for i in pcpu_id_list])), file=config)
    print('        if [ "cpu"$j = $i ]; then', file=config)
    print('            online=`cat ${cpu_path}/$i/online`', file=config)
    print('            idx=`echo $i | tr -cd "[0-9]"`', file=config)
    print('            echo $i online=$online', file=config)
    print('            if [ "$online" = "1" ] && [ "$idx" != "0" ]; then', file=config)
    print("                echo 0 > ${cpu_path}/$i/online", file=config)
    print("                online=`cat ${cpu_path}/$i/online`", file=config)
    print("                # during boot time, cpu hotplug may be disabled by pci_device_probe during a pci module insmod", file=config)
    print('                while [ "$online" = "1" ]; do', file=config)
    print("                    sleep 1", file=config)
    print("                    echo 0 > ${cpu_path}/$i/online", file=config)
    print("                    online=`cat ${cpu_path}/$i/online`", file=config)
    print("                done", file=config)
    print("                echo $idx > /sys/devices/virtual/misc/acrn_hsm/remove_cpu", file=config)
    print("            fi", file=config)
    print("        fi", file=config)
    print("    done", file=config)
    print("done", file=config)
    print("", file=config)


def run_container(board_name, user_vm_type, config):
    """
    The container contains the clearlinux as rootfs
    :param board_name: board name
    :param user_vm_type: the os name of user os
    :param config: the file pointer to store the information
    """
    # the runC.json is store in the path under board name, but for nuc7i7dnb/nuc6cayh/kbl-nuc-i7 is under nuc/
    if 'nuc' in board_name:
        board_name = 'nuc'

    if board_name not in ("apl-mrb", "nuc") or not launch_cfg_lib.is_linux_like(user_vm_type):
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


def interrupt_storm(pt_sel, config):
    if not pt_sel:
        return

    # TODO: --intr_monitor should be configurable by user
    print("#interrupt storm monitor for pass-through devices, params order:", file=config)
    print("#threshold/s,probe-period(s),intr-inject-delay-time(ms),delay-duration(ms)", file=config)
    print('intr_storm_monitor="--intr_monitor 10000,10,1,100"', file=config)
    print("", file=config)


def gpu_pt_arg_set(dm, sel, vmid, config):
    gpu_bdf = sel.bdf["gpu"][vmid]

    if gpu_bdf:
        print('   -s 2,passthru,{}/{}/{}  \\'.format(gpu_bdf[0:2], gpu_bdf[3:5], gpu_bdf[6:7]), file=config)


def log_level_set(user_vm_type, config):

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
    user_vm_type = names['user_vm_types'][vmid]

    launch_uos = common.undline_name(user_vm_type).lower()
    tap_network(virt_io, vmid, config)
    run_container(board_name, user_vm_type, config)
    print("function launch_{}()".format(launch_uos), file=config)
    print("{", file=config)


def wa_usage(user_vm_type, config):
    if user_vm_type in ("ANDROID", "ALIOS"):
        print("# WA for USB role switch hang issue, disable runtime PM of xHCI device", file=config)
        print("echo on > /sys/devices/pci0000:00/0000:00:15.0/power/control", file=config)
        print("", file=config)

def mem_size_set(args, vmid, config):
    mem_size = args['mem_size'][vmid]

    print("mem_size={}M".format(mem_size), file=config)


def user_vm_launch(names, args, virt_io, vmid, config):

    user_vm_type = names['user_vm_types'][vmid]
    launch_uos = common.undline_name(user_vm_type).lower()
    board_name = names['board_name']
    if 'nuc' in board_name:
        board_name = 'nuc'

    if user_vm_type == "CLEARLINUX" and board_name in ("apl-mrb", "nuc"):
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
            if is_mount_needed(virt_io, vmid):
                print("", file=config)
                print('launch_{} {} "{}" $debug'.format(launch_uos, vmid, vmid), file=config)
                print("", file=config)
                i = 0
                for mount_flag in launch_cfg_lib.MOUNT_FLAG_DIC[vmid]:
                    if not mount_flag:
                        i += 1
                        continue
                    print("umount /data{}".format(i), file=config)
                    i += 1

        else:
            print("else", file=config)
            print('    launch_{} {}'.format(launch_uos, vmid), file=config)
            print("fi", file=config)
            return
    elif not is_mount_needed(virt_io, vmid):
        print('launch_{} {}'.format(launch_uos, vmid), file=config)
    else:
        print("", file=config)
        print('launch_{} {} "{}" $debug'.format(launch_uos, vmid, vmid), file=config)
        print("", file=config)
        i = 0
        for mount_flag in launch_cfg_lib.MOUNT_FLAG_DIC[vmid]:
            if not mount_flag:
                i += 1
                continue
            print("umount /data{}".format(i), file=config)
            i += 1


def launch_end(names, args, virt_io, vmid, config):

    board_name = names['board_name']
    user_vm_type = names['user_vm_types'][vmid]
    mem_size = args["mem_size"][vmid]

    if user_vm_type in ("CLEARLINUX", "ANDROID", "ALIOS") and not is_nuc_whl_linux(names, vmid):
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
        i = 0
        for mount_flag in launch_cfg_lib.MOUNT_FLAG_DIC[vmid]:
            if not mount_flag:
                i += 1
                continue
            blk = virt_io['block'][vmid][i]
            root_fs = blk.split(':')[0]
            print('if [ ! -b "{}" ]; then'.format(root_fs), file=config)
            print('  echo "no {} data partition, exit"'.format(root_fs), file=config)
            print("  exit", file=config)
            print("fi", file=config)
            print("mkdir -p /data{}".format(i), file=config)
            print("mount {} /data{}".format(root_fs, i), file=config)
            print("", file=config)
            i += 1

    sos_vmid = launch_cfg_lib.get_sos_vmid()
    if args['cpu_sharing'] == "SCHED_NOOP" or common.VM_TYPES[vmid+sos_vmid] == "POST_RT_VM":
        off_line_cpus(args, vmid, user_vm_type, config)

    user_vm_launch(names, args, virt_io, vmid, config)


def set_dm_pt(names, sel, vmid, config, dm):

    user_vm_type = names['user_vm_types'][vmid]

    if sel.bdf['usb_xdci'][vmid] and sel.slot['usb_xdci'][vmid]:
        sub_attr = ''
        if user_vm_type == "WINDOWS":
            sub_attr = ',d3hot_reset'
        print('   -s {},passthru,{}/{}/{}{} \\'.format(sel.slot["usb_xdci"][vmid], sel.bdf["usb_xdci"][vmid][0:2],\
            sel.bdf["usb_xdci"][vmid][3:5], sel.bdf["usb_xdci"][vmid][6:7], sub_attr), file=config)

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
        if user_vm_type == "ANDROID":
            print("   -s {},passthru,{}/{}/{},keep_gsi \\".format(sel.slot["wifi"][vmid], sel.bdf["wifi"][vmid][0:2], \
                sel.bdf["wifi"][vmid][3:5], sel.bdf["wifi"][vmid][6:7]), file=config)
        else:
            print("   -s {},passthru,{}/{}/{} \\".format(sel.slot["wifi"][vmid], sel.bdf["wifi"][vmid][0:2], \
                sel.bdf["wifi"][vmid][3:5], sel.bdf["wifi"][vmid][6:7]), file=config)

    if sel.bdf['ipu'][vmid] or sel.bdf['ipu_i2c'][vmid]:
        print("   $boot_ipu_option      \\", file=config)

    if sel.bdf['ethernet'][vmid] and sel.slot['ethernet'][vmid]:
        if vmid in dm["enable_ptm"] and dm["enable_ptm"][vmid] == 'y':
            print("   -s {},passthru,{}/{}/{},enable_ptm \\".format(sel.slot["ethernet"][vmid], sel.bdf["ethernet"][vmid][0:2], \
                sel.bdf["ethernet"][vmid][3:5], sel.bdf["ethernet"][vmid][6:7]), file=config)
        else:
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


def xhci_args_set(dm, vmid, config):
    # usb_xhci set, the value is string
    if dm['xhci'][vmid]:
        print("   -s {},xhci,{} \\".format(
            launch_cfg_lib.virtual_dev_slot("xhci"), dm['xhci'][vmid]), file=config)


def shm_arg_set(dm, vmid, config):

    if dm['shm_enabled'] == "n":
        return
    for shm_region in dm["shm_regions"][vmid]:
        print("   -s {},ivshmem,{} \\".format(
            launch_cfg_lib.virtual_dev_slot("shm_region_{}".format(shm_region)), shm_region), file=config)


def virtio_args_set(dm, names, virt_io, vmid, config):

    user_vm_type = names['user_vm_types'][vmid]
    # virtio-input set, the value type is a list
    for input_val in virt_io['input'][vmid]:
        if input_val:
            print("   -s {},virtio-input,{} \\".format(
                launch_cfg_lib.virtual_dev_slot("virtio-input{}".format(input_val)), input_val), file=config)

    # virtio-blk set, the value type is a list
    i = 0
    for mount_flag in launch_cfg_lib.MOUNT_FLAG_DIC[vmid]:
        blk = virt_io['block'][vmid][i]
        if not mount_flag:
            if blk:
                rootfs_img = blk.strip(':')
                print("   -s {},virtio-blk,{} \\".format(launch_cfg_lib.virtual_dev_slot("virtio-blk{}".format(blk)), rootfs_img), file=config)
            i += 1
            continue

        rootfs_img = blk.split(':')[1].strip(':')
        print("   -s {},virtio-blk,/data{}/{} \\".format(launch_cfg_lib.virtual_dev_slot("blk_mount_{}".format(i)), i, rootfs_img), file=config)
        i += 1

    # virtio-net set, the value type is a list
    for net in virt_io['network'][vmid]:
        if net:
            if launch_cfg_lib.is_linux_like(user_vm_type) or user_vm_type in ("ANDROID", "ALIOS"):
                print("   -s {},virtio-net,tap={},mac_seed=$mac_seed \\".format(launch_cfg_lib.virtual_dev_slot("virtio-net{}".format(net)), net), file=config)
            else:
                print("   -s {},virtio-net,tap={} \\".format(launch_cfg_lib.virtual_dev_slot("virtio-net{}".format(net)), net), file=config)

    # virtio-console set, the value type is a string
    if virt_io['console'][vmid]:
        print("   -s {},virtio-console,{} \\".format(
            launch_cfg_lib.virtual_dev_slot("virtio-console"),
                virt_io['console'][vmid]), file=config)


def sriov_args_set(dm, sriov, vmid, config):

    # sriov-gpu
    gpu_bdf = sriov['gpu'][vmid]
    if gpu_bdf:
        bus = int(gpu_bdf[0:2], 16)
        dev = int(gpu_bdf[3:5], 16)
        fun = int(gpu_bdf[6:7], 16)
        print('   -s 2,passthru,{}/{}/{},igd-vf  \\'.format(bus, dev, fun), file=config)

    # sriov-net
    if sriov['network'][vmid]:
        net_bdf = sriov['network'][vmid]
        bus = int(net_bdf[0:2], 16)
        dev = int(net_bdf[3:5], 16)
        fun = int(net_bdf[6:7], 16)
        print("   -s {},passthru,{}/{}/{} \\".format(
            launch_cfg_lib.virtual_dev_slot("sriov-net{}".format(net_bdf)), bus, dev, fun
        ), file=config)


def get_cpu_affinity_list(cpu_affinity, vmid):
    pcpu_id_list = ''
    for user_vmid,cpus in cpu_affinity.items():
        if vmid == user_vmid:
            pcpu_id_list = [id for id in list(cpu_affinity[user_vmid]) if id != None]
    return pcpu_id_list


def get_apic_id_list(scenario_pcpu_id_list):

    apic_id_list = []
    board_etree = lxml.etree.parse(common.BOARD_INFO_FILE)
    board_root = board_etree.getroot()
    board_pcpu_list = board_root.find('CPU_PROCESSOR_INFO').text.strip().split(',')
    if isinstance(board_pcpu_list, list):
        board_pcpu_list = [cpu.strip() for cpu in board_pcpu_list]

    if scenario_pcpu_id_list is not None:
        for scenario_pcup_id in scenario_pcpu_id_list:
            if scenario_pcup_id not in board_pcpu_list:
                key = "scenario config error"
                launch_cfg_lib.ERR_LIST[key] = "No available cpu {} in {} file.".format(scenario_pcup_id, common.BOARD_INFO_FILE)

    for pcpu_id in scenario_pcpu_id_list:
        apic_id = common.get_node(f"//processors/die/core/thread[cpu_id='{pcpu_id}']/apic_id/text()", board_etree)
        apic_id_list.append(int(apic_id, 16))

    return apic_id_list


def pcpu_arg_set(dm, vmid, config):

    pcpu_id_list = get_cpu_affinity_list(dm["cpu_affinity"], vmid)
    apic_id_list = get_apic_id_list(pcpu_id_list)

    if apic_id_list:
        print("   --cpu_affinity {} \\".format(','.join([str(id) for id in apic_id_list])), file=config)


def dm_arg_set(names, sel, virt_io, dm, sriov, vmid, config):

    user_vm_type = names['user_vm_types'][vmid]
    board_name = names['board_name']

    sos_vmid = launch_cfg_lib.get_sos_vmid()

    # clearlinux/android/alios
    print('acrn-dm -m $mem_size -s 0:0,hostbridge \\', file=config)
    if launch_cfg_lib.is_linux_like(user_vm_type) or user_vm_type in ("ANDROID", "ALIOS"):
        if user_vm_type in ("ANDROID", "ALIOS"):
            print("   -s {},virtio-rpmb \\".format(launch_cfg_lib.virtual_dev_slot("virtio-rpmb")), file=config)
            print("   --enable_trusty \\", file=config)

    if dm['rtos_type'][vmid] != "no":
        if virt_io:
            print("   --virtio_poll 1000000 \\", file=config)

        if dm['rtos_type'][vmid] == "Soft RT":
            print("   --rtvm \\", file=config)

        if dm['rtos_type'][vmid] == "Hard RT":
            print("   --lapic_pt \\", file=config)

    # windows
    if user_vm_type == "WINDOWS":
        print("   --windows \\", file=config)

    # set logger_setting for all VMs
    print("   $logger_setting \\", file=config)

    # GPU PT args set
    gpu_pt_arg_set(dm, sel, vmid, config)

    # SRIOV args set
    sriov_args_set(dm, sriov, vmid, config)

    # XHCI args set
    xhci_args_set(dm, vmid, config)

    # VIRTIO args set
    virtio_args_set(dm, names, virt_io, vmid, config)

    # vbootloader setting
    vboot_arg_set(dm, vmid, config)

    # pcpu-list args set
    pcpu_arg_set(dm, vmid, config)

    # shm regions args set
    shm_arg_set(dm, vmid, config)

    # ssram set
    ssram_enabled = 'n'
    try:
        ssram_enabled = common.get_hv_item_tag(common.SCENARIO_INFO_FILE, "FEATURES", "SSRAM", "SSRAM_ENABLED")
    except:
        pass
    if user_vm_type == "PREEMPT-RT LINUX" and ssram_enabled == 'y':
        print("   --ssram \\", file=config)

    for key, value in sel.bdf.items():
        if key == 'gpu':
            continue
        if value[vmid]:
            print("   $intr_storm_monitor \\", file=config)
            break

    if user_vm_type != "PREEMPT-RT LINUX":
        print("   -s 1:0,lpc \\", file=config)

    # redirect console
    if dm['vuart0'][vmid] == "Enable":
        print("   -l com1,stdio \\", file=config)

    if launch_cfg_lib.is_linux_like(user_vm_type) or user_vm_type in ("ANDROID", "ALIOS"):
        if board_name == "apl-mrb":
            print("   -l com2,/run/acrn/ioc_$vm_name \\", file=config)

        if not is_nuc_whl_linux(names, vmid):
            print("   -s {},wdt-i6300esb \\".format(launch_cfg_lib.virtual_dev_slot("wdt-i6300esb")), file=config)

    set_dm_pt(names, sel, vmid, config, dm)

    if dm['console_vuart'][vmid] == "Enable":
        print("   -s {},uart,vuart_idx:0 \\".format(launch_cfg_lib.virtual_dev_slot("console_vuart")), file=config)
    for vuart_id in dm["communication_vuarts"][vmid]:
        if not vuart_id:
            break
        print("   -s {},uart,vuart_idx:{} \\".format(
            launch_cfg_lib.virtual_dev_slot("communication_vuart_{}".format(vuart_id)), vuart_id), file=config)


    print("   $vm_name", file=config)
    print("}", file=config)


def gen(names, pt_sel, virt_io, dm, sriov, vmid, config):

    board_name = names['board_name']
    user_vm_type = names['user_vm_types'][vmid]

    # passthrough bdf/vpid dictionay
    pt.gen_pt_head(names, dm, sriov, pt_sel, vmid, config)

    # gen launch header
    launch_begin(names, virt_io, vmid, config)
    tap_user_vm_net(names, virt_io,  vmid, config)

    # passthrough device
    pt.gen_pt(names, dm, sriov, pt_sel, vmid, config)
    wa_usage(user_vm_type, config)
    mem_size_set(dm, vmid, config)
    interrupt_storm(pt_sel, config)
    log_level_set(user_vm_type, config)

    # gen acrn-dm args
    dm_arg_set(names, pt_sel, virt_io, dm, sriov, vmid, config)

    # gen launch end
    launch_end(names, dm, virt_io, vmid, config)
