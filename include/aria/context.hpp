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

    using ExternFn = void(*)(Context* ctx);

    struct Context {
        Context();
        static Context Create();

        void load_file(const std::string& path);

        void add_standard_lib();

        // Sets up the current module which will be used up until the next
        // SetActiveModule call
        void set_active_module(const std::string& module);

        // Deallocates the given module
        void free_module(const std::string& module);

        // Run the compiled string in the VM
        // Dissasemble the byte emitted byte code
        void run();

        std::string dump_ast();
        std::string disassemble();

        void push_bool   (bool b);
        void push_char   (int8_t c);
        void push_short  (int16_t s);
        void push_int    (int32_t i);
        void push_long   (int64_t l);
        void push_float  (float f);
        void push_double (double f);
        void push_pointer(void* p);
        void push_string (std::string_view s);

        void store_bool   (int32_t index, bool b);
        void store_char   (int32_t index, int8_t c);
        void store_short  (int32_t index, int16_t s);
        void store_int    (int32_t index, int32_t i);
        void store_long   (int32_t index, int64_t l);
        void store_float  (int32_t index, float f);
        void store_double (int32_t index, double d);
        void store_pointer(int32_t index, void* p);
        void store_string (int32_t index, std::string_view str);

        void get_global(std::string_view str);
        void get_global_ptr(std::string_view str);

        bool             get_bool     (int32_t index);
        int8_t           get_char     (int32_t index);
        uint8_t          get_uchar    (int32_t index);
        int16_t          get_short    (int32_t index);
        uint16_t         get_ushort   (int32_t index);
        int32_t          get_int      (int32_t index);
        uint32_t         get_uint     (int32_t index);
        int64_t          get_long     (int32_t index);
        uint64_t         get_ulong    (int32_t index);
        float            get_float    (int32_t index);
        double           get_double   (int32_t index);
        void*            get_pointer  (int32_t index);
        std::string_view get_string   (int32_t index);

        void pop(size_t count);

        bool has_function(const std::string& str);
        void call(const std::string& str, size_t argCount);

        void add_external_function(std::string_view name, ExternFn fn);

        void set_runtime_error_handler(RuntimeErrorHandlerFn fn);

    private:
        void report_runtime_error(const std::string& error);

        friend class Internal::VM;

    private:
        std::unordered_map<std::string, Module*> m_modules;
        Module* m_active_module = nullptr;

        RuntimeErrorHandlerFn m_runtime_error_handler = nullptr;
    };

} // namespace Aria
