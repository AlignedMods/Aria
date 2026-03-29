#pragma once

#include "aria/internal/compiler/ast/expr.hpp"
#include "aria/internal/compiler/ast/stmt.hpp"
#include "aria/internal/compiler/ast/decl.hpp"

namespace Aria::Internal {

    class ASTDumper {
    public:
        ASTDumper(Stmt* rootASTNode);

        std::string& GetOutput();

    private:
        void DumpASTImpl();

        void DumpExpr(Expr* expr, size_t indentation);
        void DumpDecl(Decl* decl, size_t indentation);
        void DumpStmt(Stmt* stmt, size_t indentation);

    private:
        Stmt* m_RootASTNode = nullptr;

        std::string m_Output;
        std::string m_Indentation;

        size_t m_CurrentLine = 1;
    };

} // namespace Aria::Internal
