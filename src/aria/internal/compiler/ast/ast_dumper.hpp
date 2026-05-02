#pragma once

#include "aria/internal/compiler/ast/expr.hpp"
#include "aria/internal/compiler/ast/stmt.hpp"
#include "aria/internal/compiler/ast/decl.hpp"
#include "aria/internal/compiler/ast/specifier.hpp"

namespace Aria::Internal {

    class ASTDumper {
    public:
        ASTDumper(Stmt* rootASTNode);

        std::string& get_output();

    private:
        void dump_ast_impl();

        void dump_expr(Expr* expr, size_t indentation);
        void dump_decl(Decl* decl, size_t indentation);
        void dump_stmt(Stmt* stmt, size_t indentation);
        void dump_specifier(Specifier* spec, size_t indentation);

        void dump_function_attr(FunctionDecl::Attribute attr, size_t indentation);

    private:
        Stmt* m_RootASTNode = nullptr;

        std::string m_Output;
        std::string m_Indentation;

        size_t m_CurrentLine = 1;
    };

} // namespace Aria::Internal
