#include "aria/internal/vm/vm.hpp"
#include "aria/context.hpp"
#include "aria/internal/runtime/types.hpp"

#define PROG_SIZE() (m_OpCodes->Program.size())
#define GET_STR() (std::string_view(m_OpCodes->StringTable[static_cast<size_t>(*(++m_ProgramCounter))]))
#define GET_TYPE() (m_OpCodes->TypeTable[static_cast<size_t>(*(++m_ProgramCounter))])

namespace Aria::Internal {

    VM::VM(Context* ctx) {
        m_Stack.Reserve(static_cast<size_t>(2) * 1024, 128);
        m_Globals.Reserve(static_cast<size_t>(16) * 1024, 1024);

        m_Context = ctx;
    }

    VM::~VM() {
        const std::string& signature = "_end$()";

        ARIA_ASSERT(m_Functions.contains(signature), "No function named '_end$()' was found");
        VMFunction& func = m_Functions.at(signature);
        m_ActiveFunction = &func;

        // Perform a jump to the function
        ARIA_ASSERT(func.Labels.contains("_entry$"), "_end$() function doesn't contain a \"_entry$\" label");
        m_ProgramCounter = func.Labels.at("_entry$");
        Run();
    }

    void VM::Alloca(const VMType& type, Stack& stack) {
        size_t rawSize = GetVMTypeSize(type);
        size_t alignedSize = AlignToEight(rawSize);
        
        ARIA_ASSERT(alignedSize % 8 == 0, "Memory not aligned to 8 bytes correctly!");
        ARIA_ASSERT(stack.StackPointer + alignedSize < stack.Stack.max_size(), "Stack overflow, allocating an insane amount of memory!");

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

        ARIA_ASSERT(dstSlice.Size == srcSlice.Size, "Invalid VM::Copy() call, sizes of both sides must be the same!");

        memcpy(dstSlice.Memory, srcSlice.Memory, srcSlice.Size);
    }

    void VM::Dup(i32 slot, Stack& dst, Stack& src) {
        VMSlice srcSlot = GetVMSlice(slot, src);

        Alloca(srcSlot.Type, dst);
        VMSlice dstSlot = GetVMSlice(-1, dst);
        
        memcpy(dstSlot.Memory, srcSlot.Memory, srcSlot.Size);
    }

    void VM::AddExtern(std::string_view signature, ExternFn fn) {
        m_ExternalFunctions[signature] = fn;
    }

    void VM::Call(const std::string& signature, size_t argCount) {
        ARIA_TODO("VM::Call()");

        // ARIA_ASSERT(m_Functions.contains(signature), "Calling unknown function");
        // VMFunction& func = m_Functions.at(signature);
        // 
        // // This function can only be called when the program is in a "halted" state AKA doing nothing
        // // Therefore when we finish with execution we want to go back to that state
        // m_StackFrames.back().PreviousReturnAddress = PROG_SIZE();
        // m_StackFrames.back().PreviousFunction = nullptr;
        // m_ReturnAddress = PROG_SIZE();
        // 
        // // Save function stack
        // m_StackFrames.back().PFSSBP = m_FunctionStack.StackSlotBasePointer;
        // m_StackFrames.back().PFSBP = m_FunctionStack.StackBasePointer;
        // 
        // // Perform a jump to the function
        // ARIA_ASSERT(func.Labels.contains("_entry$"), "All functions must contain a \"_entry$\" label");
        // m_ProgramCounter = func.Labels.at("_entry$");
        // m_ActiveFunction = &func;
        // 
        // // Set up the function stack
        // m_FunctionStack.StackSlotBasePointer = m_FunctionStack.StackSlotPointer - argCount - 1;
        // m_FunctionStack.StackBasePointer = m_FunctionStack.StackSlots[m_FunctionStack.StackSlotBasePointer].Index;
        // 
        // Run();
    }

    void VM::CallExtern(const std::string& signature, size_t argCount) {
        ARIA_TODO("VM::Call()");
        // ARIA_ASSERT(m_ExternalFunctions.contains(signature), "Calling CallExtern() on a non-existent extern function!");
        // 
        // // Set up the function stack
        // m_FunctionStack.StackSlotBasePointer = m_FunctionStack.StackSlotPointer - argCount - 1;
        // m_FunctionStack.StackBasePointer = m_FunctionStack.StackSlots[m_FunctionStack.StackSlotBasePointer].Index;
        // 
        // // Do the call
        // m_ExternalFunctions.at(signature)(m_Context);
        // 
        // // Cleanup the stack
        // m_FunctionStack.StackSlotPointer = m_FunctionStack.StackSlotBasePointer;
        // m_FunctionStack.StackPointer = m_FunctionStack.StackBasePointer;
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

    void VM::StoreUInt(i32 slot, uint32_t i, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);
        ARIA_ASSERT(s.Size == 4, "Cannot store a uint in a slot with a size that isn't 4!");
        memcpy(s.Memory, &i, 4);
    }

    void VM::StoreLong(i32 slot, int64_t l, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);
        ARIA_ASSERT(s.Size == 8, "Cannot store a long in a slot with a size that isn't 8!");
        memcpy(s.Memory, &l, 8);
    }

    void VM::StoreULong(i32 slot, uint64_t l, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);
        ARIA_ASSERT(s.Size == 8, "Cannot store a ulong in a slot with a size that isn't 8!");
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

    void VM::StoreString(i32 slot, std::string_view str, Stack& stack) {
        VMSlice s = GetVMSlice(slot, stack);
        ARIA_ASSERT(s.Type.Kind == VMTypeKind::String, "Cannot store a string in a slot with a non-string type");
        
        RuntimeString& string = *reinterpret_cast<RuntimeString*>(s.Memory);
        string.RawData = str.data();
        string.Size = str.size();
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

        RuntimeString str;
        memcpy(&str, s.Memory, s.Size);

        return { str.RawData, str.Size };
    }

    void VM::RunByteCode(const OpCodes& ops) {
        m_OpCodes = &ops;
        m_ProgramCounter = 0;

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
        for (; m_ProgramCounter < &m_OpCodes->Program.back(); m_ProgramCounter++) {
            switch (*m_ProgramCounter) {
                case OP_ALLOCA: {
                    auto& type = GET_TYPE();
                    Alloca(type, m_Stack);
                    break;
                }

                case OP_LD_CONST: {
                    auto& type = GET_TYPE();
                    Alloca(type, m_Stack);
                    VMSlice slice = GetVMSlice(-1, m_Stack);

                    switch (type.Kind) {
                        case VMTypeKind::I1: memcpy(slice.Memory, ++m_ProgramCounter, sizeof(bool)); break;

                        case VMTypeKind::I8:
                        case VMTypeKind::U8: memcpy(slice.Memory, ++m_ProgramCounter, sizeof(u8)); break;
                        case VMTypeKind::I16:
                        case VMTypeKind::U16: memcpy(slice.Memory, ++m_ProgramCounter, sizeof(u16)); break;
                        case VMTypeKind::I32:
                        case VMTypeKind::U32: memcpy(slice.Memory, ++m_ProgramCounter, sizeof(u32)); m_ProgramCounter += 1; break;
                        case VMTypeKind::I64:
                        case VMTypeKind::U64: memcpy(slice.Memory, ++m_ProgramCounter, sizeof(u64)); m_ProgramCounter += 3; break;

                        case VMTypeKind::Float: memcpy(slice.Memory, ++m_ProgramCounter, sizeof(float)); m_ProgramCounter += 1; break;
                        case VMTypeKind::Double: memcpy(slice.Memory, ++m_ProgramCounter, sizeof(double)); m_ProgramCounter += 3; break;

                        default: ARIA_UNREACHABLE();
                    }

                    break;
                }

                case OP_LD_STR: {
                    Alloca({ VMTypeKind::String }, m_Stack);

                    std::string_view str = GET_STR();

                    RuntimeString vmstr;
                    // Allocate the string on the heap
                    char* newStr = new char[str.size()];
                    memcpy(newStr, str.data(), str.size());

                    vmstr.RawData = newStr;
                    vmstr.Size = str.size();

                    Alloca({ VMTypeKind::String }, m_Stack);
                    memcpy(GetVMSlice(-1, m_Stack).Memory, &vmstr, sizeof(vmstr));

                    break;
                }

                case OP_LD_LOCAL: {
                    size_t index = static_cast<size_t>(*++m_ProgramCounter);
                    Dup(index, m_Stack, m_StackFrames.back().Locals);
                    break;
                }

                case OP_LD_GLOBAL: {
                    std::string_view g = GET_STR();
                    Dup(m_GlobalMap.at(g), m_Stack, m_Globals);
                    break;
                }

                case OP_LD_PTR_LOCAL: {
                    size_t index = static_cast<size_t>(*++m_ProgramCounter);
                    VMSlice slice = GetVMSlice(index, m_StackFrames.back().Locals);
                    Alloca({ VMTypeKind::Ptr }, m_Stack);
                    StorePointer(-1, slice.Memory, m_Stack);
                    break;
                }

                case OP_LD_PTR_GLOBAL: {
                    std::string_view g = GET_STR();
                    VMSlice slice = GetVMSlice(m_GlobalMap.at(g), m_Globals);
                    Alloca({ VMTypeKind::Ptr }, m_Stack);
                    StorePointer(-1, slice.Memory, m_Stack);
                    break;
                }

                case OP_POP: {
                    Pop(1, m_Stack);
                    break;
                }

                case OP_DECL_LOCAL: {
                    size_t index = static_cast<size_t>(*++m_ProgramCounter);

                    VMSlice src = GetVMSlice(-1, m_Stack);

                    Alloca(src.Type, m_StackFrames.back().Locals);
                    VMSlice dst = GetVMSlice(-1, m_StackFrames.back().Locals);

                    memcpy(dst.Memory, src.Memory, src.Size);
                    Pop(1, m_Stack);
                    break;
                };

                case OP_DECL_GLOBAL: {
                    std::string_view g = GET_STR();

                    VMSlice src = GetVMSlice(-1, m_Stack);

                    Alloca(src.Type, m_Globals);
                    VMSlice dst = GetVMSlice(-1, m_Globals);

                    memcpy(dst.Memory, src.Memory, src.Size);
                    Pop(1, m_Stack);

                    m_GlobalMap[g] = { static_cast<i32>(m_Globals.StackSlotPointer) - 1 };
                    break;
                };

                // VVV ADD, SUB, MUL, DIV, MOD VVV //
                case OP_ADDI: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal + rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreInt(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal + rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreLong(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to addi instruction");
                    }

                    break;
                }

                case OP_ADDU: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal + rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreUInt(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal + rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreULong(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to addu instruction");
                    }

                    break;
                }

                case OP_ADDF: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal + rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreFloat(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal + rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreDouble(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to addf instruction");
                    }

                    break;
                }

                case OP_SUBI: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal - rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreInt(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal - rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreLong(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to subi instruction");
                    }

                    break;
                }

                case OP_SUBU: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal - rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreUInt(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal - rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreULong(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to subu instruction");
                    }

                    break;
                }

                case OP_SUBF: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal - rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreFloat(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal - rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreDouble(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to subf instruction");
                    }

                    break;
                }

                case OP_MULI: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal * rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreInt(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal * rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreLong(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to muli instruction");
                    }

                    break;
                }

                case OP_MULU: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal * rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreUInt(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal * rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreULong(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to mulu instruction");
                    }

                    break;
                }

                case OP_MULF: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal * rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreFloat(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal * rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreDouble(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to mulf instruction");
                    }

                    break;
                }

                case OP_DIVI: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            // TODO: check if rhsVal is zero...
                            auto result = lhsVal / rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreInt(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            // TODO: check if rhsVal is zero...
                            auto result = lhsVal / rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreLong(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to divi instruction");
                    }

                    break;
                }

                case OP_DIVU: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            // TODO: check if rhsVal is zero...
                            auto result = lhsVal / rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreUInt(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            // TODO: check if rhsVal is zero...
                            auto result = lhsVal / rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreULong(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to divu instruction");
                    }

                    break;
                }

                case OP_DIVF: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal / rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreFloat(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal / rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreDouble(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to divf instruction");
                    }

                    break;
                }

                case OP_MODI: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            // TODO: check if rhsVal is zero...
                            auto result = lhsVal % rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreInt(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            // TODO: check if rhsVal is zero...
                            auto result = lhsVal % rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreLong(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to modi instruction");
                    }

                    break;
                }

                case OP_MODU: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            // TODO: check if rhsVal is zero...
                            auto result = lhsVal % rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreUInt(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            // TODO: check if rhsVal is zero...
                            auto result = lhsVal % rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreULong(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to modu instruction");
                    }

                    break;
                }

                case OP_MODF: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = fmodf(lhsVal, rhsVal);
                            if (result < 0.0f) { result += fabsf(rhsVal); }

                            Alloca(lhs.Type, m_Stack);
                            StoreFloat(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = fmod(lhsVal, rhsVal);
                            if (result < 0.0) { result += fabsf(rhsVal); }

                            Alloca(lhs.Type, m_Stack);
                            StoreDouble(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to modf instruction");
                    }

                    break;
                }
                // ^^^ ADD, SUB, MUL, DIV, MOD ^^^ //

                // VVV CMP, LT, LTE, GT, GTE VVV //
                case OP_CMPI: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal == rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal == rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to cmpi instruction");
                    }

                    break;
                }

                case OP_CMPU: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal == rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal == rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to cmpu instruction");
                    }

                    break;
                }

                case OP_CMPF: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal == rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal == rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to cmpf instruction");
                    }

                    break;
                }

                case OP_LTI: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal < rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal < rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to lti instruction");
                    }

                    break;
                }

                case OP_LTU: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal < rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal < rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to ltu instruction");
                    }

                    break;
                }

                case OP_LTF: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal < rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal < rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to ltf instruction");
                    }

                    break;
                }

                case OP_LTEI: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal <= rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal <= rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to ltei instruction");
                    }

                    break;
                }

                case OP_LTEU: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal <= rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal <= rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to lteu instruction");
                    }

                    break;
                }

                case OP_LTEF: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal <= rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal <= rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to ltef instruction");
                    }

                    break;
                }

                case OP_GTI: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal > rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal > rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to gti instruction");
                    }

                    break;
                }

                case OP_GTU: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal > rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal > rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to gtu instruction");
                    }

                    break;
                }

                case OP_GTF: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal > rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal > rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to gtf instruction");
                    }

                    break;
                }

                case OP_GTEI: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal >= rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal >= rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to gtei instruction");
                    }

                    break;
                }

                case OP_GTEU: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal >= rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal >= rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to gteu instruction");
                    }

                    break;
                }

                case OP_GTEF: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal >= rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal >= rhsVal;

                            Alloca({ VMTypeKind::I1 }, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to gtef instruction");
                    }

                    break;
                }
                // ^^^ CMP, LT, LTE, GT, GTE ^^^ //

                // VVV SHL, SHR, AND, OR, XOR VVV ///
                case OP_SHLI: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal << rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal << rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to shli instruction");
                    }

                    break;
                }

                case OP_SHLU: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal << rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal << rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to shlu instruction");
                    }

                    break;
                }

                case OP_SHRI: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal >> rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal >> rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to shri instruction");
                    }

                    break;
                }

                case OP_SHRU: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal >> rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal >> rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to shru instruction");
                    }

                    break;
                }

                case OP_ANDI: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal & rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal & rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to andi instruction");
                    }

                    break;
                }

                case OP_ANDU: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal & rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal & rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to andu instruction");
                    }

                    break;
                }

                case OP_ORI: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal | rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal | rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to ori instruction");
                    }

                    break;
                }

                case OP_ORU: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal | rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal | rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to oru instruction");
                    }

                    break;
                }

                case OP_XORI: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal ^ rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal ^ rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to xori instruction");
                    }

                    break;
                }

                case OP_XORU: {
                    VMSlice lhs = GetVMSlice(-2, m_Stack);
                    VMSlice rhs = GetVMSlice(-1, m_Stack);

                    ARIA_ASSERT(lhs.Type.Kind == rhs.Type.Kind, "Types of both sides must be the same");

                    switch (lhs.Type.Kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal ^ rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.Memory, lhs.Size);
                            memcpy(&rhsVal, rhs.Memory, rhs.Size);
                            Pop(2, m_Stack);

                            auto result = lhsVal ^ rhsVal;

                            Alloca(lhs.Type, m_Stack);
                            StoreBool(-1, lhsVal, m_Stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to xoru instruction");
                    }

                    break;
                }
                // ^^^ SHL, SHR, AND, OR, XOR ^^^ //

                case OP_LOGAND: {
                    bool lhs = GetBool(-2, m_Stack);
                    bool rhs = GetBool(-1, m_Stack);
                    Pop(2, m_Stack);

                    Alloca({ VMTypeKind::I1 }, m_Stack);
                    StoreBool(-1, lhs && rhs, m_Stack);
                    break;
                }

                case OP_LOGOR: {
                    bool lhs = GetBool(-2, m_Stack);
                    bool rhs = GetBool(-1, m_Stack);
                    Pop(2, m_Stack);

                    Alloca({ VMTypeKind::I1 }, m_Stack);
                    StoreBool(-1, lhs || rhs, m_Stack);
                    break;
                }

                case OP_LOGNOT: {
                    bool val = GetBool(-1, m_Stack);
                    StoreBool(-1, !val, m_Stack);
                    break;
                }

                default: ARIA_UNREACHABLE();
            }
        }
    }
    
    VMSlice VM::GetVMSlice(i32 index, Stack& stack) {
        StackSlot slot;

        if (index < 0) {
            slot = stack.StackSlots[stack.StackSlotPointer + index];
        } else {
            slot = stack.StackSlots[index];
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
                                     
            case VMTypeKind::Float:  return 4;
            case VMTypeKind::Double: return 8;
                                     
            case VMTypeKind::Ptr:    return sizeof(void*);

            case VMTypeKind::String: return sizeof(RuntimeString);

            case VMTypeKind::Struct: {
                ARIA_TODO("struct size");
                // if (m_CachedStructSize.contains(type.Data)) { return m_CachedStructSize.at(type.Data); }
                // 
                // size_t size = 0;
                // const VMStruct& str = m_Program->StructTable[type.Data];
                // for (size_t field : str.Fields) {
                //     size += AlignToEight(GetVMTypeSize(GET_TYPE(field)));
                // }
                // 
                // m_CachedStructSize[type.Data] = size;
                // return size;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void VM::StopExecution() {
        m_ProgramCounter = &m_OpCodes->Program.back();
    }

    void VM::RunPrepass() {
        for (; m_ProgramCounter < &m_OpCodes->Program.back(); m_ProgramCounter++) {
            if (*m_ProgramCounter == OP_FUNCTION) {
                const OpCode* startPc = m_ProgramCounter;
                m_ProgramCounter++;
                
                std::string_view ident = GET_STR(op.Args[0].Index);
                VMFunction func;
                
                for (; m_ProgramCounter < &m_OpCodes->Program.back(); m_ProgramCounter++) {
                    if (*m_ProgramCounter == OP_LABEL) {
                        std::string_view label = GET_STR(op.Args[0].Index);
                        func.Labels[label] = m_ProgramCounter;
                    } else if (*m_ProgramCounter == OP_ENDFUNCTION) {
                        break;
                    }
                }

                m_ProgramCounter = startPc;
                m_Functions[ident] = func;
            }
        }

        m_ProgramCounter = 0; // Reset the program counter so the normal execution happens from the start
    }

    size_t VM::AlignToEight(size_t size) {
        return ((size + 8 - 1) / 8) * 8;
    }

} // namespace Aria::Internal
