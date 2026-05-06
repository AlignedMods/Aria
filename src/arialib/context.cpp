#include "aria/context.hpp"
#include "arialib/vm.hpp"
#include "arialib/deserializer.hpp"
#include "arialib/stdlib.hpp"
#include "common/op_codes.hpp"

#include <fstream>
#include <sstream>

namespace Aria {

    struct Module {
        Module(Context* ctx)
            : vm(ctx) {}

        std::string module_name;
        Internal::OpCodes ops;
        Internal::VM vm;
    };

    Context::Context() {}

    Context Context::Create() {
        Context ctx{};
        return ctx;
    }

    void Context::load_file(const std::string& path) {
        std::ifstream f(path);
        if (!f) {
            fmt::println("Could not open file '{}'", path);
            return;
        }
        std::stringstream ss;
        ss << f.rdbuf();
        std::string source = ss.str();
        ss.clear();

        Module* mod = new Module(this);
        mod->module_name = path;
        Internal::Deserializer d(source);
        mod->ops = d.get_ops();

        m_active_module = mod;

        add_standard_lib();
    }

    void Context::add_standard_lib() {
        Internal::VM& vm = m_active_module->vm;

        vm.add_extern("__aria_raw_print_stdout()", Internal::print_stdout);
        // vm.add_extern("__aria_destruct_str()", Internal::__aria_destruct_str);
        // vm.add_extern("__aria_copy_str()", Internal::__aria_copy_str);
        // vm.add_extern("__aria_append_str<char>()", Internal::__aria_append_str_char);
        // vm.add_extern("__aria_append_str<uchar>()", Internal::__aria_append_str_uchar);
        // vm.add_extern("__aria_append_str<short>()", Internal::__aria_append_str_short);
        // vm.add_extern("__aria_append_str<ushort>()", Internal::__aria_append_str_ushort);
        // vm.add_extern("__aria_append_str<int>()", Internal::__aria_append_str_int);
        // vm.add_extern("__aria_append_str<uint>()", Internal::__aria_append_str_uint);
        // vm.add_extern("__aria_append_str<long>()", Internal::__aria_append_str_long);
        // vm.add_extern("__aria_append_str<ulong>()", Internal::__aria_append_str_ulong);
        // vm.add_extern("__aria_append_str<float>()", Internal::__aria_append_str_float);
        // vm.add_extern("__aria_append_str<double>()", Internal::__aria_append_str_double);
        // vm.add_extern("__aria_append_str<string>()", Internal::__aria_append_str_string);
    }

    void Context::set_active_module(const std::string& module) {
        ARIA_ASSERT(m_modules.contains(module), "Current context does not contain the requested module!");
        m_active_module = m_modules.at(module);
    }

    void Context::free_module(const std::string& module) {
        ARIA_ASSERT(m_modules.contains(module), "Current context does not contain the requested module!");
        delete m_modules.at(module);
        m_modules.erase(module);
    }

    void Context::run() {
        m_active_module->vm.run_byte_code(m_active_module->ops);
    }

    void Context::push_bool(bool b) {
        m_active_module->vm.alloc({ Internal::VMTypeKind::I1 }, m_active_module->vm.m_stack);
        m_active_module->vm.store_bool(-1 , b, m_active_module->vm.m_stack);
    }

    void Context::push_char(int8_t c) {
        m_active_module->vm.alloc({ Internal::VMTypeKind::I8 }, m_active_module->vm.m_stack);
        m_active_module->vm.store_char(-1, c, m_active_module->vm.m_stack);
    }

    void Context::push_short(int16_t s) {
        m_active_module->vm.alloc({ Internal::VMTypeKind::I16 }, m_active_module->vm.m_stack);
        m_active_module->vm.store_short(-1, s, m_active_module->vm.m_stack);
    }

    void Context::push_int(int32_t i) {
        m_active_module->vm.alloc({ Internal::VMTypeKind::I32 }, m_active_module->vm.m_stack);
        m_active_module->vm.store_int(-1, i, m_active_module->vm.m_stack);
    }

    void Context::push_long(int64_t l) {
        m_active_module->vm.alloc({ Internal::VMTypeKind::I64 }, m_active_module->vm.m_stack);
        m_active_module->vm.store_long(-1, l, m_active_module->vm.m_stack);
    }

    void Context::push_float(float f) {
        m_active_module->vm.alloc({ Internal::VMTypeKind::Float }, m_active_module->vm.m_stack);
        m_active_module->vm.store_float(-1, f, m_active_module->vm.m_stack);
    }

    void Context::push_double(double d) {
        m_active_module->vm.alloc({ Internal::VMTypeKind::Double }, m_active_module->vm.m_stack);
        m_active_module->vm.store_double(-1, d, m_active_module->vm.m_stack);
    }

    void Context::push_pointer(void* p) {
        m_active_module->vm.alloc({ Internal::VMTypeKind::Ptr }, m_active_module->vm.m_stack);
        m_active_module->vm.store_pointer(-1, p, m_active_module->vm.m_stack);
    }

    void Context::push_string(std::string_view s) {
        m_active_module->vm.alloc({ Internal::VMTypeKind::Slice }, m_active_module->vm.m_stack);
        m_active_module->vm.store_string(-1, s, m_active_module->vm.m_stack);
    }

    void Context::store_bool(int32_t index, bool b) {
        m_active_module->vm.store_bool(index, b, m_active_module->vm.m_stack);
    }

    void Context::store_char(int32_t index, int8_t c) {
        m_active_module->vm.store_char(index, c, m_active_module->vm.m_stack);
    }

    void Context::store_short(int32_t index, int16_t s) {
        m_active_module->vm.store_short(index, s, m_active_module->vm.m_stack);
    }

    void Context::store_int(int32_t index, int32_t i) {
        m_active_module->vm.store_int(index, i, m_active_module->vm.m_stack);
    }

    void Context::store_long(int32_t index, int64_t l) {
        m_active_module->vm.store_long(index, l, m_active_module->vm.m_stack);
    }

    void Context::store_float(int32_t index, float f) {
        m_active_module->vm.store_float(index, f, m_active_module->vm.m_stack);
    }

    void Context::store_double(int32_t index, double d) {
        m_active_module->vm.store_double(index, d, m_active_module->vm.m_stack);
    }

    void Context::store_pointer(int32_t index, void* p) {
        m_active_module->vm.store_pointer(index, p, m_active_module->vm.m_stack);
    }

    void Context::store_string(int32_t index, std::string_view str) {
        m_active_module->vm.store_string(index, str, m_active_module->vm.m_stack);
    }

    void Context::get_global(std::string_view str) {
        ARIA_ASSERT(m_active_module->vm.m_global_map.contains(str), "VM does not contain global variable");
        m_active_module->vm.dup(m_active_module->vm.m_global_map[str], m_active_module->vm.m_stack, m_active_module->vm.m_globals);
    }

    void Context::get_global_ptr(std::string_view str) {
        ARIA_ASSERT(m_active_module->vm.m_global_map.contains(str), "VM does not contain global variable");
        Internal::VMSlice slice = m_active_module->vm.get_vm_slice(m_active_module->vm.m_global_map[str], m_active_module->vm.m_globals);
        push_pointer(slice.memory);
    }

    bool Context::get_bool(int32_t index) {
        return m_active_module->vm.get_bool(index, m_active_module->vm.m_stack);
    }

    int8_t Context::get_char(int32_t index) {
        return m_active_module->vm.get_char(index, m_active_module->vm.m_stack);
    }

    uint8_t Context::get_uchar(int32_t index) {
        return static_cast<uint8_t>(m_active_module->vm.get_char(index, m_active_module->vm.m_stack));
    }

    int16_t Context::get_short(int32_t index) {
        return m_active_module->vm.get_short(index, m_active_module->vm.m_stack);
    }

    uint16_t Context::get_ushort(int32_t index) {
        return static_cast<uint16_t>(m_active_module->vm.get_short(index, m_active_module->vm.m_stack));
    }

    int32_t Context::get_int(int32_t index) {
        return m_active_module->vm.get_int(index, m_active_module->vm.m_stack);
    }

    uint32_t Context::get_uint(int32_t index) {
        return m_active_module->vm.get_uint(index, m_active_module->vm.m_stack);
    }

    int64_t Context::get_long(int32_t index) {
        return m_active_module->vm.get_long(index, m_active_module->vm.m_stack);
    }

    uint64_t Context::get_ulong(int32_t index) {
        return m_active_module->vm.get_ulong(index, m_active_module->vm.m_stack);
    }

    float Context::get_float(int32_t index) {
        return m_active_module->vm.get_float(index, m_active_module->vm.m_stack);
    }

    double Context::get_double(int32_t index) {
        return m_active_module->vm.get_double(index, m_active_module->vm.m_stack);
    }

    void* Context::get_pointer(int32_t index) {
        return m_active_module->vm.get_pointer(index, m_active_module->vm.m_stack);
    }

    std::string_view Context::get_string(int32_t index) {
        return m_active_module->vm.get_string(index, m_active_module->vm.m_stack);
    }

    void Context::pop(size_t count) {
        m_active_module->vm.pop(count, m_active_module->vm.m_stack);
    }

    void Context::add_external_function(std::string_view name, ExternFn fn) {
        m_active_module->vm.add_extern(name, fn);
    }

    bool Context::has_function(const std::string& str) {
        return m_active_module->vm.m_functions.contains(str);
    }

    void Context::call(const std::string& str, size_t argCount) {
        ARIA_ASSERT(m_active_module->vm.m_functions.contains(str), "Module does not contain function");
        m_active_module->vm.call(str, argCount);
    }

    void Context::set_runtime_error_handler(RuntimeErrorHandlerFn fn) {
        m_runtime_error_handler = fn;
    }

    void Context::report_runtime_error(const std::string& error) {
        if (m_runtime_error_handler) {
            m_runtime_error_handler(error);
        } else {
            fmt::print(stderr, "A runtime error occurred!\nError message: {}", error);
        }

        m_active_module->vm.stop_execution();
    }

} // namespace Aria