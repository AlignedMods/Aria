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

    void Context::CompileFile(const std::string& path) {
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

        CompileFileRaw(contents, path);
        m_ActiveModule->CompilationContext.FinishCompilation();
        AddStandardLib();

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

    void Context::CompileFiles(const std::vector<std::string>& paths, const std::string& module) {
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

            CompileFileRaw(contents, path);
        }

        m_ActiveModule->CompilationContext.FinishCompilation();
        AddStandardLib();

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

    void Context::AddStandardLib() {
        Internal::VM& vm = m_ActiveModule->VM;

        vm.AddExtern("__aria_raw_print_stdout()", Internal::__aria_print);
        vm.AddExtern("__aria_destruct_str()", Internal::__aria_destruct_str);
        vm.AddExtern("__aria_copy_str()", Internal::__aria_copy_str);
    }

    void Context::SetActiveModule(const std::string& module) {
        ARIA_ASSERT(m_Modules.contains(module), "Current context does not contain the requested module!");
        m_ActiveModule = m_Modules.at(module);
    }

    void Context::FreeModule(const std::string& module) {
        ARIA_ASSERT(m_Modules.contains(module), "Current context does not contain the requested module!");
        delete m_Modules.at(module);
        m_Modules.erase(module);
    }

    void Context::Run() {
        if (!m_ActiveModule->CompilationContext.HasErrors) {
            m_ActiveModule->VM.RunByteCode(m_ActiveModule->CompilationContext.Ops);
        }
    }

    std::string Context::DumpAST() {
        std::string output;

        for (Internal::CompilationUnit* unit : m_ActiveModule->CompilationContext.CompilationUnits) {
            Internal::ASTDumper d(unit->RootASTNode);
            output += d.GetOutput();
            output += '\n';
        }

        return output;
    }

    std::string Context::Disassemble() {
        Internal::Disassembler d(m_ActiveModule->CompilationContext.Ops);
        return d.GetDisassembly();
    }

    void Context::PushBool(bool b) {
        m_ActiveModule->VM.Alloca({ Internal::VMTypeKind::I1 }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.StoreBool(-1 , b, m_ActiveModule->VM.m_Stack);
    }

    void Context::PushChar(int8_t c) {
        m_ActiveModule->VM.Alloca({ Internal::VMTypeKind::I8 }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.StoreChar(-1, c, m_ActiveModule->VM.m_Stack);
    }

    void Context::PushShort(int16_t s) {
        m_ActiveModule->VM.Alloca({ Internal::VMTypeKind::I16 }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.StoreShort(-1, s, m_ActiveModule->VM.m_Stack);
    }

    void Context::PushInt(int32_t i) {
        m_ActiveModule->VM.Alloca({ Internal::VMTypeKind::I32 }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.StoreInt(-1, i, m_ActiveModule->VM.m_Stack);
    }

    void Context::PushLong(int64_t l) {
        m_ActiveModule->VM.Alloca({ Internal::VMTypeKind::I64 }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.StoreLong(-1, l, m_ActiveModule->VM.m_Stack);
    }

    void Context::PushFloat(float f) {
        m_ActiveModule->VM.Alloca({ Internal::VMTypeKind::Float }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.StoreFloat(-1, f, m_ActiveModule->VM.m_Stack);
    }

    void Context::PushDouble(double d) {
        m_ActiveModule->VM.Alloca({ Internal::VMTypeKind::Double }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.StoreDouble(-1, d, m_ActiveModule->VM.m_Stack);
    }

    void Context::PushPointer(void* p) {
        m_ActiveModule->VM.Alloca({ Internal::VMTypeKind::Ptr }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.StorePointer(-1, p, m_ActiveModule->VM.m_Stack);
    }

    void Context::PushString(std::string_view s) {
        m_ActiveModule->VM.Alloca({ Internal::VMTypeKind::String }, m_ActiveModule->VM.m_Stack);
        m_ActiveModule->VM.StoreString(-1, s, m_ActiveModule->VM.m_Stack);
    }

    void Context::StoreBool(int32_t index, bool b) {
        m_ActiveModule->VM.StoreBool(index, b, m_ActiveModule->VM.m_Stack);
    }

    void Context::StoreChar(int32_t index, int8_t c) {
        m_ActiveModule->VM.StoreChar(index, c, m_ActiveModule->VM.m_Stack);
    }

    void Context::StoreShort(int32_t index, int16_t s) {
        m_ActiveModule->VM.StoreShort(index, s, m_ActiveModule->VM.m_Stack);
    }

    void Context::StoreInt(int32_t index, int32_t i) {
        m_ActiveModule->VM.StoreInt(index, i, m_ActiveModule->VM.m_Stack);
    }

    void Context::StoreLong(int32_t index, int64_t l) {
        m_ActiveModule->VM.StoreLong(index, l, m_ActiveModule->VM.m_Stack);
    }

    void Context::StoreFloat(int32_t index, float f) {
        m_ActiveModule->VM.StoreFloat(index, f, m_ActiveModule->VM.m_Stack);
    }

    void Context::StoreDouble(int32_t index, double d) {
        m_ActiveModule->VM.StoreDouble(index, d, m_ActiveModule->VM.m_Stack);
    }

    void Context::StorePointer(int32_t index, void* p) {
        m_ActiveModule->VM.StorePointer(index, p, m_ActiveModule->VM.m_Stack);
    }

    void Context::StoreString(int32_t index, std::string_view str) {
        m_ActiveModule->VM.StoreString(index, str, m_ActiveModule->VM.m_Stack);
    }

    void Context::GetGlobal(std::string_view str) {
        ARIA_ASSERT(m_ActiveModule->VM.m_GlobalMap.contains(str), "VM does not contain global variable");
        m_ActiveModule->VM.Dup(m_ActiveModule->VM.m_GlobalMap[str], m_ActiveModule->VM.m_Stack, m_ActiveModule->VM.m_Globals);
    }

    void Context::GetGlobalPtr(std::string_view str) {
        ARIA_ASSERT(m_ActiveModule->VM.m_GlobalMap.contains(str), "VM does not contain global variable");
        Internal::VMSlice slice = m_ActiveModule->VM.GetVMSlice(m_ActiveModule->VM.m_GlobalMap[str], m_ActiveModule->VM.m_Globals);
        PushPointer(slice.Memory);
    }

    void Context::GetArg(int32_t index) {
        ARIA_TODO("Context::GetArg()");
        // m_ActiveModule->VM.Dup(index + 1, m_ActiveModule->VM.m_Stack, m_ActiveModule->VM.m_FunctionStack);
    }

    void Context::GetField(int32_t index, const std::string& name) {
        // CompiledSource* src = GetCompiledSource(module);
        //
        // Internal::StackSlot slot = src->VM.GetStackSlot(index);
        // 
        // ARIA_ASSERT(slot.ResolvedType->Type == Internal::PrimitiveType::Structure, "Accessing a field can only happen with a struct");
        // 
        // for (const auto& field : std::get<Internal::StructDeclaration>(slot.ResolvedType->Data).Fields) {
        //     if (Internal::StringView(name.data(), name.size()) == field.Identifier) {
        //         src->VM.Ref({index, field.Offset, field.ResolvedType->GetSize()}, field.ResolvedType);
        //     }
        // }
        
        ARIA_ASSERT(false, "Add Context::PushField()");

        // ReportRuntimeError(fmt::format("Could find field {} in {}", name, Internal::TypeInfoToString(slot.ResolvedType)));
    }

    bool Context::GetBool(int32_t index) {
        return m_ActiveModule->VM.GetBool(index, m_ActiveModule->VM.m_Stack);
    }

    int8_t Context::GetChar(int32_t index) {
        return m_ActiveModule->VM.GetChar(index, m_ActiveModule->VM.m_Stack);
    }

    int16_t Context::GetShort(int32_t index) {
        return m_ActiveModule->VM.GetShort(index, m_ActiveModule->VM.m_Stack);
    }

    int32_t Context::GetInt(int32_t index) {
        return m_ActiveModule->VM.GetInt(index, m_ActiveModule->VM.m_Stack);
    }

    int64_t Context::GetLong(int32_t index) {
        return m_ActiveModule->VM.GetLong(index, m_ActiveModule->VM.m_Stack);
    }

    float Context::GetFloat(int32_t index) {
        return m_ActiveModule->VM.GetFloat(index, m_ActiveModule->VM.m_Stack);
    }

    double Context::GetDouble(int32_t index) {
        return m_ActiveModule->VM.GetDouble(index, m_ActiveModule->VM.m_Stack);
    }

    void* Context::GetPointer(int32_t index) {
        return m_ActiveModule->VM.GetPointer(index, m_ActiveModule->VM.m_Stack);
    }

    std::string_view Context::GetString(int32_t index) {
        return m_ActiveModule->VM.GetString(index, m_ActiveModule->VM.m_Stack);
    }

    void Context::Pop(size_t count) {
        m_ActiveModule->VM.Pop(count, m_ActiveModule->VM.m_Stack);
    }

    void Context::AddExternalFunction(std::string_view name, ExternFn fn) {
        m_ActiveModule->VM.AddExtern(name, fn);
    }

    bool Context::HasFunction(const std::string& str) {
        Internal::CompilerReflectionData& reflection = m_ActiveModule->CompilationContext.ReflectionData;
        return reflection.Declarations.contains(str) && reflection.Declarations.at(str).Kind == Internal::ReflectionKind::Function;
    }

    void Context::Call(const std::string& str, size_t argCount) {
        Internal::CompilerReflectionData& reflection = m_ActiveModule->CompilationContext.ReflectionData;
        ARIA_ASSERT(reflection.Declarations.contains(str), "Module does not contain function");

        m_ActiveModule->VM.Call(str, argCount);
    }

    void Context::SetRuntimeErrorHandler(RuntimeErrorHandlerFn fn) {
        m_RuntimeErrorHandler = fn;
    }

    void Context::SetCompilerErrorHandler(CompilerErrorHandlerFn fn) {
        m_CompilerErrorHandler = fn;
    }

    void Context::CompileFileRaw(const std::string& source, const std::string& filename) {
        m_ActiveModule->CompilationContext.CompileFile(source);
    }

    void Context::ReportRuntimeError(const std::string& error) {
        if (m_RuntimeErrorHandler) {
            m_RuntimeErrorHandler(error);
        } else {
            fmt::print(stderr, "A runtime error occurred!\nError message: {}", error);
        }

        m_ActiveModule->VM.StopExecution();
    }

} // namespace Aria