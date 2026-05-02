#pragma once

#include "aria/internal/compiler/ast/stmt.hpp"

#include <string_view>

namespace Aria::Internal {

    struct Module;

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
        ModuleDecl(std::string_view name)
            : Name(name) {}

        std::string_view Name;
    };

    struct VarDecl {
        VarDecl(std::string_view identifier, TypeInfo* type, Expr* initializer, bool global)
            : Identifier(identifier), Type(type), Initializer(initializer), GlobalVar(global) {}

        std::string_view Identifier;
        TypeInfo* Type = nullptr;
        Expr* Initializer = nullptr;
        bool GlobalVar = false;
    };

    struct ParamDecl {
        ParamDecl(std::string_view identifier, TypeInfo* type)
            : Identifier(identifier), Type(type) {}

        std::string_view Identifier;
        TypeInfo* Type = nullptr;
    };

    struct FunctionDecl {
        enum class AttributeKind {
            None = 0,
            Extern,
            NoMangle,
            Unsafe,
        };

        struct Attribute {
            AttributeKind Kind = AttributeKind::None;
            std::string_view Arg;
        };

        FunctionDecl(std::string_view identifier, TypeInfo* type, TinyVector<Decl*> params, Stmt* body, TinyVector<Attribute> attrs)
            : Identifier(identifier), Type(type), Parameters(params), Body(body), Attributes(attrs) {}

        std::string_view Identifier;
        TypeInfo* Type = nullptr;
        TinyVector<Decl*> Parameters;
        Stmt* Body = nullptr;
        TinyVector<Attribute> Attributes;
    };

    struct StructDecl {
        struct DefinitionData {
            bool HasDefaultCtor : 1;
            bool HasUserDefaultCtor : 1;

            bool HasUserDtor : 1;
            bool TrivialDtor : 1;
        };

        StructDecl(std::string_view identifier, DefinitionData defd, TinyVector<Decl*> fields)
            : Identifier(identifier), Definition(defd), Fields(fields) {}

        std::string_view Identifier;
        DefinitionData Definition;
        TinyVector<Decl*> Fields;
    };

    struct FieldDecl {
        FieldDecl(std::string_view identifier, TypeInfo* type)
            : Identifier(identifier), Type(type) {}

        std::string_view Identifier;
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
        MethodDecl(std::string_view identifier, std::string_view parsedType, TinyVector<Decl*> parameters, Stmt* body)
            : Identifier(identifier), Parameters(parameters), ParsedType(parsedType), Body(body) {}

        std::string_view Identifier;
        std::string_view ParsedType;
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
        static inline Decl* Create(CompilationContext* ctx, SourceLocation loc, SourceRange range, DeclKind kind, T t = ErrorDecl{}) { return ctx->allocate<Decl>(loc, range, kind, t); }

        DeclKind Kind = DeclKind::Invalid;

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

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, ErrorDecl error)
            : Loc(loc), Range(range), Kind(kind), Error(error) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, TranslationUnitDecl translationUnit)
            : Loc(loc), Range(range), Kind(kind), TranslationUnit(translationUnit) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, ModuleDecl module)
            : Loc(loc), Range(range), Kind(kind), Module(module) {}

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

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, ConstructorDecl ctor)
            : Loc(loc), Range(range), Kind(kind), Constructor(ctor) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, DestructorDecl dtor)
            : Loc(loc), Range(range), Kind(kind), Destructor(dtor) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, MethodDecl method)
            : Loc(loc), Range(range), Kind(kind), Method(method) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, BuiltinCopyConstructorDecl builtinCopyConstructor)
            : Loc(loc), Range(range), Kind(kind), BuiltinCopyConstructor(builtinCopyConstructor) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, BuiltinDestructorDecl builtinDestructor)
            : Loc(loc), Range(range), Kind(kind), BuiltinDestructor(builtinDestructor) {}
    };

    inline Decl error_decl = Decl(SourceLocation(), SourceRange(), DeclKind::Error, ErrorDecl());

} // namespace Aria::Internal
