#pragma once

#include "aria/internal/compiler/ast/expr.hpp"
#include "aria/internal/compiler/ast/stmt.hpp"
#include "aria/internal/compiler/ast/decl.hpp"
#include "aria/internal/compiler/ast/specifier.hpp"

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
        void DumpSpecifier(Specifier* spec, size_t indentation);

        std::string DumpDeclarationFlags(int flags);

    private:
        Stmt* m_RootASTNode = nullptr;

        std::string m_Output;
        std::string m_Indentation;

        size_t m_CurrentLine = 1;
    };

} // namespace Aria::Internal
