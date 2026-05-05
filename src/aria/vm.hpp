#pragma once

#include "common/op_codes.hpp"

#include <vector>
#include <unordered_map>

namespace Aria {
    struct Context;
    using ExternFn = void(*)(Context* ctx);
}

namespace Aria::Internal {

    // A struct the VM uses when referencing memory (stack, global, heap)
    struct VMSlice {
        void* memory = nullptr;
        size_t size = 0;
        VMType type;
    };

    // Used exlusively to represent a stack slot,
    // While VMSlice could be used for this, VMSlice holds a pointer
    // However if the stack grows the memory gets reallocated so we need to store indices
    struct StackSlot {
        size_t index = 0;
        size_t size = 0;
        VMType type;
    };

    // A function that is not external, AKA implemented in the language itself
    struct VMFunction {
        std::string_view signature;
        std::unordered_map<std::string_view, const OpCode*> labels;
    };

    // A structure which has a linear block of memory (the stack)
    // And stack slots which can be used to access the raw stack memory
    struct Stack {
        // The raw stack memory
        std::vector<u8> stack;
        size_t stack_pointer = 0;

        // Stack slots are essentially an abstraction over raw stack memory
        // They store basic things like an index into stack memory and the size of the slot
        std::vector<StackSlot> stack_slots;
        size_t stack_slot_pointer = 0;

        inline void reserve(size_t size, size_t slot_count) { stack.resize(size); stack_slots.resize(slot_count); }
    };

    struct VMStackFrame {
        VMFunction* function = nullptr;
        Stack locals;
        const OpCode* return_address = nullptr;
    };

    class VM {
    public:
        explicit VM(Context* ctx);
        ~VM();

        void alloc(const VMType& type, Stack& stack);
        void pop  (size_t count, Stack& stack);
        void copy (i32 dstSlot, i32 srcSlot, Stack& dst, Stack& src);
        void dup  (i32 slot, Stack& dst, Stack& src);

        void add_extern(std::string_view signature, ExternFn fn);

        void call(const std::string& signature, size_t arg_count);
        
        void store_bool   (i32 slot, bool b,                Stack& stack);
        void store_char   (i32 slot, int8_t c,              Stack& stack);
        void store_short  (i32 slot, int16_t ch,            Stack& stack);
        void store_int    (i32 slot, int32_t i,             Stack& stack);
        void store_uint   (i32 slot, uint32_t i,            Stack& stack);
        void store_long   (i32 slot, int64_t l,             Stack& stack);
        void store_ulong  (i32 slot, uint64_t l,            Stack& stack);
        void store_size   (i32 slot, size_t sz,             Stack& stack);
        void store_float  (i32 slot, float f,               Stack& stack);
        void store_double (i32 slot, double d,              Stack& stack);
        void store_pointer(i32 slot, void* p,               Stack& stack);
        void store_string (i32 slot, std::string_view str,  Stack& stack);

        bool             get_bool   (i32 slot, Stack& stack);
        int8_t           get_char   (i32 slot, Stack& stack);
        int16_t          get_short  (i32 slot, Stack& stack);
        int32_t          get_int    (i32 slot, Stack& stack);
        uint32_t         get_uint   (i32 slot, Stack& stack);
        int64_t          get_long   (i32 slot, Stack& stack);
        uint64_t         get_ulong  (i32 slot, Stack& stack);
        size_t           get_size   (i32 slot, Stack& stack);
        float            get_float  (i32 slot, Stack& stack);
        double           get_double (i32 slot, Stack& stack);
        void*            get_pointer(i32 slot, Stack& stack);
        std::string_view get_string (i32 slot, Stack& stack);

        void run_byte_code(const OpCodes& ops);
        void run();

        VMSlice get_vm_slice(i32 slot, Stack& stack);
        size_t get_vm_type_size(const VMType& type);

        void stop_execution();

    private:
        void run_prepass();

        size_t align_to_eight(size_t size);
        
    private:
        Stack m_stack;
        Stack m_globals;

        std::unordered_map<std::string_view, i32> m_global_map;

        const OpCodes* m_op_codes = nullptr;
        const OpCode* m_program_counter = nullptr;

        std::vector<VMStackFrame> m_stack_frames;

        std::unordered_map<std::string_view, VMFunction> m_functions;
        std::unordered_map<std::string_view, ExternFn> m_external_functions;

        Context* m_context = nullptr;

        friend Aria::Context;
    };

} // namespace Aria::Internal
