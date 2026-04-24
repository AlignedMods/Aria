#pragma once

#include "aria/internal/vm/op_codes.hpp"
#include "aria/internal/compiler/types/type_info.hpp"

#include <vector>
#include <unordered_map>

namespace Aria {
    struct Context;
    using ExternFn = void(*)(Context* ctx);
}

namespace Aria::Internal {

    // A struct the VM uses when referencing memory (stack, global, heap)
    struct VMSlice {
        void* Memory = nullptr;
        size_t Size = 0;
        VMType Type;
    };

    // Used exlusively to represent a stack slot,
    // While VMSlice could be used for this, VMSlice holds a pointer
    // However if the stack grows the memory gets reallocated so we need to store indices
    struct StackSlot {
        size_t Index = 0;
        size_t Size = 0;
        VMType Type;
    };

    // A function that is not external, AKA implemented in the language itself
    struct VMFunction {
        std::string_view Signature;
        size_t ParamCount = 0;
        std::unordered_map<std::string_view, const OpCode*> Labels;
    };

    // A structure which has a linear block of memory (the stack)
    // And stack slots which can be used to access the raw stack memory
    struct Stack {
        // The raw stack memory
        std::vector<u8> Stack;
        size_t StackPointer = 0;

        // Stack slots are essentially an abstraction over raw stack memory
        // They store basic things like an index into stack memory and the size of the slot
        std::vector<StackSlot> StackSlots;
        size_t StackSlotPointer = 0;

        inline void Reserve(size_t size, size_t slotCount) { Stack.resize(size); StackSlots.resize(slotCount); }
    };

    struct VMStackFrame {
        VMFunction* Function = nullptr;
        Stack Locals;
        const OpCode* ReturnAddress = nullptr;
    };

    class VM {
    public:
        explicit VM(Context* ctx);
        ~VM();

        // Allocates memory on a stack (local, function, global, ..)
        void Alloca(const VMType& type, Stack& stack);
        void Pop   (size_t count, Stack& stack);
        void Copy  (i32 dstSlot, i32 srcSlot, Stack& dst, Stack& src);
        void Dup   (i32 slot, Stack& dst, Stack& src);

        void AddExtern(std::string_view signature, ExternFn fn);

        void Call(const std::string& signature, size_t argCount);
        
        void StoreBool   (i32 slot, bool b,                Stack& stack);
        void StoreChar   (i32 slot, int8_t c,              Stack& stack);
        void StoreShort  (i32 slot, int16_t ch,            Stack& stack);
        void StoreInt    (i32 slot, int32_t i,             Stack& stack);
        void StoreUInt   (i32 slot, uint32_t i,            Stack& stack);
        void StoreLong   (i32 slot, int64_t l,            Stack& stack);
        void StoreULong  (i32 slot, uint64_t l,            Stack& stack);
        void StoreSize   (i32 slot, size_t sz,             Stack& stack);
        void StoreFloat  (i32 slot, float f,               Stack& stack);
        void StoreDouble (i32 slot, double d,              Stack& stack);
        void StorePointer(i32 slot, void* p,               Stack& stack);
        void StoreString (i32 slot, std::string_view str,  Stack& stack);

        bool             GetBool   (i32 slot, Stack& stack);
        int8_t           GetChar   (i32 slot, Stack& stack);
        int16_t          GetShort  (i32 slot, Stack& stack);
        int32_t          GetInt    (i32 slot, Stack& stack);
        int64_t          GetLong   (i32 slot, Stack& stack);
        size_t           GetSize   (i32 slot, Stack& stack);
        float            GetFloat  (i32 slot, Stack& stack);
        double           GetDouble (i32 slot, Stack& stack);
        void*            GetPointer(i32 slot, Stack& stack);
        std::string_view GetString (i32 slot, Stack& stack);

        void RunByteCode(const OpCodes& ops);
        void Run();

        VMSlice GetVMSlice(i32 slot, Stack& stack);
        size_t GetVMTypeSize(const VMType& type);

        void StopExecution();

    private:
        void RunPrepass();

        size_t AlignToEight(size_t size);
        
    private:
        Stack m_Stack;
        Stack m_Globals;

        std::unordered_map<std::string_view, i32> m_GlobalMap;

        const OpCodes* m_OpCodes = nullptr;
        const OpCode* m_ProgramCounter = nullptr;

        std::vector<VMStackFrame> m_StackFrames;

        std::unordered_map<size_t, size_t> m_CachedStructSize;
        std::unordered_map<std::string_view, VMFunction> m_Functions;
        std::unordered_map<std::string_view, ExternFn> m_ExternalFunctions;

        Context* m_Context = nullptr;

        friend Aria::Context;
    };

} // namespace Aria::Internal
