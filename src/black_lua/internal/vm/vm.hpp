#pragma once

#include "black_lua/internal/vm/op_codes.hpp"
#include "black_lua/core.hpp"
#include "black_lua/internal/types.hpp"
#include "black_lua/internal/compiler/core/string_view.hpp"
#include "black_lua/internal/compiler/types/type_info.hpp"

#include <vector>
#include <variant>
#include <unordered_map>

namespace BlackLua {
    struct Context;
    using ExternFn = void(*)(Context* ctx);
}

namespace BlackLua::Internal {

    struct StackSlot {
        void* Memory = nullptr;
        size_t Size = 0;
        bool ReadOnly = false;

        TypeInfo* ResolvedType = nullptr; // The VM won't directly use this however it is needed for the context to understand the types at runtime
    };

    class VM {
    public:
        explicit VM(Context* ctx);

        // Increments the stack pointer by specified amount of bytes
        // Also creates a new stack slot, which gets set as the current stack slot
        void PushBytes(size_t amount, TypeInfo* type);

        // Pops the current stack slot
        void Pop();

        // Creates a new stack frame
        void PushStackFrame();
        // Removes the current stack frame and goes back to the previous one (if there is one)
        void PopStackFrame();

        void AddExtern(const std::string& signature, ExternFn fn);

        void Call(int32_t label);
        void CallExtern(const std::string& signature);
        
        void StoreBool(StackSlotIndex slot, bool b);
        void StoreChar(StackSlotIndex slot, int8_t c);
        void StoreShort(StackSlotIndex slot, int16_t ch);
        void StoreInt(StackSlotIndex slot, int32_t i);
        void StoreLong(StackSlotIndex slot, int64_t l);
        void StoreFloat(StackSlotIndex slot, float f);
        void StoreDouble(StackSlotIndex slot, double d);
        void StorePointer(StackSlotIndex slot, void* p);

        void SetMemory(StackSlotIndex slot, void* data);
        void* GetMemory(StackSlotIndex slot);

        // Copies the memory at one slot (srcSlot) to another slot (dstSlot)
        void Copy(StackSlotIndex dstSlot, StackSlotIndex srcSlot);
        void Ref(StackSlotIndex srcSlot, TypeInfo* type);

        bool GetBool(StackSlotIndex slot);
        int8_t GetChar(StackSlotIndex slot);
        int16_t GetShort(StackSlotIndex slot);
        int32_t GetInt(StackSlotIndex slot);
        int64_t GetLong(StackSlotIndex slot);
        float GetFloat(StackSlotIndex slot);
        double GetDouble(StackSlotIndex slot);
        void* GetPointer(StackSlotIndex slot);

        // Run an array of op codes in the VM, executing each operations one at a time
        void RunByteCode(const OpCode* data, size_t count);
        void Run();

        // NOTE: The "slot" parameter can be either negative or positive
        // If it's negative, it accesses from the top of stack backwards,
        // AKA: return stack[top of stack + slot]
        // If it's positive though, it accesses from the start of the stack,
        // AKA: return stack[slot]
        StackSlot GetStackSlot(StackSlotIndex slot);
        size_t GetStackSlotIndex(int32_t slot);

        // Sets a breakpoint up for when the program counter hits a specified value
        void AddBreakPoint(int32_t pc);

        void StopExecution();

    private:
        void RegisterLables();
        
    private:
        std::vector<uint8_t> m_Stack;
        size_t m_StackPointer = 0;
        std::vector<StackSlot> m_StackSlots;
        int32_t m_StackSlotPointer = 0;

        struct StackFrame {
            size_t Offset = 0;
            size_t SlotOffset = 0;
            size_t ReturnAddress = SIZE_MAX;
            int32_t ReturnSlot = 0;
        };

        std::vector<StackFrame> m_StackFrames;
        size_t m_CurrentReturnAdress = SIZE_MAX;

        const OpCode* m_Program = nullptr;
        size_t m_ProgramSize = 0;
        size_t m_ProgramCounter = 0;

        std::unordered_map<int32_t, size_t> m_Labels;
        size_t m_LabelCount = 0;

        std::unordered_map<std::string, ExternFn> m_ExternFuncs;

        std::unordered_map<int32_t, bool> m_BreakPoints;

        Context* m_Context = nullptr;
    };

} // namespace BlackLua::Internal
