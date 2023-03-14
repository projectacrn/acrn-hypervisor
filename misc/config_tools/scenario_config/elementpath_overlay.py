#!/usr/bin/env python3
#
# Copyright (C), 2022 Intel Corporation.
# Copyright (c), 2018-2021, SISSA (International School for Advanced Studies).
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
from decimal import Decimal
from copy import copy
import operator
import elementpath

# Allow this script to find the library module at misc/config_tools/library.
#
# TODO: Reshuffle the module structure of the configuration toolset for clearer imports.
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import library.rdt as rdt

BaseParser = elementpath.XPath2Parser

class CustomParser(BaseParser):
    if hasattr(BaseParser, "SYMBOLS"):
        SYMBOLS = BaseParser.SYMBOLS | {
            # Bit-wise operations
            'bitwise-and',

            'bits-of',
            'has',
            'duplicate-values',

            'number-of-clos-id-needed',
            }

method = CustomParser.method
function = CustomParser.function

###
# Custom functions

OPERATORS_MAP = {
    'bitwise-and': operator.and_
}

def hex_to_int(value):
    if hasattr(value, 'text'):
        value = value.text
    if isinstance(value, int):
        return value
    elif isinstance(value, (float, Decimal)):
        return int(value)
    elif isinstance(value, str) and value.startswith("0x"):
        return int(value, base=16)
    else:
        raise TypeError('invalid type {!r} for integer'.format(type(value)))

@method(function('bitwise-and', nargs=2))
def evaluate(self, context=None):
    def aux(op):
        op1 = self.get_argument(context, 0)
        op2 = self.get_argument(context, 1)

        try:
            return op(hex_to_int(op1), hex_to_int(op2))
        except ValueError as err:
            raise self.error('FORG0001', err) from None
        except TypeError as err:
            raise self.error('XPTY0004', err)

    return aux(OPERATORS_MAP[self.symbol])

@method(function('bits-of', nargs=1))
def evaluate_bits_of(self, context=None):
    op = self.get_argument(context)

    try:
        value = hex_to_int(op)
        for idx, bit in enumerate(reversed(bin(value)[2:])):
            if bit == '1':
                yield idx
    except TypeError as err:
        raise self.error('XPTY0004', err)

@method(function('has', nargs=2))
def evaluate_has_function(self, context=None):
    arg2 = self.get_argument(context, index=1, cls=str)
    for item in self[0].select(context):
        value = self.data_value(item)
        if value == arg2:
            return True
    return False

@method(function('duplicate-values', nargs=1))
def select_duplicate_values_function(self, context=None):
    def duplicate_values():
        results = []
        reported = []
        for item in self[0].select(context):
            value = self.data_value(item)
            if context is not None:
                context.item = value

            if value in results:
                if value not in reported:
                    yield value
                    reported.append(value)
            else:
                results.append(value)

    yield from duplicate_values()

@method(function('number-of-clos-id-needed', nargs=1))
def evaluate_number_of_clos_id_needed(self, context=None):
    op = self.get_argument(context, index=0)
    if op is not None:
        if isinstance(op, elementpath.TypedElement):
            op = op.elem

        # This function may be invoked when the xmlschema library parses the data check schemas, in which case `op` will
        # be an object of class Xsd11Element. Only attempt to calculate the needed CLOS IDs when a real acrn-config node
        # is given.
        if hasattr(op, "xpath"):
            return len(rdt.get_policy_list(op))

    return 0

###
# Collection of counter examples

class Hashable:
    def __init__(self, obj):
        self.obj = obj

    def __hash__(self):
        return id(self.obj)

def copy_context(context):
    ret = copy(context)
    if hasattr(context, 'counter_example'):
        ret.counter_example = dict()
    return ret

def add_counter_example(context, private_context, kvlist):
    if hasattr(context, 'counter_example'):
        context.counter_example.update(kvlist)
        if private_context:
            context.counter_example.update(private_context.counter_example)

@method('every')
@method('some')
def evaluate(self, context=None):
    if context is None:
        raise self.missing_context()

    some = self.symbol == 'some'
    varrefs = [Hashable(self[k]) for k in range(0, len(self) - 1, 2)]
    varnames = [self[k][0].value for k in range(0, len(self) - 1, 2)]
    selectors = [self[k].select for k in range(1, len(self) - 1, 2)]

    for results in copy(context).iter_product(selectors, varnames):
        private_context = copy_context(context)
        private_context.variables.update(x for x in zip(varnames, results))
        if self.boolean_value([x for x in self[-1].select(private_context)]):
            if some:
                add_counter_example(context, private_context, zip(varrefs, results))
                return True
        elif not some:
            add_counter_example(context, private_context, zip(varrefs, results))
            return False

    return not some

elementpath.XPath2Parser = CustomParser
