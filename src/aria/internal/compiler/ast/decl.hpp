#pragma once

#include "aria/internal/compiler/ast/stmt.hpp"

namespace Aria::Internal {

    struct Expr;

    struct Decl : public Stmt {
        Decl(CompilationContext* ctx)
            : Stmt(ctx) {}
    };

    // Represents an entire translation unit
    // This should always be the root node of the AST
    struct TranslationUnitDecl final : public Decl {
        TranslationUnitDecl(CompilationContext* ctx, TinyVector<Stmt*> stmts)
            : Decl(ctx), m_Stmts(stmts) {}

        TinyVector<Stmt*> GetStmts() const { return m_Stmts; }

    private:
        TinyVector<Stmt*> m_Stmts;
    };

    struct VarDecl final : public Decl {
        VarDecl(CompilationContext* ctx, StringView identifier, StringView parsedType, Expr* defaultValue)
            : Decl(ctx), m_Identifier(identifier), m_ParsedType(parsedType), m_DefaultValue(defaultValue) {}

        inline std::string GetIdentifier() const { return fmt::format("{}", m_Identifier); }
        inline StringView GetRawIdentifier() const { return m_Identifier; }

        inline StringView GetParsedType() const { return m_ParsedType; }

        inline Expr* GetDefaultValue() { return m_DefaultValue; }
        inline const Expr* GetDefaultValue() const { return m_DefaultValue; }
        inline void SetDefaultValue(Expr* expr) { m_DefaultValue = expr; }

        inline TypeInfo* GetResolvedType() { return m_ResolvedType; }
        inline const TypeInfo* GetResolvedType() const { return m_ResolvedType; }
        inline void SetResolvedType(TypeInfo* type) { m_ResolvedType = type; }

    private:
        StringView m_Identifier;
        StringView m_ParsedType;
        Expr* m_DefaultValue = nullptr;

        TypeInfo* m_ResolvedType = nullptr;
    };

    struct ParamDecl final : public Decl {
        ParamDecl(CompilationContext* ctx, StringView identifier, StringView parsedType)
            : Decl(ctx), m_Identifier(identifier), m_ParsedType(parsedType) {}

        inline std::string GetIdentifier() const { return fmt::format("{}", m_Identifier); }
        inline StringView GetRawIdentifier() const { return m_Identifier; }

        inline StringView GetParsedType() const { return m_ParsedType; }

        inline TypeInfo* GetResolvedType() { return m_ResolvedType; }
        inline const TypeInfo* GetResolvedType() const { return m_ResolvedType; }
        inline void SetResolvedType(TypeInfo* type) { m_ResolvedType = type; }

    private:
        StringView m_Identifier;
        StringView m_ParsedType;

        TypeInfo* m_ResolvedType = nullptr;
    };

    struct FunctionDecl final : public Decl {
        FunctionDecl(CompilationContext* ctx, StringView identifier, StringView parsedType, TinyVector<ParamDecl*> params, bool external, CompoundStmt* body)
            : Decl(ctx), m_Identifier(identifier), m_ParsedType(parsedType), m_Parameters(params), m_Extern(external), m_Body(body) {}

        inline std::string GetIdentifier() const { return fmt::format("{}", m_Identifier); }
        inline StringView GetRawIdentifier() const { return m_Identifier; }

        inline StringView GetParsedType() const { return m_ParsedType; }

        inline TinyVector<ParamDecl*> GetParameters() const { return m_Parameters; }
        inline bool IsExtern() const { return m_Extern; }

        inline CompoundStmt* GetBody() { return m_Body; }
        inline const CompoundStmt* GetBody() const { return m_Body; }

        inline TypeInfo* GetResolvedType() { return m_ResolvedType; }
        inline const TypeInfo* GetResolvedType() const { return m_ResolvedType; }
        inline void SetResolvedType(TypeInfo* type) { m_ResolvedType = type; }

    private:
        StringView m_Identifier;
        StringView m_ParsedType;
        TinyVector<ParamDecl*> m_Parameters;
        bool m_Extern = false;

        CompoundStmt* m_Body = nullptr;

        TypeInfo* m_ResolvedType = nullptr;
    };

    struct StructDecl final : public Decl {
        StructDecl(CompilationContext* ctx, StringView identifier, TinyVector<Decl> fields)
            : Decl(ctx), m_Identifier(identifier), m_Fields(fields) {}

        inline std::string GetIdentifier() const { return fmt::format("{}", m_Identifier); }
        inline StringView GetRawIdentifier() const { return m_Identifier; }

        inline TinyVector<Decl> GetFields() const { return m_Fields; }

    private:
        StringView m_Identifier;
        TinyVector<Decl> m_Fields;
    };

    struct FieldDecl final : public Decl {
        FieldDecl(CompilationContext* ctx, StringView identifier)
            : Decl(ctx), m_Identifier(identifier) {}

        inline std::string GetIdentifier() const { return fmt::format("{}", m_Identifier); }
        inline StringView GetRawIdentifier() const { return m_Identifier; }

        inline TypeInfo* GetResolvedType() { return m_ResolvedType; }
        inline const TypeInfo* GetResolvedType() const { return m_ResolvedType; }
        inline void SetResolvedType(TypeInfo* type) { m_ResolvedType = type; }

    private:
        StringView m_Identifier;
        
        TypeInfo* m_ResolvedType = nullptr;
    };

    struct MethodDecl final : public Decl {
        MethodDecl(CompilationContext* ctx, StringView identifier, TinyVector<ParamDecl> parameters)
            : Decl(ctx), m_Identifier(identifier), m_Parameters(parameters) {}

        inline std::string GetIdentifier() const { return fmt::format("{}", m_Identifier); }
        inline StringView GetRawIdentifier() const { return m_Identifier; }

        inline TinyVector<ParamDecl> GetParameters() const { return m_Parameters; }

        inline TypeInfo* GetResolvedType() { return m_ResolvedType; }
        inline const TypeInfo* GetResolvedType() const { return m_ResolvedType; }
        inline void SetResolvedType(TypeInfo* type) { m_ResolvedType = type; }

    private:
        StringView m_Identifier;
        TinyVector<ParamDecl> m_Parameters;

        TypeInfo* m_ResolvedType = nullptr;
    };

} // namespace Aria::Internal
