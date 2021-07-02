# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import logging

from . import grammar
from .context import *
from .exception import *
from .tree import Tree, Transformer

class Factory:
    @staticmethod
    def hook_pre(context, tree):
        pass

    @staticmethod
    def hook_named(context, tree, name):
        pass

    @staticmethod
    def hook_post(context, tree):
        pass

    def __init__(self):
        self.level = 0
        self.label = "unknown"

    def mark_begin(self):
        if hasattr(self, "seq") and len(self.seq) > 1:
            logging.debug(f"%s-> {self.label}" % ("  " * self.level))
        self.level += 1

    def mark_end(self):
        self.level -= 1
        if hasattr(self, "seq") and len(self.seq) > 1:
            logging.debug(f"%s<- {self.label}" % ("  " * self.level))

    def match(self, context, stream, tree):
        raise NotImplementedError

    def parse(self, context, tree):
        self.mark_begin()
        tree.label = self.label
        tree.scope = context.get_scope()
        self.hook_pre(context, tree)
        try:
            self.match(context, context.current_stream, tree)
        except Exception as e:
            self.hook_post(context, tree)
            self.mark_end()
            raise

        self.hook_post(context, tree)
        self.mark_end()
        return tree

    def opcodes(self):
        raise NotImplementedError

################################################################################
# 20.2.2 Name Objects Encoding
################################################################################

class NameSegFactory(Factory):
    def __init__(self):
        super().__init__()
        self.__opcodes = []
        for i in range(ord('A'), ord('Z') + 1):
            self.__opcodes.append(i)
        self.__opcodes.append(ord('_'))
        self.label = "NameSeg"

    def match(self, context, stream, tree):
        tree.children = stream.get_fixed_length_string(4)

    def opcodes(self):
        return self.__opcodes

NameSeg = NameSegFactory()

class NameStringFactory(Factory):
    def __init__(self):
        super().__init__()
        self.label = "NameString"
        self.__opcodes = []
        for i in range(ord('A'), ord('Z') + 1):
            self.__opcodes.append(i)
        self.__opcodes.extend([ord('_'), ord('\\'), ord('^'), grammar.AML_DUAL_NAME_PREFIX, grammar.AML_MULTI_NAME_PREFIX])

    def match(self, context, stream, tree):
        acc = ""

        # Namespace prefixes
        char = stream.get_char()
        while char in ["\\", "^"]:
            acc += char
            char = stream.get_char()

        # Object name
        if ord(char) == grammar.AML_DUAL_NAME_PREFIX:
            if acc and acc[-1] not in ["\\", "^"]:
                acc += "."
            acc += stream.get_fixed_length_string(4)
            acc += "."
            acc += stream.get_fixed_length_string(4)
        elif ord(char) == grammar.AML_MULTI_NAME_PREFIX:
            seg_count = stream.get_integer(1)
            for i in range(0, seg_count):
                if acc and acc[-1] not in ["\\", "^"]:
                    acc += "."
                acc += stream.get_fixed_length_string(4)
        elif char == "\x00":    # NullName
            pass
        else:                   # NameSeg
            stream.seek(-1)
            acc += stream.get_fixed_length_string(4)

        tree.children = acc

    def opcodes(self):
        return self.__opcodes

NameString = NameStringFactory()

################################################################################
# 20.2.3 Data Objects Encoding
################################################################################

class ConstDataFactory(Factory):
    def __init__(self, label, width):
        super().__init__()
        self.label = label
        self.width = width

    def match(self, context, stream, tree):
        tree.children = stream.get_integer(self.width)
        return tree

    def opcodes(self):
        return None

ByteData = ConstDataFactory("ByteData", 1)
WordData = ConstDataFactory("WordData", 2)
DWordData = ConstDataFactory("DWordData", 4)
TWordData = ConstDataFactory("TWordData", 6)
QWordData = ConstDataFactory("QWordData", 8)

class StringFactory(Factory):
    def __init__(self):
        super().__init__()
        self.label = "String"

    def match(self, context, stream, tree):
        assert stream.get_opcode()[0] == grammar.AML_STRING_PREFIX

        tree.children = stream.get_string()
        return tree

    def opcodes(self):
        return [grammar.AML_STRING_PREFIX]

String = StringFactory()

class ByteListFactory(Factory):
    def __init__(self):
        super().__init__()
        self.label = "ByteList"

    def match(self, context, stream, tree):
        tree.children = stream.get_buffer()
        stream.pop_scope()

ByteList = ByteListFactory()

################################################################################
# 20.2.4 Package Length Encoding
################################################################################

class PkgLengthFactory(Factory):
    @staticmethod
    def get_package_length(byte_count, value):
        if byte_count == 0:
            total_size = (value & 0x3F)
        else:
            total_size = value & 0x0F
            for i in range(1, byte_count + 1):
                byte = (value & (0xFF << (i * 8))) >> (i * 8)
                total_size |= (byte << (i * 8 - 4))
        return total_size

    def __init__(self, label, create_new_scope):
        super().__init__()
        self.label = label
        self.create_new_scope = create_new_scope

    def match(self, context, stream, tree):
        pkg_lead_byte = stream.peek_integer(1)
        byte_count = pkg_lead_byte >> 6
        assert byte_count <= 3

        tree.children = self.get_package_length(byte_count, stream.get_integer(byte_count + 1))

        if self.create_new_scope:
            remaining = tree.children - byte_count - 1
            stream.push_scope(remaining)
            tree.package_range = (stream.current, remaining)
        return tree

PkgLength = PkgLengthFactory("PkgLength", True)
FieldLength = PkgLengthFactory("FieldLength", False)

################################################################################
# 20.2.5 Term Objects Encoding
################################################################################

class MethodInvocationFactory(Factory):
    def __init__(self):
        super().__init__()
        self.__opcodes = None
        self.label = "MethodInvocation"

    def match(self, context, stream, tree):
        child_namestring = Tree()
        globals()["NameString"].parse(context, child_namestring)
        tree.append_child(child_namestring)

        sym = context.lookup_symbol(child_namestring.children)
        if isinstance(sym, (MethodDecl, PredefinedMethodDecl)):
            for i in range(0, sym.nargs):
                child_arg = Tree()
                globals()["TermArg"].parse(context, child_arg)
                tree.append_child(child_arg)

        return tree

    def opcodes(self):
        if not self.__opcodes:
            self.__opcodes = globals()["NameString"].opcodes()
        return self.__opcodes

MethodInvocation = MethodInvocationFactory()

################################################################################
# Infrastructure Factories
################################################################################

class SequenceFactory(Factory):
    def __init__(self, label, seq):
        super().__init__()
        self.label = label
        self.seq = seq
        self.__opcodes = None

    def match(self, context, stream, tree):
        # When a TermList is empty, the stream has already come to the end of the current scope here. Do not attempt to
        # peek the next opcode in such cases.
        if stream.at_end() and \
           (self.seq[0][-1] in ["*", "?"]):
            stream.pop_scope()
            return tree

        opcode, opcode_width = stream.peek_opcode()
        package_end = 0

        # Under any case this function shall maintain the balance of stream scopes. The following flags indicate the
        # cleanup actions upon exceptions.
        to_recover_from_deferred_mode = False
        to_pop_stream_scope = False

        for i,elem in enumerate(self.seq):
            pos = stream.current
            try:
                if isinstance(elem, int):  # The leading opcode
                    assert elem == opcode
                    stream.seek(opcode_width)
                elif elem.endswith("*"):
                    elem = elem[:-1]
                    factory = globals()[elem]
                    while not stream.at_end():
                        child = Tree()
                        factory.parse(context, child)
                        tree.append_child(child)
                    stream.pop_scope()
                elif elem.endswith("?"):
                    elem = elem[:-1]
                    factory = globals()[elem]
                    if not stream.at_end():
                        sub_opcode, _ = stream.peek_opcode()
                        if sub_opcode in factory.opcodes():
                            child = Tree()
                            factory.parse(context, child)
                            tree.append_child(child)
                else:
                    factory = globals()[elem]
                    child = Tree()
                    factory.parse(context, child)
                    tree.append_child(child)
                    if child.label == "PkgLength":
                        to_pop_stream_scope = True
                        if child.package_range:
                            package_end = child.package_range[0] + child.package_range[1]
                            context.enter_deferred_mode()
                            to_recover_from_deferred_mode = True
                    elif child.label == "NameString":
                        self.hook_named(context, tree, child.children)
            except (DecodeError, DeferLater, ScopeMismatch, UndefinedSymbol) as e:
                if to_pop_stream_scope:
                    stream.pop_scope(force=True)
                    if to_recover_from_deferred_mode:
                        tree.deferred_range = (pos, package_end - pos)
                        tree.context_scope = context.get_scope()
                        tree.factory = SequenceFactory(f"{self.label}.deferred", self.seq[i:])
                        stream.seek(package_end, absolute=True)
                        break
                else:
                    raise e

        if to_recover_from_deferred_mode:
            context.exit_deferred_mode()
        return tree

    def opcodes(self):
        if not self.__opcodes:
            if isinstance(self.seq[0], int):
                self.__opcodes = [self.seq[0]]
            else:
                self.__opcodes = globals()[self.seq[0]].opcodes()
        return self.__opcodes

class OptionFactory(Factory):
    def __init__(self, label, opts):
        super().__init__()
        self.label = label
        self.opts = opts
        self.__opcodes = None

    def match(self, context, stream, tree):
        opcode, _ = stream.peek_opcode()

        for opt in self.opts:
            factory = globals()[opt]
            matched_opcodes = factory.opcodes()
            if matched_opcodes is None or opcode in matched_opcodes:
                child = Tree()
                tree.append_child(child)
                factory.parse(context, child)
                return tree

        raise DecodeError(opcode, self.label)

    def opcodes(self):
        if not self.__opcodes:
            self.__opcodes = []
            for opt in self.opts:
                self.__opcodes.extend(globals()[opt].opcodes())
        return self.__opcodes

class DeferredExpansion(Transformer):
    def __init__(self, context):
        super().__init__()
        self.context = context

        nodes = ["DefScope", "DefDevice", "DefMethod", "DefPowerRes", "DefProcessor", "DefThermalZone",
                 "DefIfElse", "DefElse", "DefWhile"]

        for i in nodes:
            setattr(self, i, self.__expand_deferred_range)

    def __expand_deferred_range(self, tree):
        if tree.deferred_range:
            start, size = tree.deferred_range
            self.context.current_stream.reset()
            self.context.current_stream.seek(start, absolute=True)
            self.context.current_stream.push_scope(size)

            aux_tree = Tree()
            self.context.change_scope(tree.context_scope)
            try:
                tree.factory.parse(self.context, aux_tree)
                tree.children.extend(aux_tree.children)
                tree.deferred_range = None
                tree.factory = None
            except (DecodeError, DeferLater, ScopeMismatch, UndefinedSymbol) as e:
                logging.debug(f"expansion of {tree.label} at {hex(tree.deferred_range[0])} failed due to: " + str(e))

            self.context.pop_scope()

        return tree

################################################################################
# Hook functions
################################################################################

def DefAlias_hook_post(context, tree):
    target = tree.children[0].children
    name = tree.children[1].children
    sym = AliasDecl(name, target, tree)
    context.register_symbol(sym)

def DefName_hook_named(context, tree, name):
    sym = NamedDecl(name, tree)
    context.register_symbol(sym)

def DefScope_hook_named(context, tree, name):
    context.change_scope(name)

def DefScope_hook_post(context, tree):
    context.pop_scope()


def DefCreateBitField_hook_named(context, tree, name):
    name = tree.children[2].children
    sym = FieldDecl(name, 1, tree)
    context.register_symbol(sym)

def DefCreateByteField_hook_named(context, tree, name):
    name = tree.children[2].children
    sym = FieldDecl(name, 8, tree)
    context.register_symbol(sym)

def DefCreateDWordField_hook_named(context, tree, name):
    name = tree.children[2].children
    sym = FieldDecl(name, 32, tree)
    context.register_symbol(sym)

def DefCreateField_hook_named(context, tree, name):
    name = tree.children[3].children
    sym = FieldDecl(name, 0, tree)
    context.register_symbol(sym)

def DefCreateQWordField_hook_named(context, tree, name):
    name = tree.children[2].children
    sym = FieldDecl(name, 64, tree)
    context.register_symbol(sym)

def DefCreateWordField_hook_named(context, tree, name):
    name = tree.children[2].children
    sym = FieldDecl(name, 16, tree)
    context.register_symbol(sym)

def DefDevice_hook_named(context, tree, name):
    sym = DeviceDecl(name, tree)
    context.register_symbol(sym)
    context.enter_scope(name)

def DefDevice_hook_post(context, tree):
    context.pop_scope()

def DefExternal_hook_post(context, tree):
    name = tree.children[0].children
    ty = tree.children[1].children[0].children
    nargs = tree.children[2].children[0].children

    if ty == 0x8:     # an external method
        sym = MethodDecl(name, nargs, tree)
    else:
        sym = NamedDecl(name, tree)
    context.register_symbol(sym)

access_width_map = {
    0: 8,    # AnyAcc
    1: 8,    # ByteAcc
    2: 16,   # WordAcc
    3: 32,   # DWordAcc
    4: 64,   # QWordAcc
    5: 8,    # BufferAcc
    # The other values are reserved
}

def DefField_hook_post(context, tree):
    # Update the fields with region & offset info
    region_name = context.lookup_symbol(tree.children[1].children).name
    flags = tree.children[2].children[0].children
    access_width = access_width_map[flags & 0xF]
    fields = tree.children[3].children
    bit_offset = 0
    for field in fields:
        field = field.children[0]
        if field.label == "NamedField":
            name = field.children[0].children
            length = field.children[1].children
            sym = context.lookup_symbol(name)
            assert isinstance(sym, OperationFieldDecl)
            sym.set_location(region_name, bit_offset, access_width)
            bit_offset += length
        elif field.label == "ReservedField":
            length = field.children[0].children
            bit_offset += length
        else:
            break

def DefIndexField_hook_post(context, tree):
    # Update the fields with region & offset info
    index_register = context.lookup_symbol(tree.children[1].children)
    data_register = context.lookup_symbol(tree.children[2].children)
    flags = tree.children[3].children[0].children
    access_width = access_width_map[flags & 0xF]
    fields = tree.children[4].children
    bit_offset = 0
    for field in fields:
        field = field.children[0]
        if field.label == "NamedField":
            name = field.children[0].children
            length = field.children[1].children
            sym = context.lookup_symbol(name)
            assert isinstance(sym, OperationFieldDecl)
            sym.set_indexed_location(index_register, data_register, bit_offset, access_width)
            bit_offset += length
        elif field.label == "ReservedField":
            length = field.children[0].children
            bit_offset += length
        else:
            break

def NamedField_hook_post(context, tree):
    name = tree.children[0].children
    length = tree.children[1].children
    sym = OperationFieldDecl(name, length, tree)
    context.register_symbol(sym)

def DefMethod_hook_named(context, tree, name):
    context.change_scope(name)

def DefMethod_hook_post(context, tree):
    context.pop_scope()
    if len(tree.children) >= 3:
        name = tree.children[1].children
        flags = tree.children[2].children[0].children
        nargs = flags & 0x7
        sym = MethodDecl(name, nargs, tree)
        context.register_symbol(sym)

def DefOpRegion_hook_named(context, tree, name):
    sym = NamedDecl(name, tree)
    context.register_symbol(sym)

def DefPowerRes_hook_named(context, tree, name):
    sym = NamedDecl(name, tree)
    context.register_symbol(sym)

def DefThermalZone_hook_named(context, tree, name):
    sym = NamedDecl(name, tree)
    context.register_symbol(sym)

################################################################################
# Instantiate parsers
################################################################################

def register_hooks(factory, label):
    if f"{sym}_hook_pre" in globals().keys():
        factory.hook_pre = globals()[f"{sym}_hook_pre"]
    if f"{sym}_hook_named" in globals().keys():
        factory.hook_named = globals()[f"{sym}_hook_named"]
    if f"{sym}_hook_post" in globals().keys():
        factory.hook_post = globals()[f"{sym}_hook_post"]

for sym in dir(grammar):
    # Ignore builtin members and opcode constants
    if sym.startswith("__") or (sym.upper() == sym):
        continue

    definition = getattr(grammar, sym)
    if isinstance(definition, tuple):
        factory = SequenceFactory(sym, definition)
        register_hooks(factory, sym)
        globals()[sym] = factory
    elif isinstance(definition, list):
        factory = OptionFactory(sym, definition)
        register_hooks(factory, sym)
        globals()[sym] = factory
