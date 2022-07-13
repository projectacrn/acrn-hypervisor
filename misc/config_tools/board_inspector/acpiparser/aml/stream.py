# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import logging

from .exception import *
from .grammar import AML_EXT_OP_PREFIX

class Stream:
    def print_binary(self, base):
        acc = f"[{hex(base):>8s}/{hex(len(self.data)):>8s}]"
        converted = ""
        for i in range(base, base + 16):
            code = self.data[i]
            acc += " %02x" % code
            if code >= 0x20 and code <= 0x7E:
                converted += chr(code)
            else:
                converted += "."
            if i > base and ((i - base) % 4) == 3:
                acc += " "
                converted += " "
        acc += f"    '{converted}'"
        logging.debug(acc)

    def __init__(self, data):
        self.data = data
        self.current = 0
        self.scopes = [len(data)]

    def peek_integer(self, count):
        if self.current + count > self.scopes[-1]:
            raise ScopeMismatch
        ret = 0
        for i in range(0, count):
            ret += (self.data[self.current + i] << (i * 8))
        return ret

    def get_integer(self, count):
        if self.current + count > self.scopes[-1]:
            raise ScopeMismatch
        ret = self.peek_integer(count)
        self.current += count
        return ret

    def get_char(self):
        if self.current + 1 > self.scopes[-1]:
            raise ScopeMismatch
        ret = chr(self.data[self.current])
        self.current += 1
        return ret

    def peek_opcode(self):
        opcode = self.peek_integer(1)
        if opcode == AML_EXT_OP_PREFIX:
            opcode = self.peek_integer(2)
            return (opcode, 2)
        return (opcode, 1)

    def get_opcode(self):
        opcode = self.get_integer(1)
        if opcode == AML_EXT_OP_PREFIX:
            opcode += (self.get_integer(1) << 8)
            return (opcode, 2)
        return (opcode, 1)

    def get_fixed_length_string(self, count):
        if self.current + count > self.scopes[-1]:
            raise ScopeMismatch
        ret = self.data[self.current : self.current + count].decode("latin-1")
        self.current += count
        return ret

    def get_string(self):
        null = self.data.find(0x00, self.current)
        assert null >= 0
        if null + 1 > self.scopes[-1]:
            raise ScopeMismatch
        ret = self.data[self.current:null].decode("latin-1")
        self.current = null + 1
        return ret

    def get_buffer(self):
        cur = self.current
        self.current = self.scopes[-1]
        return self.data[cur:self.current]

    def seek(self, offset, absolute=False):
        if absolute:
            self.current = offset
        else:
            self.current += offset

    def at_end(self):
        return self.current == self.scopes[-1]

    def push_scope(self, size):
        self.scopes.append(self.current + size)

    def pop_scope(self, force=False):
        if not force and not self.at_end():
            raise ScopeMismatch
        self.scopes.pop()

    def reset(self):
        self.current = 0
        self.scopes = [len(self.data)]

    def dump(self):
        self.print_binary(self.current - 48)
        self.print_binary(self.current - 32)
        self.print_binary(self.current - 16)
        self.print_binary(self.current)
