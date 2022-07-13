# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

from enum import Enum
from copy import copy

from . import grammar

class Tree:
    def __init__(self, label=None, children=[]):
        self.label = label
        self.children = copy(children)
        self.scope = None

        self.structure = None

        self.package_range = None

        self.deferred_range = None
        self.context_scope = None
        self.factory = None

    def append_child(self, child):
        self.children.append(child)

    def register_structure(self, structure):
        self.structure = structure

    def complete_parsing(self):
        i = 0
        for elem in self.structure:
            if isinstance(elem, str):
                if elem.endswith("?"):
                    if i < len(self.children):
                        setattr(self, elem[:-1], self.children[i])
                    else:
                        setattr(self, elem[:-1], None)
                    break
                elif elem.endswith("*"):
                    setattr(self, elem[:-1] + "s", self.children[i:])
                    break
                else:
                    setattr(self, elem, self.children[i])
                    i += 1

class Direction(Enum):
    TOPDOWN = 1
    BOTTOMUP = 2
    CUSTOMIZED = 3

class Visitor:
    def __init__(self, direction):
        self.depth = 0
        if direction == Direction.TOPDOWN:
            self.visit = self._visit_topdown
        elif direction == Direction.BOTTOMUP:
            self.visit = self._visit_bottomup

    def __visit_node(self, tree):
        fn = getattr(self, tree.label, None)
        if not fn:
            fn = getattr(self, "default", None)
        if fn:
            return fn(tree)
        else:
            return True

    def _visit_topdown(self, tree):
        go_on = self.__visit_node(tree)
        if go_on != False:
            self.depth += 1
            for child in tree.children:
                if isinstance(child, Tree):
                    self.visit(child)
            self.depth -= 1

    def _visit_bottomup(self, tree):
        self.depth += 1
        for child in tree.children:
            if isinstance(child, Tree):
                self.visit(child)
        self.depth -= 1
        self.__visit_node(tree)

    def visit(self, tree):
        raise NotImplementedError

class Transformer:
    def __init__(self, direction):
        self.depth = 0
        if direction == Direction.TOPDOWN:
            self.transform = self._transform_topdown
        elif direction == Direction.BOTTOMUP:
            self.transform = self._transform_bottomup

    def __transform_node(self, tree):
        fn = getattr(self, tree.label, None)
        if not fn:
            fn = getattr(self, "default", None)
        if fn:
            return fn(tree)
        else:
            return tree

    def _transform_topdown(self, tree):
        new_tree = self.__transform_node(tree)
        self.depth += 1
        for i, child in enumerate(tree.children):
            if isinstance(child, Tree):
                tree.children[i] = self.transform(child)
        self.depth -= 1
        return new_tree

    def _transform_bottomup(self, tree):
        self.depth += 1
        for i, child in enumerate(tree.children):
            if isinstance(child, Tree):
                tree.children[i] = self.transform(child)
        self.depth -= 1
        return self.__transform_node(tree)

    def transform(self, tree):
        raise NotImplementedError

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
