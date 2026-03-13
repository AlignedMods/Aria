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

        m_StackSlots[m_StackSlotPointer] = { m_StackPointer - alignedSize, size };
        m_StackSlotPointer++;
    }

    void VM::Pop(size_t count) {
        m_StackPointer = m_StackSlots[m_StackSlotPointer - count].Index;
        m_StackSlotPointer -= count;
    }

    void VM::Copy(i32 dstSlot, i32 srcSlot) {
        VMSlice dst = GetVMSlice(dstSlot);
        VMSlice src = GetVMSlice(srcSlot);

        ARIA_ASSERT(dst.Size == src.Size, "Invalid VM::Copy() call, sizes of both slots must be the same!");

        memcpy(dst.Memory, src.Memory, src.Size);
    }

    void VM::Dup(i32 slot) {
        VMSlice src = GetVMSlice(slot);

        Alloca(src.Size, src.ResolvedType);
        VMSlice dst = GetVMSlice(-1);
        
        memcpy(dst.Memory, src.Memory, src.Size);
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

    void VM::CallExtern(const std::string& signature, size_t argCount, size_t retCount) {
        ARIA_ASSERT(m_ExternalFunctions.contains(signature), "Calling CallExtern() on a non-existent extern function!");

        // Do the call
        m_ExternalFunctions.at(signature)(m_Context);
    }

    void VM::StoreBool(i32 slot, bool b) {
        VMSlice s = GetVMSlice(slot);
        ARIA_ASSERT(s.Size == 1, "Cannot store a bool in a slot with a size that isn't 1!");
        int8_t bb = static_cast<int8_t>(b);
        memcpy(s.Memory, &bb, 1);
    }

    void VM::StoreChar(i32 slot, int8_t c) {
        VMSlice s = GetVMSlice(slot);
        ARIA_ASSERT(s.Size == 1, "Cannot store a char in a slot with a size that isn't 1!");
        memcpy(s.Memory, &c, 1);
    }

    void VM::StoreShort(i32 slot, int16_t sh) {
        VMSlice s = GetVMSlice(slot);
        ARIA_ASSERT(s.Size == 2, "Cannot store a short in a slot with a size that isn't 2!");
        memcpy(s.Memory, &sh, 2);
    }

    void VM::StoreInt(i32 slot, int32_t i) {
        VMSlice s = GetVMSlice(slot);
        ARIA_ASSERT(s.Size == 4, "Cannot store an int in a slot with a size that isn't 4!");
        memcpy(s.Memory, &i, 4);
    }

    void VM::StoreLong(i32 slot, int64_t l) {
        VMSlice s = GetVMSlice(slot);
        ARIA_ASSERT(s.Size == 8, "Cannot store a long in a slot with a size that isn't 8!");
        memcpy(s.Memory, &l, 8);
    }

    void VM::StoreSize(i32 slot, size_t sz) {
        VMSlice s = GetVMSlice(slot);
        ARIA_ASSERT(s.Size == sizeof(size_t), "Cannot store a size_t in a slot with a size that isn't sizeof(size_t)!");
        memcpy(s.Memory, &sz, sizeof(size_t));
    }

    void VM::StoreFloat(i32 slot, float f) {
        VMSlice s = GetVMSlice(slot);
        ARIA_ASSERT(s.Size == 4, "Cannot store a float in a slot with a size that isn't 4!");
        memcpy(s.Memory, &f, 4);
    }

    void VM::StoreDouble(i32 slot, double d) {
        VMSlice s = GetVMSlice(slot);
        ARIA_ASSERT(s.Size == 8, "Cannot store a double in a slot with a size that isn't 8!");
        memcpy(s.Memory, &d, 8);
    }

    void VM::StorePointer(i32 slot, void* p) {
        VMSlice s = GetVMSlice(slot);
        ARIA_ASSERT(s.Size == sizeof(void*), "Cannot store a double in a slot with a size that isn't sizeof(void*)!");
        memcpy(s.Memory, &p, sizeof(void*));
    }

    bool VM::GetBool(i32 slot) {
        VMSlice s = GetVMSlice(slot);

        ARIA_ASSERT(s.Size == 1, "Invalid GetBool() call!");

        bool b = false;
        memcpy(&b, s.Memory, 1);
        return b;
    }

    int8_t VM::GetChar(i32 slot) {
        VMSlice s = GetVMSlice(slot);

        ARIA_ASSERT(s.Size == 1, "Invalid GetChar() call!");

        int8_t c = 0;
        memcpy(&c, s.Memory, 1);
        return c;
    }

    int16_t VM::GetShort(i32 slot) {
        VMSlice s = GetVMSlice(slot);

        ARIA_ASSERT(s.Size == 2, "Invalid GetShort() call!");

        int16_t sh = 0;
        memcpy(&sh, s.Memory, 2);
        return sh;
    }

    int32_t VM::GetInt(i32 slot) {
        VMSlice s = GetVMSlice(slot);

        ARIA_ASSERT(s.Size == 4, "Invalid GetInt() call!");

        int32_t i = 0;
        memcpy(&i, s.Memory, 4);
        return i;
    }

    int64_t VM::GetLong(i32 slot) {
        VMSlice s = GetVMSlice(slot);

        ARIA_ASSERT(s.Size == 8, "Invalid GetLong() call!");

        int64_t l = 0;
        memcpy(&l, s.Memory, 8);
        return l;
    }

    size_t VM::GetSize(i32 slot) {
        VMSlice s = GetVMSlice(slot);

        ARIA_ASSERT(s.Size == sizeof(size_t), "Invalid GetLong() call!");

        size_t sz = 0;
        memcpy(&sz, s.Memory, sizeof(size_t));
        return sz;
    }

    float VM::GetFloat(i32 slot) {
        VMSlice s = GetVMSlice(slot);

        ARIA_ASSERT(s.Size == 4, "Invalid GetFloat() call!");

        float f = 0;
        memcpy(&f, s.Memory, 4);
        return f;
    }

    double VM::GetDouble(i32 slot) {
        VMSlice s = GetVMSlice(slot);

        ARIA_ASSERT(s.Size == 8, "Invalid GetDouble() call!");

        double d = 0;
        memcpy(&d, s.Memory, 8);
        return d;
    }

    void* VM::GetPointer(i32 slot) {
        VMSlice s = GetVMSlice(slot);

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
            memcpy(GetVMSlice(-1).Memory, &std::get<builtInType>(l.Data), sizeof(builtInType)); \
            break; \
        }

        #define CASE_UNARYEXPR(_enum, builtinType, builtinOp) case OpCodeType::_enum: { \
            VMSlice s = GetVMSlice(-1); \
            builtinType value{}; \
            memcpy(&value, s.Memory, sizeof(builtinType)); \
            builtinType result = builtinOp(value); \
            Pop(1); \
            Alloca(sizeof(builtinType), s.ResolvedType); \
            VMSlice newSlot = GetVMSlice(-1); \
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
            memcpy(&lhs, GetVMSlice(-2).Memory, sizeof(builtinType)); \
            memcpy(&rhs, GetVMSlice(-1).Memory, sizeof(builtinType)); \
            builtinType result = builtinOp(lhs, rhs); \
            Pop(2); \
            Alloca(sizeof(builtinType), m.ResolvedType); \
            VMSlice s = GetVMSlice(-1); \
            memcpy(s.Memory, &result, sizeof(builtinType)); \
            break; \
        }

        #define CASE_BINEXPR_BOOL(_enum, builtinType, builtinOp) case OpCodeType::_enum: { \
            OpCodeMath m = std::get<OpCodeMath>(op.Data); \
            builtinType lhs{}; \
            builtinType rhs{}; \
            memcpy(&lhs, GetVMSlice(-2).Memory, sizeof(builtinType)); \
            memcpy(&rhs, GetVMSlice(-1).Memory, sizeof(builtinType)); \
            bool result = builtinOp(lhs, rhs); \
            Pop(2); \
            Alloca(1, m.ResolvedType); \
            VMSlice s = GetVMSlice(-1); \
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
            VMSlice s = GetVMSlice(-1); \
            sourceType t{}; \
            memcpy(&t, s.Memory, sizeof(sourceType)); \
            destType d = static_cast<destType>(t); \
            Pop(1); \
            Alloca(sizeof(destType), cast.ResolvedType); \
            VMSlice __a = GetVMSlice(-1); \
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

                case OpCodeType::Store: {
                    void* dst = GetPointer(-2);
                    VMSlice src = GetVMSlice(-1);

                    memcpy(dst, src.Memory, src.Size);
                    Pop(2);
                    break;
                }

                case OpCodeType::Dup: {
                    Dup(-1);
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
                    memcpy(GetVMSlice(-1).Memory, str.Data(), str.Size());
                    break;
                }

                case OpCodeType::DeclareGlobal: {
                    const std::string& g = std::get<std::string>(op.Data);
                    m_GlobalMap[g] = { m_StackSlotPointer - 1 };
                    break;
                };

                case OpCodeType::DeclareLocal: {
                    size_t index = std::get<size_t>(op.Data);
                    m_StackFrames.back().LocalMap[index] = { m_StackSlotPointer - 1 };
                    break;
                };

                case OpCodeType::LoadGlobal: {
                    const std::string& g = std::get<std::string>(op.Data);
                    Dup(m_GlobalMap.at(g));
                    break;
                }

                case OpCodeType::LoadLocal: {
                    size_t index = std::get<size_t>(op.Data);
                    Dup(m_StackFrames.back().LocalMap.at(index));
                    break;
                }

                case OpCodeType::LoadPtrGlobal: {
                    const std::string& g = std::get<std::string>(op.Data);
                    VMSlice slice = GetVMSlice(m_GlobalMap.at(g));
                    Alloca(sizeof(void*), slice.ResolvedType);
                    StorePointer(-1, slice.Memory);
                    break;
                }

                case OpCodeType::LoadPtrLocal: {
                    size_t index = std::get<size_t>(op.Data);
                    VMSlice slice = GetVMSlice(m_StackFrames.back().LocalMap.at(index));
                    Alloca(sizeof(void*), slice.ResolvedType);
                    StorePointer(-1, slice.Memory);
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

                    if (GetBool(-1) == true) {
                        ARIA_ASSERT(m_ActiveFunction->Labels.contains(label), "Trying to jump to an unknown label!");
                        m_ProgramCounter = m_ActiveFunction->Labels.at(label);
                    }

                    break;
                }

                case OpCodeType::Jf: {
                    const std::string& label = std::get<std::string>(op.Data);

                    if (GetBool(-1) == false) {
                        ARIA_ASSERT(m_ActiveFunction->Labels.contains(label), "Trying to jump to an unknown label!");
                        m_ProgramCounter = m_ActiveFunction->Labels.at(label);
                    }

                    break;
                }

                case OpCodeType::Call: {
                    const OpCodeCall& call = std::get<OpCodeCall>(op.Data);

                    // Stack is supposed to be:
                    // func (signature, stored as a char*)
                    // arg1
                    // arg2
                    // ...
                    // ret slot
                    std::string sig = reinterpret_cast<char*>(GetPointer(-static_cast<i32>(call.ArgCount + call.RetCount + 1)));

                    // Check if we have an external function
                    if (m_ExternalFunctions.contains(sig)) {
                        CallExtern(sig, call.ArgCount, call.RetCount);
                        break;
                    }

                    // Now check for aria functions
                    ARIA_ASSERT(m_Functions.contains(sig), "Calling unknown function");
                    VMFunction& func = m_Functions.at(sig);

                    // Save the state in the current stack frame
                    m_StackFrames.back().PreviousReturnAddress = m_ReturnAddress;
                    m_StackFrames.back().PreviousFunction = m_ActiveFunction;

                    m_ReturnAddress = m_ProgramCounter + 1;

                    // Perform a jump to the function
                    ARIA_ASSERT(func.Labels.contains("_entry$"), "All functions must contain a \"_entry$\" label");
                    m_ProgramCounter = func.Labels.at("_entry$");
                    m_ActiveFunction = &func;

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

                CASE_UNARYEXPR_GROUP(Neg, -);

                CASE_BINEXPR_GROUP(Add, Add)
                CASE_BINEXPR_GROUP(Sub, Sub)
                case OpCodeType::MulI8: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); int8_t lhs{}; int8_t rhs{}; memcpy(&lhs, GetVMSlice(-2).Memory, sizeof(int8_t)); memcpy(&rhs, GetVMSlice(-1).Memory, sizeof(int8_t)); int8_t result = Mul(lhs, rhs); Pop(2); Alloca(sizeof(int8_t), m.ResolvedType); VMSlice s = GetVMSlice(-1); memcpy(s.Memory, &result, sizeof(int8_t)); break;
                } case OpCodeType::MulI16: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); int16_t lhs{}; int16_t rhs{}; memcpy(&lhs, GetVMSlice(-2).Memory, sizeof(int16_t)); memcpy(&rhs, GetVMSlice(-1).Memory, sizeof(int16_t)); int16_t result = Mul(lhs, rhs); Pop(2); Alloca(sizeof(int16_t), m.ResolvedType); VMSlice s = GetVMSlice(-1); memcpy(s.Memory, &result, sizeof(int16_t)); break;
                } case OpCodeType::MulI32: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); 
                    int32_t lhs{}; 
                    int32_t rhs{}; 
                    memcpy(&lhs, GetVMSlice(-2).Memory, sizeof(int32_t)); 
                    memcpy(&rhs, GetVMSlice(-1).Memory, sizeof(int32_t)); 
                    int32_t result = Mul(lhs, rhs); 
                    Pop(2); 
                    Alloca(sizeof(int32_t), m.ResolvedType); 
                    VMSlice s = GetVMSlice(-1); 
                    memcpy(s.Memory, &result, sizeof(int32_t)); 
                    break;
                } case OpCodeType::MulI64: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); int64_t lhs{}; int64_t rhs{}; memcpy(&lhs, GetVMSlice(-2).Memory, sizeof(int64_t)); memcpy(&rhs, GetVMSlice(-1).Memory, sizeof(int64_t)); int64_t result = Mul(lhs, rhs); Pop(2); Alloca(sizeof(int64_t), m.ResolvedType); VMSlice s = GetVMSlice(-1); memcpy(s.Memory, &result, sizeof(int64_t)); break;
                } case OpCodeType::MulU8: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); uint8_t lhs{}; uint8_t rhs{}; memcpy(&lhs, GetVMSlice(-2).Memory, sizeof(uint8_t)); memcpy(&rhs, GetVMSlice(-1).Memory, sizeof(uint8_t)); uint8_t result = Mul(lhs, rhs); Pop(2); Alloca(sizeof(uint8_t), m.ResolvedType); VMSlice s = GetVMSlice(-1); memcpy(s.Memory, &result, sizeof(uint8_t)); break;
                } case OpCodeType::MulU16: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); uint16_t lhs{}; uint16_t rhs{}; memcpy(&lhs, GetVMSlice(-2).Memory, sizeof(uint16_t)); memcpy(&rhs, GetVMSlice(-1).Memory, sizeof(uint16_t)); uint16_t result = Mul(lhs, rhs); Pop(2); Alloca(sizeof(uint16_t), m.ResolvedType); VMSlice s = GetVMSlice(-1); memcpy(s.Memory, &result, sizeof(uint16_t)); break;
                } case OpCodeType::MulU32: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); uint32_t lhs{}; uint32_t rhs{}; memcpy(&lhs, GetVMSlice(-2).Memory, sizeof(uint32_t)); memcpy(&rhs, GetVMSlice(-1).Memory, sizeof(uint32_t)); uint32_t result = Mul(lhs, rhs); Pop(2); Alloca(sizeof(uint32_t), m.ResolvedType); VMSlice s = GetVMSlice(-1); memcpy(s.Memory, &result, sizeof(uint32_t)); break;
                } case OpCodeType::MulU64: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); uint64_t lhs{}; uint64_t rhs{}; memcpy(&lhs, GetVMSlice(-2).Memory, sizeof(uint64_t)); memcpy(&rhs, GetVMSlice(-1).Memory, sizeof(uint64_t)); uint64_t result = Mul(lhs, rhs); Pop(2); Alloca(sizeof(uint64_t), m.ResolvedType); VMSlice s = GetVMSlice(-1); memcpy(s.Memory, &result, sizeof(uint64_t)); break;
                } case OpCodeType::MulF32: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); float lhs{}; float rhs{}; memcpy(&lhs, GetVMSlice(-2).Memory, sizeof(float)); memcpy(&rhs, GetVMSlice(-1).Memory, sizeof(float)); float result = Mul(lhs, rhs); Pop(2); Alloca(sizeof(float), m.ResolvedType); VMSlice s = GetVMSlice(-1); memcpy(s.Memory, &result, sizeof(float)); break;
                } case OpCodeType::MulF64: {
                    OpCodeMath m = std::get<OpCodeMath>(op.Data); double lhs{}; double rhs{}; memcpy(&lhs, GetVMSlice(-2).Memory, sizeof(double)); memcpy(&rhs, GetVMSlice(-1).Memory, sizeof(double)); double result = Mul(lhs, rhs); Pop(2); Alloca(sizeof(double), m.ResolvedType); VMSlice s = GetVMSlice(-1); memcpy(s.Memory, &result, sizeof(double)); break;
                }
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
    
    VMSlice VM::GetVMSlice(i32 index) {
        StackSlot slot;

        if (index < 0) {
            slot = m_StackSlots[m_StackSlotPointer + index];
        } else {
            slot = m_StackSlots[m_StackFrames.back().SlotOffset + index];
        }

        return VMSlice(&m_Stack[slot.Index], slot.Size);
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