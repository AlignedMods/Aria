#pragma once

#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <string>
#include <vector>

namespace Aria::Internal {
    class VM;
}

namespace Aria {

    struct Context;

    struct Module;
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

        void CompileFile(const std::string& path);
        void CompileFiles(const std::vector<std::string>& paths, const std::string& module);

        void AddStandardLib();

        // Sets up the current module which will be used up until the next
        // SetActiveModule call
        void SetActiveModule(const std::string& module);

        // Deallocates the given module
        void FreeModule(const std::string& module);

        // Run the compiled string in the VM
        // Dissasemble the byte emitted byte code
        void Run();

        std::string DumpAST();
        std::string Disassemble();

        void PushBool   (bool b);
        void PushChar   (int8_t c);
        void PushShort  (int16_t s);
        void PushInt    (int32_t i);
        void PushLong   (int64_t l);
        void PushFloat  (float f);
        void PushDouble (double f);
        void PushPointer(void* p);

        void StoreBool   (int32_t index, bool b);
        void StoreChar   (int32_t index, int8_t c);
        void StoreShort  (int32_t index, int16_t s);
        void StoreInt    (int32_t index, int32_t i);
        void StoreLong   (int32_t index, int64_t l);
        void StoreFloat  (int32_t index, float f);
        void StoreDouble (int32_t index, double d);
        void StorePointer(int32_t index, void* p);
        void StoreString (int32_t index, std::string_view str);

        void GetGlobal(const std::string& str);
        void GetGlobalPtr(const std::string& str);
        void GetArg(int32_t index);
        void GetField(int32_t index, const std::string& name);

        bool             GetBool     (int32_t index);
        int8_t           GetChar     (int32_t index);
        int16_t          GetShort    (int32_t index);
        int32_t          GetInt      (int32_t index);
        int64_t          GetLong     (int32_t index);
        float            GetFloat    (int32_t index);
        double           GetDouble   (int32_t index);
        void*            GetPointer  (int32_t index);
        std::string_view GetString   (int32_t index);

        void Pop(size_t count);

        bool HasFunction(const std::string& str);
        void Call(const std::string& str, size_t argCount);

        void AddExternalFunction(const std::string& name, ExternFn fn);

        void SetRuntimeErrorHandler(RuntimeErrorHandlerFn fn);
        void SetCompilerErrorHandler(CompilerErrorHandlerFn fn);

    private:
        void CompileFileRaw(const std::string& source, const std::string& filename);
        void ReportRuntimeError(const std::string& error);

        friend class Internal::VM;

    private:
        std::unordered_map<std::string, Module*> m_Modules;
        Module* m_ActiveModule = nullptr;

        RuntimeErrorHandlerFn m_RuntimeErrorHandler = nullptr;
        CompilerErrorHandlerFn m_CompilerErrorHandler = nullptr;
    };

} // namespace Aria
