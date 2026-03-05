#include "black_lua/internal/vm/vm.hpp"
#include "black_lua/context.hpp"

namespace BlackLua::Internal {

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
    T Cmp(T lhs, T rhs) { return lhs == rhs; }
    template <typename T>
    T Ncmp(T lhs, T rhs) { return lhs != rhs; }
    template <typename T>
    T Lt(T lhs, T rhs) { return lhs < rhs; }
    template <typename T>
    T Lte(T lhs, T rhs) { return lhs <= rhs; }
    template <typename T>
    T Gt(T lhs, T rhs) { return lhs > rhs; }
    template <typename T>
    T Gte(T lhs, T rhs) { return lhs >= rhs; }

    VM::VM(Context* ctx) {
        m_Stack.resize(4 * 1024 * 1024); // 4MB stack by default
        m_StackSlots.resize(1024); // 1024 slots by default

        m_Context = ctx;
    }

    void VM::PushBytes(size_t amount, TypeInfo* type) {
        size_t alignedAmount = ((amount + 8 - 1) / 8) * 8; // We need to handle 8 byte alignment since some CPU's will require it
        
        BLUA_ASSERT(alignedAmount % 8 == 0, "Memory not aligned to 8 bytes correctly!");
        BLUA_ASSERT(m_StackPointer + alignedAmount < m_Stack.max_size(), "Stack overflow, allocating an insane amount of memory!");

        // Keep doubling the stack until it's big enough
        while (m_StackPointer + alignedAmount > m_Stack.size()) {
            m_Stack.resize(m_Stack.size() * 2);
        }

        m_StackPointer += alignedAmount;

        if (m_StackSlotPointer + 1 >= m_StackSlots.size()) {
            m_StackSlots.resize(m_StackSlots.size() * 2);
        }

        m_StackSlots[m_StackSlotPointer] = {&m_Stack[m_StackPointer - alignedAmount], amount, false, type};
        m_StackSlotPointer++;
    }

    void VM::Pop() {
        BLUA_ASSERT(m_StackPointer > 0, "Calling Pop() on an empty stack!");
        StackSlot s = GetStackSlot(-1);

        m_StackPointer -= s.Size;
        m_StackSlotPointer--;
    }

    void VM::PushStackFrame() {
        StackFrame newStackFrame;
        newStackFrame.Offset = m_StackPointer;
        newStackFrame.SlotOffset = m_StackSlotPointer;

        m_StackFrames.push_back(newStackFrame);
    }

    void VM::PopStackFrame() {
        BLUA_ASSERT(m_StackFrames.size() > 0, "Calling PopStackFrame() with no active stack frame!");

        StackFrame current = m_StackFrames.back();
        m_StackPointer = current.Offset;
        m_StackSlotPointer = current.SlotOffset;
        m_StackFrames.pop_back();
    }

    void VM::AddExtern(const std::string& signature, ExternFn fn) {
        m_ExternFuncs[signature] = fn;
    }

    void VM::Call(int32_t label) {
        // Perform a jump
        size_t pc = m_ProgramCounter;

        BLUA_ASSERT(m_Labels.contains(label), "Trying to jump to an unknown label!");
        m_ProgramCounter = m_Labels.at(label) + 1;

        PushStackFrame();

        m_StackFrames.back().ReturnAddress = pc;
        m_StackFrames.back().ReturnSlot = m_StackSlotPointer;

        Run();
    }

    void VM::CallExtern(const std::string& signature) {
        BLUA_ASSERT(m_ExternFuncs.contains(signature), "Calling CallExtern() on a non-existent extern function!");

        // Do the call
        m_ExternFuncs.at(signature)(m_Context);
    }

    void VM::StoreBool(StackSlotIndex slot, bool b) {
        StackSlot s = GetStackSlot(slot);
        
        BLUA_ASSERT(s.Size == 1, "Cannot store a bool in a stack slot with a size that isn't 1!");
        BLUA_ASSERT(!s.ReadOnly, "Trying to store data into a read only slot!");

        int8_t bb = static_cast<int8_t>(b);
        memcpy(s.Memory, &bb, 1);
    }

    void VM::StoreChar(StackSlotIndex slot, int8_t c) {
        StackSlot s = GetStackSlot(slot);
        
        BLUA_ASSERT(s.Size == 1, "Cannot store a char in a stack slot with a size that isn't 1!");
        BLUA_ASSERT(!s.ReadOnly, "Trying to store data into a read only slot!");

        memcpy(s.Memory, &c, 1);
    }

    void VM::StoreShort(StackSlotIndex slot, int16_t sh) {
        StackSlot s = GetStackSlot(slot);
        
        BLUA_ASSERT(s.Size == 2, "Cannot store a short in a stack slot with a size that isn't 2!");
        BLUA_ASSERT(!s.ReadOnly, "Trying to store data into a read only slot!");

        memcpy(s.Memory, &sh, 2);
    }

    void VM::StoreInt(StackSlotIndex slot, int32_t i) {
        StackSlot s = GetStackSlot(slot);
        
        BLUA_ASSERT(s.Size == 4, "Cannot store an int in a stack slot with a size that isn't 4!");
        BLUA_ASSERT(!s.ReadOnly, "Trying to store data into a read only slot!");

        memcpy(s.Memory, &i, 4);
    }

    void VM::StoreLong(StackSlotIndex slot, int64_t l) {
        StackSlot s = GetStackSlot(slot);
        
        BLUA_ASSERT(s.Size == 8, "Cannot store a long in a stack slot with a size that isn't 8!");
        BLUA_ASSERT(!s.ReadOnly, "Trying to store data into a read only slot!");

        memcpy(s.Memory, &l, 8);
    }

    void VM::StoreFloat(StackSlotIndex slot, float f) {
        StackSlot s = GetStackSlot(slot);
        
        BLUA_ASSERT(s.Size == 4, "Cannot store a float in a stack slot with a size that isn't 4!");
        BLUA_ASSERT(!s.ReadOnly, "Trying to store data into a read only slot!");

        memcpy(s.Memory, &f, 4);
    }

    void VM::StoreDouble(StackSlotIndex slot, double d) {
        StackSlot s = GetStackSlot(slot);
        
        BLUA_ASSERT(s.Size == 8, "Cannot store a double in a stack slot with a size that isn't 8!");
        BLUA_ASSERT(!s.ReadOnly, "Trying to store data into a read only slot!");

        memcpy(s.Memory, &d, 8);
    }

    void VM::StorePointer(StackSlotIndex slot, void* p) {
        StackSlot s = GetStackSlot(slot);
        
        BLUA_ASSERT(s.Size == sizeof(void*), "Cannot store a double in a stack slot with a size that isn't sizeof(void*)!");
        BLUA_ASSERT(!s.ReadOnly, "Trying to store data into a read only slot!");

        memcpy(s.Memory, &p, sizeof(void*));
    }

    void VM::SetMemory(StackSlotIndex slot, void* data) {
        StackSlot s = GetStackSlot(slot);
        BLUA_ASSERT(!s.ReadOnly, "Trying to store data into a read only slot!");
        m_StackSlots[GetStackSlotIndex(slot.Slot)].Memory = data;
    }

    void* VM::GetMemory(StackSlotIndex slot) {
        StackSlot s = GetStackSlot(slot);
        return s.Memory;
    }

    void VM::Copy(StackSlotIndex dstSlot, StackSlotIndex srcSlot) {
        StackSlot dst = GetStackSlot(dstSlot);
        StackSlot src = GetStackSlot(srcSlot);

        BLUA_ASSERT(dst.Size == src.Size, "Invalid Copy() call, sizes of both slots must be the same!");
        BLUA_ASSERT(!dst.ReadOnly, "Trying to copy data into a read only slot!");

        memcpy(dst.Memory, src.Memory, src.Size);
    }

    void VM::Ref(StackSlotIndex srcSlot, TypeInfo* type) {
        StackSlot src = GetStackSlot(srcSlot);

        StackSlot newSlot;
        newSlot.Memory = src.Memory;
        newSlot.Size = src.Size;
        newSlot.ReadOnly = src.ReadOnly;
        newSlot.ResolvedType = type;
        m_StackSlots[m_StackSlotPointer] = newSlot;
        m_StackSlotPointer++;
    }

    bool VM::GetBool(StackSlotIndex slot) {
        StackSlot s = GetStackSlot(slot);

        BLUA_ASSERT(s.Size == 1, "Invalid GetBool() call!");

        bool b = false;
        memcpy(&b, s.Memory, 1);
        return b;
    }

    int8_t VM::GetChar(StackSlotIndex slot) {
        StackSlot s = GetStackSlot(slot);

        BLUA_ASSERT(s.Size == 1, "Invalid GetChar() call!");

        int8_t c = 0;
        memcpy(&c, s.Memory, 1);
        return c;
    }

    int16_t VM::GetShort(StackSlotIndex slot) {
        StackSlot s = GetStackSlot(slot);

        BLUA_ASSERT(s.Size == 2, "Invalid GetShort() call!");

        int16_t sh = 0;
        memcpy(&sh, s.Memory, 2);
        return sh;
    }

    int32_t VM::GetInt(StackSlotIndex slot) {
        StackSlot s = GetStackSlot(slot);

        BLUA_ASSERT(s.Size == 4, "Invalid GetInt() call!");

        int32_t i = 0;
        memcpy(&i, s.Memory, 4);
        return i;
    }

    int64_t VM::GetLong(StackSlotIndex slot) {
        StackSlot s = GetStackSlot(slot);

        BLUA_ASSERT(s.Size == 8, "Invalid GetLong() call!");

        int64_t l = 0;
        memcpy(&l, s.Memory, 8);
        return l;
    }

    float VM::GetFloat(StackSlotIndex slot) {
        StackSlot s = GetStackSlot(slot);

        BLUA_ASSERT(s.Size == 4, "Invalid GetFloat() call!");

        float f = 0;
        memcpy(&f, s.Memory, 4);
        return f;
    }

    double VM::GetDouble(StackSlotIndex slot) {
        StackSlot s = GetStackSlot(slot);

        BLUA_ASSERT(s.Size == 8, "Invalid GetDouble() call!");

        double d = 0;
        memcpy(&d, s.Memory, 8);
        return d;
    }

    void* VM::GetPointer(StackSlotIndex slot) {
        StackSlot s = GetStackSlot(slot);

        BLUA_ASSERT(s.Size == sizeof(void*), "Invalid GetPointer() call!");

        void* p = nullptr;
        memcpy(&p, s.Memory, sizeof(void*));
        return p;
    }

    void VM::RunByteCode(const OpCode* data, size_t count) {
        m_Program = data;
        m_ProgramSize = count;

        RegisterLables();
        Run();
    }

    void VM::Run() {
        #define CASE_LOAD(_enum, builtInType) case OpCodeType::_enum: { \
            OpCodeLoad l = std::get<OpCodeLoad>(op.Data); \
            PushBytes(sizeof(builtInType), l.ResolvedType); \
            memcpy(GetStackSlot(-1).Memory, &std::get<builtInType>(l.Data), sizeof(builtInType)); \
            break; \
        }

        #define CASE_UNARYEXPR(_enum, builtinType, builtinOp) case OpCodeType::_enum: { \
            StackSlotIndex v = std::get<StackSlotIndex>(op.Data); \
            StackSlot s = GetStackSlot(v); \
            builtinType value{}; \
            memcpy(&value, s.Memory, sizeof(builtinType)); \
            builtinType result = builtinOp(value); \
            PushBytes(sizeof(builtinType), s.ResolvedType); \
            StackSlot newSlot = GetStackSlot(-1); \
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
            memcpy(&lhs, GetStackSlot(m.LHSSlot).Memory, sizeof(builtinType)); \
            memcpy(&rhs, GetStackSlot(m.RHSSlot).Memory, sizeof(builtinType)); \
            builtinType result = builtinOp(lhs, rhs); \
            PushBytes(sizeof(builtinType), m.ResolvedType); \
            StackSlot s = GetStackSlot(-1); \
            memcpy(s.Memory, &result, sizeof(builtinType)); \
            break; \
        }

        #define CASE_BINEXPR_BOOL(_enum, builtinType, builtinOp) case OpCodeType::_enum: { \
            OpCodeMath m = std::get<OpCodeMath>(op.Data); \
            builtinType lhs{}; \
            builtinType rhs{}; \
            memcpy(&lhs, GetStackSlot(m.LHSSlot).Memory, sizeof(builtinType)); \
            memcpy(&rhs, GetStackSlot(m.RHSSlot).Memory, sizeof(builtinType)); \
            bool result = builtinOp(lhs, rhs); \
            PushBytes(1, m.ResolvedType); \
            StackSlot s = GetStackSlot(-1); \
            memcpy(s.Memory, &result, 1); \
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
            StackSlot s = GetStackSlot(cast.Slot); \
            sourceType t{}; \
            memcpy(&t, s.Memory, sizeof(sourceType)); \
            destType d = static_cast<destType>(t); \
            PushBytes(sizeof(destType), cast.ResolvedType); \
            StackSlot __a = GetStackSlot(-1); \
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

            if (m_BreakPoints.contains(m_ProgramCounter)) {
                __debugbreak();
            }

            switch (op.Type) {
                case OpCodeType::Invalid: { BLUA_ASSERT(false, "Unreachable!"); break; }
                case OpCodeType::Nop: { continue; }

                case OpCodeType::Push: {
                    OpCodePush push = std::get<OpCodePush>(op.Data);

                    PushBytes(push.Count, push.ResolvedType);
                    break;
                }

                case OpCodeType::Pop: {
                    Pop();
                    break;
                }

                case OpCodeType::PushStackFrame: {
                    PushStackFrame();
                    break;
                }

                case OpCodeType::PopStackFrame: {
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
                    PushBytes(str.Size(), l.ResolvedType);
                    memcpy(GetStackSlot(-1).Memory, str.Data(), str.Size());
                    break;
                }

                case OpCodeType::Get: {
                    StackSlotIndex slot = std::get<StackSlotIndex>(op.Data);
                    StackSlot s = GetStackSlot(slot);
                    PushBytes(s.Size, s.ResolvedType);

                    memcpy(GetStackSlot(-1).Memory, s.Memory, s.Size);

                    break;
                }

                case OpCodeType::Copy: {
                    OpCodeCopy copy = std::get<OpCodeCopy>(op.Data);
                    StackSlot dst = GetStackSlot(copy.DstSlot);
                    StackSlot src = GetStackSlot(copy.SrcSlot);

                    BLUA_ASSERT(dst.Size == src.Size, "Sizes of both slots must be the same when copying!");
                    BLUA_ASSERT(!dst.ReadOnly, "Trying to copy data into a read only slot!");

                    memcpy(dst.Memory, src.Memory, src.Size);

                    break;
                }

                case OpCodeType::Dup: {
                    StackSlotIndex slot = std::get<StackSlotIndex>(op.Data);
                    StackSlot src = GetStackSlot(slot);

                    PushBytes(src.Size, src.ResolvedType);
                    StackSlot dst = GetStackSlot(-1);
                    
                    memcpy(dst.Memory, src.Memory, src.Size);

                    break;
                }

                case OpCodeType::Ref: {
                    StackSlotIndex slot = std::get<StackSlotIndex>(op.Data);
                    StackSlot src = GetStackSlot(slot);

                    StackSlot s;
                    s.Memory = src.Memory;
                    s.ReadOnly = src.ReadOnly;
                    s.Size = src.Size;
                    s.ResolvedType = src.ResolvedType;
                    m_StackSlots[m_StackSlotPointer] = s;
                    m_StackSlotPointer++;

                    break;
                }

                case OpCodeType::Offset: {
                    OpCodeOffset off = std::get<OpCodeOffset>(op.Data);
                    int32_t offset = GetInt(off.Offset);
                    StackSlot slot = GetStackSlot(off.Slot);

                    BLUA_ASSERT(offset < slot.Size, "Offset out of bounds!");

                    StackSlot s;
                    s.Memory = reinterpret_cast<uint8_t*>(slot.Memory) + offset;
                    s.ReadOnly = slot.ReadOnly;
                    s.Size = off.Size;
                    m_StackSlots[m_StackSlotPointer] = s;
                    m_StackSlotPointer++;
                    BLUA_ASSERT(false, "TODO");

                    break;
                }

                // When we encounter a label, instantly stop execution
                case OpCodeType::Label: m_ProgramCounter = m_ProgramSize - 1; break;

                case OpCodeType::Jmp: {
                    int32_t labelIdentifier = std::get<StackSlotIndex>(op.Data).Slot;

                    BLUA_ASSERT(m_Labels.contains(labelIdentifier), "Trying to jump to an unknown label!");
                    m_ProgramCounter = m_Labels.at(labelIdentifier);

                    break;
                }

                case OpCodeType::Jt: {
                    OpCodeJump jump = std::get<OpCodeJump>(op.Data);

                    if (GetBool(jump.Slot) == true) {
                        BLUA_ASSERT(m_Labels.contains(jump.Label), "Trying to jump to an unknown label!");
                        m_ProgramCounter = m_Labels.at(jump.Label);
                    }

                    break;
                }

                case OpCodeType::Jf: {
                    OpCodeJump jump = std::get<OpCodeJump>(op.Data);

                    if (GetBool(jump.Slot) == false) {
                        BLUA_ASSERT(m_Labels.contains(jump.Label), "Trying to jump to an unknown label!");
                        m_ProgramCounter = m_Labels.at(jump.Label);
                    }

                    break;
                }

                case OpCodeType::Call: {
                    int32_t label = std::get<StackSlotIndex>(op.Data).Slot;
                    // Perform a jump
                    size_t pc = m_ProgramCounter;

                    BLUA_ASSERT(m_Labels.contains(label), "Trying to jump to an unknown label!");
                    m_ProgramCounter = m_Labels.at(label); // Even though the label map points to the actual label op code, because the pc gets incremented in the next loop iteration, this will work

                    PushStackFrame();

                    m_StackFrames.back().ReturnAddress = pc;
                    m_StackFrames.back().ReturnSlot = m_StackSlotPointer;

                    break;
                }

                case OpCodeType::CallExtern: {
                    std::string sig = std::get<std::string>(op.Data);
                    CallExtern(sig);

                    break;
                }

                case OpCodeType::Ret: {
                    BLUA_ASSERT(m_StackFrames.size() > 0, "Trying to return out of no stack frame!");

                    // Keep popping stack frames until we find the function scope
                    while (m_StackFrames.back().ReturnAddress == SIZE_MAX) {
                        BLUA_ASSERT(m_StackFrames.size() > 0, "Trying return out of non function stack frame!");

                        PopStackFrame();
                    }

                    m_ProgramCounter = m_StackFrames.back().ReturnAddress;
                    PopStackFrame();

                    break;
                }

                CASE_UNARYEXPR_GROUP(Negate, -);

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
                CASE_BINEXPR_BOOL_GROUP(Lt, Lt)
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
            }
        }

        #undef CASE_UNARYEXPR
        #undef CASE_UNARYEXPR_GROUP
        #undef CASE_BINEXPR
        #undef CASE_BINEXPR_GROUP
        #undef CASE_CAST
        #undef CASE_CAST_GROUP
    }

    StackSlot VM::GetStackSlot(StackSlotIndex slot) {
        StackSlot s = m_StackSlots[GetStackSlotIndex(slot.Slot)];
        s.Memory = reinterpret_cast<uint8_t*>(s.Memory) + slot.Offset;
        
        if (slot.Size != 0) {
            s.Size = slot.Size;
        }

        return s;
    }

    size_t VM::GetStackSlotIndex(int32_t slot) {
        if (slot < 0) {
            BLUA_ASSERT(m_StackSlotPointer + slot >= 0, "Out of range slot!");

            return m_StackSlotPointer + slot;
        } else if (slot > 0) {
            BLUA_ASSERT((slot - 1) < m_StackSlotPointer, "Out of range slot!");
            return slot - 1;
        } else {
            BLUA_ASSERT(false, "Slot cannot be 0!");
        }

        return 0;
    }

    void VM::AddBreakPoint(int32_t pc) {
        m_BreakPoints[pc] = true;
    }

    void VM::StopExecution() {
        m_ProgramCounter = m_ProgramSize;
    }

    void VM::RegisterLables() {
        for (; m_ProgramCounter < m_ProgramSize; m_ProgramCounter++) {
            const OpCode& op = m_Program[m_ProgramCounter];

            if (op.Type == OpCodeType::Label) {
                int32_t labelIdentifier = std::get<StackSlotIndex>(op.Data).Slot;

                m_Labels[labelIdentifier] = m_ProgramCounter;
                m_LabelCount++;
            }
        }

        m_ProgramCounter = 0; // Reset the program counter so the normal execution happens from the start
    }

} // namespace BlackLua::Internal
