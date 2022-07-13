# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import logging
from copy import copy
from math import floor

from .exception import *
from .stream import Stream

class NamedDecl:
    @staticmethod
    def object_type():
        return 0

    def __init__(self, name, tree):
        self.tree = tree
        self.name = name

    def dump(self):
        print(f"{self.name}: {self.__class__.__name__}")

class FieldDecl(NamedDecl):
    def __init__(self, name, length, tree):
        super().__init__(name, tree)
        self.length = length

    def dump(self):
        print(f"{self.name}: {self.__class__.__name__}, {self.length} bits")

class OperationRegionDecl(NamedDecl):
    @staticmethod
    def object_type():
        return 10

    def __init__(self, name, tree):
        super().__init__(name, tree)

class OperationFieldDecl(NamedDecl):
    def __init__(self, name, length, tree):
        super().__init__(name, tree)
        self.region = None
        self.offset = None
        self.length = length
        self.access_width = None
        self.parent_tree = None

    def set_location(self, region, offset, access_width):
        self.region = region
        self.offset = offset
        self.access_width = access_width

    def set_indexed_location(self, index_register, data_register, index, access_width):
        self.region = (index_register, data_register)
        self.offset = index
        self.access_width = access_width

    def dump(self):
        if self.region and self.offset:
            bit_index = self.offset
            byte_index = floor(bit_index / 8)
            offset_in_byte = bit_index % 8
            if isinstance(self.region, str):
                print(f"{self.name}: {self.__class__.__name__}, {self.region}: bit {hex(bit_index)} (byte {hex(byte_index)}.{offset_in_byte}), {self.length} bits")
            else:
                print(f"{self.name}: {self.__class__.__name__}, ({self.region[0]}, {self.region[1]}): bit {hex(bit_index)} (byte {hex(byte_index)}.{offset_in_byte}), {self.length} bits")
        else:
            print(f"{self.name}: {self.__class__.__name__}, {self.length} bits")

class AliasDecl(NamedDecl):
    def __init__(self, name, source, tree):
        super().__init__(name, tree)
        self.name = name
        self.source = source

    def dump(self):
        print(f"{self.name}: {self.__class__.__name__}, aliasing {self.source}")

class MethodDecl(NamedDecl):
    @staticmethod
    def object_type():
        return 8

    def __init__(self, name, nargs, tree):
        super().__init__(name, tree)
        self.nargs = nargs

    def dump(self):
        print(f"{self.name}: {self.__class__.__name__}, {self.nargs} args")

class PredefinedMethodDecl(NamedDecl):
    @staticmethod
    def object_type():
        return 8

    def __init__(self, name, nargs, fn):
        super().__init__(name, None)
        self.nargs = nargs
        self.fn = fn

    def dump(self):
        print(f"{self.name}: {self.__class__.__name__}, {self.nargs} args")

class DeviceDecl(NamedDecl):
    @staticmethod
    def object_type():
        return 6

    def __init__(self, name, tree):
        super().__init__(name, tree)

def predefined_osi(args):
    feature = args[0].get()
    if feature.startswith("Linux"):
        return 0xffffffff
    elif feature.startswith("Windows") or \
         feature.startswith("FreeBSD") or \
         feature.startswith("HP-UX") or \
         feature.startswith("OpenVMS"):
        return 0
    return 0xffffffff

class Context:
    @staticmethod
    def realpath(scope, name):
        if name and name.startswith("\\"):
            return name

        if name and name.startswith("^"):
            parent_count = name.count("^")
            assert parent_count <= len(scope)
            scope = scope[:-parent_count]
            name = name[parent_count:]

        if scope:
            if isinstance(scope, list):
                if name:
                    return f"\\{'.'.join(scope)}.{name}"
                else:
                    return f"\\{'.'.join(scope)}"
            elif isinstance(scope, str):
                if scope == "\\":
                    return f"\\{name}"
                else:
                    return f"{scope}.{name}"
            else:
                raise NotImplementedError
        else:
            if name:
                return f"\\{name}"
            else:
                return f"\\"

    @staticmethod
    def parent(scope):
        if scope == "\\":
            return "\\"
        else:
            parent = scope[:-4]
            if parent.endswith("."):
                return parent[:-1]
            else:
                return parent

    @staticmethod
    def normalize_namepath(namepath):
        path = namepath.lstrip("\\^")
        prefix = namepath[:(len(namepath) - len(path))]
        parts = '.'.join(map(lambda x: x[:4].ljust(4, '_'), path.split(".")))
        return prefix + parts

    def __init__(self):
        self.streams = {}
        self.current_stream = None
        self.trees = {}

        # Loaded namespace
        self.__symbol_table = {}
        self.__devices = []

        # Context during parsing
        self.__current_scope = []
        self.__scope_history = []
        self.__deferred_mode_depth = 0

        # Context during interpretation
        self.__binding_table = {}
        self.__op_regions = {}

        # Register predefined objects per section 5.7, ACPI 6.4
        self.register_symbol(NamedDecl("_GL_", None))
        self.register_symbol(PredefinedMethodDecl("_OSI", 1, predefined_osi))
        self.register_symbol(NamedDecl("_OS_", None))
        self.register_symbol(NamedDecl("_REV", None))
        self.register_symbol(NamedDecl("_DLM", None))

        # Mode switches
        self.__skip_external_on_lookup = False

    def switch_stream(self, val):
        if isinstance(val, str):
            if not val in self.streams.keys():
                with open(val, "rb") as f:
                    stream = Stream(f.read())
                    self.streams[val] = stream
                    self.current_stream = stream
            else:
                self.current_stream = self.streams[val]
                self.current_stream.reset()
        elif isinstance(val, (bytes, bytearray)):
            self.current_stream = Stream(val)
        else:
            raise NotImplementedError(f"Cannot use {val} as a stream.")

    def get_scope(self):
        return self.realpath(self.__current_scope, "")

    def change_scope(self, new_scope):
        self.__scope_history.append(copy(self.__current_scope))
        if isinstance(new_scope, list):
            self.__current_scope = new_scope
        elif isinstance(new_scope, str):
            if new_scope.startswith("\\"):
                self.__current_scope = [i for i in new_scope[1:].split(".") if i]
            elif new_scope.startswith("^"):
                parent_count = new_scope.count("^")
                assert parent_count <= len(self.__current_scope)
                self.__current_scope = self.__current_scope[:-parent_count].extend(new_scope.split("."))
            else:
                self.__current_scope.extend(new_scope.split("."))
        else:
            raise InvalidPath(new_scope)

    def pop_scope(self):
        assert(self.__scope_history)
        self.__current_scope = self.__scope_history.pop()

    def __register_symbol(self, symbol):
        self.__symbol_table[symbol.name] = symbol
        if isinstance(symbol, DeviceDecl):
            self.__devices.append(symbol)

    def register_symbol(self, symbol):
        symbol.name = self.realpath(self.__current_scope, symbol.name)
        if symbol.name in self.__symbol_table.keys():
            old_tree = self.__symbol_table[symbol.name].tree
            new_tree = symbol.tree
            if old_tree.label != new_tree.label:
                if old_tree.label == "DefExternal":
                    self.__register_symbol(symbol)
                elif new_tree.label == "DefExternal":
                    pass
                else:
                    logging.debug(f"{symbol.name} is redefined as {new_tree.label} (previously was {old_tree.label})")
                    self.__register_symbol(symbol)
        else:
            self.__register_symbol(symbol)

    def unregister_object(self, realpath):
        sym = self.__symbol_table.pop(realpath, None)
        if isinstance(sym, DeviceDecl):
            self.__devices.remove(sym)

    def __lookup_symbol_in_parents(self, table, name):
        prefix_len = len(self.__current_scope)
        while prefix_len >= 0:
            path = self.realpath(self.__current_scope[:prefix_len], name)
            if path in table:
                sym = table[path]
                # External object declarations are only for parsing. At
                # interpretation time such declarations should not be looked up.
                if (not self.__skip_external_on_lookup) or \
                   isinstance(sym, PredefinedMethodDecl) or \
                   (sym.tree and sym.tree.label != "DefExternal"):
                    return sym
            prefix_len -= 1
        raise KeyError(name)

    def lookup_symbol(self, name, scope=None):
        if scope:
            self.change_scope(scope)
        try:
            if name.startswith("\\"):
                ret = self.__symbol_table[name]
            elif name.startswith("^") or name.find(".") >= 0:
                realpath = self.realpath(self.__current_scope, name)
                ret = self.__symbol_table[realpath]
            else:
                ret = self.__lookup_symbol_in_parents(self.__symbol_table, name)
        except KeyError:
            ret = None

        if scope:
            self.pop_scope()
        if not ret:
            raise UndefinedSymbol(name, scope if scope else self.get_scope())
        return ret

    def has_symbol(self, name):
        try:
            self.lookup_symbol(name)
            return True
        except UndefinedSymbol:
            return False

    def lookup_symbol_by_tree(self, tree):
        result = filter(lambda x: x[1].tree is tree, self.__symbol_table.items())
        try:
            return next(result)[1]
        except StopIteration:
            return None

    def get_fresh_name(self):
        current_scope = self.get_scope()
        for i in range(0, 10):
            name = self.realpath(current_scope, f"_T_{i}")
            if not self.lookup_symbol(name):
                return name
        raise NotImplementedError("Cannot find a proper fresh name")

    @property
    def devices(self):
        return self.__devices

    def dump_symbols(self):
        for k,v in sorted(self.__symbol_table.items()):
            v.dump()

    def enter_deferred_mode(self):
        self.__deferred_mode_depth += 1

    def exit_deferred_mode(self):
        assert self.__deferred_mode_depth > 0
        self.__deferred_mode_depth -= 1

    def in_deferred_mode(self):
        return (self.__deferred_mode_depth > 0)

    def skip_external_on_lookup(self):
        self.__skip_external_on_lookup = True

    def register_operation_region(self, name, op_region):
        self.__op_regions[name] = op_region

    def lookup_operation_region(self, name):
        try:
            if name.startswith("\\"):
                return self.__op_regions[name]
            elif name.startswith("^") or name.find(".") >= 0:
                realpath = self.realpath(self.__current_scope, name)
                return self.__op_regions[realpath]
            else:
                return self.__lookup_symbol_in_parents(self.__op_regions, name)
        except KeyError:
            return None

    def register_binding(self, name, value):
        sym = self.lookup_symbol(name)
        logging.debug(f"Bind {sym.name} to {value}")
        self.__binding_table[sym.name] = value

    def lookup_binding(self, name):
        sym = self.lookup_symbol(name)
        try:
            return self.__binding_table[sym.name]
        except KeyError:
            return None

    def dump_bindings(self):
        for k in sorted(self.__binding_table.keys()):
            v = self.__binding_table[k]
            if v:
                try:
                    val = v.get()
                    if isinstance(val, int):
                        val = hex(val)
                    print(k, val)
                except NotImplementedError:
                    print(k, f"({v.__class__.__name__})")
                except AttributeError:
                    print(k, f"(wrong type: {v})")
            else:
                print(k, "(None)")
