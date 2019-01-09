#!/usr/bin/env python

# Copyright (C) 2018 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

import ConfigParser
import uuid
import struct
import sys

type_2_guid = {
# official guid for gpt partition type
    'fat' : 'ebd0a0a2-b9e5-4433-87c0-68b6b72699c7',
    'esp' : 'c12a7328-f81f-11d2-ba4b-00a0c93ec93b',
    'linux' : '0fc63daf-8483-4772-8e79-3d69d8477de4',
    'linux-swap' : '0657fd6d-a4ab-43c4-84e5-0933c84b4f4f',
# generated guid for android
    'boot' : '49a4d17f-93a3-45c1-a0de-f50b2ebe2599',
    'recovery' : '4177c722-9e92-4aab-8644-43502bfd5506',
    'misc' : 'ef32a33b-a409-486c-9141-9ffb711f6266',
    'metadata' : '20ac26be-20b7-11e3-84c5-6cfdb94711e9',
    'tertiary' : '767941d0-2085-11e3-ad3b-6cfdb94711e9',
    'factory' : '9fdaa6ef-4b3f-40d2-ba8d-bff16bfb887b' }

def zero_pad(s, size):
    if (len(s) > size):
        print 'error', len(s)
    s += '\0' * (size - len(s))
    return s

def copy_section(cfg, a, b):
    cfg.add_section(b)
    for option in cfg.options(a):
        cfg.set(b, option, cfg.get(a, option))

def preparse_slots(cfg, partitions):
    if not cfg.has_option('base', 'nb_slot'):
        return partitions

    nb_slot = cfg.getint('base', 'nb_slot')

    parts_with_slot = []
    for p in partitions:
        section = "partition." + p
        if cfg.has_option(section, 'has_slot'):
            for i in range(ord('a'), ord('a') + nb_slot):
                suffix = "_%c" % i
                new_part = p + suffix
                new_section = "partition." + new_part

                copy_section(cfg, section, new_section)
                cfg.set(new_section, 'label', cfg.get(section, 'label') + suffix)
                parts_with_slot.append(new_part);
        else:
            parts_with_slot.append(p);

    return parts_with_slot

def preparse_partitions(gpt_in, cfg):
    with open(gpt_in, 'r') as f:
        data = f.read()

    partitions = cfg.get('base', 'partitions').split()

    for l in data.split('\n'):
        words = l.split()
        if len(words) > 2:
            if words[0] == 'partitions' and words[1] == '+=':
                partitions += words[2:]

    return partitions

def main():
    if len(sys.argv) != 2:
        print 'Usage : ', sys.argv[0], 'gpt_in1.ini'
        print '    write binary to stdout'
        sys.exit(1)

    gpt_in = sys.argv[1]

    cfg = ConfigParser.SafeConfigParser()

    cfg.read(gpt_in)

    part = preparse_partitions(gpt_in, cfg)
    part = preparse_slots(cfg, part)

    magic = 0x6a8b0da1
    start_lba = 0
    if cfg.has_option('base', 'start_lba'):
        start_lba = cfg.getint('base', 'start_lba')
    npart = len(part)

    out = sys.stdout
    out.write(struct.pack('<I', magic))
    out.write(struct.pack('<I', start_lba))
    out.write(struct.pack('<I', npart))
    for p in part:
        length = cfg.get('partition.' + p, 'len')
        out.write(struct.pack('<i', int(length)))

        label = cfg.get('partition.' + p, 'label').encode('utf-16le')
        out.write(zero_pad(label, 36 * 2))

        guid_type = cfg.get('partition.' + p, 'type')
        guid_type = uuid.UUID(type_2_guid[guid_type])
        out.write(guid_type.bytes_le)

        guid = uuid.uuid4()
        out.write(guid.bytes_le)

if __name__ == "__main__":
    main()
