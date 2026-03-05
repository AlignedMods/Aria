#pragma once

#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <string>

namespace BlackLua::Internal {
    class VM;
}

namespace BlackLua {

    struct Context;

    struct CompiledSource;
    class Allocator;

    using RuntimeErrorHandlerFn = void(*)(const std::string& error);
    using CompilerErrorHandlerFn = void(*)(size_t line, size_t column, 
                                           size_t startLine, size_t startColumn, 
                                           size_t endLine, size_t endColumn, const std::string& file, const std::string& error);

    using ExternFn = void(*)(Context* ctx);

    struct StackSlot {
        void* Memory = nullptr;
        size_t Size = 0;
    };

    struct Context {
        Context();
        static Context Create();

        void CompileFile(const std::string& path, const std::string& module);
        void CompileString(const std::string& source, const std::string& module);

        // Deallocates the given module
        void FreeModule(const std::string& module);

        // Run the compiled string in the VM
        // Dissasemble the byte emitted byte code
        void Run(const std::string& module);

        std::string DumpAST(const std::string& module);
        // Returns a string containing the disassembled byte code
        std::string Disassemble(const std::string& module);

        void PushBool(bool b,     const std::string& module = {});
        void PushChar(int8_t c,   const std::string& module = {});
        void PushShort(int16_t s, const std::string& module = {});
        void PushInt(int32_t i,   const std::string& module = {});
        void PushLong(int64_t l,  const std::string& module = {});
        void PushFloat(float f,   const std::string& module = {});
        void PushDouble(double f, const std::string& module = {});
        void PushPointer(void* p, const std::string& module = {});

        void StoreBool(size_t index, bool b,     const std::string& module = {});
        void StoreChar(size_t index, int8_t c,   const std::string& module = {});
        void StoreShort(size_t index, int16_t s, const std::string& module = {});
        void StoreInt(size_t index, int32_t i,   const std::string& module = {});
        void StoreLong(size_t index, int64_t l,  const std::string& module = {});
        void StoreFloat(size_t index, float f,   const std::string& module = {});
        void StoreDouble(size_t index, double d, const std::string& module = {});
        void StorePointer(size_t index, void* p, const std::string& module = {});

        void PushGlobal(const std::string& str, const std::string& module = {});
        void Pop(size_t count, const std::string& module = {});

        void PushField(int32_t index, const std::string& name, const std::string& module = {});

        bool      GetBool(int32_t index,    const std::string& module = {});
        int8_t    GetChar(int32_t index,    const std::string& module = {});
        int16_t   GetShort(int32_t index,   const std::string& module = {});
        int32_t   GetInt(int32_t index,     const std::string& module = {});
        int64_t   GetLong(int32_t index,    const std::string& module = {});
        float     GetFloat(int32_t index,   const std::string& module = {});
        double    GetDouble(int32_t index,  const std::string& module = {});
        void*     GetPointer(int32_t index, const std::string& module = {});
        StackSlot GetStackSlot(int32_t index, const std::string& module = {});

        void AddExternalFunction(const std::string& name, ExternFn fn, const std::string& module);
        void Call(const std::string& str, const std::string& module);

        void SetRuntimeErrorHandler(RuntimeErrorHandlerFn fn);
        void SetCompilerErrorHandler(CompilerErrorHandlerFn fn);

    private:
        CompiledSource* GetCompiledSource(const std::string& module);

        void ReportRuntimeError(const std::string& error);

        Allocator* GetAllocator();

        friend class Internal::VM;

    private:
        std::unordered_map<std::string, CompiledSource*> m_Modules;
        CompiledSource* m_CurrentCompiledSource = nullptr;

        RuntimeErrorHandlerFn m_RuntimeErrorHandler = nullptr;
        CompilerErrorHandlerFn m_CompilerErrorHandler = nullptr;
    };

} // namespace BlackLua
