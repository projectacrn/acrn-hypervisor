# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys

from acpiparser.apic import APIC
from acpiparser.asf import ASF
from acpiparser.dmar import DMAR
from acpiparser.dsdt import DSDT
from acpiparser.facp import FACP
from acpiparser.rtct import RTCT

def parse_table(signature, path=None):
    if not path:
        path = f"/sys/firmware/acpi/tables/{signature}"
    signature = signature.rstrip("!")
    fn = getattr(sys.modules[f"acpiparser.{signature.lower()}"], signature)
    return fn(path)

def make_parser(signature):
    def parse(path=None):
        return parse_table(signature, path)
    return parse

parse_apic = make_parser('APIC')
parse_asf = make_parser('ASF!')
parse_dsdt = make_parser('DSDT')
parse_dmar = make_parser('DMAR')
parse_facp = make_parser('FACP')
parse_rtct = make_parser('RTCT')
