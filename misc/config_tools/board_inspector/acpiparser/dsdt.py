# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import logging

from acpiparser.aml.stream import Stream
from acpiparser.aml.parser import AMLCode, DeferredExpansion
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
            AMLCode.parse(context, tree)
            tree = DeferredExpansion(context).transform(tree)
            context.trees[os.path.basename(t)] = tree
    except Exception as e:
        context.current_stream.dump()
        raise

    context.skip_external_on_lookup()
    visitor = ConditionallyUnregisterSymbolVisitor(ConcreteInterpreter(context))
    for tree in context.trees.values():
        visitor.visit(tree)
    return context
