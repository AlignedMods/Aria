#pragma once

#include "ariac/ast/expr.hpp"
#include "ariac/ast/stmt.hpp"
#include "ariac/ast/decl.hpp"
#include "ariac/ast/specifier.hpp"

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
        Stmt* m_root_ast_node = nullptr;
        std::string m_output;
    };

} // namespace Aria::Internal
