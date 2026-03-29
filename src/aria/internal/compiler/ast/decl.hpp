#pragma once

#include <aria/internal/compiler/ast/stmt.hpp>

namespace Aria::Internal {

    enum class DeclKind {
        Invalid = 0,

        TranslationUnit,
        Var,
        Param,
        Function,
        Struct,
        Field,
        Method,
        BuiltinCopyConstructor,
        BuiltinDestructor
    };

    enum class BuiltinKind {
        String
    };

    struct Expr;
    struct Stmt;

    // Represents an entire translation unit
    // This should always be the root node of the AST
    struct TranslationUnitDecl {
        TranslationUnitDecl(TinyVector<Stmt*> stmts)
            : Stmts(stmts) {}

        TinyVector<Stmt*> Stmts;
    };

    struct VarDecl {
        VarDecl(StringView identifier, TypeInfo* type, Expr* defaultValue)
            : Identifier(identifier), Type(type), DefaultValue(defaultValue) {}

        StringView Identifier;
        TypeInfo* Type = nullptr;
        Expr* DefaultValue = nullptr;
    };

    struct ParamDecl {
        ParamDecl(StringView identifier, TypeInfo* type)
            : Identifier(identifier), Type(type) {}

        StringView Identifier;
        TypeInfo* Type = nullptr;
    };

    struct FunctionDecl {
        FunctionDecl( StringView identifier, TypeInfo* type, TinyVector<Decl*> params, Stmt* body)
            : Identifier(identifier), Type(type), Parameters(params), Body(body) {}

        StringView Identifier;
        TypeInfo* Type = nullptr;
        TinyVector<Decl*> Parameters;
        Stmt* Body = nullptr;
    };

    struct StructDecl {
        StructDecl(StringView identifier, TinyVector<Decl*> fields)
            : Identifier(identifier), Fields(fields) {}

        StringView Identifier;
        TinyVector<Decl*> Fields;
    };

    struct FieldDecl {
        FieldDecl(StringView identifier, StringView parsedType)
            : Identifier(identifier), ParsedType(parsedType) {}

        StringView Identifier;
        StringView ParsedType;
        TypeInfo* Type = nullptr;
    };

    struct MethodDecl {
        MethodDecl(StringView identifier, StringView parsedType, TinyVector<Decl*> parameters, Stmt* body)
            : Identifier(identifier), Parameters(parameters), ParsedType(parsedType), Body(body) {}

        StringView Identifier;
        StringView ParsedType;
        TinyVector<Decl*> Parameters;
        Stmt* Body = nullptr;
        TypeInfo* Type = nullptr;
    };

    struct BuiltinCopyConstructorDecl {
        BuiltinCopyConstructorDecl(BuiltinKind kind)
            : Kind(kind) {}

        BuiltinKind Kind = BuiltinKind::String;
    };

    struct BuiltinDestructorDecl {
        BuiltinDestructorDecl(BuiltinKind kind)
            : Kind(kind) {}

        BuiltinKind Kind = BuiltinKind::String;
    };

    struct Decl {
        template <typename T>
        static inline Decl* Create(CompilationContext* ctx, SourceLocation loc, SourceRange range, DeclKind kind, T t) { return ctx->Allocate<Decl>(loc, range, kind, t); }

        DeclKind Kind = DeclKind::Invalid;

        SourceLocation Loc;
        SourceRange Range;

        union {
            TranslationUnitDecl TranslationUnit;
            VarDecl Var;
            ParamDecl Param;
            FunctionDecl Function;
            StructDecl Struct;
            FieldDecl Field;
            MethodDecl Method;
            BuiltinCopyConstructorDecl BuiltinCopyConstructor;
            BuiltinDestructorDecl BuiltinDestructor;
        };

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, TranslationUnitDecl translationUnit)
            : Loc(loc), Range(range), Kind(kind), TranslationUnit(translationUnit) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, VarDecl var)
            : Loc(loc), Range(range), Kind(kind), Var(var) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, ParamDecl param)
            : Loc(loc), Range(range), Kind(kind), Param(param) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, FunctionDecl function)
            : Loc(loc), Range(range), Kind(kind), Function(function) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, StructDecl struc)
            : Loc(loc), Range(range), Kind(kind), Struct(struc) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, FieldDecl field)
            : Loc(loc), Range(range), Kind(kind), Field(field) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, MethodDecl method)
            : Loc(loc), Range(range), Kind(kind), Method(method) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, BuiltinCopyConstructorDecl builtinCopyConstructor)
            : Loc(loc), Range(range), Kind(kind), BuiltinCopyConstructor(builtinCopyConstructor) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, BuiltinDestructorDecl builtinDestructor)
            : Loc(loc), Range(range), Kind(kind), BuiltinDestructor(builtinDestructor) {}
    };

} // namespace Aria::Internal
