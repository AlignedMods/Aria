#include "aria/context.hpp"
#include "aria/internal/compiler/compilation_context.hpp"
#include "aria/internal/compiler/codegen/disassembler.hpp"
#include "aria/internal/compiler/ast/ast_dumper.hpp"
#include "aria/internal/compiler/reflection/compiler_reflection.hpp"
#include "aria/internal/stdlib/io.hpp"
#include "aria/internal/stdlib/string.hpp"
#include "aria/internal/vm/vm.hpp"
#include "aria/internal/vm/op_codes.hpp"

#include <fstream>
#include <sstream>

namespace Aria {

    inline static std::string get_line(const std::string& str, size_t line) {
        std::vector<std::string> lines;
        std::stringstream ss(str);
        std::string item;

        while (std::getline(ss, item, '\n')) {
            lines.push_back(item);
        }

        return lines.at(line - 1);
    }

    struct Module {
        Module(Context* ctx)
            : vm(ctx), compilation_context() {}

        Internal::CompilationContext compilation_context;
        std::string module_name;

        Internal::VM vm;
    };

    Context::Context() {}

    Context Context::Create() {
        Context ctx{};
        return ctx;
    }

    void Context::compile_file(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            fmt::print(fmt::fg(fmt::color::pale_violet_red), "Failed to open file: {}!\n", path);
            return;
        }

        std::stringstream ss;
        ss << file.rdbuf();
        std::string contents = ss.str();
        ss.flush();

        Module* newModule = new Module(this);
        m_active_module = newModule;

        compile_file_raw(contents, path);
        m_active_module->compilation_context.finish_compilation();
        add_standard_lib();

        // Handle compiler diagnostics
        auto& unit = m_active_module->compilation_context.compilation_units.at(0);
        for (auto& diag : unit->diagnostics) {
            fmt::print(fg(fmt::color::gray), "{}:{}:{}: ", path, diag.line, diag.column);

            if (diag.kind == Internal::CompilerDiagKind::Error) {
                fmt::print(fg(fmt::color::pale_violet_red), "error: ");
            } else if (diag.kind == Internal::CompilerDiagKind::Warning) {
                fmt::print(fg(fmt::color::yellow), "warning: ");
            }

            fmt::print("{}\n", diag.message);

            // fmt format strings from: https://hackingcpp.com/cpp/libs/fmt
            fmt::print(" {:6} | {}\n", diag.line, get_line(unit->source, diag.line));
            fmt::print("        | {:>{w}}\n", "^", fmt::arg("w", diag.column));
        }

        m_modules[path] = newModule;
    }

    void Context::compile_files(const std::vector<std::string>& paths, const std::string& module) {
        Module* src = new Module(this);
        m_active_module = src;

        for (const auto& path : paths) {
            std::ifstream file(path);
            if (!file.is_open()) {
                fmt::print(fmt::fg(fmt::color::pale_violet_red), "Failed to open file: {}!\n", path);
                return;
            }

            std::stringstream ss;
            ss << file.rdbuf();
            std::string contents = ss.str();
            ss.flush();

            compile_file_raw(contents, path);
        }

        m_active_module->compilation_context.finish_compilation();
        add_standard_lib();

        for (size_t i = 0; i < paths.size(); i++) {
            // Handle compiler errors
            auto& unit = m_active_module->compilation_context.compilation_units[i];
            for (auto& diag : unit->diagnostics) {
                fmt::print(fg(fmt::color::gray), "{}:{}:{}: ", paths[i], diag.line, diag.column);

                if (diag.kind == Internal::CompilerDiagKind::Error) {
                    fmt::print(fg(fmt::color::pale_violet_red), "error: ");
                } else if (diag.kind == Internal::CompilerDiagKind::Warning) {
                    fmt::print(fg(fmt::color::light_golden_rod_yellow), "warning: ");
                }

                fmt::print("{}\n", diag.message);

                // fmt format strings from: https://hackingcpp.com/cpp/libs/fmt
                fmt::print(" {:6} | {}\n", diag.line, get_line(unit->source, diag.line));
                fmt::print("        | {:>{w}}\n", "^", fmt::arg("w", diag.column));
            }
        }

        m_modules[module] = src;
    }

    void Context::add_standard_lib() {
        Internal::VM& vm = m_active_module->vm;

        vm.add_extern("__aria_raw_print_stdout()", Internal::__aria_print);
        vm.add_extern("__aria_destruct_str()", Internal::__aria_destruct_str);
        vm.add_extern("__aria_copy_str()", Internal::__aria_copy_str);
        vm.add_extern("__aria_append_str<char>()", Internal::__aria_append_str_char);
        vm.add_extern("__aria_append_str<uchar>()", Internal::__aria_append_str_uchar);
        vm.add_extern("__aria_append_str<short>()", Internal::__aria_append_str_short);
        vm.add_extern("__aria_append_str<ushort>()", Internal::__aria_append_str_ushort);
        vm.add_extern("__aria_append_str<int>()", Internal::__aria_append_str_int);
        vm.add_extern("__aria_append_str<uint>()", Internal::__aria_append_str_uint);
        vm.add_extern("__aria_append_str<long>()", Internal::__aria_append_str_long);
        vm.add_extern("__aria_append_str<ulong>()", Internal::__aria_append_str_ulong);
        vm.add_extern("__aria_append_str<float>()", Internal::__aria_append_str_float);
        vm.add_extern("__aria_append_str<double>()", Internal::__aria_append_str_double);
        vm.add_extern("__aria_append_str<string>()", Internal::__aria_append_str_string);
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
        if (!m_active_module->compilation_context.has_errors) {
            m_active_module->vm.run_byte_code(m_active_module->compilation_context.ops);
        }
    }

    std::string Context::dump_ast() {
        std::string output;

        for (Internal::CompilationUnit* unit : m_active_module->compilation_context.compilation_units) {
            Internal::ASTDumper d(unit->root_ast_node);
            output += d.get_output();
            output += '\n';
        }

        return output;
    }

    std::string Context::disassemble() {
        Internal::Disassembler d(m_active_module->compilation_context.ops);
        return d.get_dissasembly();
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
        Internal::CompilerReflectionData& reflection = m_active_module->compilation_context.reflection_data;
        return reflection.declarations.contains(str) && reflection.declarations.at(str).kind == Internal::ReflectionKind::Function;
    }

    void Context::call(const std::string& str, size_t argCount) {
        Internal::CompilerReflectionData& reflection = m_active_module->compilation_context.reflection_data;
        ARIA_ASSERT(reflection.declarations.contains(str), "Module does not contain function");

        m_active_module->vm.call(str, argCount);
    }

    void Context::set_runtime_error_handler(RuntimeErrorHandlerFn fn) {
        m_runtime_error_handler = fn;
    }

    void Context::set_compiler_error_handler(CompilerErrorHandlerFn fn) {
        m_compiler_error_handler = fn;
    }

    void Context::compile_file_raw(const std::string& source, const std::string& filename) {
        m_active_module->compilation_context.compile_file(source);
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