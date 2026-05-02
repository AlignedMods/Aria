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

    inline static std::string GetLine(const std::string& str, size_t line) {
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
            : VM(ctx), CompilationContext() {}

        Internal::CompilationContext CompilationContext;
        std::string ModuleName;

        Internal::VM VM;
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
        m_ActiveModule = newModule;

        compile_file_raw(contents, path);
        m_ActiveModule->CompilationContext.finish_compilation();
        add_standard_lib();

        // Handle compiler diagnostics
        auto& unit = m_ActiveModule->CompilationContext.CompilationUnits.at(0);
        for (auto& diag : unit->Diagnostics) {
            fmt::print(fg(fmt::color::gray), "{}:{}:{}: ", path, diag.Line, diag.Column);

            if (diag.Kind == Internal::CompilerDiagKind::Error) {
                fmt::print(fg(fmt::color::pale_violet_red), "error: ");
            } else if (diag.Kind == Internal::CompilerDiagKind::Warning) {
                fmt::print(fg(fmt::color::yellow), "warning: ");
            }

            fmt::print("{}\n", diag.Message);

            // fmt format strings from: https://hackingcpp.com/cpp/libs/fmt
            fmt::print(" {:6} | {}\n", diag.Line, GetLine(unit->Source, diag.Line));
            fmt::print("        | {:>{w}}\n", "^", fmt::arg("w", diag.Column));
        }

        m_Modules[path] = newModule;
    }

    void Context::compile_files(const std::vector<std::string>& paths, const std::string& module) {
        Module* src = new Module(this);
        m_ActiveModule = src;

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

        m_ActiveModule->CompilationContext.finish_compilation();
        add_standard_lib();

        for (size_t i = 0; i < paths.size(); i++) {
            // Handle compiler errors
            auto& unit = m_ActiveModule->CompilationContext.CompilationUnits[i];
            for (auto& diag : unit->Diagnostics) {
                fmt::print(fg(fmt::color::gray), "{}:{}:{}: ", paths[i], diag.Line, diag.Column);

                if (diag.Kind == Internal::CompilerDiagKind::Error) {
                    fmt::print(fg(fmt::color::pale_violet_red), "error: ");
                } else if (diag.Kind == Internal::CompilerDiagKind::Warning) {
                    fmt::print(fg(fmt::color::light_golden_rod_yellow), "warning: ");
                }

                fmt::print("{}\n", diag.Message);

                // fmt format strings from: https://hackingcpp.com/cpp/libs/fmt
                fmt::print(" {:6} | {}\n", diag.Line, GetLine(unit->Source, diag.Line));
                fmt::print("        | {:>{w}}\n", "^", fmt::arg("w", diag.Column));
            }
        }

        m_Modules[module] = src;
    }

    void Context::add_standard_lib() {
        Internal::VM& vm = m_ActiveModule->VM;

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
        ARIA_ASSERT(m_Modules.contains(module), "Current context does not contain the requested module!");
        m_ActiveModule = m_Modules.at(module);
    }

    void Context::free_module(const std::string& module) {
        ARIA_ASSERT(m_Modules.contains(module), "Current context does not contain the requested module!");
        delete m_Modules.at(module);
        m_Modules.erase(module);
    }

    void Context::run() {
        if (!m_ActiveModule->CompilationContext.HasErrors) {
            m_ActiveModule->VM.run_byte_code(m_ActiveModule->CompilationContext.Ops);
        }
    }

    std::string Context::dump_ast() {
        std::string output;

        for (Internal::CompilationUnit* unit : m_ActiveModule->CompilationContext.CompilationUnits) {
            Internal::ASTDumper d(unit->RootASTNode);
            output += d.get_output();
            output += '\n';
        }

        return output;
    }

    std::string Context::disassemble() {
        Internal::Disassembler d(m_ActiveModule->CompilationContext.Ops);
        return d.GetDisassembly();
    }

    void Context::push_bool(bool b) {
        m_ActiveModule->VM.alloc({ Internal::VMTypeKind::I1 }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.store_bool(-1 , b, m_ActiveModule->VM.m_Stack);
    }

    void Context::push_char(int8_t c) {
        m_ActiveModule->VM.alloc({ Internal::VMTypeKind::I8 }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.store_char(-1, c, m_ActiveModule->VM.m_Stack);
    }

    void Context::push_short(int16_t s) {
        m_ActiveModule->VM.alloc({ Internal::VMTypeKind::I16 }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.store_short(-1, s, m_ActiveModule->VM.m_Stack);
    }

    void Context::push_int(int32_t i) {
        m_ActiveModule->VM.alloc({ Internal::VMTypeKind::I32 }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.store_int(-1, i, m_ActiveModule->VM.m_Stack);
    }

    void Context::push_long(int64_t l) {
        m_ActiveModule->VM.alloc({ Internal::VMTypeKind::I64 }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.store_long(-1, l, m_ActiveModule->VM.m_Stack);
    }

    void Context::push_float(float f) {
        m_ActiveModule->VM.alloc({ Internal::VMTypeKind::Float }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.store_float(-1, f, m_ActiveModule->VM.m_Stack);
    }

    void Context::push_double(double d) {
        m_ActiveModule->VM.alloc({ Internal::VMTypeKind::Double }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.store_double(-1, d, m_ActiveModule->VM.m_Stack);
    }

    void Context::push_pointer(void* p) {
        m_ActiveModule->VM.alloc({ Internal::VMTypeKind::Ptr }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.store_pointer(-1, p, m_ActiveModule->VM.m_Stack);
    }

    void Context::push_string(std::string_view s) {
        m_ActiveModule->VM.alloc({ Internal::VMTypeKind::Slice }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.store_string(-1, s, m_ActiveModule->VM.m_Stack);
    }

    void Context::store_bool(int32_t index, bool b) {
        m_ActiveModule->VM.store_bool(index, b, m_ActiveModule->VM.m_Stack);
    }

    void Context::store_char(int32_t index, int8_t c) {
        m_ActiveModule->VM.store_char(index, c, m_ActiveModule->VM.m_Stack);
    }

    void Context::store_short(int32_t index, int16_t s) {
        m_ActiveModule->VM.store_short(index, s, m_ActiveModule->VM.m_Stack);
    }

    void Context::store_int(int32_t index, int32_t i) {
        m_ActiveModule->VM.store_int(index, i, m_ActiveModule->VM.m_Stack);
    }

    void Context::store_long(int32_t index, int64_t l) {
        m_ActiveModule->VM.store_long(index, l, m_ActiveModule->VM.m_Stack);
    }

    void Context::store_float(int32_t index, float f) {
        m_ActiveModule->VM.store_float(index, f, m_ActiveModule->VM.m_Stack);
    }

    void Context::store_double(int32_t index, double d) {
        m_ActiveModule->VM.store_double(index, d, m_ActiveModule->VM.m_Stack);
    }

    void Context::store_pointer(int32_t index, void* p) {
        m_ActiveModule->VM.store_pointer(index, p, m_ActiveModule->VM.m_Stack);
    }

    void Context::store_string(int32_t index, std::string_view str) {
        m_ActiveModule->VM.store_string(index, str, m_ActiveModule->VM.m_Stack);
    }

    void Context::get_global(std::string_view str) {
        ARIA_ASSERT(m_ActiveModule->VM.m_GlobalMap.contains(str), "VM does not contain global variable");
        m_ActiveModule->VM.dup(m_ActiveModule->VM.m_GlobalMap[str], m_ActiveModule->VM.m_Stack, m_ActiveModule->VM.m_Globals);
    }

    void Context::get_global_ptr(std::string_view str) {
        ARIA_ASSERT(m_ActiveModule->VM.m_GlobalMap.contains(str), "VM does not contain global variable");
        Internal::VMSlice slice = m_ActiveModule->VM.get_vm_slice(m_ActiveModule->VM.m_GlobalMap[str], m_ActiveModule->VM.m_Globals);
        push_pointer(slice.Memory);
    }

    bool Context::get_bool(int32_t index) {
        return m_ActiveModule->VM.get_bool(index, m_ActiveModule->VM.m_Stack);
    }

    int8_t Context::get_char(int32_t index) {
        return m_ActiveModule->VM.get_char(index, m_ActiveModule->VM.m_Stack);
    }

    uint8_t Context::get_uchar(int32_t index) {
        return static_cast<uint8_t>(m_ActiveModule->VM.get_char(index, m_ActiveModule->VM.m_Stack));
    }

    int16_t Context::get_short(int32_t index) {
        return m_ActiveModule->VM.get_short(index, m_ActiveModule->VM.m_Stack);
    }

    uint16_t Context::get_ushort(int32_t index) {
        return static_cast<uint16_t>(m_ActiveModule->VM.get_short(index, m_ActiveModule->VM.m_Stack));
    }

    int32_t Context::get_int(int32_t index) {
        return m_ActiveModule->VM.get_int(index, m_ActiveModule->VM.m_Stack);
    }

    uint32_t Context::get_uint(int32_t index) {
        return m_ActiveModule->VM.get_uint(index, m_ActiveModule->VM.m_Stack);
    }

    int64_t Context::get_long(int32_t index) {
        return m_ActiveModule->VM.get_long(index, m_ActiveModule->VM.m_Stack);
    }

    uint64_t Context::get_ulong(int32_t index) {
        return m_ActiveModule->VM.get_ulong(index, m_ActiveModule->VM.m_Stack);
    }

    float Context::get_float(int32_t index) {
        return m_ActiveModule->VM.get_float(index, m_ActiveModule->VM.m_Stack);
    }

    double Context::get_double(int32_t index) {
        return m_ActiveModule->VM.get_double(index, m_ActiveModule->VM.m_Stack);
    }

    void* Context::get_pointer(int32_t index) {
        return m_ActiveModule->VM.get_pointer(index, m_ActiveModule->VM.m_Stack);
    }

    std::string_view Context::get_string(int32_t index) {
        return m_ActiveModule->VM.get_string(index, m_ActiveModule->VM.m_Stack);
    }

    void Context::pop(size_t count) {
        m_ActiveModule->VM.pop(count, m_ActiveModule->VM.m_Stack);
    }

    void Context::add_external_function(std::string_view name, ExternFn fn) {
        m_ActiveModule->VM.add_extern(name, fn);
    }

    bool Context::has_function(const std::string& str) {
        Internal::CompilerReflectionData& reflection = m_ActiveModule->CompilationContext.ReflectionData;
        return reflection.Declarations.contains(str) && reflection.Declarations.at(str).Kind == Internal::ReflectionKind::Function;
    }

    void Context::call(const std::string& str, size_t argCount) {
        Internal::CompilerReflectionData& reflection = m_ActiveModule->CompilationContext.ReflectionData;
        ARIA_ASSERT(reflection.Declarations.contains(str), "Module does not contain function");

        m_ActiveModule->VM.call(str, argCount);
    }

    void Context::set_runtime_error_handler(RuntimeErrorHandlerFn fn) {
        m_RuntimeErrorHandler = fn;
    }

    void Context::set_compiler_error_handler(CompilerErrorHandlerFn fn) {
        m_CompilerErrorHandler = fn;
    }

    void Context::compile_file_raw(const std::string& source, const std::string& filename) {
        m_ActiveModule->CompilationContext.compile_file(source);
    }

    void Context::report_runtime_error(const std::string& error) {
        if (m_RuntimeErrorHandler) {
            m_RuntimeErrorHandler(error);
        } else {
            fmt::print(stderr, "A runtime error occurred!\nError message: {}", error);
        }

        m_ActiveModule->VM.stop_execution();
    }

} // namespace Aria