#include "aria/internal/vm/vm.hpp"
#include "aria/context.hpp"
#include "aria/internal/runtime/types.hpp"

#define PROG_SIZE() (m_op_codes->program.size())
#define GET_STR() (std::string_view(m_op_codes->string_table[static_cast<size_t>(*(++m_program_counter))]))
#define GET_TYPE() (m_op_codes->type_table[static_cast<size_t>(*(++m_program_counter))])

namespace Aria::Internal {

    VM::VM(Context* ctx) {
        m_stack.reserve(static_cast<size_t>(2) * 1024, 128);
        m_globals.reserve(static_cast<size_t>(16) * 1024, 1024);

        m_context = ctx;
    }

    VM::~VM() {
        const std::string& signature = "_end$()";

        if (!m_functions.contains(signature)) { return; }

        VMFunction& func = m_functions.at(signature);

        // Perform a jump to the function
        ARIA_ASSERT(func.labels.contains("_entry$"), "_end$() function doesn't contain a \"_entry$\" label");
        m_program_counter = func.labels.at("_entry$");

        VMStackFrame sf;
        sf.function = &func;
        sf.return_address = &m_op_codes->program.back() + 1;
        m_stack_frames.push_back(sf);
        run();
    }

    void VM::alloc(const VMType& type, Stack& stack) {
        size_t rawSize = get_vm_type_size(type);
        size_t alignedSize = align_to_eight(rawSize);
        
        ARIA_ASSERT(alignedSize % 8 == 0, "Memory not aligned to 8 bytes correctly!");
        ARIA_ASSERT(stack.stack_pointer + alignedSize < stack.stack.max_size(), "Stack overflow, allocting an insane amount of memory!");

        stack.stack_pointer += alignedSize;

        if (stack.stack.size() == 0) {
            stack.reserve(static_cast<size_t>(2) * 1024, 128);
        }

        while (stack.stack_pointer >= stack.stack.size()) {
            stack.stack.resize(stack.stack.size() * 2);
        }

        // Add more stack slots if needed
        if (stack.stack_slot_pointer + 1 >= stack.stack_slots.size()) {
            stack.stack_slots.resize(stack.stack_slots.size() * 2);
        }
        
        stack.stack_slots[stack.stack_slot_pointer++] = { stack.stack_pointer - alignedSize, rawSize, type };
    }

    void VM::pop(size_t count, Stack& stack) {
        size_t sp = stack.stack_pointer;
        stack.stack_pointer = stack.stack_slots[stack.stack_slot_pointer - count].index;
        stack.stack_slot_pointer -= count;
    }

    void VM::copy(i32 dstSlot, i32 srcSlot, Stack& dst, Stack& src) {
        VMSlice dstSlice = get_vm_slice(dstSlot, dst);
        VMSlice srcSlice = get_vm_slice(srcSlot, src);

        ARIA_ASSERT(dstSlice.size == srcSlice.size, "Invalid VM::Copy() call, sizes of both sides must be the same!");

        memcpy(dstSlice.memory, srcSlice.memory, srcSlice.size);
    }

    void VM::dup(i32 slot, Stack& dst, Stack& src) {
        VMSlice srcSlot = get_vm_slice(slot, src);

        alloc(srcSlot.type, dst);
        VMSlice dstSlot = get_vm_slice(-1, dst);
        
        memcpy(dstSlot.memory, srcSlot.memory, srcSlot.size);
    }

    void VM::add_extern(std::string_view signature, ExternFn fn) {
        m_external_functions[signature] = fn;
    }

    void VM::call(const std::string& signature, size_t argCount) {
        ARIA_ASSERT(m_functions.contains(signature), "Calling unknown function");
        VMFunction& func = m_functions.at(signature);
        
        ARIA_ASSERT(func.labels.contains("_entry$"), "All functions must contain a \"_entry$\" label");
        m_program_counter = func.labels.at("_entry$");

        VMStackFrame sf;
        sf.function = &func;
        sf.return_address = &m_op_codes->program.back() + 1;
        m_stack_frames.push_back(sf);
        run();
    }

    void VM::store_bool(i32 slot, bool b, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);
        ARIA_ASSERT(s.size == 1, "Cannot store a bool in a slot with a size that isn't 1!");
        int8_t bb = static_cast<int8_t>(b);
        memcpy(s.memory, &bb, 1);
    }

    void VM::store_char(i32 slot, int8_t c, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);
        ARIA_ASSERT(s.size == 1, "Cannot store a char in a slot with a size that isn't 1!");
        memcpy(s.memory, &c, 1);
    }

    void VM::store_short(i32 slot, int16_t sh, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);
        ARIA_ASSERT(s.size == 2, "Cannot store a short in a slot with a size that isn't 2!");
        memcpy(s.memory, &sh, 2);
    }

    void VM::store_int(i32 slot, int32_t i, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);
        ARIA_ASSERT(s.size == 4, "Cannot store an int in a slot with a size that isn't 4!");
        memcpy(s.memory, &i, 4);
    }

    void VM::store_uint(i32 slot, uint32_t i, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);
        ARIA_ASSERT(s.size == 4, "Cannot store a uint in a slot with a size that isn't 4!");
        memcpy(s.memory, &i, 4);
    }

    void VM::store_long(i32 slot, int64_t l, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);
        ARIA_ASSERT(s.size == 8, "Cannot store a long in a slot with a size that isn't 8!");
        memcpy(s.memory, &l, 8);
    }

    void VM::store_ulong(i32 slot, uint64_t l, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);
        ARIA_ASSERT(s.size == 8, "Cannot store a ulong in a slot with a size that isn't 8!");
        memcpy(s.memory, &l, 8);
    }

    void VM::store_size(i32 slot, size_t sz, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);
        ARIA_ASSERT(s.size == sizeof(size_t), "Cannot store a size_t in a slot with a size that isn't sizeof(size_t)!");
        memcpy(s.memory, &sz, sizeof(size_t));
    }

    void VM::store_float(i32 slot, float f, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);
        ARIA_ASSERT(s.size == 4, "Cannot store a float in a slot with a size that isn't 4!");
        memcpy(s.memory, &f, 4);
    }

    void VM::store_double(i32 slot, double d, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);
        ARIA_ASSERT(s.size == 8, "Cannot store a double in a slot with a size that isn't 8!");
        memcpy(s.memory, &d, 8);
    }

    void VM::store_pointer(i32 slot, void* p, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);
        ARIA_ASSERT(s.size == sizeof(void*), "Cannot store a double in a slot with a size that isn't sizeof(void*)!");
        memcpy(s.memory, &p, sizeof(void*));
    }

    void VM::store_string(i32 slot, std::string_view str, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);
        ARIA_ASSERT(s.type.kind == VMTypeKind::Slice, "Cannot store a string in a slot with a non-slice type");
        
        RuntimeSlice& slice = *reinterpret_cast<RuntimeSlice*>(s.memory);
        slice.mem = const_cast<char*>(str.data());
        slice.size = str.size();
    }

    bool VM::get_bool(i32 slot, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);

        ARIA_ASSERT(s.size == 1, "Invalid get_bool() call!");

        bool b = false;
        memcpy(&b, s.memory, 1);
        return b;
    }

    int8_t VM::get_char(i32 slot, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);

        ARIA_ASSERT(s.size == 1, "Invalid get_char() call!");

        int8_t c = 0;
        memcpy(&c, s.memory, 1);
        return c;
    }

    int16_t VM::get_short(i32 slot, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);

        ARIA_ASSERT(s.size == 2, "Invalid get_short() call!");

        int16_t sh = 0;
        memcpy(&sh, s.memory, 2);
        return sh;
    }

    int32_t VM::get_int(i32 slot, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);

        ARIA_ASSERT(s.size == 4, "Invalid get_int() call!");

        int32_t i = 0;
        memcpy(&i, s.memory, 4);
        return i;
    }

    uint32_t VM::get_uint(i32 slot, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);

        ARIA_ASSERT(s.size == 4, "Invalid get_uint() call!");

        uint32_t i = 0;
        memcpy(&i, s.memory, 4);
        return i;
    }

    int64_t VM::get_long(i32 slot, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);

        ARIA_ASSERT(s.size == 8, "Invalid get_long() call!");

        int64_t l = 0;
        memcpy(&l, s.memory, 8);
        return l;
    }

    uint64_t VM::get_ulong(i32 slot, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);

        ARIA_ASSERT(s.size == 8, "Invalid get_ulong() call!");

        uint64_t l = 0;
        memcpy(&l, s.memory, 8);
        return l;
    }

    size_t VM::get_size(i32 slot, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);

        ARIA_ASSERT(s.size == sizeof(size_t), "Invalid get_size() call!");

        size_t sz = 0;
        memcpy(&sz, s.memory, sizeof(size_t));
        return sz;
    }

    float VM::get_float(i32 slot, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);

        ARIA_ASSERT(s.size == 4, "Invalid get_float() call!");

        float f = 0;
        memcpy(&f, s.memory, 4);
        return f;
    }

    double VM::get_double(i32 slot, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);

        ARIA_ASSERT(s.size == 8, "Invalid get_double() call!");

        double d = 0;
        memcpy(&d, s.memory, 8);
        return d;
    }

    void* VM::get_pointer(i32 slot, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);

        ARIA_ASSERT(s.size == sizeof(void*), "Invalid get_pointer() call!");

        void* p = nullptr;
        memcpy(&p, s.memory, sizeof(void*));
        return p;
    }

    std::string_view VM::get_string(i32 slot, Stack& stack) {
        VMSlice s = get_vm_slice(slot, stack);
        ARIA_ASSERT(s.type.kind == VMTypeKind::Slice, "Invalid GetString() call!");

        RuntimeSlice str;
        memcpy(&str, s.memory, s.size);

        return { reinterpret_cast<char*>(str.mem), str.size };
    }

    void VM::run_byte_code(const OpCodes& ops) {
        m_op_codes = &ops;
        m_program_counter = 0;

        run_prepass();

        const std::string& signature = "_start$()";

        ARIA_ASSERT(m_functions.contains(signature), "Byte code does not contain _start$() function");
        VMFunction& func = m_functions.at(signature);

        // Perform a jump to the function
        ARIA_ASSERT(func.labels.contains("_entry$"), "_start$() function doesn't contain a \"_entry$\" label");
        m_program_counter = func.labels.at("_entry$");

        VMStackFrame sf;
        sf.function = &func;
        sf.return_address = &m_op_codes->program.back() + 1;
        m_stack_frames.push_back(sf);
        run();
    }

    void VM::run() {
        while (m_program_counter < &m_op_codes->program.back()) {
            switch (*m_program_counter) {
                case OP_ALLOCA: {
                    auto& type = GET_TYPE();
                    alloc(type, m_stack);
                    break;
                }

                case OP_NEW: {
                   auto& type = GET_TYPE(); 
                   void* mem = malloc(align_to_eight(get_vm_type_size(type)));
                   ARIA_ASSERT(mem, "Too much memory allocted on the heap");
                   alloc({ VMTypeKind::Ptr }, m_stack);
                   store_pointer(-1, mem, m_stack);
                   break;
                }

                case OP_NEW_ARR: {
                   auto& type = GET_TYPE();
                   u64 size = get_ulong(-1, m_stack);

                   void* mem = malloc(align_to_eight(get_vm_type_size(type)) * size);
                   ARIA_ASSERT(mem, "Too much memory allocted on the heap");
                   alloc({ VMTypeKind::Ptr }, m_stack);
                   store_pointer(-1, mem, m_stack);
                   break;
                }

                case OP_FREE: {
                   void* mem = get_pointer(-1, m_stack);
                   ARIA_ASSERT(mem, "Trying to free a null pointer");
                   free(mem);
                   break;
                }

                case OP_LD_CONST: {
                    auto& type = GET_TYPE();
                    size_t idx = static_cast<size_t>(*(++m_program_counter));
                    alloc(type, m_stack);
                    VMSlice slice = get_vm_slice(-1, m_stack);

                    switch (type.kind) {
                        case VMTypeKind::I1: {
                            bool val = static_cast<bool>(std::get<u64>(m_op_codes->constant_table[idx]));
                            memcpy(slice.memory, &val, sizeof(bool));
                            break;
                        }

                        case VMTypeKind::I8:
                        case VMTypeKind::U8: {
                            u8 val = static_cast<u8>((std::get<u64>(m_op_codes->constant_table[idx])));
                            memcpy(slice.memory, &val, sizeof(u8));
                            break;
                        }

                        case VMTypeKind::I16:
                        case VMTypeKind::U16: {
                            u16 val = static_cast<u16>((std::get<u64>(m_op_codes->constant_table[idx])));
                            memcpy(slice.memory, &val, sizeof(u16));
                            break;
                        }

                        case VMTypeKind::I32:
                        case VMTypeKind::U32: {
                            u32 val = static_cast<u32>((std::get<u64>(m_op_codes->constant_table[idx])));
                            memcpy(slice.memory, &val, sizeof(u32));
                            break;
                        }

                        case VMTypeKind::I64:
                        case VMTypeKind::U64: {
                            u64 val = static_cast<u64>((std::get<u64>(m_op_codes->constant_table[idx])));
                            memcpy(slice.memory, &val, sizeof(u64));
                            break;
                        }

                        case VMTypeKind::Float: {
                            float val = std::get<float>(m_op_codes->constant_table[idx]);
                            memcpy(slice.memory, &val, sizeof(float));
                            break;
                        }

                        case VMTypeKind::Double: {
                            double val = std::get<double>(m_op_codes->constant_table[idx]);
                            memcpy(slice.memory, &val, sizeof(double));
                            break;
                        }

                        default: ARIA_UNREACHABLE();
                    }

                    break;
                }

                case OP_LD_STR: {
                    std::string_view str = GET_STR();

                    alloc({ VMTypeKind::Slice }, m_stack);
                    RuntimeSlice& slice = *reinterpret_cast<RuntimeSlice*>(get_vm_slice(-1, m_stack).memory);
                    slice.mem = const_cast<char*>(str.data());
                    slice.size = str.size();
                    break;
                }

                case OP_LD_NULL: {
                    alloc({ VMTypeKind::Ptr }, m_stack);
                    memset(get_vm_slice(-1, m_stack).memory, 0, 8); // All slots are padded to eight bytes
                    break;
                }

                case OP_LD: {
                    auto& type = GET_TYPE();
                    void* ptr = get_pointer(-1, m_stack);
                    alloc(type, m_stack);

                    memcpy(get_vm_slice(-1, m_stack).memory, ptr, align_to_eight(get_vm_type_size(type)));
                    break;
                }

                case OP_LD_LOCAL: {
                    size_t index = static_cast<size_t>(*++m_program_counter);
                    dup(index, m_stack, m_stack_frames.back().locals);
                    break;
                }

                case OP_LD_GLOBAL: {
                    std::string_view g = GET_STR();
                    dup(m_global_map.at(g), m_stack, m_globals);
                    break;
                }

                case OP_LD_FIELD: {
                    size_t idx = static_cast<size_t>(*(++m_program_counter));
                    auto& structType = GET_TYPE();

                    u8* src = reinterpret_cast<u8*>(get_pointer(-1, m_stack));           

                    for (size_t i = 0; i < idx; i++) {
                        auto& field = std::get<VMStruct>(structType.data).fields[i];
                        src += align_to_eight(get_vm_type_size(m_op_codes->type_table[field]));
                    }

                    pop(1, m_stack);

                    alloc(m_op_codes->type_table[std::get<VMStruct>(structType.data).fields[idx]], m_stack);
                    VMSlice dst = get_vm_slice(-1, m_stack);
                    memcpy(dst.memory, src, dst.size);
                    break;
                }

                case OP_LD_PTR_LOCAL: {
                    size_t index = static_cast<size_t>(*++m_program_counter);
                    VMSlice slice = get_vm_slice(index, m_stack_frames.back().locals);
                    alloc({ VMTypeKind::Ptr }, m_stack);
                    store_pointer(-1, slice.memory, m_stack);
                    break;
                }

                case OP_LD_PTR_GLOBAL: {
                    std::string_view g = GET_STR();
                    VMSlice slice = get_vm_slice(m_global_map.at(g), m_globals);
                    alloc({ VMTypeKind::Ptr }, m_stack);
                    store_pointer(-1, slice.memory, m_stack);
                    break;
                }

                case OP_LD_PTR_FIELD: {
                    size_t idx = static_cast<size_t>(*(++m_program_counter));
                    auto& structType = GET_TYPE();

                    u8* src = reinterpret_cast<u8*>(get_pointer(-1, m_stack));           

                    for (size_t i = 0; i < idx; i++) {
                        auto& field = std::get<VMStruct>(structType.data).fields[i];
                        src += align_to_eight(get_vm_type_size(m_op_codes->type_table[field]));
                    }

                    store_pointer(-1, src, m_stack);
                    break;
                }

                case OP_LD_PTR: {
                    VMSlice slice = get_vm_slice(-1, m_stack);
                    alloc({ VMTypeKind::Ptr }, m_stack);
                    store_pointer(-1, slice.memory, m_stack);
                    break;
                }

                case OP_ST_LOCAL: {
                    size_t index = static_cast<size_t>(*++m_program_counter);
                    VMSlice slice = get_vm_slice(-1, m_stack);

                    VMSlice dst = get_vm_slice(static_cast<i32>(index), m_stack_frames.back().locals);
                    ARIA_ASSERT(dst.size == slice.size, "Mismatched sizes");
                    memcpy(dst.memory, slice.memory, dst.size);
                    pop(1, m_stack);

                    break;
                }

                case OP_ST_GLOBAL: {
                    std::string_view g = GET_STR();
                    VMSlice slice = get_vm_slice(-1, m_stack);

                    VMSlice dst = get_vm_slice(m_global_map.at(g), m_globals);
                    ARIA_ASSERT(dst.size == slice.size, "Mismatched sizes");
                    memcpy(dst.memory, slice.memory, dst.size);
                    pop(1, m_stack);

                    break;
                }

                case OP_ST_ADDR: {
                    void* mem = get_pointer(-2, m_stack);
                    VMSlice slice = get_vm_slice(-1, m_stack);

                    memcpy(mem, slice.memory, slice.size);
                    pop(2, m_stack);
                    break;
                }

                case OP_ST_FIELD: {
                    size_t idx = static_cast<size_t>(*(++m_program_counter));
                    auto& structType = GET_TYPE();

                    u8* dst = reinterpret_cast<u8*>(get_pointer(-2, m_stack));           
                    VMSlice val = get_vm_slice(-1, m_stack);

                    for (size_t i = 0; i < idx; i++) {
                        auto& field = std::get<VMStruct>(structType.data).fields[i];
                        dst += align_to_eight(get_vm_type_size(m_op_codes->type_table[field]));
                    }

                    memcpy(dst, val.memory, val.size);
                    pop(2, m_stack);
                    break;
                }

                case OP_DUP: {
                    dup(-1, m_stack, m_stack);
                    break;
                }

                case OP_POP: {
                    pop(1, m_stack);
                    break;
                }

                case OP_DECL_LOCAL: {
                    size_t index = static_cast<size_t>(*++m_program_counter);

                    VMSlice src = get_vm_slice(-1, m_stack);

                    alloc(src.type, m_stack_frames.back().locals);
                    VMSlice dst = get_vm_slice(-1, m_stack_frames.back().locals);

                    memcpy(dst.memory, src.memory, src.size);
                    pop(1, m_stack);
                    break;
                };

                case OP_DECL_GLOBAL: {
                    std::string_view g = GET_STR();

                    VMSlice src = get_vm_slice(-1, m_stack);

                    alloc(src.type, m_globals);
                    VMSlice dst = get_vm_slice(-1, m_globals);

                    memcpy(dst.memory, src.memory, src.size);
                    pop(1, m_stack);

                    m_global_map[g] = { static_cast<i32>(m_globals.stack_slot_pointer) - 1 };
                    break;
                };

                // VVV ADD, SUB, MUL, DIV, MOD VVV //
                case OP_ADDI: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal + rhsVal;

                            alloc(lhs.type, m_stack);
                            store_int(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal + rhsVal;

                            alloc(lhs.type, m_stack);
                            store_long(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to addi instruction");
                    }

                    break;
                }

                case OP_ADDU: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal + rhsVal;

                            alloc(lhs.type, m_stack);
                            store_uint(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal + rhsVal;

                            alloc(lhs.type, m_stack);
                            store_ulong(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to addu instruction");
                    }

                    break;
                }

                case OP_ADDF: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal + rhsVal;

                            alloc(lhs.type, m_stack);
                            store_float(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal + rhsVal;

                            alloc(lhs.type, m_stack);
                            store_double(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to addf instruction");
                    }

                    break;
                }

                case OP_SUBI: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal - rhsVal;

                            alloc(lhs.type, m_stack);
                            store_int(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal - rhsVal;

                            alloc(lhs.type, m_stack);
                            store_long(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to subi instruction");
                    }

                    break;
                }

                case OP_SUBU: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal - rhsVal;

                            alloc(lhs.type, m_stack);
                            store_uint(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal - rhsVal;

                            alloc(lhs.type, m_stack);
                            store_ulong(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to subu instruction");
                    }

                    break;
                }

                case OP_SUBF: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal - rhsVal;

                            alloc(lhs.type, m_stack);
                            store_float(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal - rhsVal;

                            alloc(lhs.type, m_stack);
                            store_double(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to subf instruction");
                    }

                    break;
                }

                case OP_MULI: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal * rhsVal;

                            alloc(lhs.type, m_stack);
                            store_int(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal * rhsVal;

                            alloc(lhs.type, m_stack);
                            store_long(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to muli instruction");
                    }

                    break;
                }

                case OP_MULU: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal * rhsVal;

                            alloc(lhs.type, m_stack);
                            store_uint(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal * rhsVal;

                            alloc(lhs.type, m_stack);
                            store_ulong(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to mulu instruction");
                    }

                    break;
                }

                case OP_MULF: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal * rhsVal;

                            alloc(lhs.type, m_stack);
                            store_float(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal * rhsVal;

                            alloc(lhs.type, m_stack);
                            store_double(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to mulf instruction");
                    }

                    break;
                }

                case OP_DIVI: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            // TODO: check if rhsVal is zero...
                            auto result = lhsVal / rhsVal;

                            alloc(lhs.type, m_stack);
                            store_int(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            // TODO: check if rhsVal is zero...
                            auto result = lhsVal / rhsVal;

                            alloc(lhs.type, m_stack);
                            store_long(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to divi instruction");
                    }

                    break;
                }

                case OP_DIVU: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            // TODO: check if rhsVal is zero...
                            auto result = lhsVal / rhsVal;

                            alloc(lhs.type, m_stack);
                            store_uint(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            // TODO: check if rhsVal is zero...
                            auto result = lhsVal / rhsVal;

                            alloc(lhs.type, m_stack);
                            store_ulong(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to divu instruction");
                    }

                    break;
                }

                case OP_DIVF: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal / rhsVal;

                            alloc(lhs.type, m_stack);
                            store_float(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal / rhsVal;

                            alloc(lhs.type, m_stack);
                            store_double(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to divf instruction");
                    }

                    break;
                }

                case OP_MODI: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            // TODO: check if rhsVal is zero...
                            auto result = lhsVal % rhsVal;

                            alloc(lhs.type, m_stack);
                            store_int(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            // TODO: check if rhsVal is zero...
                            auto result = lhsVal % rhsVal;

                            alloc(lhs.type, m_stack);
                            store_long(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to modi instruction");
                    }

                    break;
                }

                case OP_MODU: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            // TODO: check if rhsVal is zero...
                            auto result = lhsVal % rhsVal;

                            alloc(lhs.type, m_stack);
                            store_uint(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            // TODO: check if rhsVal is zero...
                            auto result = lhsVal % rhsVal;

                            alloc(lhs.type, m_stack);
                            store_ulong(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to modu instruction");
                    }

                    break;
                }

                case OP_MODF: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = fmodf(lhsVal, rhsVal);
                            if (result < 0.0f) { result += fabsf(rhsVal); }

                            alloc(lhs.type, m_stack);
                            store_float(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = fmod(lhsVal, rhsVal);
                            if (result < 0.0) { result += fabsf(rhsVal); }

                            alloc(lhs.type, m_stack);
                            store_double(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to modf instruction");
                    }

                    break;
                }
                // ^^^ ADD, SUB, MUL, DIV, MOD ^^^ //

                // VVV CMP, LT, LTE, GT, GTE VVV //
                case OP_CMPI: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal == rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal == rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to cmpi instruction");
                    }

                    break;
                }

                case OP_CMPU: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal == rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal == rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to cmpu instruction");
                    }

                    break;
                }

                case OP_CMPF: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal == rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal == rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to cmpf instruction");
                    }

                    break;
                }

                case OP_LTI: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal < rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal < rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to lti instruction");
                    }

                    break;
                }

                case OP_LTU: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal < rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal < rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to ltu instruction");
                    }

                    break;
                }

                case OP_LTF: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal < rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal < rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to ltf instruction");
                    }

                    break;
                }

                case OP_LTEI: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal <= rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal <= rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to ltei instruction");
                    }

                    break;
                }

                case OP_LTEU: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal <= rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal <= rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to lteu instruction");
                    }

                    break;
                }

                case OP_LTEF: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal <= rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal <= rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to ltef instruction");
                    }

                    break;
                }

                case OP_GTI: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal > rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal > rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to gti instruction");
                    }

                    break;
                }

                case OP_GTU: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal > rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal > rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to gtu instruction");
                    }

                    break;
                }

                case OP_GTF: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal > rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal > rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to gtf instruction");
                    }

                    break;
                }

                case OP_GTEI: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal >= rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal >= rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to gtei instruction");
                    }

                    break;
                }

                case OP_GTEU: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal >= rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal >= rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to gteu instruction");
                    }

                    break;
                }

                case OP_GTEF: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::Float: {
                            float lhsVal = 0.0f;
                            float rhsVal = 0.0f;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal >= rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double lhsVal = 0.0;
                            double rhsVal = 0.0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal >= rhsVal;

                            alloc({ VMTypeKind::I1 }, m_stack);
                            store_bool(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to gtef instruction");
                    }

                    break;
                }
                // ^^^ CMP, LT, LTE, GT, GTE ^^^ //

                // VVV SHL, SHR, AND, OR, XOR VVV ///
                case OP_SHLI: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal << rhsVal;

                            alloc(lhs.type, m_stack);
                            store_int(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal << rhsVal;

                            alloc(lhs.type, m_stack);
                            store_long(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to shli instruction");
                    }

                    break;
                }

                case OP_SHLU: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal << rhsVal;

                            alloc(lhs.type, m_stack);
                            store_uint(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal << rhsVal;

                            alloc(lhs.type, m_stack);
                            store_ulong(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to shlu instruction");
                    }

                    break;
                }

                case OP_SHRI: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal >> rhsVal;

                            alloc(lhs.type, m_stack);
                            store_int(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal >> rhsVal;

                            alloc(lhs.type, m_stack);
                            store_long(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to shri instruction");
                    }

                    break;
                }

                case OP_SHRU: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal >> rhsVal;

                            alloc(lhs.type, m_stack);
                            store_uint(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal >> rhsVal;

                            alloc(lhs.type, m_stack);
                            store_ulong(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to shru instruction");
                    }

                    break;
                }

                case OP_ANDI: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal & rhsVal;

                            alloc(lhs.type, m_stack);
                            store_int(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal & rhsVal;

                            alloc(lhs.type, m_stack);
                            store_long(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to andi instruction");
                    }

                    break;
                }

                case OP_ANDU: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal & rhsVal;

                            alloc(lhs.type, m_stack);
                            store_uint(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal & rhsVal;

                            alloc(lhs.type, m_stack);
                            store_ulong(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to andu instruction");
                    }

                    break;
                }

                case OP_ORI: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal | rhsVal;

                            alloc(lhs.type, m_stack);
                            store_int(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal | rhsVal;

                            alloc(lhs.type, m_stack);
                            store_long(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to ori instruction");
                    }

                    break;
                }

                case OP_ORU: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal | rhsVal;

                            alloc(lhs.type, m_stack);
                            store_uint(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal | rhsVal;

                            alloc(lhs.type, m_stack);
                            store_ulong(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to oru instruction");
                    }

                    break;
                }

                case OP_XORI: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::I32: {
                            i32 lhsVal = 0;
                            i32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal ^ rhsVal;

                            alloc(lhs.type, m_stack);
                            store_int(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 lhsVal = 0;
                            i64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal ^ rhsVal;

                            alloc(lhs.type, m_stack);
                            store_long(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to xori instruction");
                    }

                    break;
                }

                case OP_XORU: {
                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == rhs.type.kind, "Types of both sides must be the same");

                    switch (lhs.type.kind) {
                        case VMTypeKind::U32: {
                            u32 lhsVal = 0;
                            u32 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal ^ rhsVal;

                            alloc(lhs.type, m_stack);
                            store_uint(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::U64: {
                            u64 lhsVal = 0;
                            u64 rhsVal = 0;

                            memcpy(&lhsVal, lhs.memory, lhs.size);
                            memcpy(&rhsVal, rhs.memory, rhs.size);
                            pop(2, m_stack);

                            auto result = lhsVal ^ rhsVal;

                            alloc(lhs.type, m_stack);
                            store_ulong(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to xoru instruction");
                    }

                    break;
                }
                // ^^^ SHL, SHR, AND, OR, XOR ^^^ //

                case OP_NEGI: {
                    VMSlice val = get_vm_slice(-1, m_stack);

                    switch (val.type.kind) {
                        case VMTypeKind::I32: {
                            i32 v = 0;
                            memcpy(&v, val.memory, sizeof(i32));

                            auto result = -v;
                            store_int(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            i64 v = 0;
                            memcpy(&v, val.memory, sizeof(i64));

                            auto result = -v;
                            store_long(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to negi instruction");
                    }

                    break;
                }

                case OP_NEGF: {
                    VMSlice val = get_vm_slice(-1, m_stack);

                    switch (val.type.kind) {
                        case VMTypeKind::Float: {
                            float v = 0;
                            memcpy(&v, val.memory, sizeof(float));

                            auto result = -v;
                            store_float(-1, result, m_stack);
                            break;
                        }

                        case VMTypeKind::I64: {
                            double v = 0;
                            memcpy(&v, val.memory, sizeof(double));

                            auto result = -v;
                            store_double(-1, result, m_stack);
                            break;
                        }

                        default: ARIA_ASSERT(false, "Invalid types to negf instruction");
                    }

                    break;
                }

                case OP_OFFP: {
                    auto& type = GET_TYPE();

                    VMSlice lhs = get_vm_slice(-2, m_stack);
                    VMSlice rhs = get_vm_slice(-1, m_stack);

                    ARIA_ASSERT(lhs.type.kind == VMTypeKind::Ptr || rhs.type.kind == VMTypeKind::Ptr, "offp instruction requires exactly one pointer operand");

                    if (lhs.type.kind == VMTypeKind::Ptr) {
                        ARIA_ASSERT(rhs.type.kind == VMTypeKind::U64, "offp instruction requires pointer and u64 types");

                        void* ptr = get_pointer(-2, m_stack);
                        u64 offset = get_ulong(-1, m_stack);

                        u8* result = reinterpret_cast<u8*>(ptr) + offset * get_vm_type_size(type);

                        pop(1, m_stack);
                        store_pointer(-1, result, m_stack);
                    } else {
                        ARIA_ASSERT(lhs.type.kind == VMTypeKind::U64, "offp instruction requires pointer and u64 types");

                        u64 offset = get_ulong(-2, m_stack);
                        void* ptr = get_pointer(-1, m_stack);

                        u8* result = reinterpret_cast<u8*>(ptr) + offset * align_to_eight(get_vm_type_size(type));

                        pop(2, m_stack);
                        alloc({ VMTypeKind::Ptr }, m_stack);
                        store_pointer(-1, result, m_stack);
                    }

                    break;
                }

                case OP_LOGAND: {
                    bool lhs = get_bool(-2, m_stack);
                    bool rhs = get_bool(-1, m_stack);
                    pop(2, m_stack);

                    alloc({ VMTypeKind::I1 }, m_stack);
                    store_bool(-1, lhs && rhs, m_stack);
                    break;
                }

                case OP_LOGOR: {
                    bool lhs = get_bool(-2, m_stack);
                    bool rhs = get_bool(-1, m_stack);
                    pop(2, m_stack);

                    alloc({ VMTypeKind::I1 }, m_stack);
                    store_bool(-1, lhs || rhs, m_stack);
                    break;
                }

                case OP_LOGNOT: {
                    bool val = get_bool(-1, m_stack);
                    store_bool(-1, !val, m_stack);
                    break;
                }

                case OP_JMP: {
                    std::string_view label = GET_STR();

                    ARIA_ASSERT(m_stack_frames.back().function->labels.contains(label), "Trying to jump to non-existent label");
                    m_program_counter = m_stack_frames.back().function->labels.at(label) - 1;
                    break;
                }

                case OP_JT: {
                    std::string_view label = GET_STR();

                    if (get_bool(-1, m_stack) == true) {
                        ARIA_ASSERT(m_stack_frames.back().function->labels.contains(label), "Trying to jump to non-existent label");
                        m_program_counter = m_stack_frames.back().function->labels.at(label) - 1;
                    }
                    break;
                }

                case OP_JF: {
                    std::string_view label = GET_STR();

                    if (get_bool(-1, m_stack) == false) {
                        ARIA_ASSERT(m_stack_frames.back().function->labels.contains(label), "Trying to jump to non-existent label");
                        m_program_counter = m_stack_frames.back().function->labels.at(label) - 1;
                    }
                    break;
                }

                case OP_JT_POP: {
                    std::string_view label = GET_STR();

                    if (get_bool(-1, m_stack) == true) {
                        ARIA_ASSERT(m_stack_frames.back().function->labels.contains(label), "Trying to jump to non-existent label");
                        m_program_counter = m_stack_frames.back().function->labels.at(label) - 1;
                    }
                    pop(1, m_stack);

                    break;
                }

                case OP_JF_POP: {
                    std::string_view label = GET_STR();

                    if (get_bool(-1, m_stack) == false) {
                        ARIA_ASSERT(m_stack_frames.back().function->labels.contains(label), "Trying to jump to non-existent label");
                        m_program_counter = m_stack_frames.back().function->labels.at(label) - 1;
                    }
                    pop(1, m_stack);

                    break;
                }

                case OP_CONV_ITOI: {
                    auto& type = GET_TYPE();
                    VMSlice slice = get_vm_slice(-1, m_stack);

                    #define CASE_CAST(srcTypeKind, dstTypeKind, srcType, dstType) case VMTypeKind::dstTypeKind: { \
                        srcType t; \
                        memcpy(&t, slice.memory, sizeof(t)); \
                        dstType result = static_cast<dstType>(t); \
                        pop(1, m_stack); \
                        alloc({ VMTypeKind::dstTypeKind }, m_stack); \
                        memcpy(get_vm_slice(-1, m_stack).memory, &result, sizeof(result)); \
                        break; \
                    }

                    #define CASE_CAST_OUTER(srcTypeKind, srcType) case VMTypeKind::srcTypeKind: { \
                        switch (type.kind) { \
                            CASE_CAST(srcTypeKind, I1, srcType, bool) \
                            CASE_CAST(srcTypeKind, I8, srcType, i8) \
                            CASE_CAST(srcTypeKind, U8, srcType, u8) \
                            CASE_CAST(srcTypeKind, I16, srcType, i16) \
                            CASE_CAST(srcTypeKind, U16, srcType, u16) \
                            CASE_CAST(srcTypeKind, I32, srcType, i32) \
                            CASE_CAST(srcTypeKind, U32, srcType, u32) \
                            CASE_CAST(srcTypeKind, I64, srcType, i64) \
                            CASE_CAST(srcTypeKind, U64, srcType, u64) \
                            default: ARIA_UNREACHABLE(); \
                        } \
                        break; \
                    }

                    switch (slice.type.kind) {
                        CASE_CAST_OUTER(I1, bool)
                        CASE_CAST_OUTER(I8, i8)
                        CASE_CAST_OUTER(U8, u8)
                        CASE_CAST_OUTER(I16, i16)
                        CASE_CAST_OUTER(U16, u16)
                        CASE_CAST_OUTER(I32, i32)
                        CASE_CAST_OUTER(U32, u32)
                        CASE_CAST_OUTER(I64, i64)
                        CASE_CAST_OUTER(U64, u64)

                        default: ARIA_UNREACHABLE();
                    }
                    #undef CASE_CAST_OUTER

                    break;
                }

                case OP_CONV_FTOF: {
                    auto& type = GET_TYPE();
                    VMSlice slice = get_vm_slice(-1, m_stack);

                    #define CASE_CAST_OUTER(srcTypeKind, srcType) case VMTypeKind::srcTypeKind: { \
                        switch (type.kind) { \
                            CASE_CAST(srcTypeKind, Float, srcType, float) \
                            CASE_CAST(srcTypeKind, Double, srcType, double) \
                            default: ARIA_UNREACHABLE(); \
                        } \
                        break; \
                    }

                    switch (slice.type.kind) {
                        CASE_CAST_OUTER(Float, float)
                        CASE_CAST_OUTER(Double, double)

                        default: ARIA_UNREACHABLE();
                    }
                    #undef CASE_CAST_OUTER

                    break;
                }

                case OP_CONV_ITOF: {
                    auto& type = GET_TYPE();
                    VMSlice slice = get_vm_slice(-1, m_stack);

                    #define CASE_CAST_OUTER(srcTypeKind, srcType) case VMTypeKind::srcTypeKind: { \
                        switch (type.kind) { \
                            CASE_CAST(srcTypeKind, Float, srcType, float) \
                            CASE_CAST(srcTypeKind, Double, srcType, double) \
                            default: ARIA_UNREACHABLE(); \
                        } \
                        break; \
                    }

                    switch (slice.type.kind) {
                        CASE_CAST_OUTER(I1, bool)
                        CASE_CAST_OUTER(I8, i8)
                        CASE_CAST_OUTER(U8, u8)
                        CASE_CAST_OUTER(I16, i16)
                        CASE_CAST_OUTER(U16, u16)
                        CASE_CAST_OUTER(I32, i32)
                        CASE_CAST_OUTER(U32, u32)
                        CASE_CAST_OUTER(I64, i64)
                        CASE_CAST_OUTER(U64, u64)

                        default: ARIA_UNREACHABLE();
                    }
                    #undef CASE_CAST_OUTER

                    break;
                }

                case OP_CONV_FTOI: {
                    auto& type = GET_TYPE();
                    VMSlice slice = get_vm_slice(-1, m_stack);

                    #define CASE_CAST_OUTER(srcTypeKind, srcType) case VMTypeKind::srcTypeKind: { \
                        switch (type.kind) { \
                            CASE_CAST(srcTypeKind, I1, srcType, bool) \
                            CASE_CAST(srcTypeKind, I8, srcType, i8) \
                            CASE_CAST(srcTypeKind, U8, srcType, u8) \
                            CASE_CAST(srcTypeKind, I16, srcType, i16) \
                            CASE_CAST(srcTypeKind, U16, srcType, u16) \
                            CASE_CAST(srcTypeKind, I32, srcType, i32) \
                            CASE_CAST(srcTypeKind, U32, srcType, u32) \
                            CASE_CAST(srcTypeKind, I64, srcType, i64) \
                            CASE_CAST(srcTypeKind, U64, srcType, u64) \
                            default: ARIA_UNREACHABLE(); \
                        } \
                        break; \
                    }

                    switch (slice.type.kind) {
                        CASE_CAST_OUTER(Float, float)
                        CASE_CAST_OUTER(Double, double)

                        default: ARIA_UNREACHABLE();
                    }
                    #undef CASE_CAST_OUTER

                    break;
                }

                case OP_CALL: {
                    std::string_view sig = GET_STR();

                    if (m_external_functions.contains(sig)) {
                        ExternFn func = m_external_functions.at(sig);
                        func(m_context);
                        break;
                    }

                    ARIA_ASSERT(m_functions.contains(sig), "Calling unknown function");

                    VMFunction& func = m_functions.at(sig);
                    
                    VMStackFrame sf;
                    sf.function = &func;
                    sf.return_address = m_program_counter;
                    
                    m_stack_frames.push_back(sf);

                    ARIA_ASSERT(func.labels.contains("_entry$"), "No _entry$ label inside of function");
                    m_program_counter = func.labels.at("_entry$") - 1;
                    break;
                }

                case OP_RET: {
                    VMStackFrame& sf = m_stack_frames.back();
                    m_program_counter = sf.return_address;
                    m_stack_frames.pop_back();
                    break;
                }

                case OP_RET_VAL: {
                    VMStackFrame& sf = m_stack_frames.back();
                    m_program_counter = sf.return_address;
                    m_stack_frames.pop_back();
                    break;
                }

                case OP_LABEL: m_program_counter++; break;

                default: ARIA_UNREACHABLE(); break;
            }

            m_program_counter++;
        }
    }
    
    VMSlice VM::get_vm_slice(i32 index, Stack& stack) {
        StackSlot slot;

        if (index < 0) {
            slot = stack.stack_slots[stack.stack_slot_pointer + index];
        } else {
            slot = stack.stack_slots[index];
        }

        return VMSlice(&stack.stack[slot.index], slot.size, slot.type);
    }

    size_t VM::get_vm_type_size(const VMType& type) {
        switch (type.kind) {
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

            case VMTypeKind::Slice: return sizeof(RuntimeSlice);

            case VMTypeKind::Struct: {
                size_t size = 0;
                const VMStruct& str = std::get<VMStruct>(type.data);
                for (size_t field : str.fields) {
                    size += align_to_eight(get_vm_type_size(m_op_codes->type_table.at(field)));
                }
                
                return size;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void VM::stop_execution() {
        m_program_counter = &m_op_codes->program.back();
    }

    void VM::run_prepass() {
        for (m_program_counter = &m_op_codes->program.front(); m_program_counter < &m_op_codes->program.back(); m_program_counter++) {
            if (*m_program_counter == OP_FUNCTION) {
                const OpCode* startPc = m_program_counter;
                
                std::string_view ident = GET_STR();
                VMFunction func;
                func.signature = ident;

                for (; m_program_counter < &m_op_codes->program.back(); m_program_counter++) {
                    if (*m_program_counter == OP_LABEL) {
                        std::string_view label = GET_STR();
                        func.labels[label] = ++m_program_counter;
                    } else if (*m_program_counter == OP_ENDFUNCTION) {
                        break;
                    }
                }

                m_program_counter = startPc;
                m_functions[ident] = func;
            }
        }

        m_program_counter = 0; // Reset the program counter so the normal execution happens from the start
    }

    size_t VM::align_to_eight(size_t size) {
        return ((size + 8 - 1) / 8) * 8;
    }

} // namespace Aria::Internal
