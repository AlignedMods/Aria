#pragma once

#include "black_lua/internal/compiler/core/vector.hpp"
#include "black_lua/internal/compiler/core/string_view.hpp"
#include "black_lua/internal/compiler/core/string_builder.hpp"
#include "black_lua/internal/compiler/types/type_info.hpp"
#include "black_lua/internal/compiler/core/source_location.hpp"

namespace BlackLua::Internal {

    struct Expr;
    struct VarDecl;

    struct Stmt {
        inline Stmt(CompilationContext* ctx)
            : m_Context(ctx) {}

        virtual void _Unused() const {}

    protected:
        CompilationContext* m_Context = nullptr;
    };

    struct CompoundStmt final : public Stmt {
        CompoundStmt(CompilationContext* ctx, const TinyVector<Stmt*>& stmts)
            : Stmt(ctx), m_Stmts(stmts) {}

        inline TinyVector<Stmt*>& GetStmts() { return m_Stmts; }
        inline const TinyVector<Stmt*>& GetStmts() const { return m_Stmts; }

    private:
        TinyVector<Stmt*> m_Stmts;
    };

    struct WhileStmt final : public Stmt {
        WhileStmt(CompilationContext* ctx, Expr* condition, Stmt* body)
            : Stmt(ctx), m_Condition(condition), m_Body(body) {}

        inline Expr* GetCondition() { return m_Condition; }
        inline const Expr* GetCondition() const { return m_Condition; }

        inline Stmt* GetBody() { return m_Body; }
        inline const Stmt* GetBody() const { return m_Body; }

    private:
        Expr* m_Condition = nullptr;
        Stmt* m_Body = nullptr;
    };
    
    struct DoWhileStmt final : public Stmt {
        DoWhileStmt(CompilationContext* ctx, Expr* condition, Stmt* body)
            : Stmt(ctx), m_Condition(condition), m_Body(body) {}

        inline Expr* GetCondition() { return m_Condition; }
        inline const Expr* GetCondition() const { return m_Condition; }

        inline Stmt* GetBody() { return m_Body; }
        inline const Stmt* GetBody() const { return m_Body; }

    private:
        Expr* m_Condition = nullptr;
        Stmt* m_Body = nullptr;
    };
    
    struct ForStmt final : public Stmt {
        ForStmt(CompilationContext* ctx, Stmt* prologue, Expr* condition, Expr* epilogue, Stmt* body)
            : Stmt(ctx), m_Prologue(prologue), m_Condition(condition), m_Epilogue(epilogue), m_Body(body) {}

        inline Stmt* GetPrologue() { return m_Prologue; }
        inline const Stmt* GetPrologue() const { return m_Prologue; }

        inline Expr* GetCondition() { return m_Condition; }
        inline const Expr* GetCondition() const { return m_Condition; }

        inline Expr* GetEpilogue() { return m_Epilogue; }
        inline const Expr* GetEpilogue() const { return m_Epilogue; }

        inline Stmt* GetBody() { return m_Body; }
        inline const Stmt* GetBody() const { return m_Body; }

    private:
        Stmt* m_Prologue = nullptr; // int i = 0;
        Expr* m_Condition = nullptr; // i < 5;
        Expr* m_Epilogue = nullptr; // i += 1;
        Stmt* m_Body = nullptr;
    };
    
    struct IfStmt final : public Stmt {
        IfStmt(CompilationContext* ctx, Expr* condition, Stmt* body, Stmt* elseBody)
            : Stmt(ctx), m_Condition(condition), m_Body(body), m_ElseBody(elseBody) {}

        inline Expr* GetCondition() { return m_Condition; }
        inline const Expr* GetCondition() const { return m_Condition; }

        inline Stmt* GetBody() { return m_Body; }
        inline const Stmt* GetBody() const { return m_Body; }

        inline Stmt* GetElseBody() { return m_ElseBody; }
        inline const Stmt* GetElseBody() const { return m_ElseBody; }

    private:
        Expr* m_Condition = nullptr;
        Stmt* m_Body = nullptr;
        Stmt* m_ElseBody = nullptr;
    };
    
    struct ReturnStmt final : public Stmt {
        ReturnStmt(CompilationContext* ctx, Expr* value)
            : Stmt(ctx), m_Value(value) {}

        inline Expr* GetValue() { return m_Value; }
        inline const Expr* GetValue() const { return m_Value; }
    
    private:
        Expr* m_Value = nullptr;
    };

} // namespace BlackLua::Internal
