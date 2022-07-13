# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import logging

from acpiparser.aml.stream import Stream
from acpiparser.aml import parser
from acpiparser.aml.tree import Tree
from acpiparser.aml.context import Context
from acpiparser.aml.interpreter import ConcreteInterpreter
from acpiparser.aml.visitors import ConditionallyUnregisterSymbolVisitor

def DSDT(val):
    table_dir = os.path.dirname(val)
    if not table_dir:
        table_dir = "."
    ssdt = filter(lambda x: x.startswith("SSDT"), os.listdir(table_dir))
    tables = [val] + list(map(lambda x: os.path.join(table_dir, x), ssdt))

    context = Context()
    try:
        for t in tables:
            logging.info(f"Loading {t}")
            context.switch_stream(t)
            tree = Tree()
            parser.AMLCode.parse(context, tree)
            tree = parser.DeferredExpansion(context).transform(tree)
            context.trees[os.path.basename(t)] = tree
    except Exception as e:
        context.current_stream.dump()
        raise

    context.skip_external_on_lookup()
    visitor = ConditionallyUnregisterSymbolVisitor(ConcreteInterpreter(context))
    for tree in context.trees.values():
        visitor.visit(tree)
    return context

def parse_tree(label, data):
    context = Context()
    context.switch_stream(data)
    tree = Tree()
    getattr(parser, label).parse(context, tree)
    tree = parser.DeferredExpansion(context).transform(tree)
    return tree
