#pragma once

#include <aria/internal/compiler/ast/stmt.hpp>

namespace Aria::Internal {

    enum class DeclKind {
        Invalid = 0,

        Error,
        TranslationUnit,
        Module,
        Var,
        Param,
        Function,
        OverloadedFunction,
        Struct,
        Field,
        Constructor,
        Destructor,
        Method,
        BuiltinCopyConstructor,
        BuiltinDestructor
    };

    inline const char* DeclKindToString(DeclKind kind) {
        switch (kind) {
            case DeclKind::Error: return "Error";

            case DeclKind::Var: return "Var";
            case DeclKind::Param: return "Param";
            case DeclKind::Function: return "Function";
            case DeclKind::OverloadedFunction: return "OverloadedFunction";
            case DeclKind::Struct: return "Struct";

            default: ARIA_UNREACHABLE();
        }
    }

    enum class BuiltinKind {
        String
    };

    constexpr int DECL_FLAG_EXTERN = 0x01;
    constexpr int DECL_FLAG_NOMANGLE = 0x02;
    constexpr int DECL_FLAG_PRIVATE = 0x04;

    struct Expr;
    struct Stmt;

    struct ErrorDecl {
        ErrorDecl() = default;
    };

    // Represents an entire translation unit
    // This should always be the root node of the AST
    struct TranslationUnitDecl {
        TranslationUnitDecl(TinyVector<Stmt*> stmts)
            : Stmts(stmts) {}

        TinyVector<Stmt*> Stmts;
    };

    struct ModuleDecl {
        ModuleDecl(StringView name)
            : Name(name) {}

        StringView Name;
    };

    struct VarDecl {
        VarDecl(StringView identifier, TypeInfo* type, Expr* initializer, bool global)
            : Identifier(identifier), Type(type), Initializer(initializer), GlobalVar(global) {}

        StringView Identifier;
        TypeInfo* Type = nullptr;
        Expr* Initializer = nullptr;
        bool GlobalVar = false;
    };

    struct ParamDecl {
        ParamDecl(StringView identifier, TypeInfo* type)
            : Identifier(identifier), Type(type) {}

        StringView Identifier;
        TypeInfo* Type = nullptr;
    };

    struct FunctionDecl {
        FunctionDecl(StringView identifier, TypeInfo* type, TinyVector<Decl*> params, Stmt* body)
            : Identifier(identifier), Type(type), Parameters(params), Body(body) {}

        StringView Identifier;
        TypeInfo* Type = nullptr;
        TinyVector<Decl*> Parameters;
        Stmt* Body = nullptr;
    };

    struct StructDecl {
        struct DefinitionData {
            bool HasDefaultCtor : 1;
            bool HasUserDefaultCtor : 1;

            bool HasUserDtor : 1;
            bool TrivialDtor : 1;
        };

        StructDecl(StringView identifier, DefinitionData defd, TinyVector<Decl*> fields)
            : Identifier(identifier), Definition(defd), Fields(fields) {}

        StringView Identifier;
        DefinitionData Definition;
        TinyVector<Decl*> Fields;
    };

    struct FieldDecl {
        FieldDecl(StringView identifier, TypeInfo* type)
            : Identifier(identifier), Type(type) {}

        StringView Identifier;
        TypeInfo* Type = nullptr;
    };

    struct ConstructorDecl {
        ConstructorDecl(TinyVector<Decl*> parameters, Stmt* body)
            : Parameters(parameters), Body(body) {}

        TinyVector<Decl*> Parameters;
        Stmt* Body = nullptr;
    };

    struct DestructorDecl {
        DestructorDecl(Stmt* body)
            : Body(body) {}

        Stmt* Body = nullptr;
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
        static inline Decl* Create(CompilationContext* ctx, SourceLocation loc, SourceRange range, DeclKind kind, int flags, T t = ErrorDecl{}) { return ctx->Allocate<Decl>(loc, range, kind, flags, t); }

        DeclKind Kind = DeclKind::Invalid;
        int Flags = 0;

        SourceLocation Loc;
        SourceRange Range;

        union {
            ErrorDecl Error;
            TranslationUnitDecl TranslationUnit;
            ModuleDecl Module;
            VarDecl Var;
            ParamDecl Param;
            FunctionDecl Function;
            StructDecl Struct;
            FieldDecl Field;
            ConstructorDecl Constructor;
            DestructorDecl Destructor;
            MethodDecl Method;
            BuiltinCopyConstructorDecl BuiltinCopyConstructor;
            BuiltinDestructorDecl BuiltinDestructor;
        };

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, int flags, ErrorDecl error)
            : Loc(loc), Range(range), Kind(kind), Flags(flags), Error(error) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, int flags, TranslationUnitDecl translationUnit)
            : Loc(loc), Range(range), Kind(kind), Flags(flags), TranslationUnit(translationUnit) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, int flags, ModuleDecl module)
            : Loc(loc), Range(range), Kind(kind), Flags(flags), Module(module) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, int flags, VarDecl var)
            : Loc(loc), Range(range), Kind(kind), Flags(flags), Var(var) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, int flags, ParamDecl param)
            : Loc(loc), Range(range), Kind(kind), Flags(flags), Param(param) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, int flags, FunctionDecl function)
            : Loc(loc), Range(range), Kind(kind), Flags(flags), Function(function) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, int flags, StructDecl struc)
            : Loc(loc), Range(range), Kind(kind), Flags(flags), Struct(struc) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, int flags, FieldDecl field)
            : Loc(loc), Range(range), Kind(kind), Flags(flags), Field(field) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, int flags, ConstructorDecl ctor)
            : Loc(loc), Range(range), Kind(kind), Flags(flags), Constructor(ctor) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, int flags, DestructorDecl dtor)
            : Loc(loc), Range(range), Kind(kind), Flags(flags), Destructor(dtor) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, int flags, MethodDecl method)
            : Loc(loc), Range(range), Kind(kind), Flags(flags), Method(method) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, int flags, BuiltinCopyConstructorDecl builtinCopyConstructor)
            : Loc(loc), Range(range), Kind(kind), Flags(flags), BuiltinCopyConstructor(builtinCopyConstructor) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, int flags, BuiltinDestructorDecl builtinDestructor)
            : Loc(loc), Range(range), Kind(kind), Flags(flags), BuiltinDestructor(builtinDestructor) {}
    };

    inline Decl g_ErrorDecl = Decl(SourceLocation(), SourceRange(), DeclKind::Error, 0, ErrorDecl());

} // namespace Aria::Internal
