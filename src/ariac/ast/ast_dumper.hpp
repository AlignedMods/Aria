#pragma once

#include "ariac/ast/expr.hpp"
#include "ariac/ast/stmt.hpp"
#include "ariac/ast/decl.hpp"
#include "ariac/ast/specifier.hpp"

namespace ariac {

    class ASTDumper {
    public:
        ASTDumper(CompilationUnit* unit);

        std::string& get_output();

    private:
        void dump_ast_impl();

        void dump_expr(Expr* expr, size_t indentation);
        void dump_decl(Decl* decl, size_t indentation);
        void dump_stmt(Stmt* stmt, size_t indentation);
        void dump_specifier(Specifier* spec, size_t indentation);

        void dump_const_expr_val(ConstExpr* val, size_t indentation);

        void dump_attributes(TinyVector<DeclAttribute> attrs, size_t indentation);
        void dump_attribute(DeclAttribute attr, size_t indentation);

        std::string source_loc_to_string(SourceLoc loc);

    private:
        CompilationUnit* m_unit = nullptr;
        std::string m_output;
        SourceLoc m_prev_loc;
    };

} // namespace ariac
