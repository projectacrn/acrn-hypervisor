# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

"""Base classes and infrastructure for CPUID and MSR decoding"""

from __future__ import print_function
import sys
import re
import functools
import inspect
import operator
import textwrap
import logging
from collections import namedtuple
from inspectorlib import external_tools

_wrapper = textwrap.TextWrapper(width=78, initial_indent='  ', subsequent_indent='    ')
regex_hex = "0x[0-9a-f]+"

class cpuid_result(namedtuple('cpuid_result', ['eax', 'ebx', 'ecx', 'edx'])):
    __slots__ = ()

    def __repr__(self):
        return "cpuid_result(eax={eax:#010x}, ebx={ebx:#010x}, ecx={ecx:#010x}, edx={edx:#010x})".format(**self._asdict())

def cpuid(cpu_id, leaf, subleaf):
    result = external_tools.run(["cpuid", "-l", str(leaf), "-s", str(subleaf), "-r"], check=True)
    stdout = result.stdout.decode("ascii").replace("\n", "")
    regex = re.compile(f"CPU {cpu_id}:[^:]*: eax=({regex_hex}) ebx=({regex_hex}) ecx=({regex_hex}) edx=({regex_hex})")
    m = regex.search(stdout)
    if m:
        regs = list(map(lambda idx: int(m.group(idx), base=16), range(1, 5)))
    else:
        regs = [0] * 4
    return cpuid_result(*regs)

class CPUID(object):
    # Subclasses must define a "leaf" field as part of the class definition.

    def __init__(self, regs):
        self.regs = regs

    @classmethod
    def read(cls, cpu_id, subleaf=0):
        r = cls(cpuid(cpu_id, cls.leaf, subleaf))
        r.cpu_id = cpu_id
        r.subleaf = subleaf
        return r

    # FIXME: This allows getting subleaves, but requires having an instance of
    # the class first, which means always reading subleaf 0 and then the
    # desired subleaf.
    def __getitem__(self, subleaf):
        return self.read(self.cpu_id, subleaf)

    def __eq__(self, other):
        return self.regs == other.regs

    def __ne__(self, other):
        return self.regs != other.regs

    def __str__(self):
        T = type(self)
        fields = dict((regnum, {}) for regnum in range(len(self.regs._fields)))
        properties = list()
        for field_name in dir(T):
            field = getattr(T, field_name)
            if isinstance(field, cpuidfield):
                fields[field.reg][field_name] = field
            elif isinstance(field, property):
                properties.append(field_name)

        heading = "CPU ID {:#x} -- ".format(self.cpu_id)
        heading += "CPUID (EAX={:#x}".format(self.leaf)
        if self.subleaf:
            heading += ", ECX={:#x}".format(self.subleaf)
        heading += ")"
        s = heading + "\n" + "-"*len(heading) + "\n"
        doc = inspect.getdoc(self)
        if doc:
            s += doc + "\n"

        def format_range(msb, lsb):
            if msb == lsb:
                return "[{}]".format(msb)
            return "[{}:{}]".format(msb, lsb)
        def format_field(msb, lsb, value):
            """Field formatter that special-cases single bits and drops the 0x"""
            if msb == lsb:
                return str(value)
            return "{:#x}".format(value)
        for regnum, regname in enumerate(self.regs._fields):
            s += "\n"
            s1 = "  {}={:#010x} ".format(regname, self.regs[regnum])
            s += s1
            inner = ("\n " + " " * len(s1)).join(
                    "{}{} {}={}".format(regname, format_range(field.msb, field.lsb), field_name, format_field(field.msb, field.lsb, getattr(self, field_name)))
                for field_name, field in sorted(fields[regnum].items(), key=(lambda x: x[1].lsb))
                )
            if inner:
                s += " {}".format(inner)

        properties = sorted(set(properties))
        if len(properties):
            s += "\n  Attributes derived from one or more fields:"
            for property_name in properties:
                s += '\n'
                temp = "{}={}".format(property_name, getattr(self, property_name))
                s += '\n'.join(_wrapper.wrap(temp))
        return s

class cpuidfield(property):
    def __init__(self, reg, msb, lsb, doc="Bogus"):
        self.reg = reg
        self.msb = msb
        self.lsb = lsb

        max_value = (1 << (msb - lsb + 1)) - 1
        field_mask = max_value << lsb

        def getter(self):
            return (self.regs[reg] & field_mask) >> lsb
        super(cpuidfield, self).__init__(getter, doc=doc)

class MSR(object):
    # Subclasses must define a "addr" field as part of the class definition.

    def __init__(self, value=0):
        self.value = value

    def __eq__(self, other):
        return self.value == other.value

    def __ne__(self, other):
        return self.value != other.value

    @classmethod
    def rdmsr(cls, cpu_id: int) -> int:
        try:
            with open(f'/dev/cpu/{cpu_id}/msr', 'rb', buffering=0) as msr_reader:
                msr_reader.seek(cls.addr)
                r = msr_reader.read(8)
                r = cls(int.from_bytes(r, 'little'))
        except FileNotFoundError:
            logging.critical(f"Missing CPU MSR file at /dev/cpu/{cpu_id}/msr. Check the value of CONFIG_X86_MSR " \
                             "in the kernel config.  Set it to 'Y' and rebuild the kernel. Then rerun the Board Inspector.")
            sys.exit(1)

        r.cpu_id = cpu_id
        return r

    def wrmsr(self, cpu_id=None):
        if cpu_id is None:
            cpu_id = self.cpu_id
        try:
            with open(f'/dev/cpu/{cpu_id}/msr', 'wb', buffering=0) as msr_reader:
                msr_reader.seek(self.addr)
                r = msr_reader.write(int.to_bytes(self.value, 8, 'little'))
        except FileNotFoundError:
            logging.critical(f"Missing CPU MSR file at /dev/cpu/{cpu_id}/msr. Check the value of CONFIG_X86_MSR " \
                             "in the kernel config.  Set it to 'Y' and rebuild the kernel. Then rerun the Board Inspector.")
            sys.exit(1)

    def __str__(self):
        T = type(self)
        fields = {}
        properties = []
        for field_name in dir(T):
            field = getattr(T, field_name)
            if isinstance(field, msrfield):
                fields[field_name] = field
            elif isinstance(field, property):
                properties.append(field_name)

        heading = "CPU ID {:#x} -- ".format(self.cpu_id)
        heading += "MSR {:#x}".format(self.addr)
        s = heading + "\n" + "-"*len(heading) + "\n"
        doc = inspect.getdoc(self)
        if doc:
            s += doc + "\n\n"
        s += "MSR {:#x}".format(self.addr)
        if self.value is None:
            s += ' value=GPF'
            return s

        s += ' value={:#x}'.format(self.value)

        for field_name, field in sorted(fields.items(), key=(lambda x: x[1].lsb)):
            s += '\n'
            temp = "[{}:{}] {}={:#x}".format(field.msb, field.lsb, field_name, getattr(self, field_name))
            # FIXME: check wrapper, and use a hanging indent to wrap the docstring to len(temp)+1
            if field.__doc__:
                temp += " " + inspect.getdoc(field)
            s += '\n'.join(_wrapper.wrap(temp))

        if properties:
            s += "\n  Attributes derived from one or more fields:"
            for property_name in sorted(properties):
                s += '\n'
                temp = "{}={}".format(property_name, getattr(self, property_name))
                # FIXME: check wrapper, get the property documentation string if any, and use a hanging indent to wrap the docstring to len(temp)+1
                s += '\n'.join(_wrapper.wrap(temp))
        return s

class msrfield(property):

    def __init__(self, msb, lsb, doc=None):
        self.msb = msb
        self.lsb = lsb

        max_value = (1 << (msb - lsb + 1)) - 1
        field_mask = max_value << lsb

        def getter(self):
            return (self.value & field_mask) >> lsb

        def setter(self, value):
            if value > max_value:
                if msb == lsb:
                    field = "[{0}]".format(msb)
                else:
                    field = "[{0}:{1}]".format(msb, lsb)
                raise OverflowError("Internal error: Value {value:#x} too big for MSR {self.addr:#x} field {field}.  " \
                "Rerun the Board Inspector with `--loglevel debug`.  If this issue persists," \
                "log a new issue at https://github.com/projectacrn/acrn-hypervisor/issues and attach the full logs.".format(**locals()))
            self.value = (self.value & ~field_mask) | (value << lsb)

        super(msrfield, self).__init__(getter, setter, doc=doc)
