# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import logging

from . import grammar
from .context import *
from .exception import *
from .tree import Tree, Transformer, Direction

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
        self.level += 1

    def mark_end(self):
        self.level -= 1

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

    @property
    def decoder(self):
        raise NotImplementedError

################################################################################
# 20.2.2 Name Objects Encoding
################################################################################

class NameSegFactory(Factory):
    def __init__(self):
        super().__init__()
        self.__decoder = {}
        for i in range(ord('A'), ord('Z') + 1):
            self.__decoder[i] = self
        self.__decoder[ord('_')] = self
        self.label = "NameSeg"

    def match(self, context, stream, tree):
        tree.register_structure(("value",))
        tree.append_child(stream.get_fixed_length_string(4))
        tree.complete_parsing()

    @property
    def decoder(self):
        return self.__decoder

NameSeg = NameSegFactory()

class NameStringFactory(Factory):
    def __init__(self):
        super().__init__()
        self.label = "NameString"
        self.__decoder = {}
        for i in range(ord('A'), ord('Z') + 1):
            self.__decoder[i] = self
        for i in [ord('_'), ord('\\'), ord('^'), grammar.AML_DUAL_NAME_PREFIX, grammar.AML_MULTI_NAME_PREFIX]:
            self.__decoder[i] = self

    def match(self, context, stream, tree):
        tree.register_structure(("value",))
        acc = ""

        # Namespace prefixes
        char = stream.get_char()
        while char in ["\\", "^"]:
            acc += char
            char = stream.get_char()

        # Object name
        if ord(char) == grammar.AML_DUAL_NAME_PREFIX:
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

        tree.append_child(acc)
        tree.complete_parsing()

    @property
    def decoder(self):
        return self.__decoder

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
        tree.register_structure(("value",))
        tree.append_child(stream.get_integer(self.width))
        tree.complete_parsing()
        return tree

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

        tree.register_structure(("value",))
        tree.append_child(stream.get_string())
        tree.complete_parsing()
        return tree

    @property
    def decoder(self):
        return {grammar.AML_STRING_PREFIX: self}

String = StringFactory()

class ByteListFactory(Factory):
    def __init__(self):
        super().__init__()
        self.label = "ByteList"

    def match(self, context, stream, tree):
        tree.register_structure(("value",))
        tree.append_child(stream.get_buffer())
        tree.complete_parsing()
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

        tree.register_structure(("value",))
        tree.append_child(self.get_package_length(byte_count, stream.get_integer(byte_count + 1)))
        tree.complete_parsing()

        if self.create_new_scope:
            remaining = tree.value - byte_count - 1
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
        self.__decoder = None
        self.label = "MethodInvocation"

    def match(self, context, stream, tree):
        tree.register_structure(("NameString", "TermArg*"))

        child_namestring = Tree()
        globals()["NameString"].parse(context, child_namestring)
        tree.append_child(child_namestring)

        sym = context.lookup_symbol(child_namestring.value)
        if isinstance(sym, (MethodDecl, PredefinedMethodDecl)):
            for i in range(0, sym.nargs):
                child_arg = Tree()
                globals()["TermArg"].parse(context, child_arg)
                tree.append_child(child_arg)

        tree.complete_parsing()
        return tree

    @property
    def decoder(self):
        if not self.__decoder:
            self.__decoder = {}
            for k in globals()["NameString"].decoder.keys():
                self.__decoder[k] = self
        return self.__decoder

MethodInvocation = MethodInvocationFactory()

################################################################################
# Infrastructure Factories
################################################################################

class SequenceFactory(Factory):
    def __init__(self, label, seq):
        super().__init__()
        self.label = label
        # Some objects in ACPI AML have multiple occurrences of the same type of object in the grammar. In order to
        # refer to these different occurrences, the grammar module uses the following notation to give names to each of
        # them:
        #
        #     "<object type>:<alias name>"
        #
        # The grammar module provides the get_definition() and get_names() methods to get the specification solely in
        # object types or alias names, respectively. For objects without aliases, the type is reused as the name.
        try:
            self.seq = grammar.get_definition(label)
            self.structure = grammar.get_names(label)
        except KeyError:
            self.seq = seq
            self.structure = seq
        self.__decoder = None

    def match(self, context, stream, tree):
        tree.register_structure(self.structure)

        # When a TermList is empty, the stream has already come to the end of the current scope here. Do not attempt to
        # peek the next opcode in such cases.
        if stream.at_end() and \
           (self.seq[0][-1] in ["*", "?"]):
            stream.pop_scope()
            tree.complete_parsing()
            return tree

        package_end = 0

        # Under any case this function shall maintain the balance of stream scopes. The following flags indicate the
        # cleanup actions upon exceptions.
        to_recover_from_deferred_mode = False
        to_pop_stream_scope = False
        completed = True

        for i,elem in enumerate(self.seq):
            pos = stream.current
            try:
                if isinstance(elem, int):  # The leading opcode
                    opcode, _ = stream.get_opcode()
                    assert elem == opcode
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
                        if sub_opcode in factory.decoder.keys():
                            child = Tree()
                            factory.parse(context, child)
                            tree.append_child(child)
                else:
                    # It is likely that a method body has forward definitions, while typically it does not define
                    # symbols that are referred later. Thus always defer the parsing of method bodies to the second
                    # phase.
                    #
                    # In second phase the labels of sequence factories always have the ".deferred" suffix. Thus it is
                    # safe to check self.label against "DefMethod" here.
                    if elem == "TermList" and self.label == "DefMethod":
                        raise DeferLater(self.label, [elem])
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
                        self.hook_named(context, tree, child.value)
            except (DecodeError, DeferLater, ScopeMismatch, UndefinedSymbol) as e:
                if to_pop_stream_scope:
                    stream.pop_scope(force=True)
                    if to_recover_from_deferred_mode:
                        tree.deferred_range = (pos, package_end - pos)
                        tree.context_scope = context.get_scope()
                        tree.factory = SequenceFactory(f"{self.label}.deferred", self.seq[i:])
                        stream.seek(package_end, absolute=True)
                        completed = False
                        break
                else:
                    raise e

        if completed:
            tree.complete_parsing()

        if to_recover_from_deferred_mode:
            context.exit_deferred_mode()
        return tree

    @property
    def decoder(self):
        if not self.__decoder:
            if isinstance(self.seq[0], int):
                self.__decoder = {self.seq[0]: self}
            else:
                self.__decoder = {}
                for k in globals()[self.seq[0]].decoder.keys():
                    self.__decoder[k] = self
        return self.__decoder

class OptionFactory(Factory):
    def __init__(self, label, opts):
        super().__init__()
        self.label = label
        self.opts = opts
        self.__decoder = None

    def match(self, context, stream, tree):
        opcode, _ = stream.peek_opcode()
        try:
            if len(self.opts) == 1:
                globals()[self.opts[0]].parse(context, tree)
            else:
                self.decoder[opcode].parse(context, tree)
            return tree
        except KeyError:
            raise DecodeError(opcode, self.label)

    @property
    def decoder(self):
        if not self.__decoder:
            self.__decoder = {}
            for opt in self.opts:
                self.__decoder.update(globals()[opt].decoder)
        return self.__decoder

class DeferredExpansion(Transformer):
    def __init__(self, context):
        super().__init__(Direction.TOPDOWN)
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
                tree.complete_parsing()
            except (DecodeError, DeferLater, ScopeMismatch, UndefinedSymbol) as e:
                logging.debug(f"expansion of {tree.label} at {hex(tree.deferred_range[0])} failed due to: " + str(e))

            self.context.pop_scope()

        return tree

################################################################################
# Hook functions
################################################################################

def DefAlias_hook_post(context, tree):
    source = tree.SourceObject.value
    alias = tree.AliasObject.value
    sym = AliasDecl(alias, source, tree)
    context.register_symbol(sym)

def DefName_hook_named(context, tree, name):
    sym = NamedDecl(name, tree)
    context.register_symbol(sym)

def DefScope_hook_named(context, tree, name):
    context.change_scope(name)

def DefScope_hook_post(context, tree):
    context.pop_scope()


def DefCreateBitField_hook_named(context, tree, name):
    name = tree.children[2].value
    sym = FieldDecl(name, 1, tree)
    context.register_symbol(sym)

def DefCreateByteField_hook_named(context, tree, name):
    name = tree.children[2].value
    sym = FieldDecl(name, 8, tree)
    context.register_symbol(sym)

def DefCreateDWordField_hook_named(context, tree, name):
    name = tree.children[2].value
    sym = FieldDecl(name, 32, tree)
    context.register_symbol(sym)

def DefCreateField_hook_named(context, tree, name):
    name = tree.children[3].value
    sym = FieldDecl(name, 0, tree)
    context.register_symbol(sym)

def DefCreateQWordField_hook_named(context, tree, name):
    name = tree.children[2].value
    sym = FieldDecl(name, 64, tree)
    context.register_symbol(sym)

def DefCreateWordField_hook_named(context, tree, name):
    name = tree.children[2].value
    sym = FieldDecl(name, 16, tree)
    context.register_symbol(sym)

def DefDevice_hook_named(context, tree, name):
    sym = DeviceDecl(name, tree)
    context.register_symbol(sym)
    context.change_scope(name)

def DefDevice_hook_post(context, tree):
    context.pop_scope()

def DefExternal_hook_post(context, tree):
    name = tree.NameString.value
    ty = tree.ObjectType.value
    nargs = tree.ArgumentCount.value

    if ty == MethodDecl.object_type():
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
    region_name = context.lookup_symbol(tree.NameString.value).name
    flags = tree.FieldFlags.value
    access_width = access_width_map[flags & 0xF]
    fields = tree.FieldList.FieldElements
    bit_offset = 0
    for field in fields:
        if field.label == "NamedField":
            name = field.NameSeg.value
            length = field.FieldLength.value
            sym = context.lookup_symbol(name)
            assert isinstance(sym, OperationFieldDecl)
            sym.set_location(region_name, bit_offset, access_width)
            sym.parent_tree = tree
            bit_offset += length
        elif field.label == "ReservedField":
            length = field.FieldLength.value
            bit_offset += length
        else:
            break

def DefIndexField_hook_post(context, tree):
    # Update the fields with region & offset info
    index_register = context.lookup_symbol(tree.IndexName.value)
    data_register = context.lookup_symbol(tree.DataName.value)
    flags = tree.FieldFlags.value
    access_width = access_width_map[flags & 0xF]
    fields = tree.FieldList.FieldElements
    bit_offset = 0
    for field in fields:
        if field.label == "NamedField":
            name = field.NameSeg.value
            length = field.FieldLength.value
            sym = context.lookup_symbol(name)
            assert isinstance(sym, OperationFieldDecl)
            sym.set_indexed_location(index_register, data_register, bit_offset, access_width)
            bit_offset += length
        elif field.label == "ReservedField":
            length = field.FieldLength.value
            bit_offset += length
        else:
            break

def NamedField_hook_post(context, tree):
    name = tree.NameSeg.value
    length = tree.FieldLength.value
    sym = OperationFieldDecl(name, length, tree)
    context.register_symbol(sym)

def DefMethod_hook_named(context, tree, name):
    context.change_scope(name)

def DefMethod_hook_post(context, tree):
    context.pop_scope()
    if len(tree.children) >= 3:
        # Parsing of the method may be deferred. Do not use named fields to access its children.
        name = tree.children[1].value
        flags = tree.children[2].value
        nargs = flags & 0x7
        sym = MethodDecl(name, nargs, tree)
        context.register_symbol(sym)

def DefOpRegion_hook_named(context, tree, name):
    sym = OperationRegionDecl(name, tree)
    context.register_symbol(sym)

def DefPowerRes_hook_named(context, tree, name):
    sym = NamedDecl(name, tree)
    context.register_symbol(sym)
    context.change_scope(name)

def DefPowerRes_hook_post(context, tree):
    context.pop_scope()

def DefThermalZone_hook_named(context, tree, name):
    sym = NamedDecl(name, tree)
    context.register_symbol(sym)
    context.change_scope(name)

def DefThermalZone_hook_post(context, tree):
    context.pop_scope()

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
