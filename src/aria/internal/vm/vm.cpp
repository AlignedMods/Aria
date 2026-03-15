#include "aria/internal/vm/vm.hpp"
#include "aria/context.hpp"

namespace Aria::Internal {

    template <typename T>
    T Add(T lhs, T rhs) { return lhs + rhs; }
    template <typename T>
    T Sub(T lhs, T rhs) { return lhs - rhs; }
    template <typename T>
    T Mul(T lhs, T rhs) { return lhs * rhs; }
    template <typename T>
    T Div(T lhs, T rhs) { return lhs / rhs; }
    template <std::integral T>
    T Mod(T lhs, T rhs) { return lhs % rhs; }
    template <std::floating_point T>
    T Mod(T lhs, T rhs) {
        T r = std::fmod(lhs, rhs);
        if (r < 0) { r += std::abs(rhs); }
        return r;
    }

    template <typename T>
    T And(T lhs, T rhs) { return lhs & rhs; }
    template <typename T>
    T Or(T lhs, T rhs) { return lhs | rhs; }
    template <typename T>
    T Xor(T lhs, T rhs) { return lhs ^ rhs; }

    template <typename T>
    bool Cmp(T lhs, T rhs) { return lhs == rhs; }
    template <typename T>
    bool Ncmp(T lhs, T rhs) { return lhs != rhs; }
    template <typename T>
    bool Lt(T lhs, T rhs) { return lhs < rhs; }
    template <typename T>
    bool Lte(T lhs, T rhs) { return lhs <= rhs; }
    template <typename T>
    bool Gt(T lhs, T rhs) { return lhs > rhs; }
    template <typename T>
    bool Gte(T lhs, T rhs) { return lhs >= rhs; }

    VM::VM(Context* ctx) {
        m_LocalStack.   Reserve(32 * 1024, 2048);
        m_FunctionStack.Reserve(4 * 1024, 256);
        m_GlobalStack.  Reserve(16 * 1024, 1024);

        m_Context = ctx;
    }

    void VM::Alloca(size_t size, TypeInfo* type, Stack& stack) {
        size_t alignedSize = ((size + 8 - 1) / 8) * 8; // We need to handle 8 byte alignment since some CPU's will require it
        
        ARIA_ASSERT(alignedSize % 8 == 0, "Memory not aligned to 8 bytes correctly!");
        ARIA_ASSERT(stack.StackPointer + alignedSize < stack.Stack.max_size(), "Local stack overflow, allocating an insane amount of memory!");

        stack.StackPointer += alignedSize;

        // Add more stack slots if needed
        if (stack.StackSlotPointer + 1 >= stack.StackSlots.size()) {
            stack.StackSlots.resize(stack.StackSlots.size() * 2);
        }
        
        stack.StackSlots[stack.StackSlotPointer++] = { stack.StackPointer - alignedSize, size };
    }

    void VM::Pop(size_t count, Stack& stack) {
        size_t sp = stack.StackPointer;
        stack.StackPointer = stack.StackSlots[stack.StackSlotPointer - count].Index;
        stack.StackSlotPointer -= count;

        // Fill the space we just popped with zeros (useful for debugging, however a bit of a waste in release builds)
        #ifdef _DEBUG
            memset(&stack.Stack[stack.StackPointer], 0, sp - stack.StackPointer);
        #endif
    }

    void VM::Copy(i32 dstSlot, i32 srcSlot, Stack& dst, Stack& src) {
        VMSlice dstSlice = GetVMSlice(dstSlot, dst);
        VMSlice srcSlice = GetVMSlice(srcSlot, src);

        ARIA_ASSERT(dstSlice.Size == srcSlice.Size, "Invalid VM::Copy() call, sizes of both slots must be the same!");

        memcpy(dstSlice.Memory, srcSlice.Memory, srcSlice.Size);
    }

    void VM::Dup(i32 slot, Stack& dst, Stack& src) {
        VMSlice srcSlot = GetVMSlice(slot, src);

        Alloca(srcSlot.Size, srcSlot.ResolvedType, dst);
        VMSlice dstSlot = GetVMSlice(-1, dst);
        
        memcpy(dstSlot.Memory, srcSlot.Memory, srcSlot.Size);
    }

    void VM::PushStackFrame() {
        StackFrame newStackFrame;
        newStackFrame.PLSSBP = m_LocalStack.StackSlotBasePointer;
        newStackFrame.PLSBP = m_LocalStack.StackBasePointer;
        newStackFrame.PreviousFunction = m_ActiveFunction;

        // Save the state of the local stack (function stack gets handled with call/ret)
        m_LocalStack.StackSlotBasePointer = m_LocalStack.StackSlotPointer;
        m_LocalStack.StackBasePointer = m_LocalStack.StackPointer;

        m_StackFrames.push_back(newStackFrame);
    }

    void VM::PopStackFrame() {
        ARIA_ASSERT(m_StackFrames.size() > 0, "Calling VM::PopStackFrame() with no active stack frame!");

        // Restore the state of the local stack
        m_LocalStack.StackSlotPointer = m_LocalStack.StackSlotBasePointer;
        m_LocalStack.StackPointer = m_LocalStack.StackBasePointer;

        m_LocalStack.StackSlotBasePointer = m_StackFrames.back().PLSSBP;
        m_LocalStack.StackBasePointer = m_StackFrames.back().PLSBP;

        m_StackFrames.pop_back();
    }

    void VM::AddExtern(const std::string& signature, ExternFn fn) {
        m_ExternalFunctions[signature] = fn;
    }

    void VM::Call(const std::string& signature, size_t argCount) {
        // Now check for aria functions
        ARIA_ASSERT(m_Functions.contains(signature), "Calling unknown function");
        VMFunction& func = m_Functions.at(signature);
        
        // Save the state in the current stack frame
        m_StackFrames.back().PreviousReturnAddress = m_ReturnAddress;
        m_StackFrames.back().PreviousFunction = m_ActiveFunction;

        // Save function stack
        m_StackFrames.back().PFSSBP = m_FunctionStack.StackSlotBasePointer;
        m_StackFrames.back().PFSBP = m_FunctionStack.StackBasePointer;

        m_ReturnAddress = m_ProgramCounter;

        // Perform a jump to the function
        ARIA_ASSERT(func.Labels.contains("_entry$"), "All functions must contain a \"_entry$\" label");
        m_ProgramCounter = func.Labels.at("_entry$");
        m_ActiveFunction = &func;

        // Set up the function stack
        m_FunctionStack.StackSlotBasePointer = m_FunctionStack.StackSlotPointer - argCount;
        m_FunctionStack.StackBasePointer = m_FunctionStack.StackSlots[m_FunctionStack.StackSlotBasePointer].Index;

        Run();
    }

    void VM::CallExtern(const std::string& signature) {
        ARIA_ASSERT(m_ExternalFunctions.contains(signature), "Calling CallExtern() on a non-existent extern function!");

        // Do the call
        m_ExternalFunctions.at(signature)(m_Context);

        // Cleanup the stack
        m_FunctionStack.StackSlotPointer = m_FunctionStack.StackSlotBasePointer;
        m_FunctionStack.StackPointer = m_FunctionStack.StackBasePointer;
    }

    void VM::StoreBool(i32 slot, bool b, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);
        ARIA_ASSERT(s.Size == 1, "Cannot store a bool in a slot with a size that isn't 1!");
        int8_t bb = static_cast<int8_t>(b);
        memcpy(s.Memory, &bb, 1);
    }

    void VM::StoreChar(i32 slot, int8_t c, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);
        ARIA_ASSERT(s.Size == 1, "Cannot store a char in a slot with a size that isn't 1!");
        memcpy(s.Memory, &c, 1);
    }

    void VM::StoreShort(i32 slot, int16_t sh, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);
        ARIA_ASSERT(s.Size == 2, "Cannot store a short in a slot with a size that isn't 2!");
        memcpy(s.Memory, &sh, 2);
    }

    void VM::StoreInt(i32 slot, int32_t i, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);
        ARIA_ASSERT(s.Size == 4, "Cannot store an int in a slot with a size that isn't 4!");
        memcpy(s.Memory, &i, 4);
    }

    void VM::StoreLong(i32 slot, int64_t l, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);
        ARIA_ASSERT(s.Size == 8, "Cannot store a long in a slot with a size that isn't 8!");
        memcpy(s.Memory, &l, 8);
    }

    void VM::StoreSize(i32 slot, size_t sz, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);
        ARIA_ASSERT(s.Size == sizeof(size_t), "Cannot store a size_t in a slot with a size that isn't sizeof(size_t)!");
        memcpy(s.Memory, &sz, sizeof(size_t));
    }

    void VM::StoreFloat(i32 slot, float f, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);
        ARIA_ASSERT(s.Size == 4, "Cannot store a float in a slot with a size that isn't 4!");
        memcpy(s.Memory, &f, 4);
    }

    void VM::StoreDouble(i32 slot, double d, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);
        ARIA_ASSERT(s.Size == 8, "Cannot store a double in a slot with a size that isn't 8!");
        memcpy(s.Memory, &d, 8);
    }

    void VM::StorePointer(i32 slot, void* p, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);
        ARIA_ASSERT(s.Size == sizeof(void*), "Cannot store a double in a slot with a size that isn't sizeof(void*)!");
        memcpy(s.Memory, &p, sizeof(void*));
    }

    bool VM::GetBool(i32 slot, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);

        ARIA_ASSERT(s.Size == 1, "Invalid GetBool() call!");

        bool b = false;
        memcpy(&b, s.Memory, 1);
        return b;
    }

    int8_t VM::GetChar(i32 slot, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);

        ARIA_ASSERT(s.Size == 1, "Invalid GetChar() call!");

        int8_t c = 0;
        memcpy(&c, s.Memory, 1);
        return c;
    }

    int16_t VM::GetShort(i32 slot, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);

        ARIA_ASSERT(s.Size == 2, "Invalid GetShort() call!");

        int16_t sh = 0;
        memcpy(&sh, s.Memory, 2);
        return sh;
    }

    int32_t VM::GetInt(i32 slot, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);

        ARIA_ASSERT(s.Size == 4, "Invalid GetInt() call!");

        int32_t i = 0;
        memcpy(&i, s.Memory, 4);
        return i;
    }

    int64_t VM::GetLong(i32 slot, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);

        ARIA_ASSERT(s.Size == 8, "Invalid GetLong() call!");

        int64_t l = 0;
        memcpy(&l, s.Memory, 8);
        return l;
    }

    size_t VM::GetSize(i32 slot, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);

        ARIA_ASSERT(s.Size == sizeof(size_t), "Invalid GetLong() call!");

        size_t sz = 0;
        memcpy(&sz, s.Memory, sizeof(size_t));
        return sz;
    }

    float VM::GetFloat(i32 slot, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);

        ARIA_ASSERT(s.Size == 4, "Invalid GetFloat() call!");

        float f = 0;
        memcpy(&f, s.Memory, 4);
        return f;
    }

    double VM::GetDouble(i32 slot, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);

        ARIA_ASSERT(s.Size == 8, "Invalid GetDouble() call!");

        double d = 0;
        memcpy(&d, s.Memory, 8);
        return d;
    }

    void* VM::GetPointer(i32 slot, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);

        ARIA_ASSERT(s.Size == sizeof(void*), "Invalid GetPointer() call!");

        void* p = nullptr;
        memcpy(&p, s.Memory, sizeof(void*));
        return p;
    }

    void VM::RunByteCode(const OpCode* data, size_t count) {
        m_Program = data;
        m_ProgramSize = count;

        RunPrepass();

        const std::string& signature = "_start$()";

        ARIA_ASSERT(m_Functions.contains(signature), "Byte code does not contain _start$() function");
        VMFunction& func = m_Functions.at(signature);
        m_ActiveFunction = &func;

        // Perform a jump to the function
        ARIA_ASSERT(func.Labels.contains("_entry$"), "_start$() function doesn't contain a \"_entry$\" label");
        m_ProgramCounter = func.Labels.at("_entry$");
        Run();
    }

    void VM::Run() {
        #define CASE_LOAD(_enum, builtInType) case OpCodeType::_enum: { \
            OpCodeLoad l = std::get<OpCodeLoad>(op.Data); \
            Alloca(sizeof(builtInType), l.ResolvedType, m_LocalStack); \
            memcpy(GetVMSlice(-1, m_LocalStack).Memory, &std::get<builtInType>(l.Data), sizeof(builtInType)); \
            break; \
        }

        #define CASE_UNARYEXPR(_enum, builtinType, builtinOp) case OpCodeType::_enum: { \
            VMSlice s = GetVMSlice(-1, m_LocalStack); \
            builtinType value{}; \
            memcpy(&value, s.Memory, sizeof(builtinType)); \
            builtinType result = builtinOp(value); \
            Pop(1, m_LocalStack); \
            Alloca(sizeof(builtinType), s.ResolvedType, m_LocalStack); \
            VMSlice newSlot = GetVMSlice(-1, m_LocalStack); \
            memcpy(newSlot.Memory, &result, sizeof(builtinType)); \
            break; \
        }

        #define CASE_UNARYEXPR_GROUP(unaryop, op) \
            CASE_UNARYEXPR(unaryop##I8,  int8_t,   op) \
            CASE_UNARYEXPR(unaryop##I16, int16_t,  op) \
            CASE_UNARYEXPR(unaryop##I32, int32_t,  op) \
            CASE_UNARYEXPR(unaryop##I64, int64_t,  op) \
            CASE_UNARYEXPR(unaryop##U8,  uint8_t,  op) \
            CASE_UNARYEXPR(unaryop##U16, uint16_t, op) \
            CASE_UNARYEXPR(unaryop##U32, uint32_t, op) \
            CASE_UNARYEXPR(unaryop##U64, uint64_t, op) \
            CASE_UNARYEXPR(unaryop##F32, float,    op) \
            CASE_UNARYEXPR(unaryop##F64, double,   op)

        #define CASE_BINEXPR(_enum, builtinType, builtinOp) case OpCodeType::_enum: { \
            OpCodeMath m = std::get<OpCodeMath>(op.Data); \
            builtinType lhs{}; \
            builtinType rhs{}; \
            memcpy(&lhs, GetVMSlice(-2, m_LocalStack).Memory, sizeof(builtinType)); \
            memcpy(&rhs, GetVMSlice(-1, m_LocalStack).Memory, sizeof(builtinType)); \
            builtinType result = builtinOp(lhs, rhs); \
            Pop(2, m_LocalStack); \
            Alloca(sizeof(builtinType), m.ResolvedType, m_LocalStack); \
            VMSlice s = GetVMSlice(-1, m_LocalStack); \
            memcpy(s.Memory, &result, sizeof(builtinType)); \
            break; \
        }

        #define CASE_BINEXPR_BOOL(_enum, builtinType, builtinOp) case OpCodeType::_enum: { \
            OpCodeMath m = std::get<OpCodeMath>(op.Data); \
            builtinType lhs{}; \
            builtinType rhs{}; \
            memcpy(&lhs, GetVMSlice(-2, m_LocalStack).Memory, sizeof(builtinType)); \
            memcpy(&rhs, GetVMSlice(-1, m_LocalStack).Memory, sizeof(builtinType)); \
            bool result = builtinOp(lhs, rhs); \
            Pop(2, m_LocalStack); \
            Alloca(1, m.ResolvedType, m_LocalStack); \
            StoreBool(-1, result, m_LocalStack); \
            break; \
        }

        #define CASE_BINEXPR_GROUP(mathop, op) \
            CASE_BINEXPR(mathop##I8,  int8_t,   op) \
            CASE_BINEXPR(mathop##I16, int16_t,  op) \
            CASE_BINEXPR(mathop##I32, int32_t,  op) \
            CASE_BINEXPR(mathop##I64, int64_t,  op) \
            CASE_BINEXPR(mathop##U8,  uint8_t,  op) \
            CASE_BINEXPR(mathop##U16, uint16_t, op) \
            CASE_BINEXPR(mathop##U32, uint32_t, op) \
            CASE_BINEXPR(mathop##U64, uint64_t, op) \
            CASE_BINEXPR(mathop##F32, float,    op) \
            CASE_BINEXPR(mathop##F64, double,   op)

        #define CASE_BINEXPR_INTEGRAL_GROUP(mathop, op) \
            CASE_BINEXPR(mathop##I8,  int8_t,   op) \
            CASE_BINEXPR(mathop##I16, int16_t,  op) \
            CASE_BINEXPR(mathop##I32, int32_t,  op) \
            CASE_BINEXPR(mathop##I64, int64_t,  op) \
            CASE_BINEXPR(mathop##U8,  uint8_t,  op) \
            CASE_BINEXPR(mathop##U16, uint16_t, op) \
            CASE_BINEXPR(mathop##U32, uint32_t, op) \
            CASE_BINEXPR(mathop##U64, uint64_t, op)

        #define CASE_BINEXPR_BOOL_GROUP(mathop, op) \
            CASE_BINEXPR_BOOL(mathop##I8,  int8_t,   op) \
            CASE_BINEXPR_BOOL(mathop##I16, int16_t,  op) \
            CASE_BINEXPR_BOOL(mathop##I32, int32_t,  op) \
            CASE_BINEXPR_BOOL(mathop##I64, int64_t,  op) \
            CASE_BINEXPR_BOOL(mathop##U8,  uint8_t,  op) \
            CASE_BINEXPR_BOOL(mathop##U16, uint16_t, op) \
            CASE_BINEXPR_BOOL(mathop##U32, uint32_t, op) \
            CASE_BINEXPR_BOOL(mathop##U64, uint64_t, op) \
            CASE_BINEXPR_BOOL(mathop##F32, float,    op) \
            CASE_BINEXPR_BOOL(mathop##F64, double,   op)

        #define CASE_CAST(_enum, sourceType, destType) case OpCodeType::_enum: { \
            OpCodeCast cast = std::get<OpCodeCast>(op.Data); \
            VMSlice s = GetVMSlice(-1, m_LocalStack); \
            sourceType t{}; \
            memcpy(&t, s.Memory, sizeof(sourceType)); \
            destType d = static_cast<destType>(t); \
            Pop(1, m_LocalStack); \
            Alloca(sizeof(destType), cast.ResolvedType, m_LocalStack); \
            VMSlice __a = GetVMSlice(-1, m_LocalStack); \
            memcpy(__a.Memory, &d, sizeof(destType)); \
            break; \
        }

        #define CASE_CAST_GROUP(_cast, _builtinType) \
            CASE_CAST(Cast##_cast##ToI8,  _builtinType, int8_t) \
            CASE_CAST(Cast##_cast##ToI16, _builtinType, int16_t) \
            CASE_CAST(Cast##_cast##ToI32, _builtinType, int32_t) \
            CASE_CAST(Cast##_cast##ToI64, _builtinType, int64_t) \
            CASE_CAST(Cast##_cast##ToU8,  _builtinType, uint8_t) \
            CASE_CAST(Cast##_cast##ToU16, _builtinType, uint16_t) \
            CASE_CAST(Cast##_cast##ToU32, _builtinType, uint32_t) \
            CASE_CAST(Cast##_cast##ToU64, _builtinType, uint64_t) \
            CASE_CAST(Cast##_cast##ToF32, _builtinType, float) \
            CASE_CAST(Cast##_cast##ToF64, _builtinType, double)

        for (; m_ProgramCounter < m_ProgramSize; m_ProgramCounter++) {
            const OpCode& op = m_Program[m_ProgramCounter];

            switch (op.Type) {
                case OpCodeType::Nop: { continue; }

                case OpCodeType::Alloca: {
                    OpCodeAlloca alloca = std::get<OpCodeAlloca>(op.Data);

                    Alloca(alloca.Size, alloca.ResolvedType, m_LocalStack);
                    break;
                }

                case OpCodeType::Store: {
                    void* dst = GetPointer(-2, m_LocalStack);
                    VMSlice src = GetVMSlice(-1, m_LocalStack);

                    memcpy(dst, src.Memory, src.Size);
                    Pop(2, m_LocalStack);
                    break;
                }

                case OpCodeType::Dup: {
                    Dup(-1, m_LocalStack, m_LocalStack);
                    break;
                }

                case OpCodeType::PushSF: {
                    PushStackFrame();
                    break;
                }

                case OpCodeType::PopSF: {
                    PopStackFrame();
                    break;
                }

                CASE_LOAD(LoadI8,  i8)
                CASE_LOAD(LoadI16, i16)
                CASE_LOAD(LoadI32, i32)
                CASE_LOAD(LoadI64, i64)

                CASE_LOAD(LoadU8,  u8)
                CASE_LOAD(LoadU16, u16)
                CASE_LOAD(LoadU32, u32)
                CASE_LOAD(LoadU64, u64)
                                      
                CASE_LOAD(LoadF32, f32)
                CASE_LOAD(LoadF64, f64)
                case OpCodeType::LoadStr: {
                    OpCodeLoad l = std::get<OpCodeLoad>(op.Data);
                    StringView str = std::get<StringView>(l.Data);
                    Alloca(str.Size(), l.ResolvedType, m_LocalStack);
                    memcpy(GetVMSlice(-1, m_LocalStack).Memory, str.Data(), str.Size());
                    break;
                }

                case OpCodeType::DeclareGlobal: {
                    const std::string& g = std::get<std::string>(op.Data);

                    VMSlice src = GetVMSlice(-1, m_LocalStack);

                    Alloca(src.Size, src.ResolvedType, m_GlobalStack);
                    VMSlice dst = GetVMSlice(-1, m_GlobalStack);

                    memcpy(dst.Memory, src.Memory, src.Size);
                    Pop(1, m_LocalStack); // Pop the local stack slot which we just copied

                    m_GlobalMap[g] = { static_cast<i32>(m_GlobalStack.StackSlotPointer) - 1 };
                    break;
                };

                case OpCodeType::DeclareLocal: {
                    size_t index = std::get<size_t>(op.Data);
                    m_StackFrames.back().LocalMap[index] = { static_cast<i32>(m_LocalStack.StackSlotPointer - m_LocalStack.StackSlotBasePointer) - 1 };
                    break;
                };

                case OpCodeType::DeclareArg: {
                    size_t index = std::get<size_t>(op.Data);

                    VMSlice src = GetVMSlice(-1, m_LocalStack);

                    Alloca(src.Size, src.ResolvedType, m_FunctionStack);
                    VMSlice dst = GetVMSlice(-1, m_FunctionStack);

                    memcpy(dst.Memory, src.Memory, src.Size);
                    Pop(1, m_LocalStack); // Pop the local stack slot which we just copied
                    break;
                };

                case OpCodeType::LoadGlobal: {
                    const std::string& g = std::get<std::string>(op.Data);
                    Dup(m_GlobalMap.at(g), m_LocalStack, m_GlobalStack);
                    break;
                }

                case OpCodeType::LoadLocal: {
                    size_t index = std::get<size_t>(op.Data);
                    Dup(m_StackFrames.back().LocalMap.at(index), m_LocalStack, m_LocalStack);
                    break;
                }

                case OpCodeType::LoadArg: {
                    i32 index = static_cast<i32>(std::get<size_t>(op.Data));

                    Dup(index + 1, m_LocalStack, m_FunctionStack);
                    break;
                }

                case OpCodeType::LoadFunc: {
                    const std::string& fn = std::get<std::string>(op.Data);
                    
                    Alloca(sizeof(void*), nullptr, m_FunctionStack);
                    StorePointer(-1, const_cast<char*>(fn.c_str()), m_FunctionStack);
                    break;
                }

                case OpCodeType::LoadPtrGlobal: {
                    const std::string& g = std::get<std::string>(op.Data);
                    VMSlice slice = GetVMSlice(m_GlobalMap.at(g), m_GlobalStack);
                    Alloca(sizeof(void*), slice.ResolvedType, m_LocalStack);
                    StorePointer(-1, slice.Memory, m_LocalStack);
                    break;
                }

                case OpCodeType::LoadPtrLocal: {
                    size_t index = std::get<size_t>(op.Data);
                    VMSlice slice = GetVMSlice(m_StackFrames.back().LocalMap.at(index), m_LocalStack);
                    Alloca(sizeof(void*), slice.ResolvedType, m_LocalStack);
                    StorePointer(-1, slice.Memory, m_LocalStack);
                    break;
                }

                case OpCodeType::LoadPtrRet: {
                    VMSlice slice = GetVMSlice(-(static_cast<i32>(m_LocalStack.StackSlotPointer - m_LocalStack.StackSlotBasePointer) + 1), m_LocalStack);
                    Alloca(sizeof(void*), nullptr, m_LocalStack);
                    StorePointer(-1, slice.Memory, m_LocalStack);
                    break;
                }

                case OpCodeType::Function: ARIA_ASSERT(false, "VM should never reach a function op code!"); break;
                case OpCodeType::Label: break; // We just keep going

                case OpCodeType::Jmp: {
                    const std::string& label = std::get<std::string>(op.Data);

                    ARIA_ASSERT(m_ActiveFunction->Labels.contains(label), "Trying to jump to an unknown label!");
                    m_ProgramCounter = m_ActiveFunction->Labels.at(label);

                    break;
                }

                case OpCodeType::Jt: {
                    const std::string& label = std::get<std::string>(op.Data);

                    if (GetBool(-1, m_LocalStack) == true) {
                        ARIA_ASSERT(m_ActiveFunction->Labels.contains(label), "Trying to jump to an unknown label!");
                        m_ProgramCounter = m_ActiveFunction->Labels.at(label);
                    }

                    break;
                }

                case OpCodeType::Jf: {
                    const std::string& label = std::get<std::string>(op.Data);

                    if (GetBool(-1, m_LocalStack) == false) {
                        ARIA_ASSERT(m_ActiveFunction->Labels.contains(label), "Trying to jump to an unknown label!");
                        m_ProgramCounter = m_ActiveFunction->Labels.at(label);
                    }

                    break;
                }

                case OpCodeType::Call: {
                    const OpCodeCall& call = std::get<OpCodeCall>(op.Data);
                    
                    std::string sig = reinterpret_cast<char*>(GetPointer(-(static_cast<i32>(call.ArgCount) + 1), m_FunctionStack));
                    
                    // Check if we have an external function
                    if (m_ExternalFunctions.contains(sig)) {
                        CallExtern(sig);
                        break;
                    }
                    
                    // Now check for aria functions
                    ARIA_ASSERT(m_Functions.contains(sig), "Calling unknown function");
                    VMFunction& func = m_Functions.at(sig);
                    
                    // Save the state in the current stack frame
                    m_StackFrames.back().PreviousReturnAddress = m_ReturnAddress;
                    m_StackFrames.back().PreviousFunction = m_ActiveFunction;

                    // Save function stack
                    m_StackFrames.back().PFSSBP = m_FunctionStack.StackSlotBasePointer;
                    m_StackFrames.back().PFSBP = m_FunctionStack.StackBasePointer;

                    m_ReturnAddress = m_ProgramCounter;

                    // Perform a jump to the function
                    ARIA_ASSERT(func.Labels.contains("_entry$"), "All functions must contain a \"_entry$\" label");
                    m_ProgramCounter = func.Labels.at("_entry$");
                    m_ActiveFunction = &func;

                    // Set up the function stack
                    m_FunctionStack.StackSlotBasePointer = m_FunctionStack.StackSlotPointer - call.ArgCount - 1;
                    m_FunctionStack.StackBasePointer = m_FunctionStack.StackSlots[m_FunctionStack.StackSlotBasePointer].Index;

                    break;
                }

                case OpCodeType::Ret: {
                    ARIA_ASSERT(m_StackFrames.size() > 0, "Trying to return out of no stack frame!");

                    m_FunctionStack.StackSlotPointer = m_FunctionStack.StackSlotBasePointer;
                    m_FunctionStack.StackPointer = m_FunctionStack.StackBasePointer;

                    m_FunctionStack.StackSlotBasePointer = m_StackFrames.back().PFSSBP;
                    m_FunctionStack.StackBasePointer = m_StackFrames.back().PFSBP;

                    if (m_ReturnAddress == SIZE_MAX) {
                        StopExecution();
                    } else {
                        m_ProgramCounter = m_ReturnAddress;
                    }

                    m_ReturnAddress = m_StackFrames.back().PreviousReturnAddress;
                    m_ActiveFunction = m_StackFrames.back().PreviousFunction;
                    break;
                }

                CASE_UNARYEXPR_GROUP(Neg, -);

                CASE_BINEXPR_GROUP(Add, Add)
                CASE_BINEXPR_GROUP(Sub, Sub)
                CASE_BINEXPR_GROUP(Mul, Mul)
                CASE_BINEXPR_GROUP(Div, Div)
                CASE_BINEXPR_GROUP(Mod, Mod)

                CASE_BINEXPR_INTEGRAL_GROUP(And, And)
                CASE_BINEXPR_INTEGRAL_GROUP(Or, Or)
                CASE_BINEXPR_INTEGRAL_GROUP(Xor, Xor)

                CASE_BINEXPR_BOOL_GROUP(Cmp, Cmp)
                CASE_BINEXPR_BOOL_GROUP(Ncmp, Ncmp)
                case OpCodeType::LtI8: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); int8_t lhs{}; int8_t rhs{}; memcpy(&lhs, GetVMSlice(-2, m_LocalStack).Memory, sizeof(int8_t)); memcpy(&rhs, GetVMSlice(-1, m_LocalStack).Memory, sizeof(int8_t)); bool result = Lt(lhs, rhs); Pop(2, m_LocalStack); Alloca(1, m.ResolvedType, m_LocalStack); StoreBool(-1, result, m_LocalStack); break;
                } case OpCodeType::LtI16: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); int16_t lhs{}; int16_t rhs{}; memcpy(&lhs, GetVMSlice(-2, m_LocalStack).Memory, sizeof(int16_t)); memcpy(&rhs, GetVMSlice(-1, m_LocalStack).Memory, sizeof(int16_t)); bool result = Lt(lhs, rhs); Pop(2, m_LocalStack); Alloca(1, m.ResolvedType, m_LocalStack); StoreBool(-1, result, m_LocalStack); break;
                } case OpCodeType::LtI32: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); 
                    int32_t lhs{}; 
                    int32_t rhs{}; 
                    memcpy(&lhs, GetVMSlice(-2, m_LocalStack).Memory, sizeof(int32_t)); 
                    memcpy(&rhs, GetVMSlice(-1, m_LocalStack).Memory, sizeof(int32_t)); 
                    bool result = Lt(lhs, rhs); 
                    Pop(2, m_LocalStack); 
                    Alloca(1, m.ResolvedType, m_LocalStack); 
                    StoreBool(-1, result, m_LocalStack); 
                    break;
                } case OpCodeType::LtI64: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); int64_t lhs{}; int64_t rhs{}; memcpy(&lhs, GetVMSlice(-2, m_LocalStack).Memory, sizeof(int64_t)); memcpy(&rhs, GetVMSlice(-1, m_LocalStack).Memory, sizeof(int64_t)); bool result = Lt(lhs, rhs); Pop(2, m_LocalStack); Alloca(1, m.ResolvedType, m_LocalStack); StoreBool(-1, result, m_LocalStack); break;
                } case OpCodeType::LtU8: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); uint8_t lhs{}; uint8_t rhs{}; memcpy(&lhs, GetVMSlice(-2, m_LocalStack).Memory, sizeof(uint8_t)); memcpy(&rhs, GetVMSlice(-1, m_LocalStack).Memory, sizeof(uint8_t)); bool result = Lt(lhs, rhs); Pop(2, m_LocalStack); Alloca(1, m.ResolvedType, m_LocalStack); StoreBool(-1, result, m_LocalStack); break;
                } case OpCodeType::LtU16: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); uint16_t lhs{}; uint16_t rhs{}; memcpy(&lhs, GetVMSlice(-2, m_LocalStack).Memory, sizeof(uint16_t)); memcpy(&rhs, GetVMSlice(-1, m_LocalStack).Memory, sizeof(uint16_t)); bool result = Lt(lhs, rhs); Pop(2, m_LocalStack); Alloca(1, m.ResolvedType, m_LocalStack); StoreBool(-1, result, m_LocalStack); break;
                } case OpCodeType::LtU32: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); uint32_t lhs{}; uint32_t rhs{}; memcpy(&lhs, GetVMSlice(-2, m_LocalStack).Memory, sizeof(uint32_t)); memcpy(&rhs, GetVMSlice(-1, m_LocalStack).Memory, sizeof(uint32_t)); bool result = Lt(lhs, rhs); Pop(2, m_LocalStack); Alloca(1, m.ResolvedType, m_LocalStack); StoreBool(-1, result, m_LocalStack); break;
                } case OpCodeType::LtU64: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); uint64_t lhs{}; uint64_t rhs{}; memcpy(&lhs, GetVMSlice(-2, m_LocalStack).Memory, sizeof(uint64_t)); memcpy(&rhs, GetVMSlice(-1, m_LocalStack).Memory, sizeof(uint64_t)); bool result = Lt(lhs, rhs); Pop(2, m_LocalStack); Alloca(1, m.ResolvedType, m_LocalStack); StoreBool(-1, result, m_LocalStack); break;
                } case OpCodeType::LtF32: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); float lhs{}; float rhs{}; memcpy(&lhs, GetVMSlice(-2, m_LocalStack).Memory, sizeof(float)); memcpy(&rhs, GetVMSlice(-1, m_LocalStack).Memory, sizeof(float)); bool result = Lt(lhs, rhs); Pop(2, m_LocalStack); Alloca(1, m.ResolvedType, m_LocalStack); StoreBool(-1, result, m_LocalStack); break;
                } case OpCodeType::LtF64: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); double lhs{}; double rhs{}; memcpy(&lhs, GetVMSlice(-2, m_LocalStack).Memory, sizeof(double)); memcpy(&rhs, GetVMSlice(-1, m_LocalStack).Memory, sizeof(double)); bool result = Lt(lhs, rhs); Pop(2, m_LocalStack); Alloca(1, m.ResolvedType, m_LocalStack); StoreBool(-1, result, m_LocalStack); break;
                }
                CASE_BINEXPR_BOOL_GROUP(Lte, Lte)
                CASE_BINEXPR_BOOL_GROUP(Gt, Gt)
                CASE_BINEXPR_BOOL_GROUP(Gte, Gte)

                CASE_CAST_GROUP(I8,  int8_t)
                CASE_CAST_GROUP(I16, int16_t)
                CASE_CAST_GROUP(I32, int32_t)
                CASE_CAST_GROUP(I64, int64_t)
                CASE_CAST_GROUP(U8,  uint8_t)
                CASE_CAST_GROUP(U16, uint16_t)
                CASE_CAST_GROUP(U32, uint32_t)
                CASE_CAST_GROUP(U64, uint64_t)
                CASE_CAST_GROUP(F32, float)
                CASE_CAST_GROUP(F64, double)

                case OpCodeType::Comment: continue;
            }
        }

        #undef CASE_UNARYEXPR
        #undef CASE_UNARYEXPR_GROUP
        #undef CASE_BINEXPR
        #undef CASE_BINEXPR_GROUP
        #undef CASE_CAST
        #undef CASE_CAST_GROUP
    }
    
    VMSlice VM::GetVMSlice(i32 index, Stack& stack) {
        StackSlot slot;

        if (index < 0) {
            slot = stack.StackSlots[stack.StackSlotPointer + index];
        } else {
            slot = stack.StackSlots[stack.StackSlotBasePointer + index];
        }

        return VMSlice(&stack.Stack[slot.Index], slot.Size);
    }

    void VM::StopExecution() {
        m_ProgramCounter = m_ProgramSize;
    }

    void VM::RunPrepass() {
        for (; m_ProgramCounter < m_ProgramSize; m_ProgramCounter++) {
            const OpCode& op = m_Program[m_ProgramCounter];

            if (op.Type == OpCodeType::Function) {
                size_t startPc = m_ProgramCounter;
                m_ProgramCounter++;

                const std::string& ident = std::get<std::string>(op.Data);
                VMFunction func;
                
                for (; m_ProgramCounter < m_ProgramSize; m_ProgramCounter++) {
                    const OpCode& op = m_Program[m_ProgramCounter];

                    if (op.Type == OpCodeType::Label) {
                        std::string label = std::get<std::string>(op.Data);
                        func.Labels[label] = m_ProgramCounter;
                    } else if (op.Type == OpCodeType::Function) {
                        break;
                    }
                }

                m_ProgramCounter = startPc;
                m_Functions[ident] = func;
            }
        }

        m_ProgramCounter = 0; // Reset the program counter so the normal execution happens from the start
    }

} // namespace Aria::Internal