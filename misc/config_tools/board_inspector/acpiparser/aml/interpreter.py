# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

from .context import *
from .datatypes import *
from .tree import Tree, Interpreter
from . import builder

class MethodReturn(Exception):
    """ A pseudo exception to return from a method"""
    def __init__(self):
        pass

class ConcreteInterpreter(Interpreter):
    class Argument(Object):
        def __init__(self, frame, index):
            self.__frame = frame
            self.__index = index

        def get(self):
            return self.__frame.arg_objs[self.__index].get()

        def to_buffer(self):
            return self.__frame.arg_objs[self.__index].to_buffer()

        def to_decimal_string(self):
            return self.__frame.arg_objs[self.__index].to_decimal_string()

        def to_hex_string(self):
            return self.__frame.arg_objs[self.__index].to_hex_string()

        def to_integer(self):
            return self.__frame.arg_objs[self.__index].to_integer()

        def to_string(self):
            return self.__frame.arg_objs[self.__index].to_string()

        def get_obj(self):
            return self.__frame.arg_objs[self.__index]

    class LocalVariable(Object):
        def __init__(self, frame, index):
            self.__frame = frame
            self.__index = index

        def get(self):
            return self.__frame.local_objs[self.__index].get()

        def set(self, obj):
            self.__frame.local_objs[self.__index] = obj

        def to_buffer(self):
            return self.__frame.local_objs[self.__index].to_buffer()

        def to_decimal_string(self):
            return self.__frame.local_objs[self.__index].to_decimal_string()

        def to_hex_string(self):
            return self.__frame.local_objs[self.__index].to_hex_string()

        def to_integer(self):
            return self.__frame.local_objs[self.__index].to_integer()

        def to_string(self):
            return self.__frame.local_objs[self.__index].to_string()

        def get_obj(self):
            return self.__frame.local_objs[self.__index]

    class StackFrame:
        def __init__(self, method, args):
            self.method = method
            self.arg_objs = args + [UninitializedObject()] * (7 - len(args))
            self.local_objs = [UninitializedObject()] * 8
            self.return_value = UninitializedObject()

    def __init__(self, context):
        super().__init__(context)

        self.operation_regions = {}
        self.stack = []

        self.to_break = False
        self.to_continue = False

    def interpret_method_call(self, name, *args):
        stack_depth_before = len(self.stack)
        name_string = builder.NameString(name)
        name_string.scope = self.context.parent(name)

        arg_trees = []
        for arg in args:
            v = builder.build_value(arg)
            if v != None:
                arg_trees.append(v)
            else:
                raise NotImplementedError(f"Unsupported type of method argument: {arg}")

        pseudo_invocation = builder.MethodInvocation(name_string, *arg_trees)
        try:
            val = self.interpret(pseudo_invocation)
        except:
            self.stack = self.stack[:stack_depth_before]
            raise
        assert len(self.stack) == stack_depth_before
        return val

    def dump(self):
        def format_obj(obj):
            if isinstance(obj, UninitializedObject):
                return "None"
            elif isinstance(obj, Object):
                return str(obj.get())
            else:
                return str(obj)
        print("Stack:")
        for idx, frame in enumerate(self.stack):
            arg = ', '.join(map(format_obj, frame.arg_objs))
            local = ', '.join(map(format_obj, frame.local_objs))
            print(f" {idx}@{frame.method}: args: [{arg}] locals: [{local}]")
        print("Binding:")
        self.context.dump_bindings()

    # 20.2.2 Name Objects Encoding
    def NullName(self, tree):
        return None

    def NameString(self, tree):
        name = tree.value
        obj = self.context.lookup_binding(name)
        if not obj:
            sym = self.context.lookup_symbol(name)
            if isinstance(sym, PredefinedMethodDecl):
                obj = PredefinedMethod(sym.fn)
                realpath = self.context.realpath("\\", name)
                self.context.register_binding(realpath, obj)
            else:
                obj = self.interpret(sym.tree)
                self.context.register_binding(name, obj)
        return obj

    # 20.2.3 Data Objects Encoding
    def ByteList(self, tree):
        return RawDataBuffer(tree.value)

    def ByteConst(self, tree):
        return self.interpret(tree.children[0])

    def WordConst(self, tree):
        return self.interpret(tree.children[0])

    def DWordConst(self, tree):
        return self.interpret(tree.children[0])

    def QWordConst(self, tree):
        return self.interpret(tree.children[0])

    def String(self, tree):
        return String(tree.value)

    def ByteData(self, tree):
        return Integer(tree.value)

    def WordData(self, tree):
        return Integer(tree.value)

    def DWordData(self, tree):
        return Integer(tree.value)

    def QWordData(self, tree):
        return Integer(tree.value)

    def ZeroOp(self, tree):
        return Integer(0x00)

    def OneOp(self, tree):
        return Integer(0x01)

    def OnesOp(self, tree):
        return Integer(0xffffffffffffffff)

    # 20.2.5 Term Objects Encoding
    def TermList(self, tree):
        for child in tree.children:
            self.interpret(child)
            if self.to_break or self.to_continue:
                break
        return None

    def MethodInvocation(self, tree):
        self.context.change_scope(tree.children[0].scope)
        value = self.interpret(tree.children[0])
        self.context.pop_scope()

        if isinstance(value, Method):
            # Evaluate arguments
            args = list(map(self.interpret, tree.children[1:]))

            # Switch to the scope of the callee
            realpath = self.context.realpath(value.tree.scope, value.name)
            assert realpath
            self.context.change_scope(realpath)

            # Evaluate the statements of the callee
            self.stack.append(self.StackFrame(realpath, args))
            logging.debug(f"Calling {realpath} with args {args}")
            try:
                self.interpret(value.body)
            except MethodReturn:
                pass
            frame = self.stack.pop()
            self.context.pop_scope()

            # Return the return value of the callee
            return frame.return_value
        elif isinstance(value, PredefinedMethod):
            # Evaluate arguments
            args = list(map(self.interpret, tree.children[1:]))

            # Invoke the predefined function in Python
            return Integer(value.fn(args))
        else:
            assert value == None or isinstance(value, Object), \
                f"{tree.children[0]} evaluates to a non-object value {value}"
            return value

    # 20.2.5.1 Namespace Modifier Objects Encoding
    def DefAlias(self, tree):
        return None

    def DefName(self, tree):
        self.context.change_scope(tree.children[0].scope)
        name = tree.children[0].value
        obj = self.context.lookup_binding(name)
        if not obj:
            obj = self.interpret(tree.children[1])
            self.context.register_binding(name, obj)
        self.context.pop_scope()
        return obj

    # 20.2.5.2 Named Objects Encoding
    def NamedField(self, tree):
        name = tree.children[0].value
        sym = self.context.lookup_symbol(self.context.realpath(tree.scope, name))
        assert isinstance(sym, OperationFieldDecl)
        assert sym.region, f"Field {sym.name} does not belong to any operation region."
        if isinstance(sym.region, str):
            buf = self.context.lookup_operation_region(sym.region)
            if not buf:
                buf = self.interpret(self.context.lookup_symbol(sym.region).tree)
            buf.create_field(name, sym.offset, sym.length, sym.access_width)
            field = BufferField(buf, name)
        elif isinstance(sym.region, tuple):
            index_register = self.interpret(sym.region[0].tree)
            data_register = self.interpret(sym.region[1].tree)
            buf = OperationRegion.open_indexed_region(index_register, data_register)
            buf.create_field(name, sym.offset, sym.length, sym.access_width)
            field = BufferField(buf, name)
        else:
            assert False, f"Cannot interpret the operation region: {sym.region}"
        return field

    def create_field(self, tree, bitwidth, name_idx):
        buf = self.interpret(tree.children[0])
        assert isinstance(buf, Buffer)
        index = self.interpret(tree.children[1]).get()
        name = tree.children[name_idx].value
        if bitwidth == 1 or name_idx == 3:
            buf.create_field(name, index, bitwidth, 8)
        else:
            # bitwidth is 8, 16, 32 or 64 in this case. Reuse it as the access width.
            buf.create_field(name, index * 8, bitwidth, bitwidth)
        obj = BufferField(buf, name)
        self.context.register_binding(name, obj)
        return obj

    def DefCreateBitField(self, tree):
        return self.create_field(tree, 1, 2)

    def DefCreateByteField(self, tree):
        return self.create_field(tree, 8, 2)

    def DefCreateWordField(self, tree):
        return self.create_field(tree, 16, 2)

    def DefCreateDWordField(self, tree):
        return self.create_field(tree, 32, 2)

    def DefCreateQWordField(self, tree):
        return self.create_field(tree, 64, 2)

    def DefCreateField(self, tree):
        numbits = self.interpret(tree.children[2]).get()
        return self.create_field(tree, numbits, 3)

    def DefDevice(self, tree):
        name = tree.children[1].value
        fullpath = self.context.realpath(tree.scope, name)
        sym = self.context.lookup_symbol(fullpath)
        return Device(sym)

    def DefExternal(self, tree):
        logging.debug(f"The loaded tables do not have a definition of {tree.children[0].value}")
        return None

    def DefField(self, tree):
        # Fields of operation regions are evaluated when they are used.
        return None

    def DefMethod(self, tree):
        return Method(tree)

    def DefOpRegion(self, tree):
        name = tree.children[0].value
        sym = self.context.lookup_symbol(self.context.realpath(tree.scope, name))

        space = self.interpret(tree.children[1]).get()
        offset = self.interpret(tree.children[2]).get()
        length = self.interpret(tree.children[3]).get()
        assert isinstance(space, int) and isinstance(offset, int) and (length, int)

        if space == 0x00:    # SystemMemory
            op_region = OperationRegion.open_system_memory(sym.name, offset, length)
        elif space == 0x01:  # SystemIO
            op_region = OperationRegion.open_system_io(sym.name, offset, length)
        elif space == 0x02:  # PCI_Config
            self.context.change_scope(tree.scope)
            device_path = self.context.parent(sym.name)
            bus_id = self.interpret_method_call(f"_BBN").get()
            if self.context.has_symbol(f"{device_path}._ADR"):
                device_id = self.interpret_method_call(f"{device_path}._ADR").get()
            elif self.context.has_symbol(f"{device_path}._BBN"):
                # Device objects representing PCI host bridges may not have an _ADR object
                device_id = 0
            self.context.pop_scope()
            op_region = OperationRegion.open_pci_configuration_space(bus_id, device_id, offset, length)
            pass
        else:
            raise NotImplementedError(f"Cannot load operation region in space {space}")

        self.context.register_operation_region(sym.name, op_region)
        return op_region

    def DefPowerRes(self, tree):
        return PowerResource(tree.NameString.value)

    # 20.2.5.3 Statement Opcodes Encoding
    def DefBreak(self, tree):
        self.to_break = True
        return None

    def DefContinue(self, tree):
        self.to_continue = True
        return None

    def DefElse(self, tree):
        self.interpret(tree.children[1])
        return None

    def DefIfElse(self, tree):
        cond = self.interpret(tree.children[1])
        if cond.get():
            self.interpret(tree.children[2])
        else:
            if len(tree.children) == 4:
                self.interpret(tree.children[3])
        return None

    def DefRelease(self, tree):
        return None

    def DefReturn(self, tree):
        obj = self.interpret(tree.children[0])
        while isinstance(obj, (self.Argument, self.LocalVariable)):
            obj = obj.get_obj()
        self.stack[-1].return_value = obj
        raise MethodReturn()
        return None

    def DefSignal(self, tree):
        # Skip
        return None

    def DefWhile(self, tree):
        while self.interpret(tree.children[1]).get() != 0:
            self.interpret(tree.children[2])
            if self.to_break:
                self.to_break = False
                break
        return None

    # 20.2.5.4 Expression Opcodes Encoding
    def __eval_binary_op(self, tree, op):
        lhs = self.interpret(tree.children[0])
        rhs = self.interpret(tree.children[1])
        # FIXME: The current design of AML parsing, objects are first defined in the namespace and later dropped if they
        # are in a False branch. This leads to incorrect interpretation of the AML code where:
        #
        #   1. A name T is defined in the root scope as an integer.
        #   2. A method M in an inner scope S references T.
        #   3. The name T is defined as a device, power resource or other named objects in scope S under conditions
        #      where M will not be called.
        #
        # As a workaround, check if both the left and right hand sides are integers first. If either is not the case,
        # the condition is evaluated to False.
        try:
            res = Integer(op(lhs.to_integer().get(), rhs.to_integer().get()))
        except NotImplementedError:
            if isinstance(lhs, String) and isinstance(rhs, String):
                res = Integer(op(lhs.get(), rhs.get()))
            else:
                res = Integer(0)
        if len(tree.children) >= 3:
            target = self.interpret(tree.children[2])
            if target:
                target.set(res)
        return res

    def DefAcquire(self, tree):
        # Pretend that the mutex is acquired
        return Integer(0x1)

    def DefAdd(self, tree):
        return self.__eval_binary_op(tree, lambda x,y: x + y)

    def DefAnd(self, tree):
        return self.__eval_binary_op(tree, lambda x,y: x & y)

    def DefBuffer(self, tree):
        size = self.interpret(tree.children[1]).get()
        assert isinstance(size, int)
        data = self.interpret(tree.children[2]).get()
        if len(data) < size:
            data = data + bytes(size - len(data))
        return Buffer(data)

    def DefConcat(self, tree):
        source1 = self.interpret(tree.children[0])
        source2 = self.interpret(tree.children[1])
        if isinstance(source1, Integer):
            data = bytearray()
            data.extend(source1.to_buffer().get())
            data.extend(source2.to_integer().to_buffer().get())
            result = Buffer(data)
        elif isinstance(source1, String):
            data = source1.get()
            data += source2.to_string().get()
            result = String(data)
        elif isinstance(source1, Buffer):
            data = bytearray()
            data.extend(source1.get())
            data.extend(source2.to_buffer().get())
            result = Buffer(data)
        else:
            data = source1.to_string().get() + source2.to_string().get()
            result = String(data)
        target = self.interpret(tree.children[2])
        if target:
            target.set(result)
        return result

    def DefConcatRes(self, tree):
        data = bytearray()
        source1 = self.interpret(tree.children[0])
        buf = source1.to_buffer().get()
        if len(buf) >= 2 and buf[-2] == 0x79:
            data.extend(buf[:-2])
        else:
            data.extend(buf)
        source2 = self.interpret(tree.children[1])
        data.extend(source2.to_buffer().get())
        result = Buffer(data)
        target = self.interpret(tree.children[2])
        if target:
            target.set(result)
        return result

    def DefCondRefOf(self, tree):
        try:
            source = self.interpret(tree.children[0])
            if source is not None:
                target = self.interpret(tree.children[1])
                if target:
                    target.set(ObjectReference(source))
                return Integer(1)
            else:
                return Integer(0)
        except UndefinedSymbol:
            return Integer(0)

    def DefDecrement(self, tree):
        obj = self.interpret(tree.children[0])
        obj.set(Integer(obj.get() - 1))
        return None

    def DefDerefOf(self, tree):
        ref = self.interpret(tree.children[0])
        if isinstance(ref, (self.Argument, self.LocalVariable)):
            ref = ref.get_obj()
        if isinstance(ref, ObjectReference):
            return ref.get()
        else:
            logging.debug(f"Attempt to dereference an object of type {ref.__class__.__name__}")
            return ref

    def DefDivide(self, tree):
        dividend = self.interpret(tree.children[0]).get()
        divisor = self.interpret(tree.children[1]).get()
        if len(tree.children) >= 3:
            remainer = self.interpret(tree.children[2])
            if remainer:
                remainer.set(Integer(dividend % divisor))
        res = Integer(dividend // divisor)
        if len(tree.children) >= 4:
            target = self.interpret(tree.children[3])
            if target:
                target.set(res)
        return res

    def DefIncrement(self, tree):
        obj = self.interpret(tree.children[0])
        obj.set(Integer(obj.get() + 1))
        return None

    def DefIndex(self, tree):
        obj = self.interpret(tree.children[0])
        index = self.interpret(tree.children[1])
        target = self.interpret(tree.children[2])
        ret = ObjectReference(obj.get_obj(), index.get())
        if target:
            target.set(ret)
        return ret

    def DefLAnd(self, tree):
        return self.__eval_binary_op(tree, lambda x,y: 1 if x and y else 0)

    def DefLEqual(self, tree):
        return self.__eval_binary_op(tree, lambda x,y: x == y)

    def DefLGreater(self, tree):
        return self.__eval_binary_op(tree, lambda x,y: x > y)

    def DefLLess(self, tree):
        return self.__eval_binary_op(tree, lambda x,y: x < y)

    def DefLNot(self, tree):
        operand = self.interpret(tree.children[0]).get()
        return Integer(1 if not operand else 0)

    def DefLOr(self, tree):
        return self.__eval_binary_op(tree, lambda x,y: 1 if x or y else 0)

    def __match(self, op, obj, match_obj):
        try:
            if isinstance(match_obj, String):
                return op(obj.to_string().get(), match_obj.get())
            elif isinstance(match_obj, (Integer, BufferField)):
                return op(obj.to_integer().get(), match_obj.get())
            else:
                # Comparison of buffer fields is not implemented yet
                return False
        except NotImplementedError:
            return False

    match_ops = {
        0: lambda x,y: True,    # TRUE
        1: lambda x,y: x == y,  # EQ
        2: lambda x,y: x <= y,  # LE
        3: lambda x,y: x < y,   # LT
        4: lambda x,y: x >= y,  # GE
        5: lambda x,y: x > y,   # GT
    }

    def DefMatch(self, tree):
        pkg = self.interpret(tree.SearchPkg)
        op1 = self.match_ops[tree.MatchOpcode1.value]
        match_obj1 = self.interpret(tree.Operand1)
        op2 = self.match_ops[tree.MatchOpcode2.value]
        match_obj2 = self.interpret(tree.Operand2)
        start_index = self.interpret(tree.StartIndex).get()
        if isinstance(pkg, Package) and isinstance(start_index, int):
            for i in range(start_index, len(pkg.elements)):
                obj = pkg.elements[i]
                if self.__match(op1, obj, match_obj1) and self.__match(op2, obj, match_obj2):
                    return Integer(i)
        return Integer(0xffffffffffffffff) # Ones is 64-bit in DSDT rev 2 and above

    def DefMod(self, tree):
        return self.__eval_binary_op(tree, lambda x,y: x % y)

    def DefMultiply(self, tree):
        return self.__eval_binary_op(tree, lambda x,y: x * y)

    def DefNAnd(self, tree):
        return self.__eval_binary_op(tree, lambda x,y: ~(x & y))

    def DefNOr(self, tree):
        return self.__eval_binary_op(tree, lambda x,y: ~(x | y))

    def DefNot(self, tree):
        operand = self.interpret(tree.children[0])
        target = self.interpret(tree.children[1])
        ret = Integer(~operand.get())
        if target:
            target.set(ret)
        return ret

    def DefOr(self, tree):
        return self.__eval_binary_op(tree, lambda x,y: x | y)

    def DefPackage(self, tree):
        elements = list(map(lambda x: self.interpret(x), tree.children[2].children))
        return Package(elements)

    def DefRefOf(self, tree):
        obj = self.interpret(tree.children[0])
        return ObjectReference(obj)

    def DefShiftLeft(self, tree):
        return self.__eval_binary_op(tree, lambda x,y: x << y)

    def DefShiftRight(self, tree):
        return self.__eval_binary_op(tree, lambda x,y: x >> y)

    def DefSizeOf(self, tree):
        obj = self.interpret(tree.SuperName)
        if isinstance(obj, (self.Argument, self.LocalVariable)):
            obj = obj.get_obj()

        if isinstance(obj, Buffer):
            return Integer(len(obj.get()))
        elif isinstance(obj, String):
            return Integer(len(obj.get()))
        elif isinstance(obj, Package):
            return Integer(len(obj.elements))
        raise NotImplementedError(f"Cannot calculate the size of object of type {obj.__class__.__name__}")

    def DefStore(self, tree):
        obj = self.interpret(tree.children[0])
        target = self.interpret(tree.children[1])
        target.set(obj)
        return None

    def DefSubtract(self, tree):
        return self.__eval_binary_op(tree, lambda x,y: x - y)

    def DefToHexString(self, tree):
        operand = self.interpret(tree.children[0])
        result = operand.to_hex_string()
        target = self.interpret(tree.children[1])
        if target:
            target.set(result)
        return result

    def DefToInteger(self, tree):
        operand = self.interpret(tree.children[0])
        result = operand.to_integer()
        target = self.interpret(tree.children[1])
        if target:
            target.set(result)
        return result

    # 20.2.6.2 Local Objects Encoding
    def Arg0Op(self, tree):
        return self.Argument(self.stack[-1], 0)

    def Arg1Op(self, tree):
        return self.Argument(self.stack[-1], 1)

    def Arg2Op(self, tree):
        return self.Argument(self.stack[-1], 2)

    def Arg3Op(self, tree):
        return self.Argument(self.stack[-1], 3)

    def Arg4Op(self, tree):
        return self.Argument(self.stack[-1], 4)

    def Arg5Op(self, tree):
        return self.Argument(self.stack[-1], 5)

    def Arg6Op(self, tree):
        return self.Argument(self.stack[-1], 6)

    def Local0Op(self, tree):
        return self.LocalVariable(self.stack[-1], 0)

    def Local1Op(self, tree):
        return self.LocalVariable(self.stack[-1], 1)

    def Local2Op(self, tree):
        return self.LocalVariable(self.stack[-1], 2)

    def Local3Op(self, tree):
        return self.LocalVariable(self.stack[-1], 3)

    def Local4Op(self, tree):
        return self.LocalVariable(self.stack[-1], 4)

    def Local5Op(self, tree):
        return self.LocalVariable(self.stack[-1], 5)

    def Local6Op(self, tree):
        return self.LocalVariable(self.stack[-1], 6)

    def Local7Op(self, tree):
        return self.LocalVariable(self.stack[-1], 7)
