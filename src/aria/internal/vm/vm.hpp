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
        std::unordered_map<std::string, size_t> Labels;
    };

    // A structure which has a linear block of memory (the stack)
    // And stack slots which can be used to access the raw stack memory
    struct Stack {
        // The raw stack memory
        std::vector<u8> Stack;
        size_t StackPointer = 0;
        size_t StackBasePointer = 0;

        // Stack slots are essentially an abstraction over raw stack memory
        // They store basic things like an index into stack memory and the size of the slot
        std::vector<StackSlot> StackSlots;
        size_t StackSlotPointer = 0;
        size_t StackSlotBasePointer = 0;

        inline void Reserve(size_t size, size_t slotCount) { Stack.resize(size); StackSlots.resize(slotCount); }
    };

    class VM {
    public:
        explicit VM(Context* ctx);

        // Allocates memory on a stack (local, function, global, ..)
        void Alloca(VMType type, Stack& stack);
        void Pop   (size_t count, Stack& stack);
        void Copy  (i32 dstSlot, i32 srcSlot, Stack& dst, Stack& src);
        void Dup   (i32 slot, Stack& dst, Stack& src);

        // Creates a new stack frame
        void PushStackFrame();
        // Removes the current stack frame and goes back to the previous one (if there is one)
        void PopStackFrame();

        void AddExtern(const std::string& signature, ExternFn fn);

        void Call(const std::string& signature, size_t argCount);
        void CallExtern(const std::string& signature, size_t argCount);
        
        void StoreBool   (i32 slot, bool b    , Stack& stack);
        void StoreChar   (i32 slot, int8_t c  , Stack& stack);
        void StoreShort  (i32 slot, int16_t ch, Stack& stack);
        void StoreInt    (i32 slot, int32_t i , Stack& stack);
        void StoreLong   (i32 slot, int64_t l , Stack& stack);
        void StoreSize   (i32 slot, size_t sz , Stack& stack);
        void StoreFloat  (i32 slot, float f   , Stack& stack);
        void StoreDouble (i32 slot, double d  , Stack& stack);
        void StorePointer(i32 slot, void* p   , Stack& stack);

        bool    GetBool   (i32 slot, Stack& stack);
        int8_t  GetChar   (i32 slot, Stack& stack);
        int16_t GetShort  (i32 slot, Stack& stack);
        int32_t GetInt    (i32 slot, Stack& stack);
        int64_t GetLong   (i32 slot, Stack& stack);
        size_t  GetSize   (i32 slot, Stack& stack);
        float   GetFloat  (i32 slot, Stack& stack);
        double  GetDouble (i32 slot, Stack& stack);
        void*   GetPointer(i32 slot, Stack& stack);

        // Run an array of op codes in the VM, executing each operations one at a time
        void RunByteCode(const OpCode* data, size_t count);
        void Run();

        VMSlice GetVMSlice(i32 slot, Stack& stack);
        size_t GetVMTypeSize(const VMType& type);

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
        Stack m_LocalStack; // Used for things like expressions and local variables
        Stack m_FunctionStack; // Used to store data about function calls
        Stack m_GlobalStack; // Used for global variables

        std::unordered_map<std::string, i32> m_GlobalMap;

        struct StackFrame {
            size_t PLSSBP = 0; // PreviousLocalStackSlotBasePointer
            size_t PLSBP = 0;  // PreviousLocalStackBasePointer

            size_t PFSSBP = 0; // PreviousFunctionStackSlotBasePointer
            size_t PFSBP = 0; // PreviousFunctionStackBasePointer

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

        friend Aria::Context;
    };

} // namespace Aria::Internal
