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

namespace ariac {

    class Codegen {
    private:
        struct ModuleContext {
            llvm::LLVMContext* context = nullptr;
            llvm::Module* module = nullptr;
            llvm::IRBuilder<>* builder = nullptr;
            llvm::Function* function = nullptr;
            llvm::Instruction* alloca_marker = nullptr;

            std::unordered_map<Decl*, llvm::Function*> functions;
            std::unordered_map<Decl*, llvm::AllocaInst*> named_values;
        };

        struct ABITypeInfo {
            TypeInfo* type = nullptr;

            struct {
                bool pass_direct : 1 = false;
                bool pass_by_ptr : 1 = false;
                bool pass_by_integer : 1 = false; // Pass a structure as a single integer
            };
        };

    public:
        Codegen(CompilationContext* ctx);
        ~Codegen();

    private:
        void gen_impl();
        void gen_builtin_types();

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
        llvm::Value* gen_temporary_expr(Expr* expr);
        llvm::Value* gen_call_expr(Expr* expr);
        llvm::Value* gen_construct_expr(Expr* expr);
        llvm::Value* gen_method_call_expr(Expr* expr);
        llvm::Value* gen_array_subscript_expr(Expr* expr);
        llvm::Value* gen_to_slice_expr(Expr* expr);
        llvm::Value* gen_new_expr(Expr* expr);
        llvm::Value* gen_delete_expr(Expr* expr);
        llvm::Value* gen_sizeof_expr(Expr* expr);
        llvm::Value* gen_format_expr(Expr* expr );
        llvm::Value* gen_paren_expr(Expr* expr);
        llvm::Value* gen_implicit_cast_expr(Expr* expr);
        llvm::Value* gen_cast_expr(Expr* expr);
        llvm::Value* gen_unary_operator_expr(Expr* expr);
        llvm::Value* gen_binary_operator_expr(Expr* expr);
        llvm::Value* gen_compound_assign_expr(Expr* expr);

        llvm::Value* gen_expr(Expr* expr);
        llvm::Value* gen_init_expr(Expr* expr, llvm::Value* dst);

        void gen_var_decl(Decl* decl);
        void gen_function_decl(Decl* decl);
        void gen_function_prototype(Decl* decl);
        void gen_method_prototype(Decl* decl);
        void gen_dtor_prototype(Decl* decl);
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
        void gen_expr_stmt(Stmt* stmt);
        void gen_decl_stmt(Stmt* stmt);

        void gen_stmt(Stmt* stmt);

        llvm::Type* type_info_to_llvm_type(TypeInfo* t);
        ABITypeInfo get_abi_type_info(TypeInfo* t);
        u64 get_type_size(TypeInfo* t);
        u64 get_type_alignment(TypeInfo* t);
        u64 align_value(u64 val, u64 alignment);

        std::string mangle_function(Decl* fn);
        std::string mangle_type(TypeInfo* t);
        std::string mangle_ctor(ConstructorDecl* ctor);
        std::string mangle_dtor(DestructorDecl* dtor);
        std::string mangle_method(MethodDecl* md);

        std::string valid_module_name(std::string_view name);

        llvm::AllocaInst* alloca_at_entry(llvm::Function* f, llvm::StringRef name, TypeInfo* type);

    private:
        ModuleContext m_active_module_context;
        std::unordered_map<Module*, ModuleContext> m_module_contexts;
        llvm::Triple m_triple;

        llvm::AllocaInst* m_self_value = nullptr;
    
        CompilationContext* m_context = nullptr;
    };

} // namespace ariac
