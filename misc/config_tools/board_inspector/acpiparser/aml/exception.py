# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

class DecodeError(Exception):
    def __init__(self, opcode, label):
        super().__init__(f"{hex(opcode)} is not a known opcode for {label}")

class DeferLater(Exception):
    def __init__(self, label, seq):
        super().__init__(f"{label}: defer parsing of {seq}")

class ScopeMismatch(Exception):
    def __init__(self):
        super().__init__(f"scope mismatch")

class UndefinedSymbol(Exception):
    def __init__(self, name, context):
        super().__init__(f"{name} is not a defined symbol under {context}")

class InvalidPath(Exception):
    def __init__(self, name):
        super().__init__(f"{name} is not a valid ACPI namespace path")

class FutureWork(Exception):
    pass
