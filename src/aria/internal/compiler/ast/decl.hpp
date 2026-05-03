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

    inline const char* decl_kind_to_string(DeclKind kind) {
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
            : stmts(stmts) {}

        TinyVector<Stmt*> stmts;
    };

    struct ModuleDecl {
        ModuleDecl(std::string_view name)
            : name(name) {}

        std::string_view name;
    };

    struct VarDecl {
        VarDecl(std::string_view identifier, TypeInfo* type, Expr* initializer, bool global)
            : identifier(identifier), type(type), initializer(initializer), global_var(global) {}

        std::string_view identifier;
        TypeInfo* type = nullptr;
        Expr* initializer = nullptr;
        bool global_var = false;
    };

    struct ParamDecl {
        ParamDecl(std::string_view identifier, TypeInfo* type)
            : identifier(identifier), type(type) {}

        std::string_view identifier;
        TypeInfo* type = nullptr;
    };

    struct FunctionDecl {
        enum class AttributeKind {
            None = 0,
            Extern,
            NoMangle,
            Unsafe,
        };

        struct Attribute {
            AttributeKind kind = AttributeKind::None;
            std::string_view arg;
        };

        FunctionDecl(std::string_view identifier, TypeInfo* type, TinyVector<Decl*> params, Stmt* body, TinyVector<Attribute> attrs)
            : identifier(identifier), type(type), parameters(params), body(body), attributes(attrs) {}

        std::string_view identifier;
        TypeInfo* type = nullptr;
        TinyVector<Decl*> parameters;
        Stmt* body = nullptr;
        TinyVector<Attribute> attributes;
    };

    struct StructDecl {
        struct DefinitionData {
            bool has_default_ctor : 1;
            bool has_user_default_ctor : 1;

            bool has_user_dtor : 1;
            bool trivial_dtor : 1;
        };

        StructDecl(std::string_view identifier, DefinitionData defd, TinyVector<Decl*> fields)
            : identifier(identifier), definition(defd), fields(fields) {}

        std::string_view identifier;
        DefinitionData definition;
        TinyVector<Decl*> fields;
    };

    struct FieldDecl {
        FieldDecl(std::string_view identifier, TypeInfo* type)
            : identifier(identifier), type(type) {}

        std::string_view identifier;
        TypeInfo* type = nullptr;
    };

    struct ConstructorDecl {
        ConstructorDecl(TinyVector<Decl*> parameters, Stmt* body)
            : parameters(parameters), body(body) {}

        TinyVector<Decl*> parameters;
        Stmt* body = nullptr;
    };

    struct DestructorDecl {
        DestructorDecl(Stmt* body)
            : body(body) {}

        Stmt* body = nullptr;
    };

    struct BuiltinCopyConstructorDecl {
        BuiltinCopyConstructorDecl(BuiltinKind kind)
            : kind(kind) {}

        BuiltinKind kind = BuiltinKind::String;
    };

    struct BuiltinDestructorDecl {
        BuiltinDestructorDecl(BuiltinKind kind)
            : kind(kind) {}

        BuiltinKind kind = BuiltinKind::String;
    };

    struct Decl {
        template <typename T>
        static inline Decl* Create(CompilationContext* ctx, SourceLocation loc, SourceRange range, DeclKind kind, T t = ErrorDecl{}) { return ctx->allocate<Decl>(loc, range, kind, t); }

        DeclKind kind = DeclKind::Invalid;

        SourceLocation loc;
        SourceRange range;

        union {
            ErrorDecl error;
            TranslationUnitDecl translation_unit;
            ModuleDecl module;
            VarDecl var;
            ParamDecl param;
            FunctionDecl function;
            StructDecl struct_;
            FieldDecl field;
            ConstructorDecl constructor;
            DestructorDecl destructor;
            BuiltinCopyConstructorDecl built_in_copy_constructor;
            BuiltinDestructorDecl built_in_destructor;
        };

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, ErrorDecl error)
            : loc(loc), range(range), kind(kind), error(error) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, TranslationUnitDecl translation_unit)
            : loc(loc), range(range), kind(kind), translation_unit(translation_unit) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, ModuleDecl module)
            : loc(loc), range(range), kind(kind), module(module) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, VarDecl var)
            : loc(loc), range(range), kind(kind), var(var) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, ParamDecl param)
            : loc(loc), range(range), kind(kind), param(param) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, FunctionDecl function)
            : loc(loc), range(range), kind(kind), function(function) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, StructDecl struc)
            : loc(loc), range(range), kind(kind), struct_(struc) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, FieldDecl field)
            : loc(loc), range(range), kind(kind), field(field) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, ConstructorDecl ctor)
            : loc(loc), range(range), kind(kind), constructor(ctor) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, DestructorDecl dtor)
            : loc(loc), range(range), kind(kind), destructor(dtor) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, BuiltinCopyConstructorDecl bicc)
            : loc(loc), range(range), kind(kind), built_in_copy_constructor(bicc) {}

        Decl(SourceLocation loc, SourceRange range, DeclKind kind, BuiltinDestructorDecl bid)
            : loc(loc), range(range), kind(kind), built_in_destructor(bid) {}
    };

    inline Decl error_decl = Decl(SourceLocation(), SourceRange(), DeclKind::Error, ErrorDecl());

} // namespace Aria::Internal
