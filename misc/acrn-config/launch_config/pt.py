# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import launch_cfg_lib

MEDIA_DEV = ['ipu', 'ipu_i2c', 'cse', 'audio', 'audio_codec']


def pass_through_dev(sel, pt_dev, vmid, config):

    bdf = sel.bdf[pt_dev][vmid]
    if not bdf:
        return

    print("# Passthrough {}".format(pt_dev.upper()), file=config)
    print('echo ${passthru_vpid["%s"]} > /sys/bus/pci/drivers/pci-stub/new_id'%pt_dev, file=config)
    print('echo ${passthru_bdf["%s"]} > /sys/bus/pci/devices/${passthru_bdf["%s"]}/driver/unbind'%(pt_dev, pt_dev), file=config)
    print('echo ${passthru_bdf["%s"]} > /sys/bus/pci/drivers/pci-stub/bind'%pt_dev, file=config)
    print("", file=config)


def ipu_pt(sel, cap_pt, vmid, config):

    if not sel.bdf['ipu'][vmid] and not sel.bdf['ipu_i2c'][vmid]:
        return

    bdf_ipu = sel.bdf['ipu'][vmid]
    if bdf_ipu:
        bus = bdf_ipu[0:2]
        dev = bdf_ipu[3:5]
        fun = bdf_ipu[6:7]
        slot_ipu = sel.slot['ipu'][vmid]

    bdf_ipu_i2c = sel.bdf['ipu_i2c'][vmid]
    if bdf_ipu_i2c:
        bus_i2c = bdf_ipu_i2c[0:2]
        dev_i2c = bdf_ipu_i2c[3:5]
        fun_i2c = bdf_ipu_i2c[6:7]
        slot_ipu_i2c = sel.slot['ipu_i2c'][vmid]

    if "ipu" in cap_pt or "ipu_i2c" in cap_pt:
        if bdf_ipu or bdf_ipu_i2c:
            print("ipu_passthrough=0", file=config)
            print("", file=config)
            print("# Check the device file of /dev/vbs_ipu to determine the IPU mode", file=config)
            print('if [ ! -e "/dev/vbs_ipu" ]; then', file=config)
            print("ipu_passthrough=1", file=config)
            print("fi", file=config)
            print('boot_ipu_option=""', file=config)
            print("if [ $ipu_passthrough == 1 ];then", file=config)

        if bdf_ipu:
            print("    # for ipu passthrough - ipu device", file=config)
            print('    if [ -d "/sys/bus/pci/devices/${passthru_bdf["ipu"]}" ]; then', file=config)
            print('        echo ${passthru_vpid["ipu"]} > /sys/bus/pci/drivers/pci-stub/new_id', file=config)
            print('        echo ${passthru_bdf["ipu"]} > /sys/bus/pci/devices/${passthru_bdf["ipu"]}/driver/unbind', file=config)
            print('        echo ${passthru_bdf["ipu"]} > /sys/bus/pci/drivers/pci-stub/bind', file=config)
            print('        boot_ipu_option="$boot_ipu_option"" -s {},passthru,{}/{}/{} "'.format(
                slot_ipu, bus, dev, fun), file=config)
            print("    fi", file=config)
            print("", file=config)

        if bdf_ipu_i2c:
            print("    # for ipu passthrough - ipu related i2c", file=config)
            print("    # please use virtual slot for i2c to make sure that the i2c controller", file=config)
            print("    # could get the same virtaul BDF as physical BDF", file=config)
            print('    if [ -d "/sys/bus/pci/devices/${passthru_bdf["ipu_i2c"]}" ]; then', file=config)
            print('        echo ${passthru_vpid["ipu_i2c"]} > /sys/bus/pci/drivers/pci-stub/new_id', file=config)
            print('        echo ${passthru_bdf["ipu_i2c"]} > /sys/bus/pci/devices/${passthru_bdf["ipu_i2c"]}/driver/unbind', file=config)
            print('        echo ${passthru_bdf["ipu_i2c"]} > /sys/bus/pci/drivers/pci-stub/bind', file=config)
            print('        boot_ipu_option="$boot_ipu_option"" -s {},passthru,{}/{}/{} "'.format(
                slot_ipu_i2c, bus_i2c, dev_i2c, fun_i2c), file=config)
            print("    fi", file=config)

        if bdf_ipu or bdf_ipu_i2c:
            print("else", file=config)
            print('    boot_ipu_option="$boot_ipu_option"" -s {},virtio-ipu "'.format(launch_cfg_lib.virtual_dev_slot("virtio-ipu")), file=config)
            print("fi", file=config)
            print("", file=config)


def cse_pt(sel, cap_pt, vmid, config):

    if not sel.bdf['cse'][vmid]:
        return

    bdf = sel.bdf['cse'][vmid]
    if bdf:
        bus = bdf[0:2]
        dev = bdf[3:5]
        fun = bdf[6:7]
        slot = sel.slot['cse'][vmid]

    if "cse" in cap_pt:
        if bdf:
            print("cse_passthrough=0", file=config)
            print("hbm_ver=`cat /sys/class/mei/mei0/hbm_ver`", file=config)
            print("major_ver=`echo $hbm_ver | cut -d '.' -f1`", file=config)
            print("minor_ver=`echo $hbm_ver | cut -d '.' -f2`", file=config)
            print('if [[ "$major_ver" -lt "2" ]] || \\', file=config)
            print('   [[ "$major_ver" == "2" && "$minor_ver" -lt "2" ]]; then', file=config)
            print("    cse_passthrough=1", file=config)
            print("fi", file=config)
            print('boot_cse_option=""', file=config)
            print("if [ $cse_passthrough == 1 ]; then", file=config)
            print('    echo ${passthru_vpid["cse"]} > /sys/bus/pci/drivers/pci-stub/new_id', file=config)
            print('    echo ${passthru_bdf["cse"]} > /sys/bus/pci/devices/${passthru_bdf["cse"]}/driver/unbind', file=config)
            print('    echo ${passthru_bdf["cse"]} > /sys/bus/pci/drivers/pci-stub/bind', file=config)
            print('    boot_cse_option="$boot_cse_option"" -s {},passthru,{}/{}/{} "'.format(
                slot, bus, dev, fun), file=config)
            print("else", file=config)
            print('    boot_cse_option="$boot_cse_option"" -s {},virtio-heci,{}/{}/{} "'.format(
                slot, bus, dev, fun), file=config)
            print("fi", file=config)
            print("", file=config)


def audio_pt(uos_type, sel, cap_pt, vmid, config):

    if not sel.bdf['audio'][vmid] and not sel.bdf['audio_codec'][vmid]:
        return

    bdf_audio = sel.bdf['audio'][vmid]
    if bdf_audio:
        bus = bdf_audio[0:2]
        dev = bdf_audio[3:5]
        fun = bdf_audio[6:7]
        slot_audio = sel.slot['audio'][vmid]

    bdf_codec = sel.bdf['audio_codec'][vmid]
    if bdf_codec:
        bus_codec = bdf_codec[0:2]
        dev_codec = bdf_codec[3:5]
        fun_codec = bdf_codec[6:7]
        slot_codec = sel.slot['audio_codec'][vmid]

    if uos_type == "WINDOWS":
        print('    echo ${passthru_vpid["audio"]} > /sys/bus/pci/drivers/pci-stub/new_id', file=config)
        print('    echo ${passthru_bdf["audio"]} > /sys/bus/pci/devices/${passthru_bdf["audio"]}/driver/unbind', file=config)
        print('    echo ${passthru_bdf["audio"]} > /sys/bus/pci/drivers/pci-stub/bind', file=config)
        return

    if "audio" in cap_pt or "audio_codec" in cap_pt:
        if bdf_audio:
            print("kernel_version=$(uname -r)", file=config)
            print('audio_module="/usr/lib/modules/$kernel_version/kernel/sound/soc/intel/boards/snd-soc-sst_bxt_sos_tdf8532.ko"', file=config)
            print("", file=config)
            print("# use the modprobe to force loading snd-soc-skl/sst_bxt_bdf8532", file=config)
            print("if [ ! -e $audio_module ]; then", file=config)
            print("modprobe -q snd-soc-skl", file=config)
            print("modprobe -q snd-soc-sst_bxt_tdf8532", file=config)
            print("else", file=config)
            print("", file=config)
            print("modprobe -q snd_soc_skl", file=config)
            print("modprobe -q snd_soc_tdf8532", file=config)
            print("modprobe -q snd_soc_sst_bxt_sos_tdf8532", file=config)
            print("modprobe -q snd_soc_skl_virtio_be", file=config)
            print("fi", file=config)
            print("audio_passthrough=0", file=config)
            print("", file=config)
            print("# Check the device file of /dev/vbs_k_audio to determine the audio mode", file=config)
            print('if [ ! -e "/dev/vbs_k_audio" ]; then', file=config)
            print("audio_passthrough=1", file=config)
            print("fi", file=config)
            print('boot_audio_option=""', file=config)
            print("if [ $audio_passthrough == 1 ]; then", file=config)
            print("    # for audio device", file=config)
            print('    echo ${passthru_vpid["audio"]} > /sys/bus/pci/drivers/pci-stub/new_id', file=config)
            print('    echo ${passthru_bdf["audio"]} > /sys/bus/pci/devices/${passthru_bdf["audio"]}/driver/unbind', file=config)
            print('    echo ${passthru_bdf["audio"]} > /sys/bus/pci/drivers/pci-stub/bind', file=config)
            print("", file=config)

            print("    # for audio codec", file=config)
            print('    echo ${passthru_vpid["audio_codec"]} > /sys/bus/pci/drivers/pci-stub/new_id', file=config)
            print('    echo ${passthru_bdf["audio_codec"]} > /sys/bus/pci/devices/${passthru_bdf["audio_codec"]}/driver/unbind', file=config)
            print('    echo ${passthru_bdf["audio_codec"]} > /sys/bus/pci/drivers/pci-stub/bind', file=config)
            print("", file=config)

            print('    boot_audio_option="-s {},passthru,{}/{}/{},'.format(
                slot_audio, bus, dev, fun), end="", file=config)
            print('keep_gsi -s {},passthru,{}/{}/{}"'.format(
                slot_codec, bus_codec, dev_codec, fun_codec), file=config)
            print("else", file=config)
            print('    boot_audio_option="-s {},virtio-audio"'.format(slot_audio), file=config)
            print("fi", file=config)


def media_pt(uos_type, sel, cap_pt, vmid, config):
    ipu_pt(sel, cap_pt, vmid, config)
    cse_pt(sel, cap_pt, vmid, config)
    audio_pt(uos_type, sel, cap_pt, vmid, config)


def gen_pt(names, sel, vmid, config):

    pt_none = True
    cap_pt = launch_cfg_lib.get_board_pt_dev(names, vmid)
    uos_type = names['uos_types'][vmid]
    for pt_dev in cap_pt:
        if sel.bdf[pt_dev][vmid]:
            pt_none = False
    if pt_none:
        return

    print("modprobe pci_stub", file=config)
    for pt_dev in cap_pt:
        if pt_dev not in MEDIA_DEV:
            pass_through_dev(sel, pt_dev, vmid, config)
            continue

    media_pt(uos_type, sel, cap_pt, vmid, config)

def gen_pt_head(names, sel, vmid, config):

    # get passthrough device for specify board and uos
    cap_pt = launch_cfg_lib.get_board_pt_dev(names, vmid)
    uos_type = names['uos_types'][vmid]
    pt_none = True

    for pt_dev in cap_pt:
        if sel.bdf[pt_dev][vmid]:
            pt_none = False
    if pt_none:
        return

    print("# pci devices for passthru", file=config)
    print("declare -A passthru_vpid", file=config)
    print("declare -A passthru_bdf", file=config)
    print("", file=config)

    print("passthru_vpid=(", file=config)
    for pt_dev in cap_pt:
        if not sel.vpid[pt_dev] or not sel.vpid[pt_dev][vmid]:
            continue
        print('["{}"]="{}"'.format(pt_dev, sel.vpid[pt_dev][vmid]), file=config)
    print(')', file=config)

    print("passthru_bdf=(", file=config)
    for pt_dev in cap_pt:
        if not sel.bdf[pt_dev] or not sel.bdf[pt_dev][vmid]:
            continue
        print('["{}"]="0000:{}"'.format(pt_dev, sel.bdf[pt_dev][vmid]), file=config)
    print(')', file=config)

    print("", file=config)
