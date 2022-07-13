# Copyright (c) 2015-2022 Intel Corporation.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright notice,
#       this list of conditions and the following disclaimer in the documentation
#       and/or other materials provided with the distribution.
#     * Neither the name of Intel Corporation nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Helper functions to work with bitfields.

Documentation frequently writes bitfields as the inclusive range [msb:lsb];
this module provides functions to work with bitfields using msb and lsb rather
than manually computing shifts and masks from those."""

def bitfield_max(msb, lsb=None):
    """Return the largest value that fits in the bitfield [msb:lsb] (or [msb] if lsb is None)"""
    if lsb is None:
        lsb = msb
    return (1 << (msb - lsb + 1)) - 1

def bitmask(msb, lsb=None):
    """Creates a mask with bits [msb:lsb] (or [msb] if lsb is None) set."""
    if lsb is None:
        lsb = msb
    return bitfield_max(msb, lsb) << lsb

def bitfield(value, msb, lsb=None):
    """Shift value to fit in the bitfield [msb:lsb] (or [msb] if lsb is None).

    Raise OverflowError if value does not fit in that bitfield."""
    if lsb is None:
        lsb = msb
    if value > bitfield_max(msb, lsb):
        if msb == lsb:
            field = "[{0}]".format(msb)
        else:
            field = "[{0}:{1}]".format(msb, lsb)
        raise OverflowError("Value {value:#x} too big for bitfield {field}".format(**locals()))
    return value << lsb

def getbits(value, msb, lsb=None):
    """From the specified value, extract the bitfield [msb:lsb] (or [msb] if lsb is None)"""
    if lsb is None:
        lsb = msb
    return (value >> lsb) & bitfield_max(msb, lsb)

def setbits(value, fieldvalue, msb, lsb=None):
    """In the specified value, set the bitfield [msb:lsb] (or [msb] if lsb is None) to fieldvalue"""
    value &= ~bitmask(msb, lsb)
    value |= bitfield(fieldvalue, msb, lsb)
    return value
