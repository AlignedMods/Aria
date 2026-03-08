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

        // Increments the stack pointer by specified amount of bytes
        // Also creates a new stack slot, which gets set as the current stack slot
        void Alloca(size_t size, TypeInfo* type);

        // Creates a new stack frame
        void PushStackFrame();
        // Removes the current stack frame and goes back to the previous one (if there is one)
        void PopStackFrame();

        void AddExtern(const std::string& signature, ExternFn fn);

        void Call(int32_t label);
        void CallExtern(const std::string& signature);
        
        void StoreBool   (MemRef mem, bool b);
        void StoreChar   (MemRef mem, int8_t c);
        void StoreShort  (MemRef mem, int16_t ch);
        void StoreInt    (MemRef mem, int32_t i);
        void StoreLong   (MemRef mem, int64_t l);
        void StoreFloat  (MemRef mem, float f);
        void StoreDouble (MemRef mem, double d);
        void StorePointer(MemRef mem, void* p);

        void Copy(MemRef dstMem, MemRef srcMem);

        bool    GetBool   (MemRef mem);
        int8_t  GetChar   (MemRef mem);
        int16_t GetShort  (MemRef mem);
        int32_t GetInt    (MemRef mem);
        int64_t GetLong   (MemRef mem);
        float   GetFloat  (MemRef mem);
        double  GetDouble (MemRef mem);
        void*   GetPointer(MemRef mem);

        // Run an array of op codes in the VM, executing each operations one at a time
        void RunByteCode(const OpCode* data, size_t count);
        void Run();

        VMSlice GetVMSlice(MemRef mem);

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
        // For local variables and temporaries
        std::vector<u8> m_Stack;
        size_t m_StackPointer = 0;
        std::vector<StackSlot> m_StackSlots;
        int32_t m_StackSlotPointer = 0;

        // NOTE: Global variables point to stack memory!
        // However specifically memory in the "_start$" function, which does not pop any stack frames
        // Only when the "_exit$" function gets called does the stack frame get popped
        // Note that _exit$ gets called when the VM gets destroyed
        std::unordered_map<std::string, StackSlot> m_GlobalMap;

        struct StackFrame {
            size_t Offset = 0;
            size_t SlotOffset = 0;

            size_t PreviousReturnAddress = SIZE_MAX;
            VMFunction* PreviousFunction = nullptr;
        };

        std::vector<StackFrame> m_StackFrames;
        size_t m_CurrentReturnAdress = SIZE_MAX;

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
