#pragma once

#include "ariac/ast/expr.hpp"
#include "ariac/ast/stmt.hpp"
#include "ariac/ast/decl.hpp"
#include "ariac/ast/specifier.hpp"

#pragma warning(push, 0) // Disable warnings from LLVM headers
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Process.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#pragma warning(pop)

#include <memory>

namespace ariac {

    class Codegen {
    private:
        struct DebugContext {
            llvm::DICompileUnit* unit = nullptr;
            llvm::DIBuilder* builder = nullptr;
            std::unordered_map<std::string, llvm::DIType*> cached_types;
            llvm::DIScope* scope = nullptr;
        };

        struct ModuleContext {
            llvm::LLVMContext* context = nullptr;
            llvm::Module* module = nullptr;
            llvm::IRBuilder<>* builder = nullptr;
            llvm::Function* function = nullptr;
            llvm::Instruction* alloca_marker = nullptr;

            std::unordered_map<Decl*, llvm::Function*> functions;
            std::unordered_map<Decl*, llvm::Value*> named_values;
            std::unordered_map<Decl*, std::string> generic_structs;
            std::unordered_map<std::string, llvm::Constant*> typeinfos; // Runtime type information for types

            // Debug info
            std::unordered_map<std::string, DebugContext> debug_contexts;
        };

        enum class ABIParamKind {
            Direct,
            Pointer,
            Integer
        };

        enum class ABIRetKind {
            Direct,
            Pointer,
            Integer
        };

        struct ABIParamTypeInfo {
            TypeInfo* type = nullptr;
            size_t int_bits = 0; // If we are passing a structure as an integer, how many bits does this integer have
            ABIParamKind kind = ABIParamKind::Direct;
        };

        struct ABIRetTypeInfo {
            TypeInfo* type = nullptr;
            size_t int_bits = 0; // If we are returning a structure as an integer, how many bits does this integer have
            ABIRetKind kind = ABIRetKind::Direct;
        };

        struct Scope {
            std::vector<Stmt*> defers;
            llvm::BasicBlock* loop_start_block = nullptr;
            llvm::BasicBlock* loop_end_block = nullptr;
        };

        enum class LoopKind {
            Normal, // The condition for this loop is determined at runtime
            Always, // The condition for this loop will always be true (but the loop may or may not be infinite)
            Never // The condition for this loop will always be false
        };

    public:
        Codegen();

    private:
        void gen_impl();
        void gen_builtin_types();

        void setup_env();
        void gen_mod_to_ir(Module* mod);
        void gen_mod_to_obj(Module* mod);
        void gen_mod_ir_dump(Module* mod);
        void link();

        void link_windows();

        llvm::Value* gen_boolean_literal_expr(Expr* expr);
        llvm::Value* gen_character_literal_expr(Expr* expr);
        llvm::Value* gen_integer_literal_expr(Expr* expr);
        llvm::Value* gen_floating_literal_expr(Expr* expr);
        llvm::Value* gen_string_literal_expr(Expr* expr);
        llvm::Value* gen_array_filler_expr(Expr* expr);
        llvm::Value* gen_null_expr(Expr* expr);
        llvm::Value* gen_decl_ref_expr(Expr* expr);
        llvm::Value* gen_member_expr(Expr* expr);
        llvm::Value* gen_builtin_member_expr(Expr* expr);
        llvm::Value* gen_self_expr(Expr* expr);
        llvm::Value* gen_call_expr(Expr* expr);
        llvm::Value* gen_builtin_call_expr(Expr* expr);
        llvm::Value* gen_intrinsic_call_expr(Expr* expr);
        llvm::Value* gen_construct_expr(Expr* expr);
        llvm::Value* gen_array_literal_expr(Expr* expr);
        llvm::Value* gen_method_call_expr(Expr* expr);
        llvm::Value* gen_array_subscript_expr(Expr* expr);
        llvm::Value* gen_to_slice_expr(Expr* expr);
        llvm::Value* gen_paren_expr(Expr* expr);
        llvm::Value* gen_implicit_cast_expr(Expr* expr);
        llvm::Value* gen_cast_expr(Expr* expr);
        llvm::Value* gen_unary_operator_expr(Expr* expr);
        llvm::Value* gen_binary_operator_expr(Expr* expr);
        llvm::Value* gen_compound_assign_expr(Expr* expr);
        llvm::Value* gen_const_expr(Expr* expr);

        llvm::Value* gen_expr(Expr* expr);
        llvm::Value* gen_init_expr(Expr* expr, llvm::Value* dst);

        void gen_var_decl(Decl* decl);
        void gen_function_decl(Decl* decl);
        void gen_function_prototype(Decl* decl);
        void gen_method_prototype(Decl* decl);
        void gen_struct_decl(Decl* decl);
        void gen_impl_decl(Decl* decl);

        void gen_decl(Decl* decl);

        void gen_block_stmt(Stmt* stmt);
        void gen_while_stmt(Stmt* stmt);
        void gen_do_while_stmt(Stmt* stmt);
        void gen_for_stmt(Stmt* stmt);
        void gen_if_stmt(Stmt* stmt);
        void gen_break_stmt(Stmt* stmt);
        void gen_continue_stmt(Stmt* stmt);
        void gen_return_stmt(Stmt* stmt);
        void gen_defer_stmt(Stmt* stmt);
        void gen_expr_stmt(Stmt* stmt);
        void gen_decl_stmt(Stmt* stmt);

        void gen_stmt(Stmt* stmt);

        LoopKind get_loop_kind_from_cond(Expr* cond);

        llvm::Type* type_info_to_llvm_type(TypeInfo* t);
        llvm::DIType* type_info_to_debug_type(TypeInfo* t);
        std::string mangle_type(TypeInfo* t);
        llvm::GlobalValue::LinkageTypes linkage_kind_to_llvm(LinkageKind kind);

        std::string valid_module_name(std::string_view name);

        llvm::AllocaInst* alloca_at_entry(llvm::Function* f, llvm::StringRef name, TypeInfo* type);
        llvm::AllocaInst* alloca_at_entry(llvm::Function* f, llvm::StringRef name, llvm::Type* type);

        llvm::Constant* get_sz(u64 i);
        llvm::Constant* get_i64(u64 i);
        llvm::Constant* get_null();
        llvm::Constant* get_string(std::string_view s, std::string_view name = ".str");
        llvm::Constant* get_typeinfo(TypeInfo* t);

        // Returns nullptr if there is no assert function
        llvm::Function* get_assert_func();
        void call_assert(llvm::Value* cond, u64 line, const std::string& fmt, const std::vector<llvm::Value*>& args = {}, const std::vector<TypeInfo*>& types = {});

        ABIParamTypeInfo get_param_abi_type_info(TypeInfo* t);
        ABIRetTypeInfo get_ret_abi_type_info(TypeInfo* t);
        void gen_call_param(std::vector<llvm::Value*>* args, llvm::Value* arg, TypeInfo* type);
        void gen_call_variadic(std::vector<llvm::Value*>* args, const std::vector<llvm::Value*>& vals, const std::vector<TypeInfo*>& types);
        llvm::Value* gen_call_raw(std::vector<llvm::Value*>& args, llvm::Function* func, TypeInfo* ret_type);

        void set_debug_loc(const SourceLoc& loc);

        void push_scope();
        void pop_scope();
        void pop_defers(Scope& s);

    private:
        ModuleContext m_active_module_context;
        DebugContext m_active_debug_context;
        std::unordered_map<Module*, ModuleContext> m_module_contexts;
        const llvm::Target* m_target = nullptr;
        llvm::TargetMachine* m_machine = nullptr;
        std::vector<std::string> m_object_files;

        std::vector<Scope> m_scopes;

        llvm::AllocaInst* m_self_value = nullptr;
        ABIRetTypeInfo m_ret_type_abi;
    };

} // namespace ariac
