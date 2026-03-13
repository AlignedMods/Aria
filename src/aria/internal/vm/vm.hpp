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

        TypeInfo* ResolvedType = nullptr; // The VM won't directly use this however it is needed for the context to understand the types at runtime
    };

    // Used exlusively to represent a stack slot,
    // While VMSlice could be used for this, VMSlice holds a pointer
    // However if the stack grows the memory gets reallocated so we need to store indices
    struct StackSlot {
        size_t Index = 0;
        size_t Size = 0;
    };

    // A function that is not external, AKA implemented in the language itself
    struct VMFunction {
        std::unordered_map<std::string, size_t> Labels;
    };

    class VM {
    public:
        explicit VM(Context* ctx);

        void Alloca(size_t size, TypeInfo* type);
        void Pop(size_t count);
        void Copy (i32 dstSlot, i32 srcSlot);
        void Dup  (i32 slot);

        // Creates a new stack frame
        void PushStackFrame();
        // Removes the current stack frame and goes back to the previous one (if there is one)
        void PopStackFrame();

        void AddExtern(const std::string& signature, ExternFn fn);

        void Call(int32_t label);
        void CallExtern(const std::string& signature, size_t argCount, size_t retCount);
        
        void StoreBool   (i32 slot, bool b);
        void StoreChar   (i32 slot, int8_t c);
        void StoreShort  (i32 slot, int16_t ch);
        void StoreInt    (i32 slot, int32_t i);
        void StoreLong   (i32 slot, int64_t l);
        void StoreSize   (i32 slot, size_t sz);
        void StoreFloat  (i32 slot, float f);
        void StoreDouble (i32 slot, double d);
        void StorePointer(i32 slot, void* p);

        bool    GetBool   (i32 slot);
        int8_t  GetChar   (i32 slot);
        int16_t GetShort  (i32 slot);
        int32_t GetInt    (i32 slot);
        int64_t GetLong   (i32 slot);
        size_t  GetSize   (i32 slot);
        float   GetFloat  (i32 slot);
        double  GetDouble (i32 slot);
        void*   GetPointer(i32 slot);

        // Run an array of op codes in the VM, executing each operations one at a time
        void RunByteCode(const OpCode* data, size_t count);
        void Run();

        VMSlice GetVMSlice(i32 slot);

        void StopExecution();

    private:
        // Registers all the functions and labels before running any byte code
        // This is neccessary because it is possible to jump to labels and call functions
        // that have not been declared yet
        // eg.
        // _entry$:
        //     jmp loop.end
        //     ...
        //
        // loop.end:
        //     ...
        void RunPrepass();
        
    private:
        // The raw stack memory
        // The stack is used for pretty much everything in the language
        // Even global variables are stored on the stack (a part of the stack that doesn't get freed until the end of the program)
        std::vector<u8> m_Stack;
        size_t m_StackPointer = 0;

        // Stack slots are essentially an abstraction over raw stack memory
        // They store basic things like an index into stack memory and the size of the slot
        std::vector<StackSlot> m_StackSlots;
        int32_t m_StackSlotPointer = 0;

        // A map of all global variables
        // These variables are still stored on the stack however they aren't freed until the end of the program
        std::unordered_map<std::string, i32> m_GlobalMap;

        struct StackFrame {
            size_t Offset = 0;
            size_t SlotOffset = 0;

            // A map of all local variables (including function parameters)
            // These variables are only valid during a single stack frame
            std::unordered_map<size_t, i32> LocalMap;

            size_t PreviousReturnAddress = SIZE_MAX;
            VMFunction* PreviousFunction = nullptr;
        };

        std::vector<StackFrame> m_StackFrames;

        const OpCode* m_Program = nullptr;
        size_t m_ProgramSize = 0;
        size_t m_ProgramCounter = 0;

        std::unordered_map<std::string, VMFunction> m_Functions;
        std::unordered_map<std::string, ExternFn> m_ExternalFunctions;

        size_t m_ReturnAddress = SIZE_MAX;
        VMFunction* m_ActiveFunction = nullptr;

        Context* m_Context = nullptr;
    };

} // namespace Aria::Internal
