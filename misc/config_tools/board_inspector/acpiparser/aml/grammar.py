# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Primary opcodes
AML_ZERO_OP                 = 0x00
AML_RESERVED_FIELD_PREFIX   = 0x00
AML_NULL_NAME               = 0x00
AML_ONE_OP                  = 0x01
AML_ACCESS_FIELD_PREFIX     = 0x01
AML_CONNECT_FIELD_PREFIX    = 0x02
AML_EXTENDED_ACCESS_FIELD_PREFIX  = 0x03
AML_ALIAS_OP                = 0x06
AML_NAME_OP                 = 0x08
AML_BYTE_PREFIX             = 0x0A
AML_WORD_PREFIX             = 0x0B
AML_DWORD_PREFIX            = 0x0C
AML_STRING_PREFIX           = 0x0D
AML_QWORD_PREFIX            = 0x0E
AML_SCOPE_OP                = 0x10
AML_BUFFER_OP               = 0x11
AML_PACKAGE_OP              = 0x12
AML_VAR_PACKAGE_OP          = 0x13
AML_METHOD_OP               = 0x14
AML_EXTERNAL_OP             = 0x15
AML_DUAL_NAME_PREFIX        = 0x2E
AML_MULTI_NAME_PREFIX       = 0x2F
AML_EXT_OP_PREFIX           = 0x5B
AML_FIRST_LOCAL_OP          = 0x60
AML_LOCAL0_OP               = 0x60
AML_LOCAL1_OP               = 0x61
AML_LOCAL2_OP               = 0x62
AML_LOCAL3_OP               = 0x63
AML_LOCAL4_OP               = 0x64
AML_LOCAL5_OP               = 0x65
AML_LOCAL6_OP               = 0x66
AML_LOCAL7_OP               = 0x67
AML_ARG0_OP                 = 0x68
AML_ARG1_OP                 = 0x69
AML_ARG2_OP                 = 0x6A
AML_ARG3_OP                 = 0x6B
AML_ARG4_OP                 = 0x6C
AML_ARG5_OP                 = 0x6D
AML_ARG6_OP                 = 0x6E
AML_STORE_OP                = 0x70
AML_REF_OF_OP               = 0x71
AML_ADD_OP                  = 0x72
AML_CONCAT_OP               = 0x73
AML_SUBTRACT_OP             = 0x74
AML_INCREMENT_OP            = 0x75
AML_DECREMENT_OP            = 0x76
AML_MULTIPLY_OP             = 0x77
AML_DIVIDE_OP               = 0x78
AML_SHIFT_LEFT_OP           = 0x79
AML_SHIFT_RIGHT_OP          = 0x7A
AML_AND_OP                  = 0x7B
AML_NAND_OP                 = 0x7C
AML_OR_OP                   = 0x7D
AML_NOR_OP                  = 0x7E
AML_XOR_OP                  = 0x7F
AML_NOT_OP                  = 0x80
AML_FIND_SET_LEFT_BIT_OP    = 0x81
AML_FIND_SET_RIGHT_BIT_OP   = 0x82
AML_DEREF_OF_OP             = 0x83
AML_CONCAT_RES_OP           = 0x84
AML_MOD_OP                  = 0x85
AML_NOTIFY_OP               = 0x86
AML_SIZE_OF_OP              = 0x87
AML_INDEX_OP                = 0x88
AML_MATCH_OP                = 0x89
AML_CREATE_DWORD_FIELD_OP   = 0x8A
AML_CREATE_WORD_FIELD_OP    = 0x8B
AML_CREATE_BYTE_FIELD_OP    = 0x8C
AML_CREATE_BIT_FIELD_OP     = 0x8D
AML_OBJECT_TYPE_OP          = 0x8E
AML_CREATE_QWORD_FIELD_OP   = 0x8F
AML_LAND_OP                 = 0x90
AML_LOR_OP                  = 0x91
AML_LNOT_OP                 = 0x92
AML_LEQUAL_OP               = 0x93
AML_LGREATER_OP             = 0x94
AML_LLESS_OP                = 0x95
AML_TO_BUFFER_OP            = 0x96
AML_TO_DECIMAL_STRING_OP    = 0x97
AML_TO_HEX_STRING_OP        = 0x98
AML_TO_INTEGER_OP           = 0x99
AML_TO_STRING_OP            = 0x9C
AML_COPY_OBJECT_OP          = 0x9D
AML_MID_OP                  = 0x9E
AML_CONTINUE_OP             = 0x9F
AML_IF_OP                   = 0xA0
AML_ELSE_OP                 = 0xA1
AML_WHILE_OP                = 0xA2
AML_NOOP_OP                 = 0xA3
AML_RETURN_OP               = 0xA4
AML_BREAK_OP                = 0xA5
AML_BREAKPOINT_OP           = 0xCC
AML_ONES_OP                 = 0xFF

# Prefixed opcodes, with the least byte being AML_EXT_OP_PREFIX
AML_MUTEX_OP                = 0x015b
AML_EVENT_OP                = 0x025b
AML_CONDITIONAL_REF_OF_OP   = 0x125b
AML_CREATE_FIELD_OP         = 0x135b
AML_LOAD_TABLE_OP           = 0x1f5b
AML_LOAD_OP                 = 0x205b
AML_STALL_OP                = 0x215b
AML_SLEEP_OP                = 0x225b
AML_ACQUIRE_OP              = 0x235b
AML_SIGNAL_OP               = 0x245b
AML_WAIT_OP                 = 0x255b
AML_RESET_OP                = 0x265b
AML_RELEASE_OP              = 0x275b
AML_FROM_BCD_OP             = 0x285b
AML_TO_BCD_OP               = 0x295b
AML_UNLOAD_OP               = 0x2a5b
AML_REVISION_OP             = 0x305b
AML_DEBUG_OP                = 0x315b
AML_FATAL_OP                = 0x325b
AML_TIMER_OP                = 0x335b
AML_REGION_OP               = 0x805b
AML_FIELD_OP                = 0x815b
AML_DEVICE_OP               = 0x825b
AML_PROCESSOR_OP            = 0x835b
AML_POWER_RESOURCE_OP       = 0x845b
AML_THERMAL_ZONE_OP         = 0x855b
AML_INDEX_FIELD_OP          = 0x865b
AML_BANK_FIELD_OP           = 0x875b
AML_DATA_REGION_OP          = 0x885b


################################################################################
# 20.2.1 Table and Table Header Encoding
################################################################################

AMLCode = ("DefBlockHeader", "TermObj*")
DefBlockHeader = ("TableSignature", "TableLength", "SpecCompliance", "CheckSum", "OemID", "OemTableID", "OemRevision", "CreatorID", "CreatorRevision")
TableSignature = ["DWordData"]
TableLength = ["DWordData"]
SpecCompliance = ["ByteData"]
CheckSum = ["ByteData"]
OemID = ["TWordData"]
OemTableID = ["QWordData"]
OemRevision = ["DWordData"]
CreatorID = ["DWordData"]
CreatorRevision = ["DWordData"]

################################################################################
# 20.2.2 Name Objects Encoding
################################################################################

# NameSeg is defined in parser.py
# NameString is defined in parser.py

SimpleName = ["NameString", "ArgObj", "LocalObj"]
SuperName = ["SimpleName", "DebugObj", "ReferenceTypeOpcode"]
NullName = (AML_NULL_NAME,)
Target = ["SuperName", "NullName"]

################################################################################
# 20.2.3 Data Objects Encoding
################################################################################

ComputationalData = ["ByteConst", "WordConst", "DWordConst", "QWordConst", "String", "ConstObj", "RevisionOp", "DefBuffer"]
DataObject = ["ComputationalData", "DefPackage", "DefVarPackage"]
DataRefObject = ["DataObject"]

ByteConst = (AML_BYTE_PREFIX, "ByteData")
WordConst = (AML_WORD_PREFIX, "WordData")
DWordConst = (AML_DWORD_PREFIX, "DWordData")
QWordConst = (AML_QWORD_PREFIX, "QWordData")
# String is defined in parser.py

ConstObj = ["ZeroOp", "OneOp", "OnesOp"]
# ByteList is defined in parser.py
# ByteData, WordData, DWordData and QWordData are defined in parser.py

ZeroOp = (AML_ZERO_OP,)
OneOp = (AML_ONE_OP,)
OnesOp = (AML_ONES_OP,)
RevisionOp = (AML_REVISION_OP,)

################################################################################
# 20.2.4 Package Length Encoding
################################################################################

# PkgLength is defined in parser.py

################################################################################
# 20.2.5 Term Objects Encoding
################################################################################

Object = ["NameSpaceModifierObj", "NamedObj"]
TermObj = ["Object", "StatementOpcode", "ExpressionOpcode", "ConstObj"]
TermList = ("TermObj*",)
TermArg = ["ExpressionOpcode", "DataObject", "ArgObj", "LocalObj"]
# MethodInvocation is defined in parser.py

# 20.2.5.1 Namespace Modifier Objects Encoding
################################################################################

NameSpaceModifierObj = ["DefAlias", "DefName", "DefScope"]

DefAlias = (AML_ALIAS_OP, "NameString:SourceObject", "NameString:AliasObject")
DefName = (AML_NAME_OP, "NameString", "DataRefObject")
DefScope = (AML_SCOPE_OP, "PkgLength", "NameString", "TermList")

# 20.2.5.2 Named Objects Encoding
################################################################################

NamedObj = ["DefBankField", "DefCreateBitField", "DefCreateByteField", "DefCreateDWordField", "DefCreateField",
            "DefCreateQWordField", "DefCreateWordField", "DefDataRegion", "DefDevice", "DefEvent", "DefExternal",
            "DefField", "DefIndexField", "DefMethod", "DefMutex", "DefOpRegion", "DefPowerRes", "DefProcessor",
            "DefThermalZone"]

DefBankField = (AML_BANK_FIELD_OP, "PkgLength", "NameString:RegionName", "NameString:BankName", "BankValue", "FieldFlags", "FieldList")
BankValue = ["TermArg"]
FieldFlags = ["ByteData"]
FieldList = ("FieldElement*",)
NamedField = ("NameSeg", "FieldLength")
ReservedField = (AML_RESERVED_FIELD_PREFIX, "FieldLength")
AccessField = (AML_ACCESS_FIELD_PREFIX, "AccessType", "AccessAttrib")
AccessType = ["ByteData"]
AccessAttrib = ["ByteData"]
ConnectFieldDef = ["NameString"]  # FIXME: ACPI spec allows "BufferData" here but does not define what "BufferData" is
ConnectField = (AML_CONNECT_FIELD_PREFIX, "ConnectFieldDef")

DefCreateBitField = (AML_CREATE_BIT_FIELD_OP, "SourceBuff", "BitIndex", "NameString")
SourceBuff = ["TermArg"]
BitIndex = ["TermArg"]
DefCreateByteField = (AML_CREATE_BYTE_FIELD_OP, "SourceBuff", "ByteIndex", "NameString")
ByteIndex = ["TermArg"]
DefCreateDWordField = (AML_CREATE_DWORD_FIELD_OP, "SourceBuff", "ByteIndex", "NameString")
DefCreateField = (AML_CREATE_FIELD_OP, "SourceBuff", "BitIndex", "NumBits", "NameString")
NumBits = ["TermArg"]
DefCreateQWordField = (AML_CREATE_QWORD_FIELD_OP, "SourceBuff", "ByteIndex", "NameString")
DefCreateWordField = (AML_CREATE_WORD_FIELD_OP, "SourceBuff", "ByteIndex", "NameString")

DefDataRegion = (AML_DATA_REGION_OP, "NameString", "TermArg:Signature", "TermArg:OEMID", "TermArg:OEMTableID")

DefDevice = (AML_DEVICE_OP, "PkgLength", "NameString", "TermList")

DefEvent = (AML_EVENT_OP, "NameString")

DefExternal = (AML_EXTERNAL_OP, "NameString", "ObjectType", "ArgumentCount")
ObjectType = ["ByteData"]
ArgumentCount = ["ByteData"]

DefField = (AML_FIELD_OP, "PkgLength", "NameString", "FieldFlags", "FieldList")

DefIndexField = (AML_INDEX_FIELD_OP, "PkgLength", "NameString:IndexName", "NameString:DataName", "FieldFlags", "FieldList")

DefMethod = (AML_METHOD_OP, "PkgLength", "NameString", "MethodFlags", "TermList")
MethodFlags = ["ByteData"]

DefMutex = (AML_MUTEX_OP, "NameString", "SyncFlags")
SyncFlags = ["ByteData"]

DefOpRegion = (AML_REGION_OP, "NameString", "RegionSpace", "RegionOffset", "RegionLen")
RegionSpace = ["ByteData"]
RegionOffset = ["TermArg"]
RegionLen = ["TermArg"]

DefPowerRes = (AML_POWER_RESOURCE_OP, "PkgLength", "NameString", "SystemLevel", "ResourceOrder", "TermList")
SystemLevel = ["ByteData"]
ResourceOrder = ["WordData"]

DefProcessor = (AML_PROCESSOR_OP, "PkgLength", "NameString", "ProcID", "PblkAddr", "PblkLen", "TermList")
ProcID = ["ByteData"]
PblkAddr = ["DWordData"]
PblkLen = ["ByteData"]

DefThermalZone = (AML_THERMAL_ZONE_OP, "PkgLength", "NameString", "TermList")

ExtendedAccessField = (AML_EXTENDED_ACCESS_FIELD_PREFIX, "AccessType", "ExtendedAccessAttrib", "AccessLength")
ExtendedAccessAttrib = ["ByteData"]
AccessLength = ["ByteData"]
FieldElement = ["NamedField", "ReservedField", "AccessField", "ExtendedAccessField", "ConnectField"]

# 20.2.5.3 Statement Opcodes Encoding
################################################################################

StatementOpcode = ["DefBreak", "DefBreakPoint", "DefContinue", "DefFatal", "DefIfElse", "DefNoop", "DefNotify",
                   "DefRelease", "DefReset", "DefReturn", "DefSignal", "DefSleep", "DefStall", "DefUnload", "DefWhile"]

DefBreak = (AML_BREAK_OP,)

DefBreakPoint = (AML_BREAKPOINT_OP,)

DefContinue = (AML_CONTINUE_OP,)

DefElse = (AML_ELSE_OP, "PkgLength", "TermList")

DefFatal = (AML_FATAL_OP, "FatalType", "FatalCode", "FataArg")
FatalType = ["ByteData"]
FatalCode = ["DWordData"]
FatalArg = ["TermArg"]

DefIfElse = (AML_IF_OP, "PkgLength", "Predicate", "TermList", "DefElse?")
Predicate = ["TermArg"]

DefNoop = (AML_NOOP_OP,)

DefNotify = (AML_NOTIFY_OP, "NotifyObject", "NotifyValue")
NotifyObject = ["SuperName"]
NotifyValue = ["TermArg"]

DefRelease = (AML_RELEASE_OP, "MutexObject")
MutexObject = ["SuperName"]

DefReset = (AML_RESET_OP, "EventObject")
EventObject = ["SuperName"]

DefReturn = (AML_RETURN_OP, "ArgObject")
ArgObject = ["TermArg"]

DefSignal = (AML_SIGNAL_OP, "EventObject")

DefSleep = (AML_SLEEP_OP, "MsecTime")
MsecTime = ["TermArg"]

DefStall = (AML_STALL_OP, "UsecTime")
UsecTime = ["TermArg"]

DefUnload = (AML_UNLOAD_OP, "Target")

DefWhile = (AML_WHILE_OP, "PkgLength", "Predicate", "TermList")

# 20.2.5.4 Expression Opcodes Encoding
################################################################################

ExpressionOpcode = ["DefAcquire", "DefAdd", "DefAnd", "DefBuffer", "DefConcat", "DefConcatRes", "DefCondRefOf",
                    "DefCopyObject", "DefDecrement", "DefDerefOf", "DefDivide", "DefFindSetLeftBit",
                    "DefFindSetRightBit", "DefFromBCD", "DefIncrement", "DefIndex", "DefLAnd", "DefLEqual",
                    "DefLGreater", "DefLLess", "DefMid", "DefLNot", "DefLoad", "DefLoadTable", "DefLOr", "DefMatch",
                    "DefMod", "DefMultiply", "DefNAnd", "DefNOr", "DefNot", "DefObjectType", "DefOr", "DefPackage",
                    "DefVarPackage", "DefRefOf", "DefShiftLeft", "DefShiftRight", "DefSizeOf", "DefStore",
                    "DefSubtract", "DefTimer", "DefToBCD", "DefToBuffer", "DefToDecimalString", "DefToHexString",
                    "DefToInteger", "DefToString", "DefWait", "DefXOr", "MethodInvocation" ]

ReferenceTypeOpcode = ["DefRefOf", "DefDerefOf", "DefIndex"]

DefAcquire = (AML_ACQUIRE_OP, "MutexObject", "Timeout")
Timeout = ["WordData"]

DefAdd = (AML_ADD_OP, "Operand:LeftOperand", "Operand:RightOperand", "Target")
Operand = ["TermArg"]

DefAnd = (AML_AND_OP, "Operand:LeftOperand", "Operand:RightOperand", "Target")

DefBuffer = (AML_BUFFER_OP, "PkgLength", "BufferSize", "ByteList")
BufferSize = ["TermArg"]

DefConcat = (AML_CONCAT_OP, "Data:Data1", "Data:Data2", "Target")
Data = ["TermArg"]

DefConcatRes = (AML_CONCAT_RES_OP, "BufData:BufData1", "BufData:BufData2", "Target")
BufData = ["TermArg"]

DefCondRefOf = (AML_CONDITIONAL_REF_OF_OP, "SuperName", "Target")

DefCopyObject = (AML_COPY_OBJECT_OP, "TermArg", "SimpleName")

DefDecrement = (AML_DECREMENT_OP, "SuperName")

DefDerefOf = (AML_DEREF_OF_OP, "ObjReference")
ObjReference = ["TermArg"]

DefDivide = (AML_DIVIDE_OP, "Dividend", "Divisor", "Remainder", "Quotient")
Dividend = ["TermArg"]
Divisor = ["TermArg"]
Remainder = ["Target"]
Quotient = ["Target"]

DefFindSetLeftBit = (AML_FIND_SET_LEFT_BIT_OP, "Operand", "Target")
DefFindSetRightBit = (AML_FIND_SET_RIGHT_BIT_OP, "Operand", "Target")

DefFromBCD = (AML_FROM_BCD_OP, "BCDValue", "Target")
BCDValue = ["TermArg"]

DefIncrement = (AML_INCREMENT_OP, "SuperName")

DefIndex = (AML_INDEX_OP, "BuffPkgStrObj", "IndexValue", "Target")
BuffPkgStrObj = ["TermArg"]
IndexValue = ["TermArg"]

DefLAnd = (AML_LAND_OP, "Operand:LeftOperand", "Operand:RightOperand")
DefLEqual = (AML_LEQUAL_OP, "Operand:LeftOperand", "Operand:RightOperand")
DefLGreater = (AML_LGREATER_OP, "Operand:LeftOperand", "Operand:RightOperand")
# DefLGreaterEqual is equivalent to (AML_LNOT_OP, DefLLess)
DefLLess = (AML_LLESS_OP, "Operand:LeftOperand", "Operand:RightOperand")
# DefLLessEqual is equivalent to (AML_LNOT_OP, DefLGreater)
DefLNot = (AML_LNOT_OP, "Operand")
# DefLNotEqual is equivalent to (AML_LNOT_OP, DefLEqual)

DefLoad = (AML_LOAD_OP, "NameString", "Target")

DefLoadTable = (AML_LOAD_TABLE_OP, "TermArg:Signature", "TermArg:OEMID", "TermArg:TableID", "TermArg:RootPath", "TermArg:ParameterPath", "TermArg:ParameterData")

DefLOr = (AML_LOR_OP, "Operand:LeftOperand", "Operand:RightOperand")

DefMatch = (AML_MATCH_OP, "SearchPkg", "MatchOpcode:MatchOpcode1", "Operand:Operand1", "MatchOpcode:MatchOpcode2", "Operand:Operand2", "StartIndex")
SearchPkg = ["TermArg"]
MatchOpcode = ["ByteData"]
StartIndex = ["TermArg"]

DefMid = (AML_MID_OP, "MidObj", "TermArg:Source", "TermArg:Index", "Target")
MidObj = ["TermArg"]

DefMod = (AML_MOD_OP, "Dividend", "Divisor", "Target")

DefMultiply = (AML_MULTIPLY_OP, "Operand:LeftOperand", "Operand:RightOperand", "Target")

DefNAnd = (AML_NAND_OP, "Operand:LeftOperand", "Operand:RightOperand", "Target")
DefNOr = (AML_NOR_OP, "Operand:LeftOperand", "Operand:RightOperand", "Target")
DefNot = (AML_NOT_OP, "Operand", "Target")

DefObjectType = (AML_OBJECT_TYPE_OP, "ObjectTypeContent")
ObjectTypeContent = ["SimpleName", "DebugObj", "DefRefof", "DefDerefof", "DefIndex"]

DefOr = (AML_OR_OP, "Operand:LeftOperand", "Operand:RightOperand", "Target")

DefPackage = (AML_PACKAGE_OP, "PkgLength", "NumElements", "PackageElementList")
DefVarPackage = (AML_VAR_PACKAGE_OP, "PkgLength", "VarNumElements", "PackageElementList")
NumElements = ["ByteData"]
VarNumElements = ["TermArg"]
PackageElementList = ("PackageElement*",)
PackageElement = ["DataRefObject", "NameString"]

DefRefOf = (AML_REF_OF_OP, "SuperName")

DefShiftLeft = (AML_SHIFT_LEFT_OP, "Operand", "ShiftCount", "Target")
ShiftCount = ["TermArg"]
DefShiftRight = (AML_SHIFT_RIGHT_OP, "Operand", "ShiftCount", "Target")

DefSizeOf = (AML_SIZE_OF_OP, "SuperName")

DefStore = (AML_STORE_OP, "TermArg", "SuperName")

DefSubtract = (AML_SUBTRACT_OP, "Operand:LeftOperand", "Operand:RightOperand", "Target")

DefTimer = (AML_TIMER_OP,)

DefToBCD = (AML_TO_BCD_OP, "Operand", "Target")

DefToBuffer = (AML_TO_BUFFER_OP, "Operand", "Target")

DefToDecimalString = (AML_TO_DECIMAL_STRING_OP, "Operand", "Target")

DefToHexString = (AML_TO_HEX_STRING_OP, "Operand", "Target")

DefToInteger = (AML_TO_INTEGER_OP, "Operand", "Target")

DefToString = (AML_TO_STRING_OP, "TermArg", "LengthArg", "Target")
LengthArg = ["TermArg"]

DefWait = (AML_WAIT_OP, "EventObject", "Operand")

DefXOr = (AML_XOR_OP, "Operand:LeftOperand", "Operand:RightOperand", "Target")

################################################################################
# 20.2.6 Miscellaneous Objects Encoding
################################################################################

# 20.2.6.1 Arg Objects Encoding
################################################################################

ArgObj = ["Arg0Op", "Arg1Op", "Arg2Op", "Arg3Op", "Arg4Op", "Arg5Op", "Arg6Op"]
Arg0Op = (AML_ARG0_OP,)
Arg1Op = (AML_ARG1_OP,)
Arg2Op = (AML_ARG2_OP,)
Arg3Op = (AML_ARG3_OP,)
Arg4Op = (AML_ARG4_OP,)
Arg5Op = (AML_ARG5_OP,)
Arg6Op = (AML_ARG6_OP,)

# 20.2.6.2 Local Objects Encoding
################################################################################

LocalObj = ["Local0Op", "Local1Op", "Local2Op", "Local3Op", "Local4Op", "Local5Op", "Local6Op", "Local7Op"]
Local0Op = (AML_LOCAL0_OP,)
Local1Op = (AML_LOCAL1_OP,)
Local2Op = (AML_LOCAL2_OP,)
Local3Op = (AML_LOCAL3_OP,)
Local4Op = (AML_LOCAL4_OP,)
Local5Op = (AML_LOCAL5_OP,)
Local6Op = (AML_LOCAL6_OP,)
Local7Op = (AML_LOCAL7_OP,)

# 20.2.6.3 Debug Objects Encoding
################################################################################

DebugObj = (AML_DEBUG_OP,)

################################################################################
# Helper methods
################################################################################

def __get_spec(sym, fn):
    spec = globals()[sym]
    if isinstance(spec, tuple):
        return tuple(map(fn, spec))
    else:
        return spec

def get_definition(sym):
    def __get_definition(elem):
        return elem.split(":")[0] if isinstance(elem, str) else elem
    return __get_spec(sym, __get_definition)

def get_names(sym):
    def __get_names(elem):
        return elem.split(":")[-1] if isinstance(elem, str) else elem
    return __get_spec(sym, __get_names)
