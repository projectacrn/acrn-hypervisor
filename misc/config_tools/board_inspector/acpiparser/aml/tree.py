# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

from copy import copy

from . import grammar

class Tree:
    def __init__(self, label=None, children=[]):
        self.label = label
        self.children = copy(children)
        self.scope = None

        self.package_range = None

        self.deferred_range = None
        self.context_scope = None
        self.factory = None

    def append_child(self, child):
        self.children.append(child)

class Visitor:
    def __init__(self):
        self.depth = 0

    def __visit(self, tree):
        fn = getattr(self, tree.label, None)
        if not fn:
            fn = getattr(self, "default", None)
        if fn:
            fn(tree)

    def visit_topdown(self, tree):
        self.__visit(tree)
        if isinstance(tree.children, list):
            self.depth += 1
            for child in tree.children:
                if isinstance(child, Tree):
                    self.visit_topdown(child)
            self.depth -= 1

class Transformer:
    def __init__(self):
        self.depth = 0

    def __transform(self, tree):
        fn = getattr(self, tree.label, None)
        if not fn:
            fn = getattr(self, "default", None)
        if fn:
            return fn(tree)
        else:
            return tree

    def transform_topdown(self, tree):
        new_tree = self.__transform(tree)
        if isinstance(new_tree.children, list):
            self.depth += 1
            for i, child in enumerate(tree.children):
                if isinstance(child, Tree):
                    tree.children[i] = self.transform_topdown(child)
            self.depth -= 1
        return new_tree

    def transform_bottomup(self, tree):
        if isinstance(tree.children, list):
            self.depth += 1
            for i, child in enumerate(tree.children):
                if isinstance(child, Tree):
                    tree.children[i] = self.transform_bottomup(child)
            self.depth -= 1
        return self.__transform(tree)

single_operand_op = ["MethodInvocation"]
for sym in dir(grammar):
    # Ignore builtin members and opcode constants
    if sym.startswith("__") or (sym.upper() == sym):
        continue

    definition = getattr(grammar, sym)
    if isinstance(definition, tuple) and\
       isinstance(definition[0], int) and\
       len(definition) == 2:
        single_operand_op.append(sym)

class FlattenTransformer(Transformer):
    def default(self, tree):
        if tree.label not in single_operand_op and \
           isinstance(tree.children, list) and \
           len(tree.children) == 1 and \
           not tree.deferred_range:
            return tree.children[0]
        else:
            return tree

class Interpreter:
    def __init__(self, context):
        self.context = context

    def interpret(self, expr):
        assert isinstance(expr, Tree)
        fn = getattr(self, expr.label, None)
        if not fn:
            fn = getattr(self, "default", None)
        if fn:
            return fn(expr)
        else:
            raise NotImplementedError(f"don't know how to interpret a tree node with label {expr.label}")
