# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

from . import grammar
from . import datatypes
from .tree import Tree

### Basic data types

def __build_value(label, value):
    tree = Tree(label, [value])
    tree.register_structure(("value",))
    tree.complete_parsing()
    return tree

def __build_string(label, s):
    assert isinstance(s, str)
    return __build_value(label, s)

def __build_const_data(label, data):
    assert isinstance(data, int)
    return __build_value(label, data)

NameSeg = lambda x: __build_string("NameSeg", x)
NameString = lambda x: __build_string("NameString", x)
String = lambda x: __build_string("String", x)

def ByteList(data):
    assert isinstance(data, (bytes, bytearray))
    return __build_value("ByteList", data)

ByteData = lambda x: __build_const_data("ByteData", x)
WordData = lambda x: __build_const_data("WordData", x)
DWordData = lambda x: __build_const_data("DWordData", x)
TWordData = lambda x: __build_const_data("TWordData", x)
QWordData = lambda x: __build_const_data("QWordData", x)

def PkgLength(length=0):
    return __build_const_data("PkgLength", length)

FieldLength = lambda x: __build_const_data("FieldLength", x)

### Sequences

def MethodInvocation(name, *args):
    if isinstance(name, str):
        name_tree = NameString(name)
    else:
        name_tree = name
    tree = Tree("MethodInvocation", [name])
    for arg in args:
        assert isinstance(arg, Tree)
        tree.append_child(arg)
    tree.register_structure(("NameString", "TermArg*"))
    tree.complete_parsing()
    return tree

def __create_sequence_builder(label):
    def aux(tree, elem, arg):
        if isinstance(arg, Tree):
            # TODO: validate the given arg
            tree.append_child(arg)
        else:
            tree.append_child(globals()[elem](arg))

    seq = grammar.get_definition(label)
    structure = grammar.get_names(label)

    def fn(*args):
        tree = Tree(label)
        it = iter(args)
        for elem in seq:
            if isinstance(elem, int):    # The leading opcode
                continue
            elif elem.endswith("*"):
                for arg in it:
                    aux(tree, elem, arg)
            elif elem.endswith("?"):
                try:
                    aux(tree, elem, next(it))
                except StopIteration:
                    pass
            else:
                aux(tree, elem, next(it))
        tree.register_structure(structure)
        tree.complete_parsing()
        return tree
    return fn

def build_value(value):
    if isinstance(value, (int, datatypes.Integer)):
        if isinstance(value, int):
            value = datatypes.Integer(value)
        v = value.get()
        return \
            ZeroOp() if v == 0 else \
            OneOp() if v == 1 else \
            ByteConst(v) if v <= 0xff else \
            WordConst(v) if v <= 0xffff else \
            DWordConst(v) if v <= 0xffffffff else \
            QWordConst(v)
    elif isinstance(value, datatypes.Buffer):
        buffer_size = len(value.get())
        builder = ByteConst if buffer_size <= 0xff else \
                  WordConst if buffer_size <= 0xffff else \
                  DWordConst if buffer_size <= 0xffffffff else \
                  QWordConst
        return DefBuffer(
            PkgLength(),
            builder(buffer_size),
            ByteList(value.get()))
    elif isinstance(value, datatypes.Package):
        elements = list(map(build_value, value.elements))
        return DefPackage(
            PkgLength(),
            len(value.elements),
            PackageElementList(*elements))
    elif isinstance(value, (str, datatypes.String)):
        if isinstance(value, str):
            return String(value)
        else:
            return String(value.get())
    elif isinstance(value, datatypes.BufferField):
        return build_value(value.to_integer())
    else:
        return None

for sym in dir(grammar):
    # Ignore builtin members and opcode constants
    if sym.startswith("__") or (sym.upper() == sym):
        continue

    definition = getattr(grammar, sym)
    if isinstance(definition, tuple):
        globals()[sym] = __create_sequence_builder(sym)
    elif isinstance(definition, list) and len(definition) == 1:
        if definition[0] in globals().keys():
            globals()[sym] = globals()[definition[0]]
