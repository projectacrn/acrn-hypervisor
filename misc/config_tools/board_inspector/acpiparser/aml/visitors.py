# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

from .tree import Visitor

class PrintLayoutVisitor(Visitor):
    @staticmethod
    def __is_printable(s):
        return all(ord(c) >= 0x20 and ord(c) < 0xFF for c in s)

    def default(self, tree):
        indent = "  " * self.depth
        print(f"{indent}{tree.label}", end="")
        if isinstance(tree.children, int):
            print(f" = {hex(tree.children)}", end="")
        elif isinstance(tree.children, str):
            if self.__is_printable(tree.children):
                print(f" = '{tree.children}'", end="")
        if tree.deferred_range:
            print(f" (deferred at {hex(tree.deferred_range[0])}, length {hex(tree.deferred_range[1])})", end="")
        if tree.factory:
            print(f" {tree.factory.label}: {tree.factory.seq}", end="")
        print()

class ConditionallyUnregisterSymbolVisitor(Visitor):
    def __init__(self, interpreter):
        super().__init__()
        self.context = interpreter.context
        self.interpreter = interpreter
        self.conditionally_hidden = False

        self.DefName = self.__unregister(0)
        self.DefMethod = self.__unregister(1)
        self.DefDevice = self.__unregister(1)

    def __unregister(self, name_string_idx):
        def f(tree):
            if self.conditionally_hidden:
                scope = tree.scope
                name = tree.children[name_string_idx].children
                realpath = self.context.realpath(scope, name)
                self.context.unregister_object(realpath)
        return f

    def visit_topdown(self, tree):
        if tree.label == "DefIfElse":
            self.context.change_scope(tree.scope)
            cond = self.interpreter.interpret(tree.children[1]).get()
            self.context.pop_scope()

            self.depth += 1
            if cond:
                self.visit_topdown(tree.children[2])
                if len(tree.children) == 4:
                    self.conditionally_hidden = True
                    self.visit_topdown(tree.children[3])
                    self.conditionally_hidden = False
            else:
                self.conditionally_hidden = True
                self.visit_topdown(tree.children[2])
                self.conditionally_hidden = False
                if len(tree.children) == 4:
                    self.visit_topdown(tree.children[3])
            self.depth -= 1
        elif tree.label not in ["DefMethod"]:
            super().visit_topdown(tree)
