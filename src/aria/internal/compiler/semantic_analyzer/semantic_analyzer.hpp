#pragma once

#include "aria/internal/compiler/ast/expr.hpp"
#include "aria/internal/compiler/ast/decl.hpp"
#include "aria/internal/compiler/ast/stmt.hpp"
#include "aria/internal/compiler/ast/specifier.hpp"
#include "aria/internal/compiler/compilation_context.hpp"

#include <unordered_map>

namespace Aria::Internal {

    struct ConversionCost {
        CastKind Kind = CastKind::Invalid;

        bool CastNeeded = false;
        bool ImplicitCastPossible = false;
        bool ExplicitCastPossible = false;
    };

    class SemanticAnalyzer {
    private:
        struct Declaration {
            TypeInfo* ResolvedType = nullptr;
            Decl* SourceDeclaration = nullptr;
            DeclKind Kind = DeclKind::Var;
        };

        struct Scope {
            std::unordered_map<std::string, Declaration> Declarations;
            bool AllowBreakStmt = false;
            bool AllowContinueStmt = false;
            bool ReachesEnd = true;
        };

    public:
        SemanticAnalyzer(CompilationContext* ctx);

    private:
        void sema_impl();

        // Passes
        void pass_imports();
        void pass_decls();
        void pass_code();

        void add_unit_to_module(Module* module, CompilationUnit* unit);
        void resolve_module_imports(Module* module);
        void resolve_unit_imports(Module* module, CompilationUnit* unit);

        void resolve_module_decls(Module* module);
        void resolve_unit_decls(Module* module, CompilationUnit* unit);

        void resolve_module_code(Module* module);
        void resolve_unit_code(Module* module, CompilationUnit* unit);

        void resolve_BooleanConstant_expr(Expr* expr);
        void resolve_CharacterConstant_expr(Expr* expr);
        void resolve_IntegerConstant_expr(Expr* expr);
        void resolve_FloatingConstant_expr(Expr* expr);
        void resolve_StringConstant_expr(Expr* expr);
        void resolve_Null_expr(Expr* expr);
        void resolve_DeclRef_expr(Expr* expr);
        void resolve_Member_expr(Expr* expr);
        void resolve_BuiltinMember_expr(Expr* expr);
        void resolve_Temporary_expr(Expr* expr);
        void resolve_Copy_expr(Expr* expr);
        void resolve_Call_expr(Expr* expr);
        void resolve_Construct_expr(Expr* expr);
        void resolve_MethodCall_expr(Expr* expr);
        void resolve_ArraySubscript_expr(Expr* expr);
        void resolve_ToSlice_expr(Expr* expr);
        void resolve_New_expr(Expr* expr);
        void resolve_Delete_expr(Expr* expr);
        void resolve_Format_expr(Expr* expr);
        void resolve_Paren_expr(Expr* expr);
        void resolve_Cast_expr(Expr* expr);
        void resolve_ImplicitCast_expr(Expr* expr);
        void resolve_UnaryOperator_expr(Expr* expr);
        void resolve_BinaryOperator_expr(Expr* expr);
        void resolve_CompoundAssign_expr(Expr* expr);

        void resolve_expr(Expr* expr);

        void resolve_TranslationUnit_decl(Decl* decl);
        void resolve_Module_decl(Decl* decl);
        void resolve_Var_decl(Decl* decl);
        void resolve_Param_decl(Decl* decl);
        void resolve_Function_decl(Decl* decl);
        void resolve_OverloadedFunction_decl(Decl* decl);
        void resolve_Struct_decl(Decl* decl);
        void resolve_Field_decl(Decl* decl);
        void resolve_Constructor_decl(Decl* decl);
        void resolve_Destructor_decl(Decl* decl);
        void resolve_Method_decl(Decl* decl);
        void resolve_BuiltinCopyConstructor_decl(Decl* decl);
        void resolve_BuiltinDestructor_decl(Decl* decl);

        void resolve_decl(Decl* decl);

        void resolve_Nop_stmt(Stmt* stmt);
        void resolve_Import_stmt(Stmt* stmt);
        void resolve_Block_stmt(Stmt* stmt);
        void resolve_While_stmt(Stmt* stmt);
        void resolve_DoWhile_stmt(Stmt* stmt);
        void resolve_For_stmt(Stmt* stmt);
        void resolve_If_stmt(Stmt* stmt);
        void resolve_Break_stmt(Stmt* stmt);
        void resolve_Continue_stmt(Stmt* stmt);
        void resolve_Return_stmt(Stmt* stmt);
        void resolve_Expr_stmt(Stmt* stmt);
        void resolve_Decl_stmt(Stmt* stmt);

        void resolve_stmt(Stmt* stmt);

        void resolve_type(SourceLocation loc, SourceRange range, TypeInfo* type);

        void resolve_var_initializer(Decl* decl);
        void resolve_param_initializer(TypeInfo* paramType, Expr* arg);
        void create_default_initializer(Expr** expr, TypeInfo* type, SourceLocation loc, SourceRange range);

        bool is_const_expr(Expr* expr);
        bool eval_expr_bool(Expr* expr);

        void push_scope(bool allowBreak = false, bool allowContinue = false);
        void pop_scope();

        ConversionCost get_conversion_cost(TypeInfo* dst, TypeInfo* src);
        void insert_implicit_cast(TypeInfo* dstType, TypeInfo* srcType, Expr* srcExpr, CastKind castKind);
        void require_rvalue(Expr* expr);
        void insert_arithmetic_promotion(Expr* lhs, Expr* rhs);

        void replace_expr(Expr* src, Expr* newExpr);
        void replace_decl(Decl* src, Decl* newDecl);

        bool type_is_equal(TypeInfo* lhs, TypeInfo* rhs);
        size_t type_get_size(TypeInfo* t);
        bool type_is_trivial(TypeInfo* t);

        std::string mangle_function(FunctionDecl* fn);

    private:
        std::unordered_map<std::string, bool> m_ImportedModules;

        bool m_TemporaryContext = false;
        bool m_UnsafeContext = false;

        Decl* m_BuiltInStringDestructor = nullptr;
        Decl* m_BuiltInStringCopyConstructor = nullptr;

        std::vector<Scope> m_Scopes;
        TypeInfo* m_ActiveReturnType = nullptr;
        TypeInfo* m_ActiveStruct = nullptr;

        CompilationContext* m_Context = nullptr;
    };

} // namespace Aria::Internal
