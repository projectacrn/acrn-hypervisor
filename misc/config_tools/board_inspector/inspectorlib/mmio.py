#!/usr/bin/env python3

import argparse
import lxml.etree

def mmio_regions(etree):
    ret = []

    resources = etree.xpath("//resources/mmio")
    for res in resources:
        base = res.get("min")
        top = res.get("max")
        dev = res.getparent().getparent()
        obj = dev.get("object")
        ret.append((obj, int(base, base=16), int(top, base=16)))

    return sorted(ret, key=lambda x:(x[1], x[2]))

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("file", help="board XML file")
    args = parser.parse_args()

    etree = lxml.etree.parse(args.file)
    regions = mmio_regions(etree)
    for region in regions:
        print("%-4s 0x%08x 0x%08x" % (region[0], region[1], region[2]))
