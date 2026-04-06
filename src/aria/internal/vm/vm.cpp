#include "aria/internal/vm/vm.hpp"
#include "aria/context.hpp"
#include "assert.h"

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
        m_ExpressionStack.Reserve(16 * 1024, 1024);
        m_LocalStack.     Reserve(32 * 1024, 2048);
        m_FunctionStack.  Reserve(4 * 1024, 256);
        m_GlobalStack.    Reserve(16 * 1024, 1024);

        m_Context = ctx;
    }

    VM::~VM() {
        // TODO: Add support for calling all _end$...() functions
        const std::string& signature = "_end$0()";

        if (m_Functions.contains(signature)) {
            VMFunction& func = m_Functions.at(signature);
            m_ActiveFunction = &func;

            // Perform a jump to the function
            ARIA_ASSERT(func.Labels.contains("_entry$"), "_end$0() function doesn't contain a \"_entry$\" label");
            m_ProgramCounter = func.Labels.at("_entry$");
            Run();
        }
    }

    void VM::Alloca(VMType type, Stack& stack) {
        size_t rawSize = GetVMTypeSize(type);
        size_t alignedSize = AlignToEight(rawSize);
        
        ARIA_ASSERT(alignedSize % 8 == 0, "Memory not aligned to 8 bytes correctly!");
        ARIA_ASSERT(stack.StackPointer + alignedSize < stack.Stack.max_size(), "Local stack overflow, allocating an insane amount of memory!");

        stack.StackPointer += alignedSize;

        // Add more stack slots if needed
        if (stack.StackSlotPointer + 1 >= stack.StackSlots.size()) {
            stack.StackSlots.resize(stack.StackSlots.size() * 2);
        }
        
        stack.StackSlots[stack.StackSlotPointer++] = { stack.StackPointer - alignedSize, rawSize, type };
    }

    void VM::Pop(size_t count, Stack& stack) {
        size_t sp = stack.StackPointer;
        stack.StackPointer = stack.StackSlots[stack.StackSlotPointer - count].Index;
        stack.StackSlotPointer -= count;
    }

    void VM::Copy(i32 dstSlot, i32 srcSlot, Stack& dst, Stack& src) {
        VMSlice dstSlice = GetVMSlice(dstSlot, dst);
        VMSlice srcSlice = GetVMSlice(srcSlot, src);

        ARIA_ASSERT(dstSlice.Type == srcSlice.Type, "Invalid VM::Copy() call, types of both sides must be the same!");

        memcpy(dstSlice.Memory, srcSlice.Memory, srcSlice.Size);
    }

    void VM::Dup(i32 slot, Stack& dst, Stack& src) {
        VMSlice srcSlot = GetVMSlice(slot, src);

        Alloca(srcSlot.Type, dst);
        VMSlice dstSlot = GetVMSlice(-1, dst);
        
        memcpy(dstSlot.Memory, srcSlot.Memory, srcSlot.Size);
    }

    void VM::PushStackFrame() {
        StackFrame newStackFrame;
        newStackFrame.PESSBP = m_ExpressionStack.StackSlotBasePointer;
        newStackFrame.PESBP = m_ExpressionStack.StackBasePointer;
        newStackFrame.PLSSBP = m_LocalStack.StackSlotBasePointer;
        newStackFrame.PLSBP = m_LocalStack.StackBasePointer;
        newStackFrame.PreviousFunction = m_ActiveFunction;

        // Save the state of the local and expression stack (function stack gets handled with call/ret)
        m_ExpressionStack.StackSlotBasePointer = m_ExpressionStack.StackSlotPointer;
        m_ExpressionStack.StackBasePointer = m_ExpressionStack.StackPointer;

        m_LocalStack.StackSlotBasePointer = m_LocalStack.StackSlotPointer;
        m_LocalStack.StackBasePointer = m_LocalStack.StackPointer;

        m_StackFrames.push_back(newStackFrame);
    }

    void VM::PopStackFrame() {
        ARIA_ASSERT(m_StackFrames.size() > 0, "Calling VM::PopStackFrame() with no active stack frame!");

        // Restore the state of the local and expression stack
        m_ExpressionStack.StackSlotPointer = m_ExpressionStack.StackSlotBasePointer;
        m_ExpressionStack.StackPointer = m_ExpressionStack.StackBasePointer;

        m_LocalStack.StackSlotPointer = m_LocalStack.StackSlotBasePointer;
        m_LocalStack.StackPointer = m_LocalStack.StackBasePointer;

        m_ExpressionStack.StackSlotBasePointer = m_StackFrames.back().PESSBP;
        m_ExpressionStack.StackBasePointer = m_StackFrames.back().PESBP;

        m_LocalStack.StackSlotBasePointer = m_StackFrames.back().PLSSBP;
        m_LocalStack.StackBasePointer = m_StackFrames.back().PLSBP;

        m_StackFrames.pop_back();
    }

    void VM::AddExtern(const std::string& signature, ExternFn fn) {
        m_ExternalFunctions[signature] = fn;
    }

    void VM::Call(const std::string& signature, size_t argCount) {
        ARIA_ASSERT(m_Functions.contains(signature), "Calling unknown function");
        VMFunction& func = m_Functions.at(signature);
        
        // This function can only be called when the program is in a "halted" state AKA doing nothing
        // Therefore when we finish with execution we want to go back to that state
        m_StackFrames.back().PreviousReturnAddress = m_ProgramSize;
        m_StackFrames.back().PreviousFunction = nullptr;
        m_ReturnAddress = m_ProgramSize;

        // Save function stack
        m_StackFrames.back().PFSSBP = m_FunctionStack.StackSlotBasePointer;
        m_StackFrames.back().PFSBP = m_FunctionStack.StackBasePointer;

        // Perform a jump to the function
        ARIA_ASSERT(func.Labels.contains("_entry$"), "All functions must contain a \"_entry$\" label");
        m_ProgramCounter = func.Labels.at("_entry$");
        m_ActiveFunction = &func;

        // Set up the function stack
        m_FunctionStack.StackSlotBasePointer = m_FunctionStack.StackSlotPointer - argCount - 1;
        m_FunctionStack.StackBasePointer = m_FunctionStack.StackSlots[m_FunctionStack.StackSlotBasePointer].Index;

        Run();
    }

    void VM::CallExtern(const std::string& signature, size_t argCount) {
        ARIA_ASSERT(m_ExternalFunctions.contains(signature), "Calling CallExtern() on a non-existent extern function!");

        // Set up the function stack
        m_FunctionStack.StackSlotBasePointer = m_FunctionStack.StackSlotPointer - argCount - 1;
        m_FunctionStack.StackBasePointer = m_FunctionStack.StackSlots[m_FunctionStack.StackSlotBasePointer].Index;

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

    std::string_view VM::GetString(i32 slot, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);
        ARIA_ASSERT(s.Type.Kind == VMTypeKind::String, "Invalid GetString() call!");

        VMString str;
        memcpy(&str, s.Memory, s.Size);

        return { str.Data, str.Size };
    }

    void VM::RunByteCode(const OpCode* data, size_t count) {
        m_Program = data;
        m_ProgramSize = count;

        RunPrepass();

        // TODO: Add support for calling all _start$...() functions
        const std::string& signature = "_start$0()";

        ARIA_ASSERT(m_Functions.contains(signature), "Byte code does not contain _start$0() function");
        VMFunction& func = m_Functions.at(signature);
        m_ActiveFunction = &func;

        // Perform a jump to the function
        ARIA_ASSERT(func.Labels.contains("_entry$"), "_start$0() function doesn't contain a \"_entry$\" label");
        m_ProgramCounter = func.Labels.at("_entry$");
        Run();
    }

    void VM::Run() {
        #define CASE_UNARYEXPR_TYPE(builtinOp, vmTypeKind, realType) case VMTypeKind::vmTypeKind: { \
            realType value{}; memcpy(&value, GetVMSlice(-1, m_ExpressionStack).Memory, sizeof(realType)); \
            realType result = builtinOp(value); \
            Pop(1, m_ExpressionStack); \
            Alloca(type, m_ExpressionStack); \
            VMSlice newSlot = GetVMSlice(-1, m_ExpressionStack); \
            memcpy(newSlot.Memory, &result, newSlot.Size); \
            break; \
        }

        #define CASE_UNARYEXPR(_enum, builtinOp) case OpCodeKind::_enum: { \
            const VMType& type = std::get<VMType>(op.Data); \
            switch (type.Kind) { \
                CASE_UNARYEXPR_TYPE(builtinOp, I8,  i8)   \
                CASE_UNARYEXPR_TYPE(builtinOp, U8,  u8)   \
                CASE_UNARYEXPR_TYPE(builtinOp, I16, i16)  \
                CASE_UNARYEXPR_TYPE(builtinOp, U16, u16)  \
                CASE_UNARYEXPR_TYPE(builtinOp, I32, i32)  \
                CASE_UNARYEXPR_TYPE(builtinOp, U32, u32)  \
                CASE_UNARYEXPR_TYPE(builtinOp, I64, i64)  \
                CASE_UNARYEXPR_TYPE(builtinOp, U64, u64)  \
                CASE_UNARYEXPR_TYPE(builtinOp, F32, f32)  \
                CASE_UNARYEXPR_TYPE(builtinOp, F64, f64)  \
            } \
            \
            break; \
        }

        #define CASE_BINEXPR_TYPE(builtinOp, vmTypeKind, realType) case VMTypeKind::vmTypeKind: { \
            realType lhs{}; \
            realType rhs{}; \
            memcpy(&lhs, GetVMSlice(-2, m_ExpressionStack).Memory, sizeof(realType)); \
            memcpy(&rhs, GetVMSlice(-1, m_ExpressionStack).Memory, sizeof(realType)); \
            realType result = builtinOp(lhs, rhs); \
            Pop(2, m_ExpressionStack); \
            Alloca(type, m_ExpressionStack); \
            VMSlice s = GetVMSlice(-1, m_ExpressionStack); \
            memcpy(s.Memory, &result, sizeof(realType)); \
            break; \
        }

        #define CASE_BINEXPR_TYPE_BOOL(builtinOp, vmTypeKind, realType) case VMTypeKind::vmTypeKind: { \
            realType lhs{}; \
            realType rhs{}; \
            memcpy(&lhs, GetVMSlice(-2, m_ExpressionStack).Memory, sizeof(realType)); \
            memcpy(&rhs, GetVMSlice(-1, m_ExpressionStack).Memory, sizeof(realType)); \
            bool result = builtinOp(lhs, rhs); \
            Pop(2, m_ExpressionStack); \
            Alloca({ VMTypeKind::I1 }, m_ExpressionStack); \
            VMSlice s = GetVMSlice(-1, m_ExpressionStack); \
            memcpy(s.Memory, &result, sizeof(bool)); \
            break; \
        }

        #define CASE_BINEXPR(_enum, builtinOp) case OpCodeKind::_enum: { \
            VMType type = std::get<VMType>(op.Data); \
            switch (type.Kind) { \
                CASE_BINEXPR_TYPE(builtinOp, I8,  i8)   \
                CASE_BINEXPR_TYPE(builtinOp, U8,  u8)   \
                CASE_BINEXPR_TYPE(builtinOp, I16, i16)  \
                CASE_BINEXPR_TYPE(builtinOp, U16, u16)  \
                CASE_BINEXPR_TYPE(builtinOp, I32, i32)  \
                CASE_BINEXPR_TYPE(builtinOp, U32, u32)  \
                CASE_BINEXPR_TYPE(builtinOp, I64, i64)  \
                CASE_BINEXPR_TYPE(builtinOp, U64, u64)  \
                CASE_BINEXPR_TYPE(builtinOp, F32, f32)  \
                CASE_BINEXPR_TYPE(builtinOp, F64, f64)  \
            } \
            break; \
        }

        #define CASE_BINEXPR_INTEGRAL(_enum, builtinOp) case OpCodeKind::_enum: { \
            VMType type = std::get<VMType>(op.Data); \
            switch (type.Kind) { \
                CASE_BINEXPR_TYPE(builtinOp, I1,  bool) \
                CASE_BINEXPR_TYPE(builtinOp, I8,  i8)   \
                CASE_BINEXPR_TYPE(builtinOp, U8,  u8)   \
                CASE_BINEXPR_TYPE(builtinOp, I16, i16)  \
                CASE_BINEXPR_TYPE(builtinOp, U16, u16)  \
                CASE_BINEXPR_TYPE(builtinOp, I32, i32)  \
                CASE_BINEXPR_TYPE(builtinOp, U32, u32)  \
                CASE_BINEXPR_TYPE(builtinOp, I64, i64)  \
                CASE_BINEXPR_TYPE(builtinOp, U64, u64)  \
            } \
            break; \
        }

        #define CASE_BINEXPR_BOOL(_enum, builtinOp) case OpCodeKind::_enum: { \
            VMType type = std::get<VMType>(op.Data); \
            switch (type.Kind) { \
                CASE_BINEXPR_TYPE_BOOL(builtinOp, I1,  bool) \
                CASE_BINEXPR_TYPE_BOOL(builtinOp, I8,  i8)   \
                CASE_BINEXPR_TYPE_BOOL(builtinOp, U8,  u8)   \
                CASE_BINEXPR_TYPE_BOOL(builtinOp, I16, i16)  \
                CASE_BINEXPR_TYPE_BOOL(builtinOp, U16, u16)  \
                CASE_BINEXPR_TYPE_BOOL(builtinOp, I32, i32)  \
                CASE_BINEXPR_TYPE_BOOL(builtinOp, U32, u32)  \
                CASE_BINEXPR_TYPE_BOOL(builtinOp, I64, i64)  \
                CASE_BINEXPR_TYPE_BOOL(builtinOp, U64, u64)  \
                CASE_BINEXPR_TYPE_BOOL(builtinOp, F32, f32)  \
                CASE_BINEXPR_TYPE_BOOL(builtinOp, F64, f64)  \
            } \
            break; \
        }

        for (; m_ProgramCounter < m_ProgramSize; m_ProgramCounter++) {
            const OpCode& op = m_Program[m_ProgramCounter];

            switch (op.Kind) {
                case OpCodeKind::Nop: { continue; }

                case OpCodeKind::Alloca: {
                    VMType type = std::get<VMType>(op.Data);

                    Alloca(type, m_ExpressionStack);
                    break;
                }

                case OpCodeKind::Pop: {
                    Pop(1, m_ExpressionStack);
                    break;
                }

                case OpCodeKind::Store: {
                    void* dst = GetPointer(-2, m_ExpressionStack);
                    VMSlice src = GetVMSlice(-1, m_ExpressionStack);

                    memcpy(dst, src.Memory, src.Size);
                    Pop(2, m_ExpressionStack);
                    break;
                }

                case OpCodeKind::Dup: {
                    Dup(-1, m_ExpressionStack, m_ExpressionStack);
                    break;
                }

                case OpCodeKind::DupStr: {
                    void* mem = GetPointer(-1, m_ExpressionStack);
                    Pop(1, m_ExpressionStack);

                    VMString& str = *reinterpret_cast<VMString*>(mem);
                    VMString newStr;
                    newStr.Data = new char[str.Size];
                    newStr.Size = str.Size;
                    memcpy(newStr.Data, str.Data, str.Size);

                    Alloca({ VMTypeKind::String }, m_ExpressionStack);
                    memcpy(GetVMSlice(-1, m_ExpressionStack).Memory, &newStr, sizeof(newStr));

                    break;
                }

                case OpCodeKind::PushSF: {
                    PushStackFrame();
                    break;
                }

                case OpCodeKind::PopSF: {
                    PopStackFrame();
                    break;
                }

                case OpCodeKind::Ldc: {
                    const OpCodeLdc& ldc = std::get<OpCodeLdc>(op.Data);
                    Alloca(ldc.Type, m_ExpressionStack);
                    
                    switch (ldc.Type.Kind) {
                        case VMTypeKind::I1:  memcpy(GetVMSlice(-1, m_ExpressionStack).Memory, &std::get<bool>(ldc.Data), GetVMTypeSize(ldc.Type)); break;
                        case VMTypeKind::I8:  memcpy(GetVMSlice(-1, m_ExpressionStack).Memory, &std::get<i8>(ldc.Data),   GetVMTypeSize(ldc.Type)); break;
                        case VMTypeKind::U8:  memcpy(GetVMSlice(-1, m_ExpressionStack).Memory, &std::get<u8>(ldc.Data),   GetVMTypeSize(ldc.Type)); break;
                        case VMTypeKind::I16: memcpy(GetVMSlice(-1, m_ExpressionStack).Memory, &std::get<i16>(ldc.Data),  GetVMTypeSize(ldc.Type)); break;
                        case VMTypeKind::U16: memcpy(GetVMSlice(-1, m_ExpressionStack).Memory, &std::get<u16>(ldc.Data),  GetVMTypeSize(ldc.Type)); break;
                        case VMTypeKind::I32: memcpy(GetVMSlice(-1, m_ExpressionStack).Memory, &std::get<i32>(ldc.Data),  GetVMTypeSize(ldc.Type)); break;
                        case VMTypeKind::U32: memcpy(GetVMSlice(-1, m_ExpressionStack).Memory, &std::get<u32>(ldc.Data),  GetVMTypeSize(ldc.Type)); break;
                        case VMTypeKind::I64: memcpy(GetVMSlice(-1, m_ExpressionStack).Memory, &std::get<i64>(ldc.Data),  GetVMTypeSize(ldc.Type)); break;
                        case VMTypeKind::U64: memcpy(GetVMSlice(-1, m_ExpressionStack).Memory, &std::get<u64>(ldc.Data),  GetVMTypeSize(ldc.Type)); break;
                        case VMTypeKind::F32: memcpy(GetVMSlice(-1, m_ExpressionStack).Memory, &std::get<f32>(ldc.Data),  GetVMTypeSize(ldc.Type)); break;
                        case VMTypeKind::F64: memcpy(GetVMSlice(-1, m_ExpressionStack).Memory, &std::get<f64>(ldc.Data),  GetVMTypeSize(ldc.Type)); break;
                        default: ARIA_ASSERT(false, "Invalid \"ldc\" type");
                    }

                    break;
                }

                case OpCodeKind::LdStr: {
                    const std::string& str = std::get<std::string>(op.Data);

                    VMString vmstr;
                    // Allocate the string on the heap
                    char* newStr = new char[str.size()];
                    memcpy(newStr, str.data(), str.size());

                    vmstr.Data = newStr;
                    vmstr.Size = str.size();

                    Alloca({ VMTypeKind::String }, m_ExpressionStack);
                    memcpy(GetVMSlice(-1, m_ExpressionStack).Memory, &vmstr, sizeof(vmstr));
                    break;
                }

                case OpCodeKind::Deref: {
                    VMType type = std::get<VMType>(op.Data);

                    void* ptr = GetPointer(-1, m_ExpressionStack);
                    Pop(1, m_ExpressionStack);
                    Alloca(type, m_ExpressionStack);
                    VMSlice slice = GetVMSlice(-1, m_ExpressionStack);
                    memcpy(slice.Memory, ptr, slice.Size);
                    break;
                }

                case OpCodeKind::Struct: continue;

                case OpCodeKind::DeclareGlobal: {
                    const std::string& g = std::get<std::string>(op.Data);

                    VMSlice src = GetVMSlice(-1, m_ExpressionStack);

                    Alloca(src.Type, m_GlobalStack);
                    VMSlice dst = GetVMSlice(-1, m_GlobalStack);

                    memcpy(dst.Memory, src.Memory, src.Size);
                    Pop(1, m_ExpressionStack);

                    m_GlobalMap[g] = { static_cast<i32>(m_GlobalStack.StackSlotPointer) - 1 };
                    break;
                };

                case OpCodeKind::DeclareLocal: {
                    size_t index = std::get<size_t>(op.Data);

                    VMSlice src = GetVMSlice(-1, m_ExpressionStack);
                    VMSlice dst;

                    if (!m_StackFrames.back().LocalMap.contains(index)) {
                        Alloca(src.Type, m_LocalStack);
                        dst = GetVMSlice(-1, m_LocalStack);
                    } else {
                        dst = GetVMSlice(m_StackFrames.back().LocalMap.at(index), m_LocalStack);
                    }

                    memcpy(dst.Memory, src.Memory, src.Size);
                    Pop(1, m_ExpressionStack);

                    m_StackFrames.back().LocalMap[index] = { static_cast<i32>(m_LocalStack.StackSlotPointer - m_LocalStack.StackSlotBasePointer) - 1 };
                    break;
                };

                case OpCodeKind::DeclareArg: {
                    size_t index = std::get<size_t>(op.Data);

                    VMSlice src = GetVMSlice(-1, m_ExpressionStack);

                    Alloca(src.Type, m_FunctionStack);
                    VMSlice dst = GetVMSlice(-1, m_FunctionStack);

                    memcpy(dst.Memory, src.Memory, src.Size);
                    Pop(1, m_ExpressionStack); // Pop the local stack slot which we just copied
                    break;
                };

                case OpCodeKind::LdGlobal: {
                    const std::string& g = std::get<std::string>(op.Data);
                    Dup(m_GlobalMap.at(g), m_ExpressionStack, m_GlobalStack);
                    break;
                }

                case OpCodeKind::LdLocal: {
                    size_t index = std::get<size_t>(op.Data);
                    Dup(m_StackFrames.back().LocalMap.at(index), m_ExpressionStack, m_LocalStack);
                    break;
                }

                case OpCodeKind::LdMember: {
                    OpCodeMember mem = std::get<OpCodeMember>(op.Data);
                    u8* base = reinterpret_cast<u8*>(GetPointer(-1, m_ExpressionStack));
                    Pop(1, m_ExpressionStack);
                    
                    VMStruct s = m_Structs.at(fmt::format("{}", mem.StructType.Data));

                    Alloca(mem.MemberType, m_ExpressionStack);
                    memcpy(GetVMSlice(-1, m_ExpressionStack).Memory, base + s.FieldOffsets[mem.Index], GetVMTypeSize(mem.MemberType));
                    break;
                }

                case OpCodeKind::LdArg: {
                    i32 index = static_cast<i32>(std::get<size_t>(op.Data));
                    Dup(index + 1, m_ExpressionStack, m_FunctionStack);
                    break;
                }

                case OpCodeKind::LdFunc: {
                    const std::string& fn = std::get<std::string>(op.Data);
                    
                    Alloca({ VMTypeKind::Ptr }, m_FunctionStack);
                    StorePointer(-1, const_cast<char*>(fn.c_str()), m_FunctionStack);
                    break;
                }

                case OpCodeKind::LdPtrGlobal: {
                    const std::string& g = std::get<std::string>(op.Data);
                    VMSlice slice = GetVMSlice(m_GlobalMap.at(g), m_GlobalStack);
                    Alloca({ VMTypeKind::Ptr }, m_ExpressionStack);
                    StorePointer(-1, slice.Memory, m_ExpressionStack);
                    break;
                }

                case OpCodeKind::LdPtrLocal: {
                    size_t index = std::get<size_t>(op.Data);
                    VMSlice slice = GetVMSlice(m_StackFrames.back().LocalMap.at(index), m_LocalStack);
                    Alloca({ VMTypeKind::Ptr }, m_ExpressionStack);
                    StorePointer(-1, slice.Memory, m_ExpressionStack);
                    break;
                }

                case OpCodeKind::LdPtrMember: {
                    OpCodeMember mem = std::get<OpCodeMember>(op.Data);
                    u8* base = reinterpret_cast<u8*>(GetPointer(-1, m_ExpressionStack));
                    Pop(1, m_ExpressionStack);

                    VMStruct s = m_Structs.at(fmt::format("{}", mem.StructType.Data));

                    Alloca(mem.MemberType, m_ExpressionStack);
                    StorePointer(-1, base + s.FieldOffsets[mem.Index], m_ExpressionStack);
                    break;
                }

                case OpCodeKind::LdPtrArg: {
                    i32 index = static_cast<i32>(std::get<size_t>(op.Data));
                    VMSlice slice = GetVMSlice(index + 1, m_FunctionStack);
                    Alloca({ VMTypeKind::Ptr }, m_ExpressionStack);
                    StorePointer(-1, slice.Memory, m_ExpressionStack);
                    break;
                }

                case OpCodeKind::LdPtrRet: {
                    VMSlice slice = GetVMSlice(-(static_cast<i32>(m_ExpressionStack.StackSlotPointer - m_ExpressionStack.StackSlotBasePointer) + 1), m_ExpressionStack);
                    Alloca({ VMTypeKind::Ptr }, m_ExpressionStack);
                    StorePointer(-1, slice.Memory, m_ExpressionStack);
                    break;
                }

                case OpCodeKind::DestructStr: {
                    void* mem = GetPointer(-1, m_ExpressionStack);
                    Pop(1, m_ExpressionStack);

                    VMString& str = *reinterpret_cast<VMString*>(mem);
                    delete[] str.Data;
                    str.Data = nullptr;
                    str.Size = 0;
                    break;
                }

                case OpCodeKind::Function: ARIA_ASSERT(false, "VM should never reach a function op code!"); break;
                case OpCodeKind::Label: break; // We just keep going

                case OpCodeKind::Jmp: {
                    const std::string& label = std::get<std::string>(op.Data);

                    ARIA_ASSERT(m_ActiveFunction->Labels.contains(label), "Trying to jump to an unknown label!");
                    m_ProgramCounter = m_ActiveFunction->Labels.at(label);

                    break;
                }

                case OpCodeKind::Jt: {
                    const std::string& label = std::get<std::string>(op.Data);

                    if (GetBool(-1, m_ExpressionStack) == true) {
                        ARIA_ASSERT(m_ActiveFunction->Labels.contains(label), "Trying to jump to an unknown label!");
                        m_ProgramCounter = m_ActiveFunction->Labels.at(label);
                    }

                    break;
                }

                case OpCodeKind::JtPop: {
                    const std::string& label = std::get<std::string>(op.Data);

                    if (GetBool(-1, m_ExpressionStack) == true) {
                        ARIA_ASSERT(m_ActiveFunction->Labels.contains(label), "Trying to jump to an unknown label!");
                        m_ProgramCounter = m_ActiveFunction->Labels.at(label);
                    }

                    Pop(1, m_ExpressionStack);
                    break;
                }

                case OpCodeKind::Jf: {
                    const std::string& label = std::get<std::string>(op.Data);

                    if (GetBool(-1, m_ExpressionStack) == false) {
                        ARIA_ASSERT(m_ActiveFunction->Labels.contains(label), "Trying to jump to an unknown label!");
                        m_ProgramCounter = m_ActiveFunction->Labels.at(label);
                    }

                    break;
                }

                case OpCodeKind::JfPop: {
                    const std::string& label = std::get<std::string>(op.Data);

                    if (GetBool(-1, m_ExpressionStack) == false) {
                        ARIA_ASSERT(m_ActiveFunction->Labels.contains(label), "Trying to jump to an unknown label!");
                        m_ProgramCounter = m_ActiveFunction->Labels.at(label);
                    }

                    Pop(1, m_ExpressionStack);
                    break;
                }

                case OpCodeKind::Call: {
                    const OpCodeCall& call = std::get<OpCodeCall>(op.Data);
                    
                    std::string sig = reinterpret_cast<char*>(GetPointer(-(static_cast<i32>(call.ArgCount) + 1), m_FunctionStack));
                    
                    // Check if we have an external function
                    if (m_ExternalFunctions.contains(sig)) {
                        CallExtern(sig, call.ArgCount);
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

                case OpCodeKind::Ret: {
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

                CASE_UNARYEXPR(Neg, -);

                CASE_BINEXPR(Add, Add)
                CASE_BINEXPR(Sub, Sub)
                CASE_BINEXPR(Mul, Mul)
                CASE_BINEXPR(Div, Div)
                CASE_BINEXPR(Mod, Mod)

                CASE_BINEXPR_INTEGRAL(And, And)
                CASE_BINEXPR_INTEGRAL(Or, Or)
                CASE_BINEXPR_INTEGRAL(Xor, Xor)

                CASE_BINEXPR_BOOL(Cmp, Cmp)
                CASE_BINEXPR_BOOL(Ncmp, Ncmp)
                CASE_BINEXPR_BOOL(Lt, Lt)
                CASE_BINEXPR_BOOL(Lte, Lte)
                CASE_BINEXPR_BOOL(Gt, Gt)
                CASE_BINEXPR_BOOL(Gte, Gte)

                case OpCodeKind::Cast: {
                    VMType dstType = std::get<VMType>(op.Data);

                    VMSlice slice = GetVMSlice(-1, m_ExpressionStack);
                    VMType srcType = slice.Type;

                    #define CASE_CAST(srcVMType, dstVMType, srcRealType, dstRealType) case VMTypeKind::srcVMType: { \
                        srcRealType val{}; \
                        memcpy(&val, slice.Memory, slice.Size); \
                        dstRealType result = static_cast<dstRealType>(val); \
                        Pop(1, m_ExpressionStack); \
                        Alloca({ dstVMType }, m_ExpressionStack); \
                        memcpy(GetVMSlice(-1, m_ExpressionStack).Memory, &result, sizeof(result)); \
                        break; \
                    }

                    #define CASE_CAST_GROUP(dstVMType, dstRealType) case VMTypeKind::dstVMType: { \
                        switch (srcType.Kind) { \
                            CASE_CAST(I1,  VMTypeKind::dstVMType, bool, dstRealType) \
                            \
                            CASE_CAST(I8,  VMTypeKind::dstVMType, i8, dstRealType) \
                            CASE_CAST(U8,  VMTypeKind::dstVMType, u8, dstRealType) \
                            CASE_CAST(I16, VMTypeKind::dstVMType, i16, dstRealType) \
                            CASE_CAST(U16, VMTypeKind::dstVMType, u16, dstRealType) \
                            CASE_CAST(I32, VMTypeKind::dstVMType, i32, dstRealType) \
                            CASE_CAST(U32, VMTypeKind::dstVMType, u32, dstRealType) \
                            CASE_CAST(I64, VMTypeKind::dstVMType, i64, dstRealType) \
                            CASE_CAST(U64, VMTypeKind::dstVMType, u64, dstRealType) \
                            \
                            CASE_CAST(F32, VMTypeKind::dstVMType, f32, dstRealType) \
                            CASE_CAST(F64, VMTypeKind::dstVMType, f64, dstRealType) \
                            \
                            default: ARIA_UNREACHABLE(); \
                        } \
                        \
                        break; \
                    }

                    switch (dstType.Kind) {
                        CASE_CAST_GROUP(I1, bool)

                        CASE_CAST_GROUP(I8, i8)
                        CASE_CAST_GROUP(U8, u8)
                        CASE_CAST_GROUP(I16, i16)
                        CASE_CAST_GROUP(U16, u16)
                        CASE_CAST_GROUP(I32, i32)
                        CASE_CAST_GROUP(U32, u32)
                        CASE_CAST_GROUP(I64, i64)
                        CASE_CAST_GROUP(U64, u64)
                        CASE_CAST_GROUP(F32, f32)
                        CASE_CAST_GROUP(F64, f64)

                        default: ARIA_UNREACHABLE();
                    }

                    break;
                }

                case OpCodeKind::Comment: continue;
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

        return VMSlice(&stack.Stack[slot.Index], slot.Size, slot.Type);
    }

    size_t VM::GetVMTypeSize(const VMType& type) {
        switch (type.Kind) {
            case VMTypeKind::Void:   return 0;
                                     
            case VMTypeKind::I1:     return 1;
                                     
            case VMTypeKind::I8:     return 1;
            case VMTypeKind::U8:     return 1;
                                     
            case VMTypeKind::I16:    return 2;
            case VMTypeKind::U16:    return 2;
                                     
            case VMTypeKind::I32:    return 4;
            case VMTypeKind::U32:    return 4;
                                     
            case VMTypeKind::I64:    return 8;
            case VMTypeKind::U64:    return 8;
                                     
            case VMTypeKind::F32:    return 4;
            case VMTypeKind::F64:    return 8;
                                     
            case VMTypeKind::Ptr:    return sizeof(void*);

            case VMTypeKind::String: return sizeof(VMString);

            case VMTypeKind::Struct: return m_Structs.at(std::string(type.Data)).Size;

            default: ARIA_UNREACHABLE();
        }
    }

    void VM::StopExecution() {
        m_ProgramCounter = m_ProgramSize;
    }

    void VM::RunPrepass() {
        for (; m_ProgramCounter < m_ProgramSize; m_ProgramCounter++) {
            const OpCode& op = m_Program[m_ProgramCounter];

            if (op.Kind == OpCodeKind::Function) {
                size_t startPc = m_ProgramCounter;
                m_ProgramCounter++;

                const std::string& ident = std::get<std::string>(op.Data);
                VMFunction func;
                
                for (; m_ProgramCounter < m_ProgramSize; m_ProgramCounter++) {
                    const OpCode& op = m_Program[m_ProgramCounter];

                    if (op.Kind == OpCodeKind::Label) {
                        std::string label = std::get<std::string>(op.Data);
                        func.Labels[label] = m_ProgramCounter;
                    } else if (op.Kind == OpCodeKind::Function) {
                        break;
                    }
                }

                m_ProgramCounter = startPc;
                m_Functions[ident] = func;
            } else if (op.Kind == OpCodeKind::Struct) {
                const OpCodeStruct& s = std::get<OpCodeStruct>(op.Data);

                VMStruct stru;
                size_t offset = 0;
                
                for (const VMType& field : s.Fields) {
                    size_t size = AlignToEight(GetVMTypeSize(field));
                    stru.FieldOffsets.push_back(offset);
                    stru.Size += size;
                    offset += size;
                }

                m_Structs[std::string(s.Identifier)] = stru;
            }
        }

        m_ProgramCounter = 0; // Reset the program counter so the normal execution happens from the start
    }

    size_t VM::AlignToEight(size_t size) {
        return ((size + 8 - 1) / 8) * 8;
    }

} // namespace Aria::Internal