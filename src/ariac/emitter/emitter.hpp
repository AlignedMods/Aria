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

namespace Aria::Internal {

    class Emitter {
    private:
        struct ModuleContext {
            llvm::LLVMContext* context = nullptr;
            llvm::Module* module = nullptr;
            llvm::IRBuilder<>* builder = nullptr;
            llvm::Function* function = nullptr;
        };

    public:
        Emitter(CompilationContext* ctx);

    private:
        void emit_impl();
        void emit_builtin_types();

        llvm::Value* emit_boolean_literal_expr(Expr* expr);
        llvm::Value* emit_character_literal_expr(Expr* expr);
        llvm::Value* emit_integer_literal_expr(Expr* expr);
        llvm::Value* emit_floating_literal_expr(Expr* expr);
        llvm::Value* emit_string_literal_expr(Expr* expr);
        llvm::Value* emit_array_filler_expr(Expr* expr);
        llvm::Value* emit_null_expr(Expr* expr);
        llvm::Value* emit_decl_ref_expr(Expr* expr);
        llvm::Value* emit_member_expr(Expr* expr);
        llvm::Value* emit_builtin_member_expr(Expr* expr);
        llvm::Value* emit_self_expr(Expr* expr);
        llvm::Value* emit_temporary_expr(Expr* expr);
        llvm::Value* emit_call_expr(Expr* expr);
        llvm::Value* emit_construct_expr(Expr* expr);
        llvm::Value* emit_method_call_expr(Expr* expr);
        llvm::Value* emit_array_subscript_expr(Expr* expr);
        llvm::Value* emit_to_slice_expr(Expr* expr);
        llvm::Value* emit_new_expr(Expr* expr);
        llvm::Value* emit_delete_expr(Expr* expr);
        llvm::Value* emit_format_expr(Expr* expr );
        llvm::Value* emit_paren_expr(Expr* expr);
        llvm::Value* emit_implicit_cast_expr(Expr* expr);
        llvm::Value* emit_cast_expr(Expr* expr);
        llvm::Value* emit_unary_operator_expr(Expr* expr);
        llvm::Value* emit_binary_operator_expr(Expr* expr);
        llvm::Value* emit_compound_assign_expr(Expr* expr);

        llvm::Value* emit_expr(Expr* expr);
        llvm::Value* emit_init_expr(Expr* expr, llvm::Value* dst);

        void emit_var_decl(Decl* decl);
        void emit_function_decl(Decl* decl);
        void emit_function_prototype(Decl* decl);
        void emit_struct_decl(Decl* decl);
        void emit_impl_decl(Decl* decl);

        void emit_decl(Decl* decl);

        void emit_block_stmt(Stmt* stmt);
        void emit_while_stmt(Stmt* stmt);
        void emit_do_while_stmt(Stmt* stmt);
        void emit_for_stmt(Stmt* stmt);
        void emit_if_stmt(Stmt* stmt);
        void emit_break_stmt(Stmt* stmt);
        void emit_continue_stmt(Stmt* stmt);
        void emit_return_stmt(Stmt* stmt);
        void emit_expr_stmt(Stmt* stmt);
        void emit_decl_stmt(Stmt* stmt);

        void emit_stmt(Stmt* stmt);

        void push_stack_frame(const std::string& name);
        void pop_stack_frame();

        void push_scope();
        void pop_scope();

        void merge_pending_op_codes();

        llvm::Type* type_info_to_llvm_type(TypeInfo* t);

        std::string mangle_function(Decl* fn);
        std::string mangle_ctor(ConstructorDecl* ctor);
        std::string mangle_dtor(DestructorDecl* dtor);
        std::string mangle_method(MethodDecl* md);

        std::string valid_module_name(std::string_view name);

        llvm::AllocaInst* alloca_at_entry(llvm::Function* f, llvm::StringRef name, TypeInfo* type);

    private:
        CompilerReflectionData m_reflection_data;
        ModuleContext m_active_module_context;

        std::string m_namespace;
        std::string m_active_namespace;

        Stmt* m_root_ast_node = nullptr;

        std::unordered_map<TypeKind, u16> m_basic_types;
        u16 m_struct_index = 0;

        std::unordered_map<Decl*, llvm::AllocaInst*> m_named_values;
        std::unordered_map<Decl*, llvm::Function*> m_functions;

        // Counters
        size_t m_arr_init_counter = 0;
        size_t m_check_counter = 0;
        size_t m_and_counter = 0;
        size_t m_or_counter = 0;
        size_t m_loop_counter = 0;
        size_t m_if_counter = 0;
    
        CompilationContext* m_context = nullptr;
    };

} // namespace Aria::Internal
