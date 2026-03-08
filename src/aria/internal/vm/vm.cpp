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

    void VM::Alloca(size_t size, TypeInfo* type) {
        size_t alignedSize = ((size + 8 - 1) / 8) * 8; // We need to handle 8 byte alignment since some CPU's will require it
        
        ARIA_ASSERT(alignedSize % 8 == 0, "Memory not aligned to 8 bytes correctly!");
        ARIA_ASSERT(m_StackPointer + alignedSize < m_Stack.max_size(), "Stack overflow, allocating an insane amount of memory!");

        m_StackPointer += alignedSize;

        if (m_StackSlotPointer + 1 >= m_StackSlots.size()) {
            m_StackSlots.resize(m_StackSlots.size() * 2);
        }

        m_StackSlots[m_StackSlotPointer] = {m_StackPointer - alignedSize, size};
        m_StackSlotPointer++;
    }

    void VM::PushStackFrame() {
        StackFrame newStackFrame;
        newStackFrame.Offset = m_StackPointer;
        newStackFrame.SlotOffset = m_StackSlotPointer;
        newStackFrame.PreviousFunction = m_ActiveFunction;

        m_StackFrames.push_back(newStackFrame);
    }

    void VM::PopStackFrame() {
        ARIA_ASSERT(m_StackFrames.size() > 0, "Calling PopStackFrame() with no active stack frame!");

        StackFrame current = m_StackFrames.back();
        m_StackPointer = current.Offset;
        m_StackSlotPointer = current.SlotOffset;
        m_StackFrames.pop_back();
    }

    void VM::AddExtern(const std::string& signature, ExternFn fn) {
        m_ExternalFunctions[signature] = fn;
    }

    void VM::Call(int32_t label) {
        ARIA_ASSERT(false, "todo: VM::Call()");
        // // Perform a jump
        // size_t pc = m_ProgramCounter;

        // ARIA_ASSERT(m_Labels.contains(label), "Trying to jump to an unknown label!");
        // m_ProgramCounter = m_Labels.at(label) + 1;

        // PushStackFrame();

        // m_StackFrames.back().ReturnAddress = pc;
        // m_StackFrames.back().ReturnSlot = m_StackSlotPointer;

        // Run();
    }

    void VM::CallExtern(const std::string& signature) {
        ARIA_ASSERT(m_ExternalFunctions.contains(signature), "Calling CallExtern() on a non-existent extern function!");

        // Do the call
        m_ExternalFunctions.at(signature)(m_Context);
    }

    void VM::StoreBool(MemRef mem, bool b) {
        VMSlice s = GetVMSlice(mem);
        
        ARIA_ASSERT(s.Size == 1, "Cannot store a bool in a slice with a size that isn't 1!");

        int8_t bb = static_cast<int8_t>(b);
        memcpy(s.Memory, &bb, 1);
    }

    void VM::StoreChar(MemRef mem, int8_t c) {
        VMSlice s = GetVMSlice(mem);
        
        ARIA_ASSERT(s.Size == 1, "Cannot store a char in a slice with a size that isn't 1!");

        memcpy(s.Memory, &c, 1);
    }

    void VM::StoreShort(MemRef mem, int16_t sh) {
        VMSlice s = GetVMSlice(mem);
        
        ARIA_ASSERT(s.Size == 2, "Cannot store a short in a stack mem with a size that isn't 2!");

        memcpy(s.Memory, &sh, 2);
    }

    void VM::StoreInt(MemRef mem, int32_t i) {
        VMSlice s = GetVMSlice(mem);
        
        ARIA_ASSERT(s.Size == 4, "Cannot store an int in a stack mem with a size that isn't 4!");

        memcpy(s.Memory, &i, 4);
    }

    void VM::StoreLong(MemRef mem, int64_t l) {
        VMSlice s = GetVMSlice(mem);
        
        ARIA_ASSERT(s.Size == 8, "Cannot store a long in a stack mem with a size that isn't 8!");

        memcpy(s.Memory, &l, 8);
    }

    void VM::StoreFloat(MemRef mem, float f) {
        VMSlice s = GetVMSlice(mem);
        
        ARIA_ASSERT(s.Size == 4, "Cannot store a float in a stack mem with a size that isn't 4!");

        memcpy(s.Memory, &f, 4);
    }

    void VM::StoreDouble(MemRef mem, double d) {
        VMSlice s = GetVMSlice(mem);
        
        ARIA_ASSERT(s.Size == 8, "Cannot store a double in a stack mem with a size that isn't 8!");

        memcpy(s.Memory, &d, 8);
    }

    void VM::StorePointer(MemRef mem, void* p) {
        VMSlice s = GetVMSlice(mem);
        
        ARIA_ASSERT(s.Size == sizeof(void*), "Cannot store a double in a stack mem with a size that isn't sizeof(void*)!");

        memcpy(s.Memory, &p, sizeof(void*));
    }

    void VM::Copy(MemRef dstMem, MemRef srcMem) {
        VMSlice dst = GetVMSlice(dstMem);
        VMSlice src = GetVMSlice(srcMem);

        ARIA_ASSERT(dst.Size == src.Size, "Invalid VM::Copy() call, sizes of both mems must be the same!");

        memcpy(dst.Memory, src.Memory, src.Size);
    }

    bool VM::GetBool(MemRef mem) {
        VMSlice s = GetVMSlice(mem);

        ARIA_ASSERT(s.Size == 1, "Invalid GetBool() call!");

        bool b = false;
        memcpy(&b, s.Memory, 1);
        return b;
    }

    int8_t VM::GetChar(MemRef mem) {
        VMSlice s = GetVMSlice(mem);

        ARIA_ASSERT(s.Size == 1, "Invalid GetChar() call!");

        int8_t c = 0;
        memcpy(&c, s.Memory, 1);
        return c;
    }

    int16_t VM::GetShort(MemRef mem) {
        VMSlice s = GetVMSlice(mem);

        ARIA_ASSERT(s.Size == 2, "Invalid GetShort() call!");

        int16_t sh = 0;
        memcpy(&sh, s.Memory, 2);
        return sh;
    }

    int32_t VM::GetInt(MemRef mem) {
        VMSlice s = GetVMSlice(mem);

        ARIA_ASSERT(s.Size == 4, "Invalid GetInt() call!");

        int32_t i = 0;
        memcpy(&i, s.Memory, 4);
        return i;
    }

    int64_t VM::GetLong(MemRef mem) {
        VMSlice s = GetVMSlice(mem);

        ARIA_ASSERT(s.Size == 8, "Invalid GetLong() call!");

        int64_t l = 0;
        memcpy(&l, s.Memory, 8);
        return l;
    }

    float VM::GetFloat(MemRef mem) {
        VMSlice s = GetVMSlice(mem);

        ARIA_ASSERT(s.Size == 4, "Invalid GetFloat() call!");

        float f = 0;
        memcpy(&f, s.Memory, 4);
        return f;
    }

    double VM::GetDouble(MemRef mem) {
        VMSlice s = GetVMSlice(mem);

        ARIA_ASSERT(s.Size == 8, "Invalid GetDouble() call!");

        double d = 0;
        memcpy(&d, s.Memory, 8);
        return d;
    }

    void* VM::GetPointer(MemRef mem) {
        VMSlice s = GetVMSlice(mem);

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

        // Perform a jump to the function
        ARIA_ASSERT(func.Labels.contains("_entry$"), "_start$() function doesn't contain a \"_entry$\" label");
        m_ProgramCounter = func.Labels.at("_entry$");
        Run();
    }

    void VM::Run() {
        #define CASE_LOAD(_enum, builtInType) case OpCodeType::_enum: { \
            OpCodeLoad l = std::get<OpCodeLoad>(op.Data); \
            Alloca(sizeof(builtInType), l.ResolvedType); \
            memcpy(GetVMSlice(MemRef(-1, sizeof(builtInType))).Memory, &std::get<builtInType>(l.Data), sizeof(builtInType)); \
            break; \
        }

        #define CASE_UNARYEXPR(_enum, builtinType, builtinOp) case OpCodeType::_enum: { \
            MemRef v = std::get<MemRef>(op.Data); \
            VMSlice s = GetVMSlice(v); \
            builtinType value{}; \
            memcpy(&value, s.Memory, sizeof(builtinType)); \
            builtinType result = builtinOp(value); \
            Alloca(sizeof(builtinType), s.ResolvedType); \
            VMSlice newSlot = GetVMSlice(MemRef(-1, s.Size)); \
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
            memcpy(&lhs, GetVMSlice(m.LHSMem).Memory, sizeof(builtinType)); \
            memcpy(&rhs, GetVMSlice(m.RHSMem).Memory, sizeof(builtinType)); \
            builtinType result = builtinOp(lhs, rhs); \
            Alloca(sizeof(builtinType), m.ResolvedType); \
            VMSlice s = GetVMSlice(MemRef(-1, sizeof(builtinType))); \
            memcpy(s.Memory, &result, sizeof(builtinType)); \
            break; \
        }

        #define CASE_BINEXPR_BOOL(_enum, builtinType, builtinOp) case OpCodeType::_enum: { \
            OpCodeMath m = std::get<OpCodeMath>(op.Data); \
            builtinType lhs{}; \
            builtinType rhs{}; \
            memcpy(&lhs, GetVMSlice(m.LHSMem).Memory, sizeof(builtinType)); \
            memcpy(&rhs, GetVMSlice(m.RHSMem).Memory, sizeof(builtinType)); \
            bool result = builtinOp(lhs, rhs); \
            Alloca(1, m.ResolvedType); \
            VMSlice s = GetVMSlice(MemRef(-1, 1)); \
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
            VMSlice s = GetVMSlice(cast.Mem); \
            sourceType t{}; \
            memcpy(&t, s.Memory, sizeof(sourceType)); \
            destType d = static_cast<destType>(t); \
            Alloca(sizeof(destType), cast.ResolvedType); \
            VMSlice __a = GetVMSlice(MemRef(-1, sizeof(destType))); \
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

                    Alloca(alloca.Size, alloca.ResolvedType);
                    break;
                }

                case OpCodeType::Copy: {
                    OpCodeCopy copy = std::get<OpCodeCopy>(op.Data);
                    Copy(copy.DstMem, copy.SrcMem);
                    break;
                }

                case OpCodeType::Dup: {
                    MemRef mem = std::get<MemRef>(op.Data);
                    VMSlice src = GetVMSlice(mem);

                    Alloca(src.Size, src.ResolvedType);
                    VMSlice dst = GetVMSlice(MemRef(-1, src.Size));
                    
                    memcpy(dst.Memory, src.Memory, src.Size);
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
                    Alloca(str.Size(), l.ResolvedType);
                    memcpy(GetVMSlice(MemRef(-1, str.Size())).Memory, str.Data(), str.Size());
                    break;
                }

                case OpCodeType::SetGlobal: {
                    const OpCodeSetGlobal& g = std::get<OpCodeSetGlobal>(op.Data);
                    m_GlobalMap[g.Name] = { m_StackSlots[m_StackSlotPointer - 1].Index, m_StackSlots[m_StackSlotPointer - 1].Size };
                    break;
                };

                case OpCodeType::Function: ARIA_ASSERT(false, "VM should never reach a function op code!"); break;
                case OpCodeType::Label: break; // We just keep going

                case OpCodeType::Jmp: {
                    const std::string& labelIdentifier = std::get<std::string>(op.Data);

                    ARIA_ASSERT(m_ActiveFunction->Labels.contains(labelIdentifier), "Trying to jump to an unknown label!");
                    m_ProgramCounter = m_ActiveFunction->Labels.at(labelIdentifier);

                    break;
                }

                case OpCodeType::Jt: {
                    OpCodeConditionalJump jump = std::get<OpCodeConditionalJump>(op.Data);

                    if (GetBool(jump.Mem) == true) {
                        ARIA_ASSERT(m_ActiveFunction->Labels.contains(jump.Label), "Trying to jump to an unknown label!");
                        m_ProgramCounter = m_ActiveFunction->Labels.at(jump.Label);
                    }

                    break;
                }

                case OpCodeType::Jf: {
                    OpCodeConditionalJump jump = std::get<OpCodeConditionalJump>(op.Data);

                    if (GetBool(jump.Mem) == false) {
                        ARIA_ASSERT(m_ActiveFunction->Labels.contains(jump.Label), "Trying to jump to an unknown label!");
                        m_ProgramCounter = m_ActiveFunction->Labels.at(jump.Label);
                    }

                    break;
                }

                case OpCodeType::Call: {
                    const std::string& signature = std::get<std::string>(op.Data);

                    // Save the state in the current stack frame
                    m_StackFrames.back().PreviousReturnAddress = m_ReturnAddress;
                    m_StackFrames.back().PreviousFunction = m_ActiveFunction;

                    m_ReturnAddress = m_ProgramCounter + 1;

                    ARIA_ASSERT(m_Functions.contains(signature), "Calling unknown function");
                    VMFunction& func = m_Functions.at(signature);

                    // Perform a jump to the function
                    ARIA_ASSERT(func.Labels.contains("_entry$"), "All functions must contain a \"_entry$\" label");
                    m_ProgramCounter = func.Labels.at("_entry$");

                    break;
                }

                case OpCodeType::CallExtern: {
                    std::string sig = std::get<std::string>(op.Data);
                    CallExtern(sig);

                    break;
                }

                case OpCodeType::Ret: {
                    ARIA_ASSERT(m_StackFrames.size() > 0, "Trying to return out of no stack frame!");

                    if (m_ReturnAddress == SIZE_MAX) {
                        StopExecution();
                    } else {
                        m_ProgramCounter = m_ReturnAddress;
                    }

                    m_ReturnAddress = m_StackFrames.back().PreviousReturnAddress;
                    m_ActiveFunction = m_StackFrames.back().PreviousFunction;
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
    
    VMSlice VM::GetVMSlice(MemRef mem) {
        if (mem.ContainsStackSlot()) {
            auto s = mem.GetStackSlot();
            ARIA_ASSERT(s.Slot < m_StackSlotPointer, "Out of bounds stack slot index!");
            StackSlot slot;

            if (s.Slot >= 0) {
                slot = m_StackSlots[s.Slot];
            } else {
                slot = m_StackSlots[m_StackSlotPointer + s.Slot];
            }

            ARIA_ASSERT(s.Size <= slot.Size, "Stack slot index size is bigger than stack slot!");
            ARIA_ASSERT(slot.Index + s.Offset < m_StackPointer, "Out of bounds stack slot!");
            return VMSlice(&m_Stack[slot.Index + s.Offset], (s.Size != 0) ? s.Size : slot.Size);
        } else if (mem.ContainsGlobal()) {
            const std::string& ident = mem.GetGlobal();
            ARIA_ASSERT(m_GlobalMap.contains(ident), "Unknown global identifier!");
            StackSlot slot = m_GlobalMap.at(ident);
            return VMSlice(&m_Stack[slot.Index], slot.Size);
        }

        ARIA_UNREACHABLE();
    }

    void VM::StopExecution() {
        m_ProgramCounter = m_ProgramSize;
    }

    void VM::RunPrepass() {
        for (; m_ProgramCounter < m_ProgramSize; m_ProgramCounter++) {
            const OpCode& op = m_Program[m_ProgramCounter];

            if (op.Type == OpCodeType::Function) {
                size_t startPc = m_ProgramCounter;

                const std::string& ident = std::get<std::string>(op.Data);
                VMFunction func;
                
                for (; m_ProgramCounter < m_ProgramSize; m_ProgramCounter++) {
                    const OpCode& op = m_Program[m_ProgramCounter];

                    if (op.Type == OpCodeType::Label) {
                        std::string label = std::get<std::string>(op.Data);
                        func.Labels[label] = m_ProgramCounter;
                    } else if (op.Type == OpCodeType::Ret) {
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
