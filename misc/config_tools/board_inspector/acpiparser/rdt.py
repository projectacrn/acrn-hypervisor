# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import ctypes

import lib.cdata as cdata
import lib.unpack as unpack

# 6.4.2 Small Resource Data Type

class SmallResourceDataTag(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('length', ctypes.c_uint8, 3),
        ('name', ctypes.c_uint8, 4),
        ('type', ctypes.c_uint8, 1),
    ]

SMALL_RESOURCE_ITEM_IRQ_FORMAT                    = 0x04
SMALL_RESOURCE_ITEM_DMA_FORMAT                    = 0x05
SMALL_RESOURCE_ITEM_START_DEPENDENT_FUNCTIONS     = 0x06
SMALL_RESOURCE_ITEM_END_DEPENDENT_FUNCTIONS       = 0x07
SMALL_RESOURCE_ITEM_IO_PORT                       = 0x08
SMALL_RESOURCE_ITEM_FIXED_LOCATION_IO_PORT        = 0x09
SMALL_RESOURCE_ITEM_FIXED_DMA                     = 0x0A
SMALL_RESOURCE_ITEM_VENDOR_DEFINED                = 0x0E
SMALL_RESOURCE_ITEM_END_TAG                       = 0x0F

# 6.4.2.1 IRQ Descriptor

def SmallResourceItemIRQ_factory(_len):
    class SmallResourceItemIRQ(cdata.Struct):
        _pack_ = 1
        _fields_ = SmallResourceDataTag._fields_ + [
            ('_INT', ctypes.c_uint16),
        ] + ([
            ('_HE', ctypes.c_uint8, 1),
            ('ingored', ctypes.c_uint8, 2),
            ('_LL', ctypes.c_uint8, 1),
            ('_SHR', ctypes.c_uint8, 1),
            ('_WKC', ctypes.c_uint8, 1),
            ('reserved', ctypes.c_uint8, 2),
        ] if (_len > 2) else [])

        @property
        def irqs(self):
            return [i for i in range(0, 16) if ((self._INT & (1 << i)) != 0)]
    return SmallResourceItemIRQ

# 6.4.2.2 DMA Descriptor

class SmallResourceItemDMA(cdata.Struct):
    _pack_ = 1
    _fields_ = SmallResourceDataTag._fields_ + [
        ('_DMA', ctypes.c_uint8),
        ('_SIZ', ctypes.c_uint8, 2),
        ('_BM', ctypes.c_uint8, 1),
        ('ignored', ctypes.c_uint8, 2),
        ('_TYP', ctypes.c_uint8, 2),
        ('reserved', ctypes.c_uint8, 1),
    ]

# 6.4.2.3 Start Dependent Functions Descriptor

def SmallResourceItemStartDependentFunctions_factory(_len):
    class SmallResourceItemStartDependentFunctions(cdata.Struct):
        _pack_ = 1
        _fields_ = SmallResourceDataTag._fields_ + ([
            ('compatibility', ctypes.c_uint8, 2),
            ('performance', ctypes.c_uint8, 2),
            ('reserved', ctypes.c_uint8, 4),
        ] if (_len > 0) else [])
    return SmallResourceItemStartDependentFunctions

# 6.4.2.4 End Dependent Functions Descriptor

class SmallResourceItemEndDependentFunctions(cdata.Struct):
    _pack_ = 1
    _fields_ = SmallResourceDataTag._fields_

# 6.4.2.5 I/O Port Descriptor

io_port_decoding = {
    0b0: 'Decodes bits[9:0]',
    0b1: 'Decodes bits[15:0]',
}

class SmallResourceItemIOPort(cdata.Struct):
    _pack_ = 1
    _fields_ = SmallResourceDataTag._fields_ + [
        ('_DEC', ctypes.c_uint8, 1),
        ('reserved', ctypes.c_uint8, 7),
        ('_MIN', ctypes.c_uint16),
        ('_MAX', ctypes.c_uint16),
        ('_ALN', ctypes.c_uint8),
        ('_LEN', ctypes.c_uint8),
    ]
    _formats = {
        '_DEC': unpack.format_table("{}", io_port_decoding)
    }

# 6.4.2.6 Fixed Location I/O Port Descriptor

class SmallResourceItemFixedLocationIOPort(cdata.Struct):
    _pack_ = 1
    _fields_ = SmallResourceDataTag._fields_ + [
        ('_BAS', ctypes.c_uint16),
        ('_LEN', ctypes.c_uint8),
    ]

# 6.4.2.7 Fixed DMA Descriptor

class SmallResourceItemFixedDMA(cdata.Struct):
    _pack_ = 1
    _fields_ = SmallResourceDataTag._fields_ + [
        ('_DMA', ctypes.c_uint16),
        ('_TYP', ctypes.c_uint16),
        ('_SIZ', ctypes.c_uint8),
    ]

# 6.4.2.8 Vendor-Defined Descriptor, Type 0

def SmallResourceItemVendorDefined_factory(_len):
    class SmallResourceItemVendorDefined(cdata.Struct):
        _pack_ = 1
        _fields_ = SmallResourceDataTag._fields_ + [
            ('data', ctypes.c_uint8 * _len),
        ]
    return SmallResourceItemVendorDefined

# 6.4.2.9 End Tag

class SmallResourceItemEndTag(cdata.Struct):
    _pack_ = 1
    _fields_ = SmallResourceDataTag._fields_ + [
        ('checksum', ctypes.c_uint8)
    ]

# 6.4.3 Large Resource Data Type

class LargeResourceDataTag(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('name', ctypes.c_uint8, 7),
        ('type', ctypes.c_uint8, 1),
        ('length', ctypes.c_uint16),
    ]

LARGE_RESOURCE_ITEM_24BIT_MEMORY_RANGE            = 0x01
LARGE_RESOURCE_ITEM_GENERIC_REGISTER              = 0x02
LARGE_RESOURCE_ITEM_VENDOR_DEFINED                = 0x04
LARGE_RESOURCE_ITEM_32BIT_MEMORY_RANGE            = 0x05
LARGE_RESOURCE_ITEM_32BIT_FIXED_MEMORY_RANGE      = 0x06
LARGE_RESOURCE_ITEM_ADDRESS_SPACE_RESOURCE        = 0x07
LARGE_RESOURCE_ITEM_WORD_ADDRESS_SPACE            = 0x08
LARGE_RESOURCE_ITEM_EXTENDED_INTERRUPT            = 0x09
LARGE_RESOURCE_ITEM_QWORD_ADDRESS_SPACE           = 0x0A
LARGE_RESOURCE_ITEM_EXTENDED_ADDRESS_SPACE        = 0x0B
LARGE_RESOURCE_ITEM_GPIO_CONNECTION               = 0x0C
LARGE_RESOURCE_ITEM_PIN_FUNCTION                  = 0x0D
LARGE_RESOURCE_ITEM_GENERIC_SERIAL_BUS_CONNECTION = 0x0E
LARGE_RESOURCE_ITEM_PIN_CONFIGURATION             = 0x0F
LARGE_RESOURCE_ITEM_PIN_GROUP                     = 0x10
LARGE_RESOURCE_ITEM_PIN_GROUP_FUNCTION            = 0x11
LARGE_RESOURCE_ITEM_PIN_GROUP_CONFIGURATION       = 0x12

# 6.4.3.1 24-Bit Memory Range Descriptor

class LargeResourceItem24BitMemoryRange(cdata.Struct):
    _pack_ = 1
    _fields_ = LargeResourceDataTag._fields_ + [
        ('_RW', ctypes.c_uint8, 1),
        ('ignored', ctypes.c_uint8, 7),
        ('_MIN', ctypes.c_uint16),
        ('_MAX', ctypes.c_uint16),
        ('_ALN', ctypes.c_uint16),
        ('_LEN', ctypes.c_uint16),
    ]

# 6.4.3.2 Vendor-Defined Descriptor, Type 1

def LargeResourceItemVendorDefined_factory(_len):
    class LargeResourceItemVendorDefined(cdata.Struct):
        _pack_ = 1
        _fields_ = SmallResourceDataTag._fields_ + [
            ('subtype', ctypes.c_uint8),
            ('UUID', ctypes.c_uint8 * 16),
            ('data', ctypes.c_uint8 * (_len - 17)),
        ]
    return LargeResourceItemVendorDefined

# 6.4.3.3 32-Bit Memory Range Descriptor

class LargeResourceItem32BitMemoryRange(cdata.Struct):
    _pack_ = 1
    _fields_ = LargeResourceDataTag._fields_ + [
        ('_RW', ctypes.c_uint8, 1),
        ('ignored', ctypes.c_uint8, 7),
        ('_MIN', ctypes.c_uint32),
        ('_MAX', ctypes.c_uint32),
        ('_ALN', ctypes.c_uint32),
        ('_LEN', ctypes.c_uint32),
    ]

# 6.4.3.4 32-Bit Fixed Memory Range Descriptor

class LargeResourceItem32BitFixedMemoryRange(cdata.Struct):
    _pack_ = 1
    _fields_ = LargeResourceDataTag._fields_ + [
        ('_RW', ctypes.c_uint8, 1),
        ('ignored', ctypes.c_uint8, 7),
        ('_BAS', ctypes.c_uint32),
        ('_LEN', ctypes.c_uint32),
    ]

# 6.4.3.5 Address Space Resource Descriptors

resource_type = {
    0x00: 'Memory range',
    0x01: 'I/O range',
    0x02: 'Bus number',
}

decode_type = {
    0b0: 'This bridge positively decodes this address',
    0b1: 'This bridge subtractively decodes this address',
}

min_address_fixed = {
    0b0: 'The specified minimum address is not fixed',
    0b1: 'The specified minimum address is fixed',
}

max_address_fixed = {
    0b0: 'The specified maximum address is not fixed',
    0b1: 'The specified maximum address is fixed',
}

# 6.4.3.5.1 QWord Address Space Descriptor

def LargeResourceItemQWordAddressSpace_factory(_len=43):
    class LargeResourceItemQWordAddressSpace(cdata.Struct):
        _pack_ = 1
        _fields_ = LargeResourceDataTag._fields_ + [
            ('_TYP', ctypes.c_uint8),
            ('ignored', ctypes.c_uint8, 1),
            ('_DEC', ctypes.c_uint8, 1),
            ('_MIF', ctypes.c_uint8, 1),
            ('_MAF', ctypes.c_uint8, 1),
            ('reserved1', ctypes.c_uint8, 4),
            ('flags', ctypes.c_uint8),
            ('_GRA', ctypes.c_uint64),
            ('_MIN', ctypes.c_uint64),
            ('_MAX', ctypes.c_uint64),
            ('_TRA', ctypes.c_uint64),
            ('_LEN', ctypes.c_uint64),
        ] + ([
            ('reserved2', ctypes.c_uint8)
        ] if (_len > 43) else []) + ([
            ('source', ctypes.c_char * (_len - 44))
        ] if (_len > 44) else [])
        _formats = {
            '_TYP': unpack.format_table("{}", resource_type),
            '_DEC': unpack.format_table("{}", decode_type),
            '_MIF': unpack.format_table("{}", min_address_fixed),
            '_MAF': unpack.format_table("{}", max_address_fixed),
        }
    return LargeResourceItemQWordAddressSpace

# 6.4.3.5.2 DWord Address Space Descriptor

def LargeResourceItemDWordAddressSpace_factory(_len=23):
    class LargeResourceItemDWordAddressSpace(cdata.Struct):
        _pack_ = 1
        _fields_ = LargeResourceDataTag._fields_ + [
            ('_TYP', ctypes.c_uint8),
            ('ignored', ctypes.c_uint8, 1),
            ('_DEC', ctypes.c_uint8, 1),
            ('_MIF', ctypes.c_uint8, 1),
            ('_MAF', ctypes.c_uint8, 1),
            ('reserved1', ctypes.c_uint8, 4),
            ('flags', ctypes.c_uint8),
            ('_GRA', ctypes.c_uint32),
            ('_MIN', ctypes.c_uint32),
            ('_MAX', ctypes.c_uint32),
            ('_TRA', ctypes.c_uint32),
            ('_LEN', ctypes.c_uint32),
        ] + ([
            ('reserved2', ctypes.c_uint8)
        ] if (_len > 23) else []) + ([
            ('source', ctypes.c_char * (_len - 24))
        ] if (_len > 24) else [])
        _formats = {
            '_TYP': unpack.format_table("{}", resource_type),
            '_DEC': unpack.format_table("{}", decode_type),
            '_MIF': unpack.format_table("{}", min_address_fixed),
            '_MAF': unpack.format_table("{}", max_address_fixed),
        }
    return LargeResourceItemDWordAddressSpace

# 6.4.3.5.3 Word Address Space Descriptor

def LargeResourceItemWordAddressSpace_factory(_len=13):
    class LargeResourceItemWordAddressSpace(cdata.Struct):
        _pack_ = 1
        _fields_ = LargeResourceDataTag._fields_ + [
            ('_TYP', ctypes.c_uint8),
            ('ignored', ctypes.c_uint8, 1),
            ('_DEC', ctypes.c_uint8, 1),
            ('_MIF', ctypes.c_uint8, 1),
            ('_MAF', ctypes.c_uint8, 1),
            ('reserved1', ctypes.c_uint8, 4),
            ('flags', ctypes.c_uint8),
            ('_GRA', ctypes.c_uint16),
            ('_MIN', ctypes.c_uint16),
            ('_MAX', ctypes.c_uint16),
            ('_TRA', ctypes.c_uint16),
            ('_LEN', ctypes.c_uint16),
        ] + ([
            ('reserved2', ctypes.c_uint8)
        ] if (_len > 13) else []) + ([
            ('source', ctypes.c_char * (_len - 14))
        ] if (_len > 14) else [])
        _formats = {
            '_TYP': unpack.format_table("{}", resource_type),
            '_DEC': unpack.format_table("{}", decode_type),
            '_MIF': unpack.format_table("{}", min_address_fixed),
            '_MAF': unpack.format_table("{}", max_address_fixed),
        }
    return LargeResourceItemWordAddressSpace

# 6.4.3.5.4 Extended Address Space Descriptor

class LargeResourceItemExtendedAddressSpace(cdata.Struct):
    _pack_ = 1
    _fields_ = LargeResourceDataTag._fields_ + [
        ('_TYP', ctypes.c_uint8),
        ('ignored', ctypes.c_uint8, 1),
        ('_DEC', ctypes.c_uint8, 1),
        ('_MIF', ctypes.c_uint8, 1),
        ('_MAF', ctypes.c_uint8, 1),
        ('reserved1', ctypes.c_uint8, 4),
        ('flags', ctypes.c_uint8),
        ('revision', ctypes.c_uint8),
        ('reserved2', ctypes.c_uint8),
        ('_GRA', ctypes.c_uint64),
        ('_MIN', ctypes.c_uint64),
        ('_MAX', ctypes.c_uint64),
        ('_TRA', ctypes.c_uint64),
        ('_LEN', ctypes.c_uint64),
        ('_ATT', ctypes.c_uint64),
    ]
    _formats = {
        '_TYP': unpack.format_table("{}", resource_type),
        '_DEC': unpack.format_table("{}", decode_type),
        '_MIF': unpack.format_table("{}", min_address_fixed),
        '_MAF': unpack.format_table("{}", max_address_fixed),
    }

# 6.4.3.6 Extended Interrupt Descriptor

class LargeResourceItemExtendedInterruptHeader(cdata.Struct):
    _pack_ = 1
    _fields_ = LargeResourceDataTag._fields_ + [
        ('flags', ctypes.c_uint8),
        ('_LEN', ctypes.c_uint8),
    ]

def LargeResourceItemExtendedInterrupt_factory(_len, _it_len):
    class LargeResourceItemExtendedInterrupt(cdata.Struct):
        _pack_ = 1
        _fields_ = LargeResourceDataTag._fields_ + [
            ('_CP', ctypes.c_uint8, 1),
            ('_HE', ctypes.c_uint8, 1),
            ('_LL', ctypes.c_uint8, 1),
            ('_SHR', ctypes.c_uint8, 1),
            ('_WKC', ctypes.c_uint8, 1),
            ('reserved1', ctypes.c_uint8, 3),
            ('_LEN', ctypes.c_uint8),
            ('_INT', ctypes.c_uint32 * _it_len),
        ] + ([
            ('reserved2', ctypes.c_uint8),
        ] if (_len > 2 + _it_len * 4) else []) + ([
            ('source', ctypes.c_char * (_len - _it_len * 4 - 3))
        ] if (_len > 2 + _it_len * 4 + 1) else [])
    return LargeResourceItemExtendedInterrupt

# 6.4.3.7 Generic Register Descriptor

class LargeResourceItemGenericRegister(cdata.Struct):
    _pack_ = 1
    _fields_ = LargeResourceDataTag._fields_ + [
        ('_ASI', ctypes.c_uint8),
        ('_RBW', ctypes.c_uint8),
        ('_RBO', ctypes.c_uint8),
        ('_ASZ', ctypes.c_uint8),
        ('_ADR', ctypes.c_uint64),
    ]

# 6.4.3.8.1 GPIO Connection Descriptor (TODO)

def LargeResourceItemGPIOConnection_factory(_len):
    class LargeResourceItemGPIOConnection(cdata.Struct):
        _pack_ = 1
        _fields_ = LargeResourceDataTag._fields_ + [
            ('_REV', ctypes.c_uint8),
            ('_TYP', ctypes.c_uint8),
            ('data', ctypes.c_uint8 * (_len - 2)),
        ]
    return LargeResourceItemGPIOConnection

# 6.4.3.8.2 GenericSerialBus Connection Descriptors (TODO)

def LargeResourceItemGenericSerialBusConnection_factory(_len):
    class LargeResourceItemGenericSerialBusConnection(cdata.Struct):
        _pack_ = 1
        _fields_ = LargeResourceDataTag._fields_ + [
            ('_REV', ctypes.c_uint8),
            ('_RSI', ctypes.c_uint8),
            ('_TYP', ctypes.c_uint8),
            ('data', ctypes.c_uint8 * (_len - 3)),
        ]
    return LargeResourceItemGenericSerialBusConnection

# 6.4.3.9 Pin Function Descriptor

def LargeResourceItemPinFunction_factory(_len):
    class LargeResourceItemPinFunction(cdata.Struct):
        _pack_ = 1
        _fields_ = LargeResourceDataTag._fields_ + [
            ('_REV', ctypes.c_uint8),
            ('_SHR', ctypes.c_uint8, 1),
            ('reserved1', ctypes.c_uint16, 15),
            ('_PPC', ctypes.c_uint8),
            ('_FUN', ctypes.c_uint16),
            ('_PTO', ctypes.c_uint16),
            ('reserved2', ctypes.c_uint8),
            ('_RNI', ctypes.c_uint16),
            ('_VDO', ctypes.c_uint16),
            ('_VDL', ctypes.c_uint16),
            ('data', ctypes.c_uint8 * (_len - 18)),
        ]
    return LargeResourceItemPinFunction

# 6.4.3.10 Pin Configuration Descriptor

def LargeResourceItemPinConfiguration_factory(_len):
    class LargeResourceItemPinConfiguration(cdata.Struct):
        _pack_ = 1
        _fields_ = LargeResourceDataTag._fields_ + [
            ('_REV', ctypes.c_uint8),
            ('_SHR', ctypes.c_uint8, 1),
            ('_CP', ctypes.c_uint8, 1),
            ('reserved1', ctypes.c_uint16, 14),
            ('_TYP', ctypes.c_uint8),
            ('_VAL', ctypes.c_uint32),
            ('_PTO', ctypes.c_uint16),
            ('reserved2', ctypes.c_uint8),
            ('_RNO', ctypes.c_uint16),
            ('_VDO', ctypes.c_uint16),
            ('_VDL', ctypes.c_uint16),
            ('data', ctypes.c_uint8 * (_len - 20)),
        ]
    return LargeResourceItemPinConfiguration

# 6.4.3.11 Pin Group Descriptor

def LargeResourceItemPinGroup_factory(_len):
    class LargeResourceItemPinGroup(cdata.Struct):
        _pack_ = 1
        _fields_ = LargeResourceDataTag._fields_ + [
            ('_REV', ctypes.c_uint8),
            ('_CP', ctypes.c_uint8, 1),
            ('reserved1', ctypes.c_uint16, 15),
            ('_PTO', ctypes.c_uint16),
            ('_RLO', ctypes.c_uint16),
            ('_VDO', ctypes.c_uint16),
            ('_VDL', ctypes.c_uint16),
            ('data', ctypes.c_uint8 * (_len - 14)),
        ]
    return LargeResourceItemPinGroup

# 6.4.3.12 Pin Group Function Descriptor

def LargeResourceItemPinGroupFunction_factory(_len):
    class LargeResourceItemPinGroupFunction(cdata.Struct):
        _pack_ = 1
        _fields_ = LargeResourceDataTag._fields_ + [
            ('_REV', ctypes.c_uint8),
            ('_SHR', ctypes.c_uint8, 1),
            ('_CP', ctypes.c_uint8, 1),
            ('reserved1', ctypes.c_uint16, 14),
            ('_FUN', ctypes.c_uint16),
            ('reserved2', ctypes.c_uint8),
            ('_RNI', ctypes.c_uint16),
            ('_RLO', ctypes.c_uint16),
            ('_VDO', ctypes.c_uint16),
            ('_VDL', ctypes.c_uint16),
            ('data', ctypes.c_uint8 * (_len - 17)),
        ]
    return LargeResourceItemPinGroupFunction

# 6.4.3.13 Pin Group Configuration Descriptor

def LargeResourceItemPinGroupConfiguration_factory(_len):
    class LargeResourceItemPinGroupConfiguration(cdata.Struct):
        _pack_ = 1
        _fields_ = LargeResourceDataTag._fields_ + [
            ('_REV', ctypes.c_uint8),
            ('_SHR', ctypes.c_uint8, 1),
            ('_CP', ctypes.c_uint8, 1),
            ('reserved1', ctypes.c_uint16, 14),
            ('_TYP', ctypes.c_uint8),
            ('_VAL', ctypes.c_uint32),
            ('reserved2', ctypes.c_uint8),
            ('_RNO', ctypes.c_uint16),
            ('_RLO', ctypes.c_uint16),
            ('_VDO', ctypes.c_uint16),
            ('_VDL', ctypes.c_uint16),
            ('data', ctypes.c_uint8 * (_len - 20)),
        ]
    return LargeResourceItemPinGroupConfiguration

# The parser

def rdt_item_list(addr, length):
    end = addr + length
    field_list = list()
    item_num = 0
    while addr < end:
        item_num += 1
        tag = SmallResourceDataTag.from_address(addr)
        if tag.type == 0:  # Small type
            if tag.name == SMALL_RESOURCE_ITEM_IRQ_FORMAT:
                cls = SmallResourceItemIRQ_factory(tag.length)
            elif tag.name == SMALL_RESOURCE_ITEM_DMA_FORMAT:
                cls = SmallResourceItemDMA
            elif tag.name == SMALL_RESOURCE_ITEM_START_DEPENDENT_FUNCTIONS:
                cls = SmallResourceItemStartDependentFunctions_factory(tag.length)
            elif tag.name == SMALL_RESOURCE_ITEM_END_DEPENDENT_FUNCTIONS:
                cls = SmallResourceItemEndDependentFunctions
            elif tag.name == SMALL_RESOURCE_ITEM_IO_PORT:
                cls = SmallResourceItemIOPort
            elif tag.name == SMALL_RESOURCE_ITEM_FIXED_LOCATION_IO_PORT:
                cls = SmallResourceItemFixedLocationIOPort
            elif tag.name == SMALL_RESOURCE_ITEM_FIXED_DMA:
                cls = SmallResourceItemFixedDMA
            elif tag.name == SMALL_RESOURCE_ITEM_VENDOR_DEFINED:
                cls = SmallResourceItemVendorDefined_factory(tag.length)
            elif tag.name == SMALL_RESOURCE_ITEM_END_TAG:
                cls = SmallResourceItemEndTag
            else:
                raise NotImplementedError(f"Unknown small resource item name: {tag.name}, offset {addr}")
            size = ctypes.sizeof(cls)
            assert size == tag.length + 1, f"{cls}: {size} != {tag.length} + 1"
            addr += size
        else:              # Large type
            tag = LargeResourceDataTag.from_address(addr)
            if tag.name == LARGE_RESOURCE_ITEM_24BIT_MEMORY_RANGE:
                cls = LargeResourceItem24BitMemoryRange
            elif tag.name == LARGE_RESOURCE_ITEM_GENERIC_REGISTER:
                cls = LargeResourceItemGenericRegister
            elif tag.name == LARGE_RESOURCE_ITEM_VENDOR_DEFINED:
                cls = LargeResourceItemVendorDefined_factory(tag.length)
            elif tag.name == LARGE_RESOURCE_ITEM_32BIT_MEMORY_RANGE:
                cls = LargeResourceItem32BitMemoryRange
            elif tag.name == LARGE_RESOURCE_ITEM_32BIT_FIXED_MEMORY_RANGE:
                cls = LargeResourceItem32BitFixedMemoryRange
            elif tag.name == LARGE_RESOURCE_ITEM_ADDRESS_SPACE_RESOURCE:
                cls = LargeResourceItemDWordAddressSpace_factory(tag.length)
            elif tag.name == LARGE_RESOURCE_ITEM_WORD_ADDRESS_SPACE:
                cls = LargeResourceItemWordAddressSpace_factory(tag.length)
            elif tag.name == LARGE_RESOURCE_ITEM_EXTENDED_INTERRUPT:
                hdr = LargeResourceItemExtendedInterruptHeader.from_address(addr)
                cls = LargeResourceItemExtendedInterrupt_factory(tag.length, hdr._LEN)
            elif tag.name == LARGE_RESOURCE_ITEM_QWORD_ADDRESS_SPACE:
                cls = LargeResourceItemQWordAddressSpace_factory(tag.length)
            elif tag.name == LARGE_RESOURCE_ITEM_EXTENDED_ADDRESS_SPACE:
                cls = LargeResourceItemExtendedAddressSpace
            elif tag.name == LARGE_RESOURCE_ITEM_GPIO_CONNECTION:
                cls = LargeResourceItemGPIOConnection_factory(tag.length)
            elif tag.name == LARGE_RESOURCE_ITEM_PIN_FUNCTION:
                cls = LargeResourceItemPinFunction_factory(tag.length)
            elif tag.name == LARGE_RESOURCE_ITEM_GENERIC_SERIAL_BUS_CONNECTION:
                cls = LargeResourceItemGenericSerialBusConnection_factory(tag.length)
            elif tag.name == LARGE_RESOURCE_ITEM_PIN_CONFIGURATION:
                cls = LargeResourceItemPinConfiguration_factory(tag.length)
            elif tag.name == LARGE_RESOURCE_ITEM_PIN_GROUP:
                cls = LargeResourceItemPinGroup_factory(tag.length)
            elif tag.name == LARGE_RESOURCE_ITEM_PIN_GROUP_FUNCTION:
                cls = LargeResourceItemPinGroupFunction_factory(tag.length)
            elif tag.name == LARGE_RESOURCE_ITEM_PIN_GROUP_CONFIGURATION:
                cls = LargeResourceItemPinGroupConfiguration_factory(tag.length)
            else:
                raise NotImplementedError(f"Unknown Large resource item name: {tag.name}, offset {addr}")
            size = ctypes.sizeof(cls)
            assert size == tag.length + 3, f"{cls}: {size} != {tag.length} + 3"
            addr += size
        field_list.append((f'item{item_num}', cls))
    return field_list

def rdt_factory(field_list):

    class items(cdata.Struct):
        _pack_ = 1
        _fields_ = field_list

        def __iter__(self):
            for f in self._fields_:
                yield getattr(self, f[0])

        def __getitem__(self, index):
            return getattr(self, self._fields_[index][0])

    class ResourceData(cdata.Struct):
        _pack_ = 1
        _fields_ = [
            ('items', items)
        ]
    return ResourceData

def parse_resource_data(data):
    """Parse ACPI resource data types returned by _CRS, _PRS and _SRS control methods."""
    buf = ctypes.create_string_buffer(bytes(data), len(data))
    addr = ctypes.addressof(buf)
    item_list = rdt_item_list(addr, len(data))
    return rdt_factory(item_list).from_buffer_copy(data)
