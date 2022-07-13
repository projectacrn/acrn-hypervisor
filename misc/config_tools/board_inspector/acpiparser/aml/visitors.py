# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys

from .tree import Visitor, Direction
from . import grammar

class PrintLayoutVisitor(Visitor):
    @staticmethod
    def __is_printable(s):
        return all(ord(c) >= 0x20 and ord(c) < 0xFF for c in s)

    def __init__(self):
        super().__init__(Direction.TOPDOWN)

    def default(self, tree):
        indent = "  " * self.depth
        print(f"{indent}{tree.label}", end="")
        if hasattr(tree, "value"):
            if isinstance(tree.value, int):
                print(f" = {hex(tree.value)}", end="")
            elif isinstance(tree.value, str):
                if self.__is_printable(tree.value):
                    print(f" = '{tree.value}'", end="")
        if tree.deferred_range:
            print(f" (deferred at {hex(tree.deferred_range[0])}, length {hex(tree.deferred_range[1])})", end="")
        if tree.factory:
            print(f" {tree.factory.label}: {tree.factory.seq}", end="")
        print()

class ConditionallyUnregisterSymbolVisitor(Visitor):
    def __init__(self, interpreter):
        super().__init__(Direction.CUSTOMIZED)
        self.context = interpreter.context
        self.interpreter = interpreter
        self.conditionally_hidden = False

        self.DefName = self.__unregister(0, False)
        self.DefMethod = self.__unregister(1, False)
        self.DefDevice = self.__unregister(1, True)

    def __unregister(self, name_string_idx, go_on):
        def f(tree):
            if self.conditionally_hidden:
                scope = tree.scope
                name = tree.children[name_string_idx].value
                realpath = self.context.realpath(scope, name)
                self.context.unregister_object(realpath)
            return go_on
        return f

    def visit(self, tree):
        if not self.conditionally_hidden and tree.label == "DefIfElse":
            self.context.change_scope(tree.scope)
            cond = self.interpreter.interpret(tree.children[1]).get()
            self.context.pop_scope()

            self.depth += 1
            if cond:
                if hasattr(tree, "TermList"):
                    self.visit(tree.TermList)
                if hasattr(tree, "DefElse") and tree.DefElse:
                    self.conditionally_hidden = True
                    self.visit(tree.DefElse)
                    self.conditionally_hidden = False
            else:
                if hasattr(tree, "TermList"):
                    self.conditionally_hidden = True
                    self.visit(tree.TermList)
                    self.conditionally_hidden = False
                if hasattr(tree, "DefElse") and tree.DefElse:
                    self.visit(tree.DefElse)
            self.depth -= 1
        else:
            self._visit_topdown(tree)

class GenerateBinaryVisitor(Visitor):
    def __init__(self):
        super().__init__(Direction.BOTTOMUP)

    @staticmethod
    def __format_length(length):
        assert length <= 0x0FFFFFFF
        if length <= 0x3F:
            return length.to_bytes(1, sys.byteorder)
        else:
            first_byte_value = length & 0xF
            rest_byte_value = length >> 4
            if rest_byte_value <= 0xFF:
                first_byte = (first_byte_value + (1 << 6)).to_bytes(1, sys.byteorder)
                rest_bytes = rest_byte_value.to_bytes(1, sys.byteorder)
            elif rest_byte_value <= 0xFFFF:
                first_byte = (first_byte_value + (2 << 6)).to_bytes(1, sys.byteorder)
                rest_bytes = rest_byte_value.to_bytes(2, sys.byteorder)
            else:
                first_byte = (first_byte_value + (3 << 6)).to_bytes(1, sys.byteorder)
                rest_bytes = rest_byte_value.to_bytes(3, sys.byteorder)
            return first_byte + rest_bytes

    @staticmethod
    def __format_pkg_length(length):
        assert length <= 0x0FFFFFFB
        if length <= 0x3E:
            length += 1
        elif length <= 0x0FFD:
            length += 2
        elif length <= 0x0FFFFC:
            length += 3
        else:
            length += 4
        return GenerateBinaryVisitor.__format_length(length)

    def generate(self, tree):
        self.acc = []
        self.visit(tree)
        assert len(self.acc) == 1
        return self.acc.pop()

    def ByteData(self, tree):
        self.acc.append(tree.value.to_bytes(1, sys.byteorder))

    def WordData(self, tree):
        self.acc.append(tree.value.to_bytes(2, sys.byteorder))

    def DWordData(self, tree):
        self.acc.append(tree.value.to_bytes(4, sys.byteorder))

    def TWordData(self, tree):
        self.acc.append(tree.value.to_bytes(6, sys.byteorder))

    def QWordData(self, tree):
        self.acc.append(tree.value.to_bytes(8, sys.byteorder))

    def NameSeg(self, tree):
        name = tree.value[:4]
        if len(name) < 4:
            name += ("_" * (4 - len(name)))
        self.acc.append(bytes(name, "ascii"))

    def NameString(self, tree):
        acc = bytearray()
        segs = tree.value.lstrip("^\\")
        if len(segs) < len(tree.value):
            acc.extend(bytes(tree.value[:len(tree.value) - len(segs)], "ascii"))
        if segs:
            nr_dots = segs.count(".")
            if nr_dots == 1:
                acc.append(grammar.AML_DUAL_NAME_PREFIX)
            elif nr_dots >= 2:
                acc.append(grammar.AML_MULTI_NAME_PREFIX)
                acc.append(nr_dots + 1)
            acc.extend(bytes(segs.replace(".", ""), "ascii"))
        else:
            acc.append(grammar.AML_NULL_NAME)
        self.acc.append(acc)

    def String(self, tree):
        acc = bytearray()
        acc.append(grammar.AML_STRING_PREFIX)
        acc.extend(bytes(tree.value, "latin-1"))
        acc.append(0)
        self.acc.append(acc)

    def ByteList(self, tree):
        self.acc.append(tree.value)

    def PkgLength(self, tree):
        pass

    def FieldLength(self, tree):
        self.acc.append(self.__format_length(tree.value))

    def default(self, tree):
        assert tree.structure and isinstance(tree.structure, tuple)
        assert tree.structure != ("value",), f"{tree.label} is not expected to be handled by the default handler"

        parts = []
        for child in reversed(tree.children):
            if child.label == "PkgLength":
                if tree.label == "DefIfElse" and tree.DefElse:
                    # The last component of a DefIfElse clause (i.e. DefElse) is not counted by the PkgLength of the
                    # DefIfElse.
                    length = sum(map(len, parts[1:]))
                else:
                    length = sum(map(len, parts))
                parts.append(self.__format_pkg_length(length))
            else:
                parts.append(self.acc.pop())
        if isinstance(tree.structure[0], int):
            opcode = tree.structure[0]
            if opcode <= 0xFF:
                parts.append(opcode.to_bytes(1, sys.byteorder))
            else:
                parts.append(opcode.to_bytes(2, sys.byteorder))

        self.acc.append(b''.join(reversed(parts)))
