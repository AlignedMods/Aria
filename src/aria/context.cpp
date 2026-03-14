#include "aria/context.hpp"
#include "aria/internal/compiler/compilation_context.hpp"
#include "aria/internal/compiler/codegen/disassembler.hpp"
#include "aria/internal/compiler/ast/ast_dumper.hpp"
#include "aria/internal/compiler/reflection/compiler_reflection.hpp"
#include "aria/internal/stdlib/array.hpp"
#include "aria/internal/stdlib/string.hpp"
#include "aria/internal/vm/vm.hpp"

#include <fstream>
#include <sstream>

namespace Aria {

    struct CompiledSource {
        CompiledSource(Context* ctx, const std::string& sourceCode)
            : VM(ctx), CompilationContext(sourceCode) {}

        Internal::CompilationContext CompilationContext;
        std::string Module;

        Internal::VM VM;
    };

    Context::Context() {}

    Context Context::Create() {
        Context ctx{};
        return ctx;
    }

    void Context::CompileFile(const std::string& path, const std::string& module) {
        std::ifstream file(path);
        if (!file.is_open()) {
            fmt::print(stderr, "Failed to open file: {}!\n", path);
            return;
        }

        std::stringstream ss;
        ss << file.rdbuf();
        std::string contents = ss.str();
        ss.flush();

        CompileString(contents, module);
    }

    void Context::CompileString(const std::string& source, const std::string& module) {
        bool valid = true;

        CompiledSource* src = new CompiledSource(this, source);
        m_CurrentCompiledSource = src;

        src->CompilationContext.Compile();

        src->VM.AddExtern("bl__array__init__", Aria::Internal::bl__array__init__);
        src->VM.AddExtern("bl__array__destruct__", Aria::Internal::bl__array__destruct__);
        src->VM.AddExtern("bl__array__copy__", Aria::Internal::bl__array__copy__);

        src->VM.AddExtern("bl__array__append__", Aria::Internal::bl__array__append__);
        src->VM.AddExtern("bl__array__index__", Aria::Internal::bl__array__index__);

        src->VM.AddExtern("bl__string__construct__", Aria::Internal::bl__string__construct__);
        src->VM.AddExtern("bl__string__construct_from_literal__", Aria::Internal::bl__string__construct_from_literal__);

        src->VM.AddExtern("bl__string__copy__", Aria::Internal::bl__string__copy__);
        src->VM.AddExtern("bl__string__assign__", Aria::Internal::bl__string__assign__);

        m_Modules[module] = src;
    }

    void Context::FreeModule(const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);

        delete src;
        m_Modules.erase(module);
    }

    void Context::Run(const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);

        m_CurrentCompiledSource = src;
        m_CurrentCompiledSource->VM.RunByteCode(src->CompilationContext.GetOpCodes().data(), src->CompilationContext.GetOpCodes().size());
    }

    std::string Context::DumpAST(const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);

        Internal::ASTDumper d(src->CompilationContext.GetRootASTNode());
        return d.GetOutput();
    }

    std::string Context::Disassemble(const std::string& module, bool verbose) {
        CompiledSource* src = GetCompiledSource(module);

        Internal::Disassembler d(&src->CompilationContext.GetOpCodes(), verbose);
        return d.GetDisassembly();
    }

    void Context::PushBool(bool b, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.Alloca(sizeof(b), Internal::TypeInfo::Create(&src->CompilationContext, Internal::PrimitiveType::Bool), src->VM.m_LocalStack);
        src->VM.StoreBool(-1 , b, src->VM.m_LocalStack);
    }

    void Context::PushChar(int8_t c, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.Alloca(sizeof(c), Internal::TypeInfo::Create(&src->CompilationContext, Internal::PrimitiveType::Bool), src->VM.m_LocalStack);
        src->VM.StoreChar(-1, c, src->VM.m_LocalStack);
    }

    void Context::PushShort(int16_t s, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.Alloca(sizeof(s), Internal::TypeInfo::Create(&src->CompilationContext, Internal::PrimitiveType::Bool), src->VM.m_LocalStack);
        src->VM.StoreShort(-1, s, src->VM.m_LocalStack);
    }

    void Context::PushInt(int32_t i, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.Alloca(sizeof(i), Internal::TypeInfo::Create(&src->CompilationContext, Internal::PrimitiveType::Bool), src->VM.m_LocalStack);
        src->VM.StoreInt(-1, i, src->VM.m_LocalStack);
    }

    void Context::PushLong(int64_t l, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.Alloca(sizeof(l), Internal::TypeInfo::Create(&src->CompilationContext, Internal::PrimitiveType::Bool), src->VM.m_LocalStack);
        src->VM.StoreLong(-1, l, src->VM.m_LocalStack);
    }

    void Context::PushFloat(float f, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.Alloca(sizeof(f), Internal::TypeInfo::Create(&src->CompilationContext, Internal::PrimitiveType::Bool), src->VM.m_LocalStack);
        src->VM.StoreFloat(-1, f, src->VM.m_LocalStack);
    }

    void Context::PushDouble(double d, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.Alloca(sizeof(d), Internal::TypeInfo::Create(&src->CompilationContext, Internal::PrimitiveType::Bool), src->VM.m_LocalStack);
        src->VM.StoreDouble(-1, d, src->VM.m_LocalStack);
    }

    void Context::PushPointer(void* p, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.Alloca(sizeof(p), Internal::TypeInfo::Create(&src->CompilationContext, Internal::PrimitiveType::Bool), src->VM.m_LocalStack);
        src->VM.StorePointer(-1, p, src->VM.m_LocalStack);
    }

    void Context::StoreBool(size_t index, bool b, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.StoreBool(index, b, src->VM.m_LocalStack);
    }

    void Context::StoreChar(size_t index, int8_t c, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.StoreChar(index, c, src->VM.m_LocalStack);
    }

    void Context::StoreShort(size_t index, int16_t s, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.StoreShort(index, s, src->VM.m_LocalStack);
    }

    void Context::StoreInt(size_t index, int32_t i, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.StoreInt(index, i, src->VM.m_LocalStack);
    }

    void Context::StoreLong(size_t index, int64_t l, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.StoreLong(index, l, src->VM.m_LocalStack);
    }

    void Context::StoreFloat(size_t index, float f, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.StoreFloat(index, f, src->VM.m_LocalStack);
    }

    void Context::StoreDouble(size_t index, double d, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.StoreDouble(index, d, src->VM.m_LocalStack);
    }

    void Context::StorePointer(size_t index, void* p, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.StorePointer(index, p, src->VM.m_LocalStack);
    }

    void Context::PushGlobal(const std::string& str, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.Dup(src->VM.m_GlobalMap[str], src->VM.m_LocalStack, src->VM.m_GlobalStack);
    }

    void Context::PushArg(size_t index, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.Dup(index + 1, src->VM.m_LocalStack, src->VM.m_FunctionStack);
    }

    void Context::PushField(int32_t index, const std::string& name, const std::string& module) {
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

    bool Context::GetBool(int32_t index, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        return src->VM.GetBool(index, src->VM.m_LocalStack);
    }

    int8_t Context::GetChar(int32_t index, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        return src->VM.GetChar(index, src->VM.m_LocalStack);
    }

    int16_t Context::GetShort(int32_t index, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        return src->VM.GetShort(index, src->VM.m_LocalStack);
    }

    int32_t Context::GetInt(int32_t index, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        return src->VM.GetInt(index, src->VM.m_LocalStack);
    }

    int64_t Context::GetLong(int32_t index, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        return src->VM.GetLong(index, src->VM.m_LocalStack);
    }

    float Context::GetFloat(int32_t index, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        return src->VM.GetFloat(index, src->VM.m_LocalStack);
    }

    double Context::GetDouble(int32_t index, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        return src->VM.GetDouble(index, src->VM.m_LocalStack);
    }

    void* Context::GetPointer(int32_t index, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        return src->VM.GetPointer(index, src->VM.m_LocalStack);
    }

    StackSlot Context::GetStackSlot(int32_t index, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        // Internal::VMSlice slice = src->VM.GetVMSlice(index, src->VM.);
        // return {slice.Memory, slice.Size};
        ARIA_ASSERT(false, "todo");
    }

    void Context::Pop(size_t count, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);

        src->VM.Pop(count, src->VM.m_LocalStack);
    }

    void Context::AddExternalFunction(const std::string& name, ExternFn fn, const std::string& module) {
        CompiledSource* src = GetCompiledSource(module);
        src->VM.AddExtern(name, fn);
    }

    void Context::Call(const std::string& str, const std::string& module) {
        ARIA_ASSERT(m_Modules.contains(module), "Current context does not contain the requested module!");
        CompiledSource* src = m_Modules.at(module);

        ARIA_ASSERT(false, "Add Context::Call()");
        // ARIA_ASSERT(src->ReflectionData.Declarations.contains(str), "Trying to call an unknown function");
        // ARIA_ASSERT(src->ReflectionData.Declarations.at(str).Type == Internal::ReflectionType::Function, "Trying to call an non-function");
        // size_t size = src->ReflectionData.Declarations.at(str).ResolvedType->GetSize();
        // if (size > 0) {
        //     src->VM.PushBytes(size, src->ReflectionData.Declarations.at(str).ResolvedType);
        // }
        // src->VM.Call(std::get<int32_t>(src->ReflectionData.Declarations.at(str).Data));
    }

    void Context::SetRuntimeErrorHandler(RuntimeErrorHandlerFn fn) {
        m_RuntimeErrorHandler = fn;
    }

    void Context::SetCompilerErrorHandler(CompilerErrorHandlerFn fn) {
        m_CompilerErrorHandler = fn;
    }

    CompiledSource* Context::GetCompiledSource(const std::string& module) {
        if (module.empty()) {
            ARIA_ASSERT(m_CurrentCompiledSource, "Cannot get any active module!");
            return m_CurrentCompiledSource;
        }

        ARIA_ASSERT(m_Modules.contains(module), "Current context does not contain the requested module!");
        return m_Modules.at(module);
    }

    void Context::ReportRuntimeError(const std::string& error) {
        if (m_RuntimeErrorHandler) {
            m_RuntimeErrorHandler(error);
        } else {
            fmt::print(stderr, "A runtime error occurred!\nError message: {}", error);
        }

        m_CurrentCompiledSource->VM.StopExecution();
    }

} // namespace Aria
